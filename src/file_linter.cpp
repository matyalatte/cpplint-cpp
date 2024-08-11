#include "file_linter.h"
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <utility>
#include <vector>
#include "c_header_list.h"
#include "cleanse.h"
#include "common.h"
#include "cpplint_state.h"
#include "error_suppressions.h"
#include "getline.h"
#include "line_utils.h"
#include "nest_info.h"
#include "options.h"
#include "regex_utils.h"
#include "states.h"
#include "string_utils.h"

namespace fs = std::filesystem;

#define IS_SPACE(c) isspace((uint8_t)(c))

void FileLinter::CheckForCopyright(const std::vector<std::string>& lines) {
    // We'll say it should occur by line 10. Don't forget there's a
    // placeholder line at the front.
    static const regex_code RE_PATTERN_COPYRIGHT =
        RegexCompile("Copyright", REGEX_OPTIONS_ICASE);
    for (size_t i = 1; i < MIN(lines.size(), 11); i++) {
        if (RegexSearch(RE_PATTERN_COPYRIGHT, lines[i], m_re_result_temp)) {
            return;
        }
    }

    // no copyright line was found
    Error(0, "legal/copyright", 5,
          "No copyright message found.  "
          "You should have a line: \"Copyright [year] <Copyright Owner>\"");
}

static size_t FindNextMultiLineCommentStart(const std::vector<std::string>& lines,
                                            size_t lineix) {
    // Find the beginning marker for a multiline comment.
    while (lineix < lines.size()) {
        const std::string& line = lines[lineix];
        size_t pos = GetFirstNonSpacePos(line);
        if (pos != INDEX_NONE) {
            const char* lp = &line[pos];
            if (*lp == '/' && *(lp + 1) == '*') {
                // Only return this marker if the comment goes beyond this line
                if (line.find("*/", pos + 2) == std::string::npos)
                    return lineix;
            }
        }
        lineix++;
    }
    return lines.size();
}

static size_t FindNextMultiLineCommentEnd(const std::vector<std::string>& lines,
                                          size_t lineix) {
    // We are inside a comment, find the end marker.
    while (lineix < lines.size()) {
        std::string stripped = StrStrip(lines[lineix]);
        if (stripped.ends_with("*/"))
            return lineix;
        lineix += 1;
    }
    return lines.size();
}

void FileLinter::RemoveMultiLineComments(std::vector<std::string>& lines) {
    size_t lineix = 0;
    while (lineix < lines.size()) {
        size_t lineix_begin = FindNextMultiLineCommentStart(lines, lineix);
        if (lineix_begin >= lines.size()) {
            return;
        }
        size_t lineix_end = FindNextMultiLineCommentEnd(lines, lineix_begin);
        if (lineix_end >= lines.size()) {
            Error(lineix_begin + 1, "readability/multiline_comment", 5,
                  "Could not find end of multi-line comment");
            return;
        }

        // Clears a range of lines for multi-line comments.
        // Having // <empty> comments makes the lines non-empty, so we will not get
        // unnecessary blank line warnings later in the code.
        for (size_t i = lineix_begin; i < lineix_end + 1; i++) {
            lines[i] = "/**/";
        }

        lineix = lineix_end + 1;
    }
}

void FileLinter::CheckForNewlineAtEOF(const std::vector<std::string>& lines) {
    // The array lines() was created by adding two newlines to the original file.
    // To verify that the file ends in \n, we just have to make sure the
    // last-but-two element of lines() exists and is not empty.
    if (lines.size() < 3 || !lines[lines.size() - 2].empty()) {
        Error(lines.size() - 2, "whitespace/ending_newline", 5,
              "Could not find a newline character at the end of the file.");
    }

    /* TODO: a new rule for redundant lines at EOF?
    if (lines.size() >= 4 && lines[lines.size() - 2].empty() && lines[lines.size() - 3].empty()) {
        Error(lines.size() - 2, "whitespace/ending_newline", 5,
              "Found multiple newline characters at the end of the file.");
    }
    */
}

void FileLinter::ParseNolintSuppressions(const std::string& raw_line,
                                         size_t linenum) {
    // TODO(matyalatte): Use this function only for comment lines
    bool match = RegexSearch(R"(\bNOLINT(NEXTLINE|BEGIN|END)?\b(\([^)]+\))?)", raw_line, m_re_result);
    if (match) {
        std::string no_lint_type = GetMatchStr(m_re_result, raw_line, 1);
        std::function<void(const std::string&, size_t)> ProcessCategory;

        if (no_lint_type == "NEXTLINE") {
            ProcessCategory = [this](const std::string& category, size_t linenum) {
                m_error_suppressions.AddLineSuppression(category, linenum + 1);
            };
        } else if (no_lint_type == "BEGIN") {
            if (m_error_suppressions.HasOpenBlock()) {
                Error(linenum, "readability/nolint", 5,
                      "NONLINT block already defined on line " +
                      m_error_suppressions.GetOpenBlockStart());
            }
            ProcessCategory = [this](const std::string& category, size_t linenum) {
                m_error_suppressions.StartBlockSuppression(category, linenum);
            };
        } else if (no_lint_type == "END") {
            if (!m_error_suppressions.HasOpenBlock())
                Error(linenum, "readability/nolint", 5, "Not in a NOLINT block");

            ProcessCategory = [this](const std::string& category, size_t linenum) {
                if (!category.empty()) {
                    Error(linenum, "readability/nolint", 5,
                          "NOLINT categories not supported in block END: " + category);
                }
                m_error_suppressions.EndBlockSuppression(linenum);
            };
        } else {
            ProcessCategory = [this](const std::string& category, size_t linenum) {
                m_error_suppressions.AddLineSuppression(category, linenum);
            };
        }
        std::string categories = GetMatchStr(m_re_result, raw_line, 2);
        if (categories.empty() || categories == "(*)") {  // => "suppress all"
            ProcessCategory("", linenum);
        } else if (categories.starts_with('(') && categories.ends_with(')')) {
            categories = categories.substr(1, categories.size() - 2);
            std::set<std::string> category_set = ParseCommaSeparetedList(categories);
            for (const std::string& category : category_set) {
                if (InErrorCategories(category)) {
                    ProcessCategory(category, linenum);
                } else if (!InOtherNolintCategories(category) &&
                           !InLegacyErrorCategories(category)) {
                    Error(linenum, "readability/nolint", 5,
                          "Unknown NOLINT error category: " + category);
                }
            }
        }
    }
}

void FileLinter::ProcessGlobalSuppressions(const std::vector<std::string>& lines) {
    static const regex_code RE_SEARCH_C_FILE =
        RegexCompile(R"(\b(?:LINT_C_FILE|)"
                     R"(vim?:\s*.*(\s*|:)filetype=c(\s*|:|$)))");
    static const regex_code RE_SEARCH_KERNEL_FILE =
        RegexCompile(R"(\b(?:LINT_KERNEL_FILE))");
    for (const std::string& line : lines) {
        if (RegexSearch(RE_SEARCH_C_FILE, line, m_re_result_temp))
            m_error_suppressions.AddDefaultCSuppressions();
        if (RegexSearch(RE_SEARCH_KERNEL_FILE, line, m_re_result_temp))
            m_error_suppressions.AddDefaultKernelSuppressions();
    }
}

// Make a path relative from a repository path specified with --repository
fs::path FileLinter::GetRelativeFromRepository(const fs::path& file, const fs::path& repository) {
    fs::path project_dir = file.parent_path();

    // If the user specified a repository path, it exists, and the file is
    // contained in it, use the specified repository path
    if (!repository.empty()) {
        fs::path repo = fs::canonical(repository).make_preferred();
        fs::path root_dir = project_dir;
        fs::path one_up_dir;
        while (fs::exists(root_dir)) {
            // allow case insensitive compare on Windows
        #ifdef _WIN32
            if (StrToLower(root_dir.string()) == StrToLower(repo.string())) {
        #else
            if (root_dir == repo) {
        #endif
                return fs::relative(file, root_dir);
            }
            one_up_dir = root_dir.parent_path();
            if (one_up_dir == root_dir)
                break;
            root_dir = one_up_dir;
        }
    }

    if (fs::exists(project_dir / ".svn")) {
        // If there's a .svn file in the current directory, we recursively look
        // up the directory tree for the top of the SVN checkout
        fs::path root_dir = project_dir;
        fs::path one_up_dir = root_dir.parent_path();
        while (fs::exists(one_up_dir / ".svn") && one_up_dir != root_dir) {
            root_dir = one_up_dir;
            one_up_dir = one_up_dir.parent_path();
        }

        return fs::relative(file, root_dir);
    }

    // Not SVN <= 1.6? Try to find a git, hg, or svn top level directory by
    // searching up from the current path.
    fs::path current_dir = project_dir;
    fs::path one_up_dir = current_dir.parent_path();
    while (current_dir != one_up_dir) {
        if (fs::exists(current_dir / ".git") ||
            fs::exists(current_dir / ".hg") ||
            fs::exists(current_dir / ".svn")) {
            project_dir = current_dir;
            break;
        }
        current_dir = one_up_dir;
        one_up_dir = one_up_dir.parent_path();
    }

    return fs::relative(file, project_dir);
}

// Make a path relative from a sub dir specified with --root
// FixupPathFromRoot() in cpplint.py
fs::path FileLinter::GetRelativeFromSubdir(const fs::path& file, const fs::path& subdir) {
    if (subdir.empty()) {
        return file;  // --root is not specified.
    }

    // Try to make file relative from subdir.
    fs::path subdir_pref = fs::path(subdir).make_preferred();
    if (file.string().starts_with(subdir_pref.string())) {
        return fs::relative(file, subdir_pref);
    }

    // Try it again with absolute paths.
    fs::path file_abs = fs::canonical(file);
    fs::path subdir_abs = fs::canonical(subdir_pref);
    if (file_abs.string().starts_with(subdir_abs.string())) {
        return fs::relative(file_abs, subdir_abs);
    }

    // --root option is ignored.
    return file;
}

// Returns the CPP variable that should be used as a header guard.
std::string FileLinter::GetHeaderGuardCPPVariable() {
    std::string cppvar;
    std::string new_filename = m_filename;
    // Restores original filename in case that cpplint is invoked from Emacs's
    // flymake.
    static const regex_code RE_PATTERN_FLYMAKE =
        RegexCompile(R"(_flymake\.h$)");
    RegexReplace(RE_PATTERN_FLYMAKE, ".h", &new_filename, m_re_result_temp);
    static const regex_code RE_PATTERN_FLYMAKE2 =
        RegexCompile(R"(/\.flymake/([^/]*)$)");
    RegexReplace(RE_PATTERN_FLYMAKE2, R"(/\1)", &new_filename, m_re_result_temp);

    // Replace 'c++' with 'cpp'.
    new_filename = StrReplaceAll(new_filename, "C++", "cpp");
    new_filename = StrReplaceAll(new_filename, "c++", "cpp");

    fs::path file = new_filename;
    fs::path file_from_repo = GetRelativeFromRepository(file, m_options.Repository());
    fs::path file_path_from_root = GetRelativeFromSubdir(file_from_repo, m_options.Root());
    std::string root_str = file_path_from_root.string();
    static const regex_code RE_PATTERN_ALPHANUM =
        RegexCompile("[^a-zA-Z0-9]");
    RegexReplace(RE_PATTERN_ALPHANUM, "_", &root_str, m_re_result_temp);
    cppvar = StrToUpper(root_str) + "_";
    return cppvar;
}

void FileLinter::CheckForHeaderGuard(const CleansedLines& clean_lines) {
    // Don't check for header guards if there are error suppression
    // comments somewhere in this file.
    //
    // Because this is silencing a warning for a nonexistent line, we
    // only support the very specific NOLINT(build/header_guard) syntax,
    // and not the general NOLINT or NOLINT(*) syntax.
    const std::vector<std::string>& raw_lines = clean_lines.GetLinesWithoutRawStrings();
    static const regex_code RE_PATTERN_NOLINT_HEADER =
        RegexCompile(R"(//\s*NOLINT\(build/header_guard\))");
    for (const std::string& line : raw_lines) {
        if (RegexSearch(RE_PATTERN_NOLINT_HEADER, line, m_re_result_temp))
            return;
    }

    // Allow pragma once instead of header guards
    static const regex_code RE_PATTERN_PRAGMA_ONCE =
        RegexCompile(R"(^\s*#pragma\s+once)");
    for (const std::string& line : raw_lines) {
        if (RegexSearch(RE_PATTERN_PRAGMA_ONCE, line, m_re_result_temp))
            return;
    }

    std::string ifndef = "";
    size_t ifndef_linenum = 0;
    std::string define = "";
    std::string endif = "";
    size_t endif_linenum = 0;
    size_t linenum = 0;
    for (const std::string& line : raw_lines) {
        if (line[0] == '#') {
            std::vector<std::string> linesplit = StrSplit(line, 2);
            if (linesplit.size() >= 2) {
                // find the first occurrence of #ifndef and #define, save arg
                if (ifndef.empty() && linesplit[0] == "#ifndef") {
                    // set ifndef to the header guard presented on the #ifndef line.
                    ifndef = linesplit[1];
                    ifndef_linenum = linenum;
                }
                if (define.empty() && linesplit[0] == "#define")
                    define = linesplit[1];
            }
            // find the last occurrence of #endif, save entire line
            if (line.starts_with("#endif")) {
                endif = line;
                endif_linenum = linenum;
            }
        }
        linenum++;
    }

    if (ifndef.empty() || define.empty() || (ifndef != define)) {
        Error(0, "build/header_guard", 5,
              "No #ifndef header guard found, suggested CPP variable is: " + m_cppvar);
        return;
    }

    // The guard should be PATH_FILE_H_, but we also allow PATH_FILE_H__
    // for backward compatibility.
    if (ifndef != m_cppvar) {
        int error_level = 0;
        if (ifndef != m_cppvar + "_")
            error_level = 5;

        Error(ifndef_linenum, "build/header_guard", error_level,
              "#ifndef header guard has wrong style, please use: " + m_cppvar);
    }

    // Check for "//" comments on endif line.
    bool match = RegexMatch(R"(#endif\s*//\s*)" + m_cppvar + R"((_)?\b)",
                            endif, m_re_result);
    if (match) {
        if (StrIsChar(GetMatchStr(m_re_result, endif, 1), '_')) {
            // Issue low severity warning for deprecated double trailing underscore
            Error(endif_linenum, "build/header_guard", 0,
                  "#endif line should be \"#endif  // " + m_cppvar + "\"");
        }
        return;
    }

    // Didn't find the corresponding "//" comment.  If this file does not
    // contain any "//" comments at all, it could be that the compiler
    // only wants "/**/" comments, look for those instead.
    static const regex_code RE_PATTERN_MULTILINE_COMMENT =
        RegexCompile(R"(^(?:(?:\'(?:\.|[^\'])*\')|(?:"(?:\.|[^"])*")|[^\'"])*//)");
    bool no_single_line_comments = true;
    for (size_t i = 1; i < raw_lines.size() - 1; i++) {
        const std::string& line = raw_lines[i];
        if (RegexMatch(RE_PATTERN_MULTILINE_COMMENT, line, m_re_result_temp)) {
            no_single_line_comments = false;
            break;
        }
    }

    if (no_single_line_comments) {
        std::string pattern = R"(#endif\s*/\*\s*)" + m_cppvar + R"((_)?\s*\*/)";
        match = RegexMatch(pattern, endif, m_re_result);
        if (match) {
            if (StrIsChar(GetMatchStr(m_re_result, endif, 1), '_')) {
                // Low severity warning for double trailing underscore
                Error(endif_linenum, "build/header_guard", 0,
                      "#endif line should be \"#endif  /* " + m_cppvar + " */\"");
            }
            return;
        }
    }

    // Didn't find anything
    Error(endif_linenum, "build/header_guard", 5,
          "#endif line should be \"#endif  // " + m_cppvar + "\"");
}

bool FileLinter::IsForwardClassDeclaration(const std::string& elided_line) {
    static const regex_code RE_PATTERN_CLASS_DECL =
        RegexCompile(R"(^\s*(\btemplate\b)*.*class\s+\w+;\s*$)");
    return RegexMatch(RE_PATTERN_CLASS_DECL, elided_line, m_re_result_temp);
}

bool FileLinter::IsMacroDefinition(const CleansedLines& clean_lines,
                                   const std::string& elided_line, size_t linenum) {
    if (elided_line.starts_with("#define"))
        return true;

    if (linenum > 0 && RegexSearch(R"(\\$)",
                                   clean_lines.GetElidedAt(linenum - 1),
                                   m_re_result_temp))
        return true;

    return false;
}

void FileLinter::CheckForNamespaceIndentation(const CleansedLines& clean_lines,
                                              const std::string& elided_line, size_t linenum,
                                              NestingState* nesting_state) {
    bool is_namespace_indent_item = nesting_state->IsNamespaceIndentInfo();
    bool is_forward_declaration = IsForwardClassDeclaration(elided_line);
    if (!is_namespace_indent_item && !is_forward_declaration)
        return;

    if (IsMacroDefinition(clean_lines, elided_line, linenum) ||
        !nesting_state->IsBlockInNameSpace(is_forward_declaration))
        return;

    if (IS_SPACE(elided_line[0])) {
        Error(linenum, "whitespace/indent_namespace", 4,
              "Do not indent within a namespace.");
    }
}

void FileLinter::CheckForFunctionLengths(const CleansedLines& clean_lines, size_t linenum,
                                         FunctionState* function_state) {
    const std::string& line = clean_lines.GetLineAt(linenum);
    std::string joined_line = "";

    bool starting_func = false;
    // decls * & space::name( ...
    static const regex_code RE_PATTERN_SPACE_NAME =
        RegexCompile(R"((\w(\w|::|\*|\&|\s)*)\()");
    bool match = RegexMatch(RE_PATTERN_SPACE_NAME,
                            line, m_re_result);
    if (match) {
        // If the name is all caps and underscores, figure it's a macro and
        // ignore it, unless it's TEST or TEST_F.
        std::string function_name = StrSplit(GetMatchStr(m_re_result, line, 1)).back();
        if (function_name == "TEST" || function_name == "TEST_F" ||
                !RegexMatch("[A-Z_]+$", function_name, m_re_result_temp))
            starting_func = true;
    }

    static const regex_code RE_PATTERN_FUNC_END =
        RegexCompile(R"(^\}\s*$)");
    if (starting_func) {
        bool body_found = false;
        for (size_t start_linenum = linenum;
                start_linenum < clean_lines.NumLines(); start_linenum++) {
            const std::string& start_line = clean_lines.GetLineAt(start_linenum);
            joined_line += " " + StrLstrip(start_line);
            if (StrContain(start_line, ';') || StrContain(start_line, '}')) {
                // Declarations and trivial functions
                body_found = true;
                break;
            }
            if (StrContain(start_line, '{')) {
                body_found = true;
                bool search = RegexSearch(R"(((\w|:)*)\()",
                                          line, m_re_result);
                if (!search)
                    break;
                std::string function = GetMatchStr(m_re_result, line, 1);
                if (function.starts_with("TEST")) {    // Handle TEST... macros
                    search = RegexSearch(R"((\(.*\)))",
                                         joined_line, m_re_result);
                    if (search)             // Ignore bad syntax
                        function += GetMatchStr(m_re_result, joined_line, 1);
                } else {
                    function += "()";
                }
                function_state->Begin(function);
                break;
            }
        }
        if (!body_found) {
            // No body for the function (or evidence of a non-function) was found.
            Error(linenum, "readability/fn_size", 5,
                  "Lint failed to find start of function body.");
        }
    } else if (RegexMatch(RE_PATTERN_FUNC_END, line, m_re_result_temp)) {  // function end
        function_state->Check(this, linenum, m_cpplint_state->VerboseLevel());
        function_state->End();
    } else if (!StrIsBlank(line)) {
        function_state->Count();  // Count non-blank/non-comment lines.
    }
}

void FileLinter::CheckForMultilineCommentsAndStrings(const std::string& elided_line,
                                                     size_t linenum) {
    // Remove all \\ (escaped backslashes) from the line. They are OK, and the
    // second (escaped) slash may trigger later \" detection erroneously.
    bool escaped = false;
    char last_char = ' ';
    int quote_count = 0;  // counter for "
    int comment_open_count = 0;  // counter for /*
    int comment_close_count = 0;  // counter for */

    for (const char c : elided_line) {
        if (escaped) {
            escaped = false;
            last_char = ' ';
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        } else if (c == '"') {
            quote_count++;
        } else if (c == '*') {
            if (last_char == '/')
                comment_open_count++;
        } else if (c == '/') {
            if (last_char == '*')
                comment_close_count++;
        }
        last_char = c;
    }

    if (comment_open_count > comment_close_count) {
        Error(linenum, "readability/multiline_comment", 5,
              "Complex multi-line /*...*/-style comment found. "
              "Lint may give bogus warnings.  "
              "Consider replacing these with //-style comments, "
              "with #if 0...#endif, "
              "or with more clearly structured multi-line comments.");
    }

    if (quote_count % 2) {
        Error(linenum, "readability/multiline_string", 5,
              "Multi-line string (\"...\") found.  This lint script doesn\'t "
              "do well with such strings, and may give bogus warnings.  "
              "Use C++11 raw strings or concatenation instead.");
    }
}

static const std::string& GetPreviousNonBlankLine(const CleansedLines& clean_lines,
                                                  size_t linenum) {
    // Return the most recent non-blank line and its line number.
    static const std::string empty("");
    if (linenum == 0)
        return empty;

    size_t prevlinenum = linenum - 1;
    while (true) {
        const std::string& prevline = clean_lines.GetElidedAt(prevlinenum);
        if (!StrIsBlank(prevline))     // if not a blank line...
            return prevline;
        if (prevlinenum == 0)
            break;
        prevlinenum--;
    }
    return empty;
}

void FileLinter::CheckBraces(const CleansedLines& clean_lines,
                             const std::string& elided_line, size_t linenum) {
    // get rid of comments and strings
    const std::string& line = elided_line;

    static const regex_code RE_PATTERN_OPEN_BRACE =
        RegexCompile(R"(\s*{\s*$)");
    if (RegexMatch(RE_PATTERN_OPEN_BRACE, line, m_re_result_temp)) {
        // We allow an open brace to start a line in the case where someone is using
        // braces in a block to explicitly create a new scope, which is commonly used
        // to control the lifetime of stack-allocated variables.  Braces are also
        // used for brace initializers inside function calls.  We don't detect this
        // perfectly: we just don't complain if the last non-whitespace character on
        // the previous non-blank line is ',', ';', ':', '(', '{', or '}', or if the
        // previous line starts a preprocessor block. We also allow a brace on the
        // following line if it is part of an array initialization and would not fit
        // within the 80 character limit of the preceding line.
        const std::string& prevline = GetPreviousNonBlankLine(clean_lines, linenum);
        if (!RegexSearch(R"([,;:}{(]\s*$)", prevline, m_re_result_temp) &&
            !CheckFirstNonSpace(prevline, '#') &&
            !(GetLineWidth(prevline) > m_options.LineLength() - 2 && StrContain(prevline, "[]"))) {
            Error(linenum, "whitespace/braces", 4,
                  "{ should almost always be at the end of the previous line");
        }
    }

    // An else clause should be on the same line as the preceding closing brace.
    static const regex_code RE_PATTERN_ELSE_AFTER_BRACE =
        RegexCompile(R"(\s*else\b\s*(?:if\b|\{|$))");
    bool last_wrong = RegexMatch(RE_PATTERN_ELSE_AFTER_BRACE, line, m_re_result);
    if (last_wrong) {
        const std::string& prevline = GetPreviousNonBlankLine(clean_lines, linenum);
        if (RegexMatch(R"(\s*}\s*$)", prevline, m_re_result_temp)) {
            Error(linenum, "whitespace/newline", 4,
                  "An else should appear on the same line as the preceding }");
        } else {
            last_wrong = false;
        }
    }

    // If braces come on one side of an else, they should be on both.
    // However, we have to worry about "else if" that spans multiple lines!
    static const regex_code RE_PATTERN_ELSE_IF =
        RegexCompile(R"(else if\s*\()");
    static const regex_code RE_PATTERN_ELSE_BRACE_L =
        RegexCompile(R"(}\s*else[^{]*$)");
    static const regex_code RE_PATTERN_ELSE_BRACE_R =
        RegexCompile(R"([^}]*else\s*{)");
    if (RegexSearch(RE_PATTERN_ELSE_IF, line, m_re_result_temp)) {       // could be multi-line if
        bool brace_on_left = RegexSearch(R"(}\s*else if\s*\()", line, m_re_result_temp);
        // find the ( after the if
        size_t pos = line.find("else if");
        pos = line.find('(', pos);
        if (pos > 0 && pos != std::string::npos) {
            size_t endlinenum = linenum;
            size_t endpos = pos;
            const std::string& endline = CloseExpression(clean_lines, &endlinenum, &endpos);
            bool brace_on_right = endpos != INDEX_NONE &&
                                  endline.substr(endpos).find('{') != std::string::npos;
            if (brace_on_left != brace_on_right) {    // must be brace after if
                Error(linenum, "readability/braces", 5,
                      "If an else has a brace on one side, it should have it on both");
            }
        }

    // Prevent detection if statement has { and we detected an improper newline after }
    } else if (RegexSearch(RE_PATTERN_ELSE_BRACE_L, line,
                           m_re_result_temp) ||
               (RegexMatch(RE_PATTERN_ELSE_BRACE_R, line,
                           m_re_result_temp) && !last_wrong)) {
        Error(linenum, "readability/braces", 5,
              "If an else has a brace on one side, it should have it on both");
    }

    // No control clauses with braces should have its contents on the same line
    // Exclude } which will be covered by empty-block detect
    // Exclude ; which may be used by while in a do-while
    static const regex_code RE_PATTERN_CONTROL_PARENS =
        RegexCompile(
            R"(\b(else if|if|while|for|switch))"  // These have parens
            R"(\s*\(.*\)\s*(?:\[\[(?:un)?likely\]\]\s*)?{\s*[^\s\\};])");
    bool search = RegexSearch(RE_PATTERN_CONTROL_PARENS, line,
                              m_re_result);
    if (search) {
        Error(linenum, "whitespace/newline", 5,
              "Controlled statements inside brackets of " +
              GetMatchStr(m_re_result, line, 1) + " clause"
              " should be on a separate line");
    } else {
        static const regex_code RE_PATTERN_CONTROL_NO_PARENS =
            RegexCompile(
                R"(\b(else|do|try))"  // These don't have parens
                R"(\s*(?:\[\[(?:un)?likely\]\]\s*)?{\s*[^\s\\}])");
        search = RegexSearch(RE_PATTERN_CONTROL_NO_PARENS, line,
                             m_re_result);
        if (search) {
        Error(linenum, "whitespace/newline", 5,
              "Controlled statements inside brackets of " +
              GetMatchStr(m_re_result, line, 1) + " clause"
              " should be on a separate line");
        }
    }

    // TODO(unknown): Err on if...else and do...while statements without braces;
    // style guide has changed since the below comment was written

    // Check single-line if/else bodies. The style guide says 'curly braces are not
    // required for single-line statements'. We additionally allow multi-line,
    // single statements, but we reject anything with more than one semicolon in
    // it. This means that the first semicolon after the if should be at the end of
    // its line, and the line after that should have an indent level equal to or
    // lower than the if. We also check for ambiguous if/else nesting without
    // braces.
    static const regex_code RE_PATTERN_IF_ELSE =
        RegexCompile(R"(\b(if\s*(|constexpr)\s*\(|else\b))");
    bool if_else_match = RegexSearch(RE_PATTERN_IF_ELSE, line, m_re_result);
    if (if_else_match && !CheckFirstNonSpace(line, '#')) {
        size_t if_indent = GetIndentLevel(line);
        std::string endline = line;
        size_t endlinenum = linenum;
        size_t endpos = GetMatchEnd(m_re_result, 0);
        bool if_match = RegexSearch(R"(\bif\s*(|constexpr)\s*\()", line, m_re_result);
        if (if_match) {
            // This could be a multiline if condition, so find the end first.
            size_t pos = GetMatchEnd(m_re_result, 0) - 1;
            endpos = pos;
            endline = CloseExpression(clean_lines, &endlinenum, &endpos);
        }

        std::string endline_sub = "";
        if (endpos != INDEX_NONE) {
            endline_sub = endline.substr(endpos);
        }
        // Check for an opening brace, either directly after the if or on the next
        // line. If found, this isn't a single-statement conditional.
        if (!RegexMatch(R"(\s*(?:\[\[(?:un)?likely\]\]\s*)?{)", endline_sub, m_re_result_temp) &&
            !(StrIsBlank(endline_sub) &&
              endlinenum < (clean_lines.NumLines() - 1) &&
              CheckFirstNonSpace(clean_lines.GetElidedAt(endlinenum + 1), '{'))) {
            while (endlinenum < clean_lines.NumLines() &&
                   !(endpos != INDEX_NONE &&
                     StrContain(clean_lines.GetElidedAt(endlinenum).substr(endpos), ';'))) {
                endlinenum += 1;
                endpos = 0;
            }
            if (endlinenum < clean_lines.NumLines()) {
                endline = clean_lines.GetElidedAt(endlinenum);
                // We allow a mix of whitespace and closing braces (e.g. for one-liner
                // methods) and a single \ after the semicolon (for macros)
                endpos = endline.find(';');
                if (!RegexMatch(R"(;[\s}]*(\\?)$)", endline.substr(endpos), m_re_result_temp)) {
                    // Semicolon isn't the last character, there's something trailing.
                    // Output a warning if the semicolon is not contained inside
                    // a lambda expression.
                    if (!RegexMatch(R"(^[^{};]*\[[^\[\]]*\][^{}]*\{[^{}]*\}\s*\)*[;,]\s*$)", endline, m_re_result_temp)) {
                        Error(linenum, "readability/braces", 4,
                              "If/else bodies with multiple statements require braces");
                    }
                } else if (endlinenum < clean_lines.NumLines() - 1) {
                    // Make sure the next line is dedented
                    const std::string& next_line = clean_lines.GetElidedAt(endlinenum + 1);
                    size_t next_indent = GetIndentLevel(next_line);
                    // With ambiguous nested if statements, this will error out on the
                    // if that *doesn't* match the else, regardless of whether it's the
                    // inner one or outer one.
                    if (if_match && RegexMatch(R"(\s*else\b)", next_line, m_re_result_temp) &&
                        next_indent != if_indent) {
                        Error(linenum, "readability/braces", 4,
                              "Else clause should be indented at the same level as if. "
                              "Ambiguous nested if/else chains require braces.");
                    } else if (next_indent > if_indent) {
                        Error(linenum, "readability/braces", 4,
                              "If/else bodies with multiple statements require braces");
                    }
                }
            }
        }
    }
}

void FileLinter::CheckTrailingSemicolon(const CleansedLines& clean_lines,
                                        const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;

    // Block bodies should not be followed by a semicolon.  Due to C++11
    // brace initialization, there are more places where semicolons are
    // required than not, so we explicitly list the allowed rules rather
    // than listing the disallowed ones.  These are the places where "};"
    // should be replaced by just "}":
    // 1. Some flavor of block following closing parenthesis:
    //    for (;;) {};
    //    while (...) {};
    //    switch (...) {};
    //    Function(...) {};
    //    if (...) {};
    //    if (...) else if (...) {};
    //
    // 2. else block:
    //    if (...) else {};
    //
    // 3. const member function:
    //    Function(...) const {};
    //
    // 4. Block following some statement:
    //    x = 42;
    //    {};
    //
    // 5. Block at the beginning of a function:
    //    Function(...) {
    //      {};
    //    }
    //
    //    Note that naively checking for the preceding "{" will also match
    //    braces inside multi-dimensional arrays, but this is fine since
    //    that expression will not contain semicolons.
    //
    // 6. Block following another block:
    //    while (true) {}
    //    {};
    //
    // 7. End of namespaces:
    //    namespace {};
    //
    //    These semicolons seems far more common than other kinds of
    //    redundant semicolons, possibly due to people converting classes
    //    to namespaces.  For now we do not warn for this case.
    //
    // Try matching case 1 first.
    static const regex_code RE_PATTERN_CLOSING_PARENS =
        RegexCompile(R"(^(.*\)\s*)\{)");
    bool match = RegexMatch(RE_PATTERN_CLOSING_PARENS, line,
                            m_re_result);
    if (match) {
        // Matched closing parenthesis (case 1).  Check the token before the
        // matching opening parenthesis, and don't warn if it looks like a
        // macro.  This avoids these false positives:
        //  - macro that defines a base class
        //  - multi-line macro that defines a base class
        //  - macro that defines the whole class-head
        //
        // But we still issue warnings for macros that we know are safe to
        // warn, specifically:
        //  - TEST, TEST_F, TEST_P, MATCHER, MATCHER_P
        //  - TYPED_TEST
        //  - INTERFACE_DEF
        //  - EXCLUSIVE_LOCKS_REQUIRED, SHARED_LOCKS_REQUIRED, LOCKS_EXCLUDED:
        //
        // We implement a list of safe macros instead of a list of
        // unsafe macros, even though the latter appears less frequently in
        // google code and would have been easier to implement.  This is because
        // the downside for getting the allowed checks wrong means some extra
        // semicolons, while the downside for getting disallowed checks wrong
        // would result in compile errors.
        //
        // In addition to macros, we also don't want to warn on
        //  - Compound literals
        //  - Lambdas
        //  - alignas specifier with anonymous structs
        //  - decltype
        size_t endlinenum = linenum;
        size_t endpos = GetMatchStr(m_re_result, line, 1).rfind(')');
        const std::string& opening_parenthesis = ReverseCloseExpression(
                clean_lines, &endlinenum, &endpos);
        if (endpos != INDEX_NONE) {
            std::string line_prefix = opening_parenthesis.substr(0, endpos);
            static const regex_code RE_PATTERN_MACRO =
                RegexCompile(R"(\b([A-Z_][A-Z0-9_]*)\s*$)");
            thread_local regex_match macro_m = RegexCreateMatchData(RE_PATTERN_MACRO);
            bool macro = RegexSearch(RE_PATTERN_MACRO, line_prefix, macro_m);
            thread_local regex_match func_m = RegexCreateMatchData(1);
            bool func = RegexMatch(R"(^(.*\])\s*$)", line_prefix, func_m);
            if ((macro && !InStrVec({
                            "TEST", "TEST_F", "MATCHER", "MATCHER_P", "TYPED_TEST",
                            "EXCLUSIVE_LOCKS_REQUIRED", "SHARED_LOCKS_REQUIRED",
                            "LOCKS_EXCLUDED", "INTERFACE_DEF"},
                            GetMatchStr(macro_m, line_prefix, 1))) ||
                    (func && !RegexSearch(R"(\boperator\s*\[\s*\])",
                                GetMatchStr(func_m, line_prefix, 1), m_re_result_temp)) ||
                    RegexSearch(R"(\b(?:struct|union)\s+alignas\s*$)", line_prefix, m_re_result_temp) ||
                    RegexSearch(R"(\bdecltype$)", line_prefix, m_re_result_temp) ||
                    RegexSearch(R"(\s+=\s*$)", line_prefix, m_re_result_temp)) {
                match = false;
            }
        }
        if (match &&
                endlinenum > 1 &&
                RegexSearch(R"(\]\s*$)", clean_lines.GetElidedAt(endlinenum - 1), m_re_result_temp)) {
            // Multi-line lambda-expression
            match = false;
        }
    } else {
        // Try matching cases 2-3.
        static const regex_code RE_PATTERN_ELSE_BLOCK =
            RegexCompile(R"(^(.*(?:else|\)\s*const)\s*)\{)");
        match = RegexMatch(RE_PATTERN_ELSE_BLOCK, line,
                           m_re_result);
        if (!match) {
            // Try matching cases 4-6.  These are always matched on separate lines.
            //
            // Note that we can't simply concatenate the previous line to the
            // current line and do a single match, otherwise we may output
            // duplicate warnings for the blank line case:
            //   if (cond) {
            //     // blank line
            //   }
            const std::string& prevline = GetPreviousNonBlankLine(clean_lines, linenum);
            static const regex_code RE_PATTERN_TRAIL_SPACE =
                RegexCompile(R"([;{}]\s*$)");
            static const regex_code RE_PATTERN_BLOCK_START =
                RegexCompile(R"(^(\s*)\{)");
            if (!prevline.empty() && RegexSearch(RE_PATTERN_TRAIL_SPACE, prevline,
                                                 m_re_result_temp)) {
                match = RegexMatch(RE_PATTERN_BLOCK_START, line, m_re_result);
            }
        }
    }

    // Check matching closing brace
    if (match) {
        size_t endlinenum = linenum;
        size_t endpos = GetMatchEnd(m_re_result, 1);
        const std::string& endline = CloseExpression(
                                        clean_lines, &endlinenum, &endpos);
        if (endpos != INDEX_NONE && CheckFirstNonSpace(endline.substr(endpos), ';')) {
            // Current {} pair is eligible for semicolon check, and we have found
            // the redundant semicolon, output warning here.
            //
            // Note: because we are scanning forward for opening braces, and
            // outputting warnings for the matching closing brace, if there are
            // nested blocks with trailing semicolons, we will get the error
            // messages in reversed order.

            Error(endlinenum, "readability/braces", 4,
                  "You don't need a ; after a }");
        }
    }
}

void FileLinter::CheckEmptyBlockBody(const CleansedLines& clean_lines,
                                     const std::string& elided_line, size_t linenum) {
    // Search for loop keywords at the beginning of the line.  Because only
    // whitespaces are allowed before the keywords, this will also ignore most
    // do-while-loops, since those lines should start with closing brace.
    //
    // We also check "if" blocks here, since an empty conditional block
    // is likely an error.
    const std::string& line = elided_line;
    static const regex_code RE_PATTERN_CONDITIONAL =
        RegexCompile(R"(\s*(for|while|if)\s*\()");
    bool matched = RegexMatch(RE_PATTERN_CONDITIONAL, line,
                              m_re_result);
    if (matched) {
        // Find the end of the conditional expression.
        size_t end_linenum = linenum;
        size_t end_pos = line.find('(');
        const std::string& end_line = CloseExpression(clean_lines, &end_linenum, &end_pos);

        // Output warning if what follows the condition expression is a semicolon.
        // No warning for all other cases, including whitespace or newline, since we
        // have a separate check for semicolons preceded by whitespace.
        if (end_pos != INDEX_NONE && end_line.substr(end_pos).starts_with(';')) {
            if (GetMatchStr(m_re_result, line, 1) == "if") {
                Error(end_linenum, "whitespace/empty_conditional_body", 5,
                      "Empty conditional bodies should use {}");
            } else {
                Error(end_linenum, "whitespace/empty_loop_body", 5,
                      "Empty loop bodies should use {} or continue");
            }
        }

        // Check for if statements that have completely empty bodies (no comments)
        // and no else clauses.
        if (end_pos != INDEX_NONE && GetMatchStr(m_re_result, line, 1) == "if") {
            // Find the position of the opening { for the if statement.
            // Return without logging an error if it has no brackets.
            size_t opening_linenum = end_linenum;
            std::string opening_line_fragment = end_line.substr(end_pos);
            // Loop until EOF or find anything that's not whitespace or opening {.
            while (!CheckFirstNonSpace(opening_line_fragment, '{')) {
                if (RegexSearch(R"(^(?!\s*$))", opening_line_fragment, m_re_result_temp)) {
                    // Conditional has no brackets.
                    return;
                }
                opening_linenum++;
                if (opening_linenum == clean_lines.NumLines()) {
                    // Couldn't find conditional's opening { or any code before EOF.
                    return;
                }
                opening_line_fragment = clean_lines.GetElidedAt(opening_linenum);
            }

            // Set opening_line (opening_line_fragment may not be entire opening line).
            const std::string& opening_line = clean_lines.GetElidedAt(opening_linenum);

            // Find the position of the closing }.
            size_t opening_pos = opening_line_fragment.find('{');
            if (opening_linenum == end_linenum) {
                // We need to make opening_pos relative to the start of the entire line.
                opening_pos += end_pos;
            }

            size_t closing_linenum = opening_linenum;
            size_t closing_pos = opening_pos;
            const std::string& closing_line = CloseExpression(
                                                clean_lines, &closing_linenum, &closing_pos);
            if (closing_pos == INDEX_NONE)
                return;

            // Now construct the body of the conditional. This consists of the portion
            // of the opening line after the {, all lines until the closing line,
            // and the portion of the closing line before the }.
            bool dummy;
            if (clean_lines.GetRawLineAt(opening_linenum) !=
                CleanseComments(clean_lines.GetRawLineAt(opening_linenum),
                                &dummy, m_re_result_temp)) {
                // Opening line ends with a comment, so conditional isn't empty.
                return;
            }
            std::string body;
            if (closing_linenum > opening_linenum) {
                // Opening line after the {. Ignore comments here since we checked above.
                body = opening_line.substr(opening_pos + 1);
                // All lines until closing line, excluding closing line, with comments.
                for (size_t i = opening_linenum + 1; i < closing_linenum; i++) {
                    body += "\n" + clean_lines.GetRawLineAt(i);
                }
                // Closing line before the }. Won't (and can't) have comments.
                body += "\n" + clean_lines.GetElidedAt(closing_linenum).substr(0, closing_pos - 1);
            } else {
                // If statement has brackets and fits on a single line.
                body = opening_line.substr(opening_pos + 1, closing_pos - 1 - opening_pos - 1);
            }

            // Check if the body is empty
            if (!StrIsBlank(body))
                return;

            // The body is empty. Now make sure there's not an else clause.
            size_t current_linenum = closing_linenum;
            std::string current_line_fragment = closing_line.substr(closing_pos);
            // Loop until EOF or find anything that's not whitespace or else clause.
            while (RegexSearch(R"(^\s*$|^(?=\s*else))",
                               current_line_fragment, m_re_result_temp)) {
                if (RegexSearch(R"(^(?=\s*else))", current_line_fragment, m_re_result_temp)) {
                    // Found an else clause, so don't log an error.
                    return;
                }
                current_linenum++;
                if (current_linenum == clean_lines.NumLines())
                    break;
                current_line_fragment = clean_lines.GetElidedAt(current_linenum);
            }

            // The body is empty and there's no else clause until EOF or other code.
            Error(end_linenum, "whitespace/empty_if_body", 4,
                  "If statement had no body and no else clause");
        }
    }
}

void FileLinter::CheckComment(const std::string& line,
                              size_t linenum, size_t next_line_start) {
    size_t commentpos = line.find("//");
    if (commentpos == std::string::npos)
        return;
    // Check if the // may be in quotes.  If so, ignore it
    std::string substr = line.substr(0, commentpos);
    if ((StrCount(substr, '"') - StrCount(substr, "\\\"")) % 2 != 0)
        return;

    // Allow one space for new scopes, two spaces otherwise:
    static const regex_code RE_PATTERN_SPACE_FOR_SCOPE =
        RegexCompile(R"(^.*{ *//)");
    if (!(RegexMatch(RE_PATTERN_SPACE_FOR_SCOPE, line,
                     m_re_result_temp) && next_line_start == commentpos) &&
        ((commentpos >= 1 && !IS_SPACE(line[commentpos-1])) ||
         (commentpos >= 2 && !IS_SPACE(line[commentpos-2])))) {
        Error(linenum, "whitespace/comments", 2,
              "At least two spaces is best between code and comments");
    }

    // Checks for common mistakes in TODO comments.
    std::string comment = line.substr(commentpos);
    static const regex_code RE_PATTERN_TODO =
        RegexCompile(R"(^//(\s*)TODO(\(.+?\))?:?(\s|$)?)");
    bool match = RegexMatch(RE_PATTERN_TODO, comment, m_re_result);
    if (match) {
        // One whitespace is correct; zero whitespace is handled elsewhere.
        const std::string& leading_whitespace = GetMatchStr(m_re_result, comment, 1);
        if (leading_whitespace.size() > 1) {
            Error(linenum, "whitespace/todo", 2,
                  "Too many spaces before TODO");
        }

        const std::string& username = GetMatchStr(m_re_result, comment, 2);
        if (username.empty()) {
            Error(linenum, "readability/todo", 2,
                  "Missing username in TODO; it should look like "
                  "\"// TODO(my_username): Stuff.\"");
        }

        const std::string& middle_whitespace = GetMatchStr(m_re_result, comment, 3);
        // Comparisons made explicit for correctness
        //  -- pylint: disable=g-explicit-bool-comparison
        if (!IsMatched(m_re_result, 3) ||
                (!StrIsChar(middle_whitespace, ' ') &&
                    !middle_whitespace.empty())) {
            Error(linenum, "whitespace/todo", 2,
                "TODO(my_username) should be followed by a space");
        }
    }

    // If the comment contains an alphanumeric character, there
    // should be a space somewhere between it and the // unless
    // it's a /// or //! Doxygen comment.
    if (RegexMatch(R"(//[^ ]*\w)", comment, m_re_result) &&
        !RegexMatch(R"((///|//\!)(\s+|$))", comment, m_re_result)) {
        Error(linenum, "whitespace/comments", 4,
              "Should have a space between // and comment");
    }
}

void FileLinter::CheckSpacing(const CleansedLines& clean_lines,
                              const std::string& elided_line, size_t linenum,
                              NestingState* nesting_state) {
    // Don't use "elided" lines here, otherwise we can't check commented lines.
    // Don't want to use "raw" either, because we don't want to check inside C++11
    // raw strings,
    const std::string& line = clean_lines.GetLineWithoutRawStringAt(linenum);

    // Before nixing comments, check if the line is blank for no good
    // reason.  This includes the first line after a block is opened, and
    // blank lines at the end of a function (ie, right before a line like '}'
    //
    // Skip all the blank line checks if we are immediately inside a
    // namespace body.  In other words, don't issue blank line warnings
    // for this block:
    //   namespace {
    //
    //   }
    //
    // A warning about missing end of namespace comments will be issued instead.
    //
    // Also skip blank line checks for 'extern "C"' blocks, which are formatted
    // like namespaces.
    if (StrIsBlank(line) &&
        !nesting_state->InNamespaceBody() &&
        !nesting_state->InExternC()) {
        const std::string& prev_line = clean_lines.GetElidedAt(linenum - 1);
        size_t prevbrace = prev_line.rfind('{');
        // TODO(unknown): Don't complain if line before blank line, and line after,
        //                both start with alnums and are indented the same amount.
        //                This ignores whitespace at the start of a namespace block
        //                because those are not usually indented.
        if (prevbrace != std::string::npos &&
            prev_line.substr(prevbrace).find('}') == std::string::npos) {
            // OK, we have a blank line at the start of a code block.  Before we
            // complain, we check if it is an exception to the rule: The previous
            // non-empty line has the parameters of a function header that are indented
            // 4 spaces (because they did not fit in a 80 column line when placed on
            // the same line as the function name).  We also check for the case where
            // the previous line is indented 6 spaces, which may happen when the
            // initializers of a constructor do not fit into a 80 column line.
            bool exception = false;
            if (RegexMatch(R"( {6}\w)", prev_line, m_re_result_temp)) {  // Initializer list?
                // We are looking for the opening column of initializer list, which
                // should be indented 4 spaces to cause 6 space indentation afterwards.
                size_t search_position = (linenum >= 2) ? linenum - 2 : INDEX_NONE;
                while (search_position != INDEX_NONE &&
                       RegexMatch(R"( {6}\w)", clean_lines.GetElidedAt(search_position),
                                  m_re_result_temp)) {
                    search_position--;
                }
                exception = (search_position != INDEX_NONE &&
                             clean_lines.GetElidedAt(search_position).starts_with("    :"));
            } else {
                // Search for the function arguments or an initializer list.  We use a
                // simple heuristic here: If the line is indented 4 spaces; and we have a
                // closing paren, without the opening paren, followed by an opening brace
                // or colon (for initializer lists) we assume that it is the last line of
                // a function header.  If we have a colon indented 4 spaces, it is an
                // initializer list.
                exception = (RegexMatch(R"( {4}\w[^\(]*\)\s*(const\s*)?(\{\s*$|:))",
                                           prev_line, m_re_result_temp) ||
                             RegexMatch(R"( {4}:)", prev_line, m_re_result_temp));
            }

            if (!exception) {
                Error(linenum, "whitespace/blank_line", 2,
                      "Redundant blank line at the start of a code block "
                      "should be deleted.");
            }
        }
        // Ignore blank lines at the end of a block in a long if-else
        // chain, like this:
        //   if (condition1) {
        //     // Something followed by a blank line
        //
        //   } else if (condition2) {
        //     // Something else
        //   }
        if (linenum + 1 < clean_lines.NumLines()) {
            const std::string& next_line = clean_lines.GetLineWithoutRawStringAt(linenum + 1);
            if (!next_line.empty() &&
                CheckFirstNonSpace(next_line, '}') &&
                next_line.find("} else ") == std::string::npos) {
                Error(linenum, "whitespace/blank_line", 3,
                      "Redundant blank line at the end of a code block "
                      "should be deleted.");
            }
        }

        bool matched = RegexMatch(R"(\s*(public|protected|private):)",
                                  prev_line, m_re_result);
        if (matched) {
            Error(linenum, "whitespace/blank_line", 3,
                  "Do not leave a blank line after \"" +
                  GetMatchStr(m_re_result, prev_line, 1) + ":\"");
        }
    }

    // Next, check comments
    size_t next_line_start = 0;
    if (linenum + 1 < clean_lines.NumLines()) {
        const std::string& next_line = clean_lines.GetLineWithoutRawStringAt(linenum + 1);
        next_line_start = next_line.size() - StrLstripSize(next_line);
    }
    CheckComment(line, linenum, next_line_start);

    // get rid of comments and strings
    const std::string& elided = elided_line;

    // You shouldn't have spaces before your brackets, except for C++11 attributes
    // or maybe after 'delete []', 'return []() {};', or 'auto [abc, ...] = ...;'.
    static const regex_code RE_PATTERN_BRACKETS =
        RegexCompile(R"(\w\s+\[(?!\[))");
    static const regex_code RE_PATTERN_ATTRIBUTES =
        RegexCompile(R"((?:auto&?|delete|return)\s+\[)");
    if (RegexSearch(RE_PATTERN_BRACKETS, elided, m_re_result_temp) &&
        !RegexSearch(RE_PATTERN_ATTRIBUTES, elided, m_re_result_temp)) {
        Error(linenum, "whitespace/braces", 5,
              "Extra space before [");
    }

    // In range-based for, we wanted spaces before and after the colon, but
    // not around "::" tokens that might appear.
    static const regex_code RE_PATTERN_FORCOLON =
        RegexCompile(R"(for *\(.*[^:]:[^: ])");
    static const regex_code RE_PATTERN_FORCOLON2 =
        RegexCompile(R"(for *\(.*[^: ]:[^:])");
    if (RegexSearch(RE_PATTERN_FORCOLON, elided, m_re_result_temp) ||
        RegexSearch(RE_PATTERN_FORCOLON2, elided, m_re_result_temp)) {
        Error(linenum, "whitespace/forcolon", 2,
              "Missing space around colon in range-based for loop");
    }
}

void FileLinter::CheckOperatorSpacing(const CleansedLines& clean_lines,
                                      const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;

    // Don't try to do spacing checks for operator methods.  Do this by
    // replacing the troublesome characters with something else,
    // preserving column position for all other characters.
    //
    // The replacement is done repeatedly to avoid false positives from
    // operators that call operators.
    static const regex_code RE_PATTERN_OPERATOR_SPACING =
        RegexCompile(R"(^(.*\boperator\b)(\S+)(\s*\(.*)$)");

    bool match = RegexMatch(RE_PATTERN_OPERATOR_SPACING, line, m_re_result);
    if (match) {
        std::string new_line = GetMatchStr(m_re_result, line, 1) +
                               std::string(GetMatchSize(m_re_result, 2), '_') +
                               GetMatchStr(m_re_result, line, 3);
        CheckOperatorSpacing(clean_lines, new_line, linenum);
        return;
    }

    // We allow no-spaces around = within an if: "if ( (a=Foo()) == 0 )".
    // Otherwise not.  Note we only check for non-spaces on *both* sides;
    // sometimes people put non-spaces on one side when aligning ='s among
    // many lines (not that this is behavior that I approve of...)
    static const regex_code RE_PATTERN_EQ =
        RegexCompile(R"([\w.]=|=[\w.])");
    static const regex_code RE_PATTERN_BLOCK =
        RegexCompile(R"(\b(if|while|for) )");
    static const regex_code RE_PATTERN_EQ_OPERATORS =
        RegexCompile(R"((>=|<=|==|!=|&=|\^=|\|=|\+=|\*=|\/=|\%=))");
    if (RegexSearch(RE_PATTERN_EQ, line, m_re_result)
            && !RegexSearch(RE_PATTERN_BLOCK, line, m_re_result)
            // Operators taken from [lex.operators] in C++11 standard.
            && !RegexSearch(RE_PATTERN_EQ_OPERATORS, line, m_re_result)
            && !StrContain(line, "operator=")) {
        Error(linenum, "whitespace/operators", 4,
              "Missing spaces around =");
    }

    // It's ok not to have spaces around binary operators like + - * /, but if
    // there's too little whitespace, we get concerned.  It's hard to tell,
    // though, so we punt on this one for now.  TODO.

    // You should always have whitespace around binary operators.
    //
    // Check <= and >= first to avoid false positives with < and >, then
    // check non-include lines for spacing around < and >.
    //
    // If the operator is followed by a comma, assume it's be used in a
    // macro context and don't do any checks.  This avoids false
    // positives.
    //
    // Note that && is not included here.  This is because there are too
    // many false positives due to RValue references.

    static const regex_code RE_PATTERN_OPERATOR_SPACING2 =
        RegexCompile(R"([^<>=!\s](==|!=|<=|>=|\|\|)[^<>=!\s,;\)])");
    match = RegexSearch(RE_PATTERN_OPERATOR_SPACING2, line, m_re_result);
    if (match) {
        // TODO(unknown): support alternate operators
        Error(linenum, "whitespace/operators", 3,
              "Missing spaces around "+ GetMatchStr(m_re_result, line, 1));
    } else if (!line.starts_with('#') || !StrContain(line, "include")) {
        // Look for < that is not surrounded by spaces.  This is only
        // triggered if both sides are missing spaces, even though
        // technically should should flag if at least one side is missing a
        // space.  This is done to avoid some false positives with shifts.
        static const regex_code RE_PATTERN_LESS_SPACING =
            RegexCompile(R"(^(.*[^\s<])<[^\s=<,])");
        match = RegexMatch(RE_PATTERN_LESS_SPACING, line, m_re_result);
        if (match) {
            size_t end_linenum = linenum;
            size_t end_pos = GetMatchSize(m_re_result, 1);
            CloseExpression(clean_lines, &end_linenum, &end_pos);
            if (end_pos == INDEX_NONE) {
                Error(linenum, "whitespace/operators", 3,
                      "Missing spaces around <");
            }
        }

        // Look for > that is not surrounded by spaces.  Similar to the
        // above, we only trigger if both sides are missing spaces to avoid
        // false positives with shifts.
        static const regex_code RE_PATTERN_GREATER_SPACING =
            RegexCompile(R"(^(.*[^-\s>])>[^\s=>,])");
        match = RegexMatch(RE_PATTERN_GREATER_SPACING, line, m_re_result);
        if (match) {
            size_t start_linenum = linenum;
            size_t start_pos = GetMatchSize(m_re_result, 1);
            ReverseCloseExpression(clean_lines, &start_linenum, &start_pos);
            if (start_pos == INDEX_NONE) {
                Error(linenum, "whitespace/operators", 3,
                      "Missing spaces around >");
            }
        }
    }

    // We allow no-spaces around << when used like this: 10<<20, but
    // not otherwise (particularly, not when used as streams)
    //
    // We also allow operators following an opening parenthesis, since
    // those tend to be macros that deal with operators.
    static const regex_code RE_PATTERN_LSHIFT_SPACING =
        RegexCompile(R"((operator|[^\s(<])(?:L|UL|LL|ULL|l|ul|ll|ull)?<<([^\s,=<]))");
    match = RegexSearch(
                RE_PATTERN_LSHIFT_SPACING, line, m_re_result);
    if (match && !(StrIsDigit(GetMatchStr(m_re_result, line, 1)) &&
                   StrIsDigit(GetMatchStr(m_re_result, line, 2))) &&
            !(GetMatchStr(m_re_result, line, 1) == "operator" &&
              StrIsChar(GetMatchStr(m_re_result, line, 2), ';'))) {
        Error(linenum, "whitespace/operators", 3,
                              "Missing spaces around <<");
    }

    // We allow no-spaces around >> for almost anything.  This is because
    // C++11 allows ">>" to close nested templates, which accounts for
    // most cases when ">>" is not followed by a space.
    //
    // We still warn on ">>" followed by alpha character, because that is
    // likely due to ">>" being used for right shifts, e.g.:
    //   value >> alpha
    //
    // When ">>" is used to close templates, the alphanumeric letter that
    // follows would be part of an identifier, and there should still be
    // a space separating the template type and the identifier.
    //   type<type<type>> alpha
    static const regex_code RE_PATTERN_OPERATOR_RSHIFT =
        RegexCompile(">>[a-zA-Z_]");
    match = RegexSearch(RE_PATTERN_OPERATOR_RSHIFT, line, m_re_result);
    if (match) {
        Error(linenum, "whitespace/operators", 3,
              "Missing spaces around >>");
    }

    // There shouldn't be space around unary operators
    static const regex_code RE_PATTERN_OPERATOR_SPACING3 =
        RegexCompile(R"((!\s|~\s|[\s]--[\s;]|[\s]\+\+[\s;]))");
    match = RegexSearch(RE_PATTERN_OPERATOR_SPACING3, line, m_re_result);
    if (match) {
        Error(linenum, "whitespace/operators", 4,
              "Extra space for operator " + GetMatchStr(m_re_result, line, 1));
    }
}

void FileLinter::CheckParenthesisSpacing(const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;

    // No spaces after an if, while, switch, or for
    static const regex_code RE_PATTERN_PARENS_SPACING =
        RegexCompile(R"(\b(if\(|for\(|while\(|switch\())");
    bool match = RegexSearch(RE_PATTERN_PARENS_SPACING, line, m_re_result);
    if (match) {
        Error(linenum, "whitespace/parens", 5,
              "Missing space before ( in " + GetMatchStr(m_re_result, line, 1));
    }

    // For if/for/while/switch, the left and right parens should be
    // consistent about how many spaces are inside the parens, and
    // there should either be zero or one spaces inside the parens.
    // We don't want: "if ( foo)" or "if ( foo   )".
    // Exception: "for ( ; foo; bar)" and "for (foo; bar; )" are allowed.
    static const regex_code RE_PATTERN_PARENS =
        RegexCompile(R"(\b(if|for|while|switch)\s*)"
                     R"(\(([ ]*)(.).*[^ ]+([ ]*)\)\s*{\s*$)");
    match = RegexSearch(RE_PATTERN_PARENS, line, m_re_result);
    if (match) {
        size_t str2_size = GetMatchSize(m_re_result, 2);
        if (str2_size != GetMatchSize(m_re_result, 4)) {
            if (!((StrIsChar(GetMatchStr(m_re_result, line, 3), ';') &&
                (str2_size == 1 + GetMatchSize(m_re_result, 4))) ||
                (str2_size == 0 && RegexSearch(R"(\bfor\s*\(.*; \))", line, m_re_result_temp)))) {
                Error(linenum, "whitespace/parens", 5,
                      "Mismatching spaces inside () in " + GetMatchStr(m_re_result, line, 1));
            }
        }
        if (str2_size != 0 && str2_size != 1) {
            Error(linenum, "whitespace/parens", 5,
                  "Should have zero or one spaces inside ( and ) in " +
                  GetMatchStr(m_re_result, line, 1));
        }
    }
}

void FileLinter::CheckCommaSpacing(const CleansedLines& clean_lines,
                                   const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;

    // You should always have a space after a comma (either as fn arg or operator)
    //
    // This does not apply when the non-space character following the
    // comma is another comma, since the only time when that happens is
    // for empty macro arguments.
    //
    // We run this check in two passes: first pass on elided lines to
    // verify that lines contain missing whitespaces, second pass on raw
    // lines to confirm that those missing whitespaces are not due to
    // elided comments.
    static const regex_code RE_PATTERN_COMMA_SPACING =
        RegexCompile(R"(,[^,\s])");
    static const regex_code RE_PATTERN_VA_OPT =
        RegexCompile(R"(\b__VA_OPT__\s*\(,\))");
    static const regex_code RE_PATTERN_OPERATOR =
        RegexCompile(R"(\boperator\s*,\s*\()");
    bool match;

    // We don't need to run RegexReplace in most cases.
    if (StrContain(line, "operator") || StrContain(line, "__VA_OPT__")) {
        match = RegexSearch(RE_PATTERN_COMMA_SPACING,
            RegexReplace(RE_PATTERN_VA_OPT, "",
                RegexReplace(RE_PATTERN_OPERATOR, "F(", line, m_re_result_temp),
                m_re_result_temp),
            m_re_result_temp);
    } else {
        match = RegexSearch(RE_PATTERN_COMMA_SPACING, line, m_re_result_temp);
    }

    if (match && RegexSearch(RE_PATTERN_COMMA_SPACING,
                             clean_lines.GetLineWithoutRawStringAt(linenum),
                             m_re_result_temp)) {
        Error(linenum, "whitespace/comma", 3,
                              "Missing space after ,");
    }

    // You should always have a space after a semicolon
    // except for few corner cases
    // TODO(unknown): clarify if 'if (1) { return 1;}' is requires one more
    // space after ;
    static const regex_code RE_PATTERN_SEMICOLON_SPACING =
        RegexCompile(R"(;[^\s};\\)/])");
    if (RegexSearch(RE_PATTERN_SEMICOLON_SPACING, line, m_re_result_temp)) {
        Error(linenum, "whitespace/semicolon", 3,
                              "Missing space after ;");
    }
}

const regex_code RE_PATTERN_TYPES =
    RegexCompile(
        "^(?:"
        // [dcl.type.simple]
        "(char(16_t|32_t)?)|wchar_t|"
        "bool|short|int|long|signed|unsigned|float|double|"
        // [support.types]
        "(ptrdiff_t|size_t|max_align_t|nullptr_t)|"
        // [cstdint.syn]
        "(u?int(_fast|_least)?(8|16|32|64)_t)|"
        "(u?int(max|ptr)_t)|"
        ")$");

static bool IsType(const CleansedLines& clean_lines,
                   NestingState* nesting_state,
                   const std::string& expr,
                   regex_match& re_result,
                   regex_match& re_result_temp) {
    // Check if expression looks like a type name, returns true if so.
    // Keep only the last token in the expression
    bool match = RegexMatch(R"(^.*(\b\S+)$)", expr, re_result);
    std::string token;
    if (match)
        token = GetMatchStr(re_result, expr, 1);
    else
        token = expr;

    // Match native types and stdint types
    if (RegexMatch(RE_PATTERN_TYPES, token, re_result_temp))
        return true;

    // Try a bit harder to match templated types.  Walk up the nesting
    // stack until we find something that resembles a typename
    // declaration for what we are looking for.
    std::string typename_pattern = R"(\b(?:typename|class|struct)\s+)" + RegexEscape(token) +
                                            R"(\b)";
    size_t block_index = (nesting_state->GetStackSize() >= 1) ?
                            nesting_state->GetStackSize() - 1: INDEX_NONE;
    while (block_index != INDEX_NONE) {
        if (nesting_state->GetStackAt(block_index)->IsNamespaceInfo())
            return false;

        // Found where the opening brace is.  We want to scan from this
        // line up to the beginning of the function, minus a few lines.
        //   template <typename Type1,  // stop scanning here
        //             ...>
        //   class C
        //     : public ... {  // start scanning here
        size_t last_line = nesting_state->GetStackAt(block_index)->StartingLinenum();

        size_t next_block_start = 0;
        if (block_index > 0)
            next_block_start = nesting_state->GetStackAt(block_index - 1)->StartingLinenum();
        size_t first_line = last_line;
        while (first_line >= next_block_start && first_line != INDEX_NONE) {
            if (clean_lines.GetElidedAt(first_line).find("template") != std::string::npos)
                break;
            first_line--;
        }
        if (first_line < next_block_start || first_line == INDEX_NONE) {
            // Didn't find any "template" keyword before reaching the next block,
            // there are probably no template things to check for this block
            block_index--;
            continue;
        }

        // Look for typename in the specified range
        for (size_t i = first_line; i < last_line + 1; i++) {
            if (RegexSearch(typename_pattern, clean_lines.GetElidedAt(i), re_result_temp))
                return true;
        }
        block_index--;
    }

    return false;
}

void FileLinter::CheckBracesSpacing(const CleansedLines& clean_lines,
                                    const std::string& elided_line, size_t linenum,
                                    NestingState* nesting_state) {
    const std::string& line = elided_line;

    // Except after an opening paren, or after another opening brace (in case of
    // an initializer list, for instance), you should have spaces before your
    // braces when they are delimiting blocks, classes, namespaces etc.
    // And since you should never have braces at the beginning of a line,
    // this is an easy test.  Except that braces used for initialization don't
    // follow the same rule; we often don't want spaces before those.
    static const regex_code RE_PATTERN_BLACE_SPACING =
        RegexCompile("^(.*[^ ({>]){");
    bool match = RegexMatch(RE_PATTERN_BLACE_SPACING, line, m_re_result);

    if (match) {
        // Try a bit harder to check for brace initialization.  This
        // happens in one of the following forms:
        //   Constructor() : initializer_list_{} { ... }
        //   Constructor{}.MemberFunction()
        //   Type variable{};
        //   FunctionCall(type{}, ...);
        //   LastArgument(..., type{});
        //   LOG(INFO) << type{} << " ...";
        //   map_of_type[{...}] = ...;
        //   ternary = expr ? new type{} : nullptr;
        //   OuterTemplate<InnerTemplateConstructor<Type>{}>
        //
        // We check for the character following the closing brace, and
        // silence the warning if it's one of those listed above, i.e.
        // "{.;,)<>]:".
        //
        // To account for nested initializer list, we allow any number of
        // closing braces up to "{;,)<".  We can't simply silence the
        // warning on first sight of closing brace, because that would
        // cause false negatives for things that are not initializer lists.
        //   Silence this:         But not this:
        //     Outer{                if (...) {
        //       Inner{...}            if (...){  // Missing space before {
        //     };                    }
        //
        // There is a false negative with this approach if people inserted
        // spurious semicolons, e.g. "if (cond){};", but we will catch the
        // spurious semicolon with a separate check.
        const std::string& leading_text = GetMatchStr(m_re_result, line, 1);
        size_t endlinenum = linenum;
        size_t endpos = GetMatchEnd(m_re_result, 1);
        const std::string& endline = CloseExpression(clean_lines, &endlinenum, &endpos);
        std::string trailing_text = "";
        if (endpos != INDEX_NONE)
            trailing_text = endline.substr(endpos);
        for (size_t offset = endlinenum + 1;
             offset < MIN(endlinenum + 3, clean_lines.NumLines() - 1); offset++) {
            trailing_text += clean_lines.GetElidedAt(offset);
        }
        // We also suppress warnings for `uint64_t{expression}` etc., as the style
        // guide recommends brace initialization for integral types to avoid
        // overflow/truncation.
        if (!RegexMatch(R"(^[\s}]*[{.;,)<>\]:])", trailing_text, m_re_result_temp)
                && !IsType(clean_lines, nesting_state, leading_text,
                           m_re_result, m_re_result_temp)) {
            Error(linenum, "whitespace/braces", 5,
                  "Missing space before {");
        }
    }

    // Make sure '} else {' has spaces.
    if (StrContain(line, "}else")) {
        Error(linenum, "whitespace/braces", 5,
              "Missing space before else");
    }

    // You shouldn't have a space before a semicolon at the end of the line.
    // There's a special case for "for" since the style guide allows space before
    // the semicolon there.
    static const regex_code RE_PATTERN_SEMICOLON_EMPTY =
        RegexCompile(R"(:\s*;\s*$)");
    static const regex_code RE_PATTERN_SEMICOLON_ONLY =
        RegexCompile(R"(^\s*;\s*$)");
    static const regex_code RE_PATTERN_SEMICOLON_EXTRA =
        RegexCompile(R"(\s+;\s*$)");
    static const regex_code RE_PATTERN_SEMICOLON_FOR =
        RegexCompile(R"(\bfor\b)");
    if (RegexSearch(RE_PATTERN_SEMICOLON_EMPTY, line, m_re_result_temp)) {
        Error(linenum, "whitespace/semicolon", 5,
              "Semicolon defining empty statement. Use {} instead.");
    } else if (RegexSearch(RE_PATTERN_SEMICOLON_ONLY, line, m_re_result_temp)) {
        Error(linenum, "whitespace/semicolon", 5,
              "Line contains only semicolon. If this should be an empty statement, "
              "use {} instead.");
    } else if (RegexSearch(RE_PATTERN_SEMICOLON_EXTRA, line, m_re_result_temp) &&
               !RegexSearch(RE_PATTERN_SEMICOLON_FOR, line, m_re_result_temp)) {
        Error(linenum, "whitespace/semicolon", 5,
              "Extra space before last semicolon. If this should be an empty "
              "statement, use {} instead.");
    }
}

void FileLinter::CheckSpacingForFunctionCall(const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;

    // Since function calls often occur inside if/for/while/switch
    // expressions - which have their own, more liberal conventions - we
    // first see if we should be looking inside such an expression for a
    // function call, to which we can apply more strict standards.
    static const regex_code RE_PATTERN_IF_FOR_SWITCH =
        RegexCompile(R"(\b(if|for|switch)\s*\((.*)\)\s*{)");
    static const regex_code RE_PATTERN_WHILE =
        RegexCompile(R"(\bwhile\s*\((.*)\)\s*[{;])");

    bool match = RegexSearch(RE_PATTERN_IF_FOR_SWITCH, line, m_re_result);
    if (match) {
        // look inside the parens for function calls
        CheckSpacingForFunctionCallBase(
            line, GetMatchStr(m_re_result, line, 2), linenum);
    } else {
        match = RegexSearch(RE_PATTERN_WHILE, line, m_re_result);
        if (match) {
            // look inside the parens for function calls
            CheckSpacingForFunctionCallBase(
                line, GetMatchStr(m_re_result, line, 1), linenum);
            return;
        }
    }

    CheckSpacingForFunctionCallBase(line, line, linenum);
}

void FileLinter::CheckSpacingForFunctionCallBase(const std::string& line,
                                                 const std::string& fncall, size_t linenum) {
    // Except in if/for/while/switch, there should never be space
    // immediately inside parens (eg "f( 3, 4 )").  We make an exception
    // for nested parens ( (a+b) + c ).  Likewise, there should never be
    // a space before a ( when it's a function argument.  I assume it's a
    // function argument when the char before the whitespace is legal in
    // a function name (alnum + _) and we're not starting a macro. Also ignore
    // pointers and references to arrays and functions coz they're too tricky:
    // we use a very simple way to recognize these:
    // " (something)(maybe-something)" or
    // " (something)(maybe-something," or
    // " (something)[something]"
    // Note that we assume the contents of [] to be short enough that
    // they'll never need to wrap.
    static const regex_code RE_PATTERN_CTRL_STRUCT =
        RegexCompile(R"(\b(if|elif|for|while|switch|return|new|delete|catch|sizeof)\b)");
    static const regex_code RE_PATTERN_FUNC_REF =
        RegexCompile(R"( \([^)]+\)\([^)]*(\)|,$))");
    static const regex_code RE_PATTERN_ARRAY_REF =
        RegexCompile(R"( \([^)]+\)\[[^\]]+\])");
    if (  // Ignore control structures.
          RegexSearch(RE_PATTERN_CTRL_STRUCT, fncall, m_re_result_temp) ||
          // Ignore pointers/references to functions.
          RegexSearch(RE_PATTERN_FUNC_REF, fncall, m_re_result_temp) ||
          // Ignore pointers/references to arrays.
          RegexSearch(RE_PATTERN_ARRAY_REF, fncall, m_re_result_temp))
        return;

    static const regex_code RE_PATTERN_FUNC_PARENS =
        RegexCompile(R"(\w\s*\(\s(?!\s*\\$))");
    static const regex_code RE_PATTERN_PARENS =
        RegexCompile(R"(\(\s+(?!(\s*\\)|\())");
    if (RegexSearch(RE_PATTERN_FUNC_PARENS, fncall, m_re_result_temp)) {  // a ( used for a fn call
        Error(linenum, "whitespace/parens", 4,
              "Extra space after ( in function call");
    } else if (RegexSearch(RE_PATTERN_PARENS, fncall, m_re_result_temp)) {
        Error(linenum, "whitespace/parens", 2,
              "Extra space after (");
    }

    static const regex_code RE_PATTERN_SPACE_BEFORE_PARENS =
        RegexCompile(R"(\w\s+\()");
    static const regex_code RE_PATTERN_ASM =
        RegexCompile(R"(_{0,2}asm_{0,2}\s+_{0,2}volatile_{0,2}\s+\()");
    static const regex_code RE_PATTERN_MACRO =
        RegexCompile(R"(#\s*define|typedef|using\s+\w+\s*=)");
    static const regex_code RE_PATTERN_COLON =
        RegexCompile(R"(\w\s+\((\w+::)*\*\w+\)\()");
    static const regex_code RE_PATTERN_CASE =
        RegexCompile(R"(\bcase\s+\()");
    if (RegexSearch(RE_PATTERN_SPACE_BEFORE_PARENS, fncall, m_re_result_temp) &&
        !RegexSearch(RE_PATTERN_ASM, fncall, m_re_result_temp) &&
        !RegexSearch(RE_PATTERN_MACRO, fncall, m_re_result_temp) &&
        !RegexSearch(RE_PATTERN_COLON, fncall, m_re_result_temp) &&
        !RegexSearch(RE_PATTERN_CASE, fncall, m_re_result_temp)) {
        // TODO(unknown): Space after an operator function seem to be a common
        // error, silence those for now by restricting them to highest verbosity.
        if (RegexSearch(R"(\boperator_*\b)", line, m_re_result_temp)) {
            Error(linenum, "whitespace/parens", 0,
                  "Extra space before ( in function call");
        } else {
            Error(linenum, "whitespace/parens", 4,
                  "Extra space before ( in function call");
        }
    }

    // Return if the ) is followed only by a newline or a { + newline, assume it's
    // part of a control statement (if/while/etc), and don't complain
    static const regex_code RE_PATTERN_EOL_BRACE =
        RegexCompile(R"([^)]\s+\)\s*[^{\s])");
    if (!RegexSearch(RE_PATTERN_EOL_BRACE, fncall, m_re_result_temp))
        return;

    // If the closing parenthesis is preceded by only whitespaces,
    // try to give a more descriptive error message.
    static const regex_code RE_PATTERN_CLOSE_PARENS =
        RegexCompile(R"(^\s+\))");
    if (RegexSearch(RE_PATTERN_CLOSE_PARENS, fncall, m_re_result_temp)) {
        Error(linenum, "whitespace/parens", 2,
                              "Closing ) should be moved to the previous line");
    } else {
        Error(linenum, "whitespace/parens", 2,
                              "Extra space before )");
    }
}

// Assertion macros.  These are defined in base/logging.h and
// testing/base/public/gunit.h.
std::vector<std::string> CHECK_MACROS = {
    "DCHECK", "CHECK",
    "EXPECT_TRUE", "ASSERT_TRUE",
    "EXPECT_FALSE", "ASSERT_FALSE",
};

// Replacement macros for CHECK/DCHECK/EXPECT_TRUE/EXPECT_FALSE
typedef std::map<std::string, std::map<std::string, std::string>> macro_map_t;

macro_map_t InitializeMacroMap() {
    macro_map_t macros = {
        { "DCHECK", {} },
        { "CHECK", {} },
        { "EXPECT_TRUE", {} },
        { "ASSERT_TRUE", {} },
        { "EXPECT_FALSE", {} },
        { "ASSERT_FALSE", {} },
    };
    for (const auto& item : { std::make_pair("==", "EQ"), std::make_pair("!=", "NE"),
                              std::make_pair(">=", "GE"), std::make_pair(">", "GT"),
                              std::make_pair("<=", "LE"), std::make_pair("<", "LT") }) {
        macros["DCHECK"][item.first] = std::string("DCHECK_") + item.second;
        macros["CHECK"][item.first] = std::string("CHECK_") + item.second;
        macros["EXPECT_TRUE"][item.first] = std::string("EXPECT_") + item.second;
        macros["ASSERT_TRUE"][item.first] = std::string("ASSERT_") + item.second;
    }
    for (const auto& item : { std::make_pair("==", "NE"), std::make_pair("!=", "EQ"),
                              std::make_pair(">=", "LT"), std::make_pair(">", "LE"),
                              std::make_pair("<=", "GT"), std::make_pair("<", "GE") }) {
        macros["EXPECT_FALSE"][item.first] = std::string("EXPECT_") + item.second;
        macros["ASSERT_FALSE"][item.first] = std::string("ASSERT_") + item.second;
    }
    return macros;
}

macro_map_t CHECK_REPLACEMENT = InitializeMacroMap();

static std::string FindCheckMacro(const std::string& line, size_t* start_pos,
                                  regex_match& re_result) {
    // Find a replaceable CHECK-like macro.
    for (const std::string& macro : CHECK_MACROS) {
        size_t i = line.find(macro);
        if (i != std::string::npos) {
            // Find opening parenthesis.  Do a regular expression match here
            // to make sure that we are matching the expected CHECK macro, as
            // opposed to some other macro that happens to contain the CHECK
            // substring.
            bool matched = RegexMatch(R"(^(.*\b)" + macro + R"(\s*)\()",
                                      line, re_result);
            if (!matched)
                continue;
            *start_pos = GetMatchSize(re_result, 1);
            return macro;
        }
    }
    *start_pos = INDEX_NONE;
    return "";
}

void FileLinter::CheckCheck(const CleansedLines& clean_lines,
                            const std::string& elided_line, size_t linenum) {
    // Decide the set of replacement macros that should be suggested
    size_t start_pos;
    std::string check_macro = FindCheckMacro(elided_line, &start_pos, m_re_result);
    if (check_macro.empty())
        return;

    // Find end of the boolean expression by matching parentheses
    size_t end_line = linenum;
    size_t end_pos = start_pos;
    const std::string& last_line = CloseExpression(clean_lines, &end_line, &end_pos);
    if (end_pos == INDEX_NONE)
        return;

    // If the check macro is followed by something other than a
    // semicolon, assume users will log their own custom error messages
    // and don't suggest any replacements.
    if (!CheckFirstNonSpace(last_line.substr(end_pos), ';'))
        return;

    std::string expression = "";
    if (linenum == end_line) {
        expression = elided_line.substr(start_pos + 1, end_pos - 1 - start_pos - 1);
    } else {
        expression = elided_line.substr(start_pos + 1);
        for (size_t i = linenum + 1; i < end_line; i++)
            expression += clean_lines.GetElidedAt(i);
        expression += last_line.substr(0, end_pos - 1);
    }

    // Parse expression so that we can take parentheses into account.
    // This avoids false positives for inputs like "CHECK((a < 4) == b)",
    // which is not replaceable by CHECK_LE.
    std::string lhs = "";
    std::string rhs = "";
    std::string op = "";
    static const regex_code RE_PATTERN_CHECK =
        RegexCompile(R"(^\s*(<<|<<=|>>|>>=|->\*|->|&&|\|\||)"
                     R"(==|!=|>=|>|<=|<|\()(.*)$)");
    while (!expression.empty()) {
        bool matched = RegexMatch(RE_PATTERN_CHECK, expression, m_re_result);
        if (matched) {
            const std::string& token = GetMatchStr(m_re_result, expression, 1);
            if (StrIsChar(token, '(')) {
                // Parenthesized operand
                expression = GetMatchStr(m_re_result, expression, 2);
                size_t end = 0;
                std::stack<char> stack = {};
                stack.push('(');
                FindEndOfExpressionInLine(expression, &end, &stack);
                if (end == INDEX_NONE)
                    return;  // Unmatched parenthesis
                lhs += "(" + expression.substr(0, end);
                expression = expression.substr(end);
            } else if (token == "&&" || token == "||") {
                // Logical and/or operators.  This means the expression
                // contains more than one term, for example:
                //   CHECK(42 < a && a < b);
                //
                // These are not replaceable with CHECK_LE, so bail out early.
                return;
            } else if (InStrVec({"<<", "<<=", ">>", ">>=", "->*", "->"}, token)) {
                // Non-relational operator
                lhs += token;
                expression = GetMatchStr(m_re_result, expression, 2);
            } else {
                // Relational operator
                op = token;
                rhs = GetMatchStr(m_re_result, expression, 2);
                break;
            }
        } else {
            // Unparenthesized operand.  Instead of appending to lhs one character
            // at a time, we do another regular expression match to consume several
            // characters at once if possible.  Trivial benchmark shows that this
            // is more efficient when the operands are longer than a single
            // character, which is generally the case.
            matched = RegexMatch(R"(^([^-=!<>()&|]+)(.*)$)", expression, m_re_result);
            if (!matched) {
                matched = RegexMatch(R"(^(\s*\S)(.*)$)", expression, m_re_result);
                if (!matched)
                    break;
            }
            lhs += GetMatchStr(m_re_result, expression, 1);
            expression = GetMatchStr(m_re_result, expression, 2);
        }
    }

    // Only apply checks if we got all parts of the boolean expression
    if (lhs.empty() || op.empty() || rhs.empty())
        return;

    // Check that rhs do not contain logical operators.  We already know
    // that lhs is fine since the loop above parses out && and ||.
    if (rhs.find("&&") != std::string::npos || rhs.find("||") != std::string::npos)
        return;

    // At least one of the operands must be a constant literal.  This is
    // to avoid suggesting replacements for unprintable things like
    // CHECK(variable != iterator)
    //
    // The following pattern matches decimal, hex integers, strings, and
    // characters (in that order).
    lhs = StrStrip(lhs);
    rhs = StrStrip(rhs);
    static const regex_code RE_PATTERN_MATCH_CONSTANT =
        RegexCompile(R"(^([-+]?(\d+|0[xX][0-9a-fA-F]+)[lLuU]{0,3}|".*"|\'.*\')$)");
    if (RegexMatch(RE_PATTERN_MATCH_CONSTANT, lhs, m_re_result_temp) ||
        RegexMatch(RE_PATTERN_MATCH_CONSTANT, rhs, m_re_result_temp)) {
        // Note: since we know both lhs and rhs, we can provide a more
        // descriptive error message like:
        //   Consider using CHECK_EQ(x, 42) instead of CHECK(x == 42)
        // Instead of:
        //   Consider using CHECK_EQ instead of CHECK(a == b)
        //
        // We are still keeping the less descriptive message because if lhs
        // or rhs gets long, the error message might become unreadable.
        Error(linenum, "readability/check", 2,
              "Consider using " + CHECK_REPLACEMENT[check_macro][op] +
              " instead of " + check_macro + "(a " + op + " b)");
    }
}

void FileLinter::CheckAltTokens(const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;
    // Avoid preprocessor lines
    if (CheckFirstNonSpace(line, '#'))
        return;

    // Last ditch effort to avoid multi-line comments.  This will not help
    // if the comment started before the current line or ended after the
    // current line, but it catches most of the false positives.  At least,
    // it provides a way to workaround this warning for people who use
    // multi-line comments in preprocessor macros.
    //
    // TODO(unknown): remove this once cpplint has better support for
    // multi-line comments.
    if ((line.find("/*") != std::string::npos) || (line.find("*/") != std::string::npos))
        return;

    if (!RegexSearch(RE_PATTERN_ALT_TOKEN_REPLACEMENT, line, m_re_result))
        return;

    std::string str = line;
    {
        const std::string& key = GetMatchStr(m_re_result, str, 2);
        const std::string& token = AltTokenToToken(key);
        Error(linenum, "readability/alt_tokens", 2,
              "Use operator " + token + " instead of " + key);
        str = str.substr(GetMatchEnd(m_re_result, 0));  // remove the replaced part from str
    }

    while (!str.empty()) {
        bool match = RegexSearch(RE_PATTERN_ALT_TOKEN_REPLACEMENT, str, m_re_result);
        if (!match)
            break;  // replaced all tokens
        const std::string& key = GetMatchStr(m_re_result, str, 2);
        const std::string& token = AltTokenToToken(key);
        Error(linenum, "readability/alt_tokens", 2,
              "Use operator " + token + " instead of " + key);
        str = str.substr(GetMatchEnd(m_re_result, 0));  // remove the replaced part from str
    }
}

void FileLinter::CheckSectionSpacing(const CleansedLines& clean_lines,
                                     ClassInfo* classinfo, size_t linenum) {
    // Skip checks if the class is small, where small means 25 lines or less.
    // 25 lines seems like a good cutoff since that's the usual height of
    // terminals, and any class that can't fit in one screen can't really
    // be considered "small".
    //
    // Also skip checks if we are on the first line.  This accounts for
    // classes that look like
    //   class Foo { public: ... };
    //
    // If we didn't find the end of the class, last_line would be zero,
    // and the check will be skipped by the first condition.
    if (classinfo->LastLine() - classinfo->StartingLinenum() <= 24 ||
        linenum <= classinfo->StartingLinenum())
        return;

    const std::string& line = clean_lines.GetLineAt(linenum);
    bool matched = RegexMatch(R"(\s*(public|protected|private):)",
                              line, m_re_result);
    if (!matched)
        return;

    // Issue warning if the line before public/protected/private was
    // not a blank line, but don't do this if the previous line contains
    // "class" or "struct".  This can happen two ways:
    //  - We are at the beginning of the class.
    //  - We are forward-declaring an inner class that is semantically
    //    private, but needed to be public for implementation reasons.
    // Also ignores cases where the previous line ends with a backslash as can be
    // common when defining classes in C macros.
    const std::string& prev_line = clean_lines.GetLineAt(linenum - 1);
    if (!StrIsBlank(prev_line) &&
        !RegexSearch(R"(\b(class|struct)\b)", prev_line, m_re_result_temp) &&
        !RegexSearch(R"(\\$)", prev_line, m_re_result_temp)) {
        // Try a bit harder to find the beginning of the class.  This is to
        // account for multi-line base-specifier lists, e.g.:
        //   class Derived
        //       : public Base {
        size_t end_class_head = classinfo->StartingLinenum();
        for (size_t i = end_class_head; i < linenum; i++) {
            if (RegexSearch(R"(\{\s*$)", clean_lines.GetLineAt(i), m_re_result_temp)) {
                end_class_head = i;
                break;
            }
        }
        if (end_class_head < linenum - 1) {
            Error(linenum, "whitespace/blank_line", 3,
                  "\"" + GetMatchStr(m_re_result, line, 1) +
                  ":\" should be preceded by a blank line");
        }
    }
}

void FileLinter::CheckStyle(const CleansedLines& clean_lines,
                            const std::string& elided_line,
                            size_t linenum,
                            bool is_header_extension) {
    // Don't use "elided" lines here, otherwise we can't check commented lines.
    // Don't want to use "raw" either, because we don't want to check inside C++11
    // raw strings,
    const std::string& line = clean_lines.GetLineWithoutRawStringAt(linenum);

    if (line.find('\t') != std::string::npos) {
        Error(linenum, "whitespace/tab", 1,
              "Tab found; better to use spaces");
    }

    // One or three blank spaces at the beginning of the line is weird; it's
    // hard to reconcile that with 2-space indents.
    // NOTE: here are the conditions rob pike used for his tests.  Mine aren't
    // as sophisticated, but it may be worth becoming so:  RLENGTH==initial_spaces
    // if(RLENGTH > 20) complain = 0;
    // if(match($0, " +(error|private|public|protected):")) complain = 0;
    // if(match(prev, "&& *$")) complain = 0;
    // if(match(prev, "\\|\\| *$")) complain = 0;
    // if(match(prev, "[\",=><] *$")) complain = 0;
    // if(match($0, " <<")) complain = 0;
    // if(match(prev, " +for \\(")) complain = 0;
    // if(prevodd && match(prevprev, " +for \\(")) complain = 0;
    size_t initial_spaces = 0;
    const std::string& cleansed_line = elided_line;
    while (initial_spaces < line.size() && line[initial_spaces] == ' ')
        initial_spaces += 1;
    // There are certain situations we allow one space, notably for
    // section labels, and also lines containing multi-line raw strings.
    // We also don't check for lines that look like continuation lines
    // (of lines ending in double quotes, commas, equals, or angle brackets)
    // because the rules for how to indent those are non-trivial.
    static const regex_code RE_PATTERN_SPACES =
        RegexCompile(R"([",=><] *$)");
    static const regex_code RE_PATTERN_SCOPE_OR_LABEL =
        RegexCompile(R"(\s*(?:public|private|protected|signals)(?:\s+(?:slots\s*)?)?:\s*\\?$)");
    if (!(linenum > 0 &&
          RegexSearch(RE_PATTERN_SPACES,
                      clean_lines.GetLineWithoutRawStringAt(linenum - 1),
                      m_re_result_temp)) &&
        (initial_spaces == 1 || initial_spaces == 3) &&
        !RegexMatch(RE_PATTERN_SCOPE_OR_LABEL, cleansed_line, m_re_result_temp) &&
        !(clean_lines.GetRawLineAt(linenum) != line &&
          RegexMatch(R"(^\s*"")", line, m_re_result_temp))) {
        Error(linenum, "whitespace/indent", 3,
              "Weird number of spaces at line-start.  "
              "Are you using a 2-space indent?");
    }

    if (!line.empty() && IS_SPACE(line.back())) {
        Error(linenum, "whitespace/end_of_line", 4,
              "Line ends in whitespace.  Consider deleting these extra spaces.");
    }

    // Check if the line is a header guard.
    bool is_header_guard = false;
    if (is_header_extension && line[0] == '#') {
        if (line.starts_with("#ifndef " + m_cppvar) ||
            line.starts_with("#define " + m_cppvar) ||
            line.starts_with("#endif  // " + m_cppvar)) {
            is_header_guard = true;
        }
    }

    // #include lines and header guards can be long, since there's no clean way to
    // split them.
    //
    // URLs can be long too.  It's possible to split these, but it makes them
    // harder to cut&paste.
    //
    // The "$Id:...$" comment may also get very long without it being the
    // developers fault.
    //
    // Doxygen documentation copying can get pretty long when using an overloaded
    // function declaration
    static const regex_code RE_PATTERN_URL =
        RegexCompile(R"(^\s*//.*http(s?)://\S*$)");
    static const regex_code RE_PATTERN_COMMENT =
        RegexCompile(R"(^\s*//\s*[^\s]*$)");
    static const regex_code RE_PATTERN_ID =
        RegexCompile(R"(^// \$Id:.*#[0-9]+ \$$)");
    static const regex_code RE_PATTERN_DOC =
        RegexCompile(R"(^\s*/// [@\\](copydoc|copydetails|copybrief) .*$)");
    if (!line.starts_with("#include") && !is_header_guard &&
        !RegexMatch(RE_PATTERN_URL, line, m_re_result_temp) &&
        !RegexMatch(RE_PATTERN_COMMENT, line, m_re_result_temp) &&
        !RegexMatch(RE_PATTERN_ID, line, m_re_result_temp) &&
        !RegexMatch(RE_PATTERN_DOC, line, m_re_result_temp)) {
        size_t line_width = GetLineWidth(line);
        size_t line_length = m_options.LineLength();
        if (line_width > line_length) {
            Error(linenum, "whitespace/line_length", 2,
                  "Lines should be <= " + std::to_string(line_length) + " characters long");
        }
    }

    if (StrCount(cleansed_line, ';') > 1 &&
            // allow simple single line lambdas
            !RegexMatch(R"(^[^{};]*\[[^\[\]]*\][^{}]*\{[^{}\n\r]*\})", line, m_re_result_temp) &&
            // for loops are allowed two ;'s (and may run over two lines).
            !StrContain(cleansed_line, "for")) {
        const std::string& prev_line = GetPreviousNonBlankLine(clean_lines, linenum);
        if ((!StrContain(prev_line, "for") || StrContain(prev_line, ';')) &&
                // It's ok to have many commands in a switch case that fits in 1 line
                !((StrContain(cleansed_line, "case ") ||
                    StrContain(cleansed_line, "default:")) &&
                    StrContain(cleansed_line, "break;"))) {
            Error(linenum, "whitespace/newline", 0,
                  "More than one command on the same line");
        }
    }

    // Some more style checks
    CheckBraces(clean_lines, elided_line, linenum);
    CheckTrailingSemicolon(clean_lines, elided_line, linenum);
    CheckEmptyBlockBody(clean_lines, elided_line, linenum);
    CheckOperatorSpacing(clean_lines, elided_line, linenum);
    CheckParenthesisSpacing(elided_line, linenum);
    CheckCommaSpacing(clean_lines, elided_line, linenum);
    CheckSpacingForFunctionCall(elided_line, linenum);
    CheckCheck(clean_lines, elided_line, linenum);
    CheckAltTokens(elided_line, linenum);
}

void FileLinter::CheckStyleWithState(
                            const CleansedLines& clean_lines,
                            const std::string& elided_line,
                            size_t linenum,
                            NestingState* nesting_state) {
    CheckSpacing(clean_lines, elided_line, linenum, nesting_state);
    CheckBracesSpacing(clean_lines, elided_line, linenum, nesting_state);

    ClassInfo* classinfo = nesting_state->InnermostClass();
    if (classinfo != nullptr)
        CheckSectionSpacing(clean_lines, classinfo, linenum);
}

fs::path FileLinter::DropCommonSuffixes(const fs::path& file) {
    std::string basename = file.filename().string();
    fs::path dir = file.parent_path();
    for (const std::string& ext : m_non_header_extensions) {
        for (const char* const test_suffix : { "test.", "regtest.", "unittest." }) {
            std::string suffix = test_suffix + ext;
            if (basename.ends_with(suffix) && basename.size() > suffix.size()) {
                char c = basename[basename.size() - suffix.size() - 1];
                if (c == '-' || c == '_')
                    return dir / basename.substr(0, basename.size() - suffix.size() - 1);
            }
        }
    }
    for (const std::string& ext : m_header_extensions) {
        for (std::string suffix : { "inl.", "imp.", "internal." }) {
            suffix += ext;
            if (basename.ends_with(suffix) && basename.size() > suffix.size()) {
                char c = basename[basename.size() - suffix.size() - 1];
                if (c == '-' || c == '_')
                    return dir / basename.substr(0, basename.size() - suffix.size() - 1);
            }
        }
    }
    std::string ext = file.extension().string();
    return dir / basename.substr(0, basename.size() - ext.size());
}

int FileLinter::ClassifyInclude(const fs::path& path_from_repo,
                                const fs::path& include,
                                bool used_angle_brackets) {
    const std::string& include_order = m_options.IncludeOrder();
    std::string include_str = include.string();
    // This is a list of all standard c++ header files, except
    // those already checked for above.
    bool is_cpp_header = InStrVec(CPP_HEADERS, include_str);

    // Mark include as C header if in list or in a known folder for standard-ish C headers.
    bool is_std_c_header = (include_order == "default") || (InStrVec(C_HEADERS, include_str) ||
                            // additional linux glibc header folders
                            RegexSearch(GetHeaderFoldersPattern(), include_str, m_re_result_temp));

    // Headers with C++ extensions shouldn't be considered C system headers
    std::string include_ext = include.extension().string();
    bool is_system = used_angle_brackets &&
                     !InStrVec({ ".hh", ".hpp", ".hxx", ".h++" }, include_ext);

    if (is_system) {
        if (is_cpp_header)
            return CPP_SYS_HEADER;
        if (is_std_c_header)
            return C_SYS_HEADER;
        else
            return OTHER_SYS_HEADER;
    }

    // If the target file and the include we're checking share a
    // basename when we drop common extensions, and the include
    // lives in . , then it's likely to be owned by the target file.
    fs::path target_file = DropCommonSuffixes(path_from_repo);
    fs::path target_dir = target_file.parent_path();
    std::string target_base = target_file.filename().string();
    fs::path include_file = DropCommonSuffixes(include);
    fs::path include_dir = include_file.parent_path();
    std::string include_base = include_file.filename().string();
    fs::path target_dir_pub = (target_dir / "/../public").lexically_normal();
    if (target_base == include_base && (
            include_dir == target_dir ||
            include_dir == target_dir_pub))
        return LIKELY_MY_HEADER;

    // If the target and include share some initial basename
    // component, it's possible the target is implementing the
    // include, so it's allowed to be first, but we'll never
    // complain if it's not there.
    static const regex_code RE_PATTERN_FIRST_COMMENT = RegexCompile(R"(^[^-_.]+)");
    thread_local regex_match target_match = RegexCreateMatchData(RE_PATTERN_FIRST_COMMENT);
    thread_local regex_match include_match = RegexCreateMatchData(RE_PATTERN_FIRST_COMMENT);
    bool target_first_component =
        RegexMatch(RE_PATTERN_FIRST_COMMENT, target_base, target_match);
    bool include_first_component =
        RegexMatch(RE_PATTERN_FIRST_COMMENT, include_base, include_match);
    if (target_first_component && include_first_component &&
            GetMatchStr(target_match, target_base, 0) ==
                GetMatchStr(include_match, include_base, 0))
        return POSSIBLE_MY_HEADER;

    return OTHER_HEADER;
}

void FileLinter::CheckIncludeLine(const CleansedLines& clean_lines, size_t linenum,
                                  IncludeState* include_state) {
    const std::string& line = clean_lines.GetLineAt(linenum);

    // "include" should use the new style "foo/bar.h" instead of just "bar.h"
    // Only do this check if the included header follows google naming
    // conventions.  If not, assume that it's a 3rd party API that
    // requires special include conventions.
    //
    // We also make an exception for Lua headers, which follow google
    // naming convention but not the include convention.
    bool match = RegexMatch(R"---(#include\s*"([^/]+\.(.*))")---", line, m_re_result);
    static const regex_code RE_PATTERN_INCLUDE_EXT =
        RegexCompile(R"(^(?:[^/]*[A-Z][^/]*\.h|lua\.h|lauxlib\.h|lualib\.h)$)");
    if (match) {
        if (m_options.IsHeaderExtension(GetMatchStr(m_re_result, line, 2)) &&
            !RegexMatch(RE_PATTERN_INCLUDE_EXT,
                        GetMatchStr(m_re_result, line, 1), m_re_result_temp)) {
            Error(linenum, "build/include_subdir", 4,
                  "Include the directory when naming header files");
        }
    }

    // we shouldn't include a file more than once. actually, there are a
    // handful of instances where doing so is okay, but in general it's
    // not.
    match = RegexSearch(RE_PATTERN_INCLUDE, line, m_re_result);
    if (match) {
        std::string include = GetMatchStr(m_re_result, line, 2);
        bool used_angle_brackets = StrIsChar(GetMatchStr(m_re_result, line, 1), '<');
        size_t duplicate_line = include_state->FindHeader(include);
        if (duplicate_line != INDEX_NONE) {
            Error(linenum, "build/include", 4,
                  "\"" + include + "\" already included at " +
                  m_filename + ":" + std::to_string(duplicate_line));
            return;
        }

        for (const std::string& extension : m_non_header_extensions) {
            if (include.ends_with("." + extension) &&
                    m_file_from_repo.parent_path() !=
                        fs::path(include).make_preferred().parent_path()) {
                Error(linenum, "build/include", 4,
                      "Do not include ." + extension + " files from other packages");
                return;
            }
        }

        // We DO want to include a 3rd party looking header if it matches the
        // filename. Otherwise we get an erroneous error "...should include its
        // header" error later.
        bool third_src_header = false;
        thread_local std::string basefilename_relative =
            m_file_from_repo.string().substr(
                0, m_file_from_repo.string().size() -
                m_file_extension.size());
        for (const std::string& ext : m_header_extensions) {
            std::string headername = basefilename_relative + ext;
            if (StrContain(headername, include) || StrContain(include, headername)) {
                third_src_header = true;
                break;
            }
        }

        if (third_src_header || !RegexMatch(RE_PATTERN_INCLUDE_EXT, include, m_re_result_temp)) {
            include_state->LastIncludeList().emplace_back(include, linenum);

            // We want to ensure that headers appear in the right order:
            // 1) for foo.cc, foo.h  (preferred location)
            // 2) c system files
            // 3) cpp system files
            // 4) for foo.cc, foo.h  (deprecated location)
            // 5) other google headers
            //
            // We classify each include statement as one of those 5 types
            // using a number of techniques. The include_state object keeps
            // track of the highest type seen, and complains if we see a
            // lower type after that.
            std::string error_message = include_state->CheckNextIncludeOrder(
                    ClassifyInclude(m_file_from_repo, include, used_angle_brackets));
            if (!error_message.empty()) {
                std::string basename = m_file.filename().string();
                basename = basename.substr(0, basename.size() - m_file_extension.size() - 1);
                Error(linenum, "build/include_order", 4,
                      error_message + ". Should be: " + basename + ".h, c system,"
                      " c++ system, other.");
            }
            std::string canonical_include = include_state->CanonicalizeAlphabeticalOrder(include);
            if (!include_state->IsInAlphabeticalOrder(
                    clean_lines, linenum, canonical_include)) {
                Error(linenum, "build/include_alpha", 4,
                      "Include \"" + include + "\" not in alphabetical order");
            }
            include_state->SetLastHeader(canonical_include);
        }
    }
}

bool FileLinter::ExpectingFunctionArgs(const CleansedLines& clean_lines,
                                       const std::string& elided_line, size_t linenum) {
    static const regex_code RE_PATTERN_MOCK =
        RegexCompile(R"(^\s*MOCK_(CONST_)?METHOD\d+(_T)?\()");
    static const regex_code RE_PATTERN_MOCK2 =
        RegexCompile(R"(^\s*MOCK_(?:CONST_)?METHOD\d+(?:_T)?\((?:\S+,)?\s*$)");
    static const regex_code RE_PATTERN_MOCK3 =
        RegexCompile(R"(^\s*MOCK_(?:CONST_)?METHOD\d+(?:_T)?\(\s*$)");
    static const regex_code RE_PATTERN_STD_M =
        RegexCompile(R"(\bstd::m?function\s*\<\s*$)");
    return (RegexMatch(RE_PATTERN_MOCK, elided_line, m_re_result_temp) ||
            (linenum >= 2 &&
             (RegexMatch(RE_PATTERN_MOCK2,
                         clean_lines.GetElidedAt(linenum - 1), m_re_result_temp) ||
              RegexMatch(RE_PATTERN_MOCK3,
                         clean_lines.GetElidedAt(linenum - 2), m_re_result_temp) ||
              RegexSearch(RE_PATTERN_STD_M,
                          clean_lines.GetElidedAt(linenum - 1), m_re_result_temp))));
}

bool FileLinter::CheckCStyleCast(const CleansedLines& clean_lines,
                                 const std::string& elided_line, size_t linenum,
                                 const std::string& cast_type,
                                 const regex_code& pattern) {
    const std::string& line = elided_line;
    bool match = RegexSearch(pattern, line, m_re_result);
    if (!match)
        return false;

    // Exclude lines with keywords that tend to look like casts
    size_t pos = GetMatchStart(m_re_result, 1);
    std::string context = line.substr(0, (pos == 0) ? 0 : pos - 1);
    if (RegexMatch(R"(.*\b(?:sizeof|alignof|alignas|[_A-Z][_A-Z0-9]*)\s*$)",
                   context, m_re_result_temp))
        return false;

    // Try expanding current context to see if we one level of
    // parentheses inside a macro.
    if (linenum > 0) {
        size_t min_line = (linenum >= 5) ? linenum - 5 : 0;
        for (size_t i = linenum - 1; i > min_line; i--)
            context = clean_lines.GetElidedAt(i) + context;
    }
    if (RegexMatch(R"(.*\b[_A-Z][_A-Z0-9]*\s*\((?:\([^()]*\)|[^()])*$)",
                   context, m_re_result_temp))
        return false;

    // operator++(int) and operator--(int)
    if (context.ends_with(" operator++") || context.ends_with(" operator--") ||
            context.ends_with("::operator++") || context.ends_with("::operator--"))
        return false;

    // A single unnamed argument for a function tends to look like old style cast.
    // If we see those, don't issue warnings for deprecated casts.
    std::string remainder = line.substr(GetMatchEnd(m_re_result, 0));
    if (RegexMatch(R"(^\s*(?:;|const\b|throw\b|final\b|override\b|[=>{),]|->))",
                   remainder, m_re_result_temp))
        return false;

    // At this point, all that should be left is actual casts.
    Error(linenum, "readability/casting", 4,
          "Using C-style cast.  Use " + cast_type +
          "<" + GetMatchStr(m_re_result, line, 1) + ">(...) instead");

    return true;
}

void FileLinter::CheckCasts(const CleansedLines& clean_lines,
                            const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;

    // Check to see if they're using an conversion function cast.
    // I just try to capture the most common basic types, though there are more.
    // Parameterless conversion functions, such as bool(), are allowed as they are
    // probably a member operator declaration or default constructor.
    static const regex_code RE_PATTERN_CAST =
        RegexCompile(R"((\bnew\s+(?:const\s+)?|\S<\s*(?:const\s+)?)?\b)"
                     R"((int|float|double|bool|char|int32|uint32|int64|uint64))"
                     R"((\([^)].*))");
    bool match = RegexSearch(RE_PATTERN_CAST, line, m_re_result);
    bool expecting_function = ExpectingFunctionArgs(clean_lines, elided_line, linenum);
    if (match && !expecting_function) {
        const std::string& matched_type = GetMatchStr(m_re_result, line, 2);

        // matched_new_or_template is used to silence two false positives:
        // - New operators
        // - Template arguments with function types
        //
        // For template arguments, we match on types immediately following
        // an opening bracket without any spaces.  This is a fast way to
        // silence the common case where the function type is the first
        // template argument.  False negative with less-than comparison is
        // avoided because those operators are usually followed by a space.
        //
        //   function<double(double)>   // bracket + no space = false positive
        //   value < double(42)         // bracket + space = true positive
        std::string matched_new_or_template = GetMatchStr(m_re_result, line, 1);

        // Avoid arrays by looking for brackets that come after the closing
        // parenthesis.
        if (RegexMatch(R"(\([^()]+\)\s*\[)",
                       GetMatchStr(m_re_result, line, 3), m_re_result_temp))
            return;

        // Other things to ignore:
        // - Function pointers
        // - Casts to pointer types
        // - Placement new
        // - Alias declarations
        const std::string& matched_funcptr = GetMatchStr(m_re_result, line, 3);
        if (matched_new_or_template.empty() &&
                !(!matched_funcptr.empty() &&
                  (RegexMatch(R"(\((?:[^() ]+::\s*\*\s*)?[^() ]+\)\s*\()",
                                 matched_funcptr) ||
                   matched_funcptr.starts_with("(*)"))) &&
                !RegexMatch(R"(\s*using\s+\S+\s*=\s*)" + matched_type, line) &&
                !RegexSearch(R"(new\(\S+\)\s*)" + matched_type, line)) {
            Error(linenum, "readability/casting", 4,
                  "Using deprecated casting style.  "
                  "Use static_cast<" + matched_type + ">(...) instead");
        }
    }

    if (!expecting_function) {
        static const regex_code RE_PATTERN_STATIC_CAST =
            RegexCompile(R"(\((int|float|double|bool|char|u?int(16|32|64)|size_t)\))");
        CheckCStyleCast(clean_lines,
                        elided_line, linenum, "static_cast",
                        RE_PATTERN_STATIC_CAST);
    }

    // This doesn't catch all cases. Consider (const char * const)"hello".
    //
    // (char *) "foo" should always be a const_cast (reinterpret_cast won't
    // compile).
    static const regex_code RE_PATTERN_CONST_CAST =
        RegexCompile(R"(\((char\s?\*+\s?)\)\s*")");
    if (CheckCStyleCast(clean_lines,
                        elided_line, linenum, "const_cast",
                        RE_PATTERN_CONST_CAST)) {
    } else {
        // Check pointer casts for other than string constants
        static const regex_code RE_PATTERN_REINTERPRET_CAST =
            RegexCompile(R"(\((\w+\s?\*+\s?)\))");
        CheckCStyleCast(clean_lines,
                        elided_line, linenum, "reinterpret_cast",
                        RE_PATTERN_REINTERPRET_CAST);
    }

    // In addition, we look for people taking the address of a cast.  This
    // is dangerous -- casts can assign to temporaries, so the pointer doesn't
    // point where you think.
    //
    // Some non-identifier character is required before the '&' for the
    // expression to be recognized as a cast.  These are casts:
    //   expression = &static_cast<int*>(temporary());
    //   function(&(int*)(temporary()));
    //
    // This is not a cast:
    //   reference_type&(int* function_param);
    static const regex_code RE_PATTERN_CAST_TYPE =
        RegexCompile(R"((?:[^\w]&\(([^)*][^)]*)\)[\w(])|)"
                     R"((?:[^\w]&(static|dynamic|down|reinterpret)_cast\b))");
    match = RegexSearch(RE_PATTERN_CAST_TYPE, line, m_re_result_temp);
    if (match) {
        // Try a better error message when the & is bound to something
        // dereferenced by the casted pointer, as opposed to the casted
        // pointer itself.
        bool parenthesis_error = false;
        match = RegexMatch(
                    R"(^(.*&(?:static|dynamic|down|reinterpret)_cast\b)<)",
                    line, m_re_result);
        if (match) {
            size_t y1 = linenum;
            size_t x1 = GetMatchSize(m_re_result, 1);
            CloseExpression(clean_lines, &y1, &x1);
            if (x1 != INDEX_NONE && clean_lines.GetElidedAt(y1)[x1] == '(') {
                size_t y2 = y1;
                size_t x2 = x1;
                CloseExpression(clean_lines, &y2, &x2);
                if (x2 != INDEX_NONE) {
                    std::string extended_line = clean_lines.GetElidedAt(y2).substr(x2);
                    if (y2 < clean_lines.NumLines() - 1)
                        extended_line += clean_lines.GetElidedAt(y2 + 1);
                    if (RegexMatch(R"(\s*(?:->|\[))", extended_line, m_re_result_temp))
                        parenthesis_error = true;
                }
            }
        }

        if (parenthesis_error) {
            Error(linenum, "readability/casting", 4,
                    "Are you taking an address of something dereferenced "
                    "from a cast?  Wrapping the dereferenced expression in "
                    "parentheses will make the binding more obvious");
        } else {
            Error(linenum, "runtime/casting", 4,
                    "Are you taking an address of a cast?  "
                    "This is dangerous: could be a temp var.  "
                    "Take the address before doing the cast, rather than after");
        }
    }
}

void FileLinter::CheckGlobalStatic(const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;

    // Check for people declaring static/global STL strings at the top level.
    // This is dangerous because the C++ language does not guarantee that
    // globals with constructors are initialized before the first access, and
    // also because globals can be destroyed when some threads are still running.
    // TODO(unknown): Generalize this to also find static unique_ptr instances.
    // TODO(unknown): File bugs for clang-tidy to find these.
    static const regex_code RE_PATTERN_GLOBAL_CAST =
        RegexCompile("((?:|static +)(?:|const +))(?::*std::)?string( +const)? +"
                     R"(([a-zA-Z0-9_:]+)\b(.*))");
    bool match = RegexMatch(RE_PATTERN_GLOBAL_CAST, line, m_re_result);

    // Remove false positives:
    // - String pointers (as opposed to values).
    //    string *pointer
    //    const string *pointer
    //    string const *pointer
    //    string *const pointer
    //
    // - Functions and template specializations.
    //    string Function<Type>(...
    //    string Class<Type>::Method(...
    //
    // - Operators.  These are matched separately because operator names
    //   cross non-word boundaries, and trying to match both operators
    //   and functions at the same time would decrease accuracy of
    //   matching identifiers.
    //    string Class::operator*()
    if (match &&
        !RegexSearch(R"(\bstring\b(\s+const)?\s*[\*\&]\s*(const\s+)?\w)",
                     line, m_re_result_temp) &&
        !RegexSearch(R"(\boperator\W)", line, m_re_result_temp) &&
        !RegexMatch(R"(\s*(<.*>)?(::[a-zA-Z0-9_]+)*\s*\(([^"]|$))",
                    GetMatchStr(m_re_result, line, 4), m_re_result_temp)) {
        if (RegexSearch(R"(\bconst\b)", line, m_re_result_temp)) {
            Error(linenum, "runtime/string", 4,
                  "For a static/global string constant, use a C style string instead:"
                  " \"" + GetMatchStr(m_re_result, line, 1) + "char" +
                  GetMatchStr(m_re_result, line, 2) + " " +
                  GetMatchStr(m_re_result, line, 3) + "[]\".");
        } else {
            Error(linenum, "runtime/string", 4,
                  "Static/global string variables are not permitted.");
        }
    }

    static const regex_code RE_PATTERN_INIT_WITH_ITSELF =
        RegexCompile(R"(\b([A-Za-z0-9_]*_)\(\1\))");
    static const regex_code RE_PATTERN_INIT_WITH_ITSELF2 =
        RegexCompile(R"(\b([A-Za-z0-9_]*_)\(CHECK_NOTNULL\(\1\)\))");
    if (RegexSearch(RE_PATTERN_INIT_WITH_ITSELF, line, m_re_result_temp) ||
        RegexSearch(RE_PATTERN_INIT_WITH_ITSELF2, line, m_re_result_temp)) {
        Error(linenum, "runtime/init", 4,
              "You seem to be initializing a member variable with itself.");
    }
}

void FileLinter::CheckPrintf(const std::string& elided_line, size_t linenum) {
    const std::string& line = elided_line;
    bool match;

    // We don't need to search two regex patterns
    // if the line does not have printf.
    if (StrContain(line, "printf")) {
        // When snprintf is used, the second argument shouldn't be a literal.
        static const regex_code RE_PATTERN_SNPRINTF =
            RegexCompile(R"(snprintf\s*\(([^,]*),\s*([0-9]*)\s*,)");
        match = RegexSearch(
                        RE_PATTERN_SNPRINTF, line, m_re_result);
        if (match && !StrIsChar(GetMatchStr(m_re_result, line, 2), '0')) {
            // If 2nd arg is zero, snprintf is used to calculate size.
            Error(linenum, "runtime/printf", 3, "If you can, use"
                " sizeof(" + GetMatchStr(m_re_result, line, 1) + ") instead of " +
                GetMatchStr(m_re_result, line, 2) +
                " as the 2nd arg to snprintf.");
        }

        // Check if some verboten C functions are being used.
        static const regex_code RE_PATTERN_SPRINTF =
            RegexCompile(R"(\bsprintf\s*\()");
        if (RegexSearch(RE_PATTERN_SPRINTF, line, m_re_result_temp)) {
            Error(linenum, "runtime/printf", 5,
                "Never use sprintf. Use snprintf instead.");
        }
    }

    static const regex_code RE_PATTERN_STRFUNC =
        RegexCompile(R"(\b(strcpy|strcat)\s*\()");
    match = RegexSearch(RE_PATTERN_STRFUNC, line, m_re_result);
    if (match) {
        Error(linenum, "runtime/printf", 4,
              "Almost always, snprintf is better than " + GetMatchStr(m_re_result, line, 1));
    }
}

static std::string GetTextInside(const std::string& text,
                                 const regex_code& start_pattern,
                                 regex_match& re_result) {
    /*
    Retrieves all the text between matching open and close parentheses.

    Given a string of lines and a regular expression string, retrieve all the text
    following the expression and between opening punctuation symbols like
    (, [, or {, and the matching close-punctuation symbol. This properly nested
    occurrences of the punctuations, so for the text like
        printf(a(), b(c()));
    a call to _GetTextInside(text, r'printf\(') will return 'a(), b(c())'.
    start_pattern must match string having an open punctuation symbol at the end.
    */
    // TODO(unknown): Audit cpplint.cpp to see what places could be profitably
    // rewritten to use _GetTextInside (and use inferior regexp matching today).

    // Find the position to start extracting text.
    bool match = RegexSearch(start_pattern, text, re_result);
    if (!match)  // start_pattern not found in text.
        return "";
    size_t start_position = GetMatchEnd(re_result, 0);

    // start_pattern must ends with an opening punctuation.
    assert(start_position > 0);
    // start_pattern must ends with an opening punctuation.
    assert(StrContain("({[", text[start_position - 1]));

    // Stack of closing punctuations we expect to have in text after position.
    std::stack<char> punctuation_stack;
    {
        char c = text[start_position - 1];
        if (c == '(')
            punctuation_stack.push(')');
        else if (c == '{')
            punctuation_stack.push('}');
        else if (c == '[')
            punctuation_stack.push(']');
    }
    size_t position = start_position;
    while (!punctuation_stack.empty() && position < text.size()) {
        char c = text[position];
        if (c == punctuation_stack.top()) {
            punctuation_stack.pop();
        } else if (c == ')' || c == ']' || c == '}') {
            // A closing punctuation without matching opening punctuations.
            return "";
        } else if (c == '(' || c == '[' || c == '{') {
            if (c == '(')
                punctuation_stack.push(')');
            else if (c == '{')
                punctuation_stack.push('}');
            else if (c == '[')
                punctuation_stack.push(']');
        }
        position++;
    }
    if (!punctuation_stack.empty()) {
        // Opening punctuations left without matching close-punctuations.
        return "";
    }
    // punctuations match.
    return text.substr(start_position, position - 1 - start_position);
}

void FileLinter::CheckLanguage(const CleansedLines& clean_lines,
                               const std::string& elided_line, size_t linenum,
                               bool is_header_extension,
                               IncludeState* include_state) {
    // If the line is empty or consists of entirely a comment, no need to
    // check it.
    const std::string& line = elided_line;
    if (line.empty())
        return;

    bool match = RegexSearch(RE_PATTERN_INCLUDE, line, m_re_result_temp);
    if (match) {
        CheckIncludeLine(clean_lines, linenum, include_state);
        return;
    }

    // Reset include state across preprocessor directives.  This is meant
    // to silence warnings for conditional includes.
    regex_match m;
    static const regex_code RE_PATTERN_CONDITIONAL_MACRO =
        RegexCompile(R"(^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b)");
    match = RegexMatch(RE_PATTERN_CONDITIONAL_MACRO, line, m_re_result);
    if (match)
        include_state->ResetSection(GetMatchStr(m_re_result, line, 1));


    // Perform other checks now that we are sure that this is not an include line
    CheckCasts(clean_lines, elided_line, linenum);
    static const regex_code RE_PATTERN_MULTILINE_TOKEN =
        RegexCompile("[;({]");
    if (linenum + 1 < clean_lines.NumLines() &&
            !RegexSearch(RE_PATTERN_MULTILINE_TOKEN, line, m_re_result_temp)) {
        // Match two lines at a time to support multiline declarations
        std::string new_line = line + StrStrip(clean_lines.GetElidedAt(linenum + 1));
        CheckGlobalStatic(new_line, linenum);
    } else {
        CheckGlobalStatic(elided_line, linenum);
    }
    CheckPrintf(elided_line, linenum);

    if (is_header_extension) {
        // TODO(unknown): check that 1-arg constructors are explicit.
        //                How to tell it's a constructor?
        //                (handled in CheckForNonStandardConstructs for now)
        // TODO(unknown): check that classes declare or disable copy/assign
        //                (level 1 error)
    }

    // Check if people are using the verboten C basic types.  The only exception
    // we regularly allow is "unsigned short port" for port.
    static const regex_code RE_PATTERN_SHORT_PORT =
        RegexCompile(R"(\bshort port\b)");
    if (RegexSearch(RE_PATTERN_SHORT_PORT, line, m_re_result_temp)) {
        if (!RegexSearch(R"(\bunsigned short port\b)", line, m_re_result_temp)) {
            Error(linenum, "runtime/int", 4,
                  "Use \"unsigned short\" for ports, not \"short\"");
        }
    } else {
        static const regex_code RE_PATTERN_CINT =
            RegexCompile(R"(\b(short|long(?! +double)|long long)\b)");
        match = RegexSearch(RE_PATTERN_CINT, line, m_re_result);
        if (match) {
            Error(linenum, "runtime/int", 4,
                  "Use int16/int64/etc, rather than the C type " +
                  GetMatchStr(m_re_result, line, 1));
        }
    }

    // Check if some verboten operator overloading is going on
    // TODO(unknown): catch out-of-line unary operator&:
    //   class X {};
    //   int operator&(const X& x) { return 42; }  // unary operator&
    // The trick is it's hard to tell apart from binary operator&:
    //   class Y { int operator&(const Y& x) { return 23; } }; // binary operator&
    static const regex_code RE_PATTERN_UNARY_OP =
        RegexCompile(R"(\boperator\s*&\s*\(\s*\))");
    if (RegexSearch(RE_PATTERN_UNARY_OP, line, m_re_result_temp)) {
        Error(linenum, "runtime/operator", 4,
              "Unary operator& is dangerous.  Do not use it.");
    }

    // Check for suspicious usage of "if" like
    // } if (a == b) {
    static const regex_code RE_PATTERN_IF_AFTER_BRACE =
        RegexCompile(R"(\}\s*if\s*\()");
    if (RegexSearch(RE_PATTERN_IF_AFTER_BRACE, line, m_re_result_temp)) {
        Error(linenum, "readability/braces", 4,
              "Did you mean \"else if\"? If not, start a new line for \"if\".");
    }

    // Check for potential format string bugs like printf(foo).
    // We constrain the pattern not to pick things like DocidForPrintf(foo).
    // Not perfect but it can catch printf(foo.c_str()) and printf(foo->c_str())
    // TODO(unknown): Catch the following case. Need to change the calling
    // convention of the whole function to process multiple line to handle it.
    //   printf(
    //       boy_this_is_a_really_long_variable_that_cannot_fit_on_the_prev_line);
    static const regex_code RE_PATTERN_PRINTF_ARGS =
        RegexCompile(R"((?i)\b(string)?printf\s*\()", REGEX_OPTIONS_MULTILINE);
    std::string printf_args = GetTextInside(line, RE_PATTERN_PRINTF_ARGS, m_re_result);
    if (!printf_args.empty()) {
        match = RegexMatch(R"(([\w.\->()]+)$)", printf_args, m_re_result);
        if (match && GetMatchStr(m_re_result, printf_args, 1) != "__VA_ARGS__") {
            thread_local regex_match funcm = RegexCreateMatchData(2);
            RegexSearch(R"(\b((?:string)?printf)\s*\()",
                        line, funcm, REGEX_OPTIONS_ICASE);
            const std::string& function_name = GetMatchStr(funcm, line, 1);
            Error(linenum, "runtime/printf", 4,
                  "Potential format string bug. Do " +
                  function_name + "(\"%s\", " +
                  GetMatchStr(m_re_result, printf_args, 1) + ") instead.");
        }
    }

    // Check for potential memset bugs like memset(buf, sizeof(buf), 0).
    static const regex_code RE_PATTERN_MEMSET =
        RegexCompile(R"(memset\s*\(([^,]*),\s*([^,]*),\s*0\s*\))");
    match = RegexSearch(RE_PATTERN_MEMSET, line, m_re_result);
    if (match && !RegexMatch(R"(^''|-?[0-9]+|0x[0-9A-Fa-f]$)",
                             GetMatchStr(m_re_result, line, 2), m_re_result_temp)) {
        Error(linenum, "runtime/memset", 4,
              "Did you mean \"memset(" + GetMatchStr(m_re_result, line, 1) +
              ", 0, " + GetMatchStr(m_re_result, line, 2) + ")\"?");
    }

    static const regex_code RE_PATTERN_NAMESPACE_USING =
        RegexCompile(R"(\busing namespace\b)");
    if (RegexSearch(RE_PATTERN_NAMESPACE_USING, line, m_re_result_temp)) {
        if (RegexSearch(R"(\bliterals\b)", line, m_re_result_temp)) {
            Error(linenum, "build/namespaces_literals", 5,
                  "Do not use namespace using-directives.  "
                  "Use using-declarations instead.");
        } else {
            Error(linenum, "build/namespaces", 5,
                  "Do not use namespace using-directives.  "
                  "Use using-declarations instead.");
        }
    }

    // Detect variable-length arrays.
    static const regex_code RE_PATTERN_VARIABLE_LENGTH_ARRAY =
        RegexCompile(R"(\s*(.+::)?(\w+) [a-z]\w*\[(.+)];)");
    match = RegexMatch(RE_PATTERN_VARIABLE_LENGTH_ARRAY, line, m_re_result);

    if (match) {
        std::string str2 = GetMatchStr(m_re_result, line, 2);
        std::string str3 = GetMatchStr(m_re_result, line, 3);

        if (str2 != "return" && str2 != "delete" &&
            str3.find(']') == std::string::npos) {
            // Split the size using space and arithmetic operators as delimiters.
            // If any of the resulting tokens are not compile time constants then
            // report the error.
            std::vector<std::string> tokens = RegexSplit(R"(\s|\+|\-|\*|\/|<<|>>])", str3);
            bool is_const = true;
            bool skip_next = false;
            for (const std::string& tok : tokens) {
                if (skip_next) {
                    skip_next = false;
                    continue;
                }

                if (RegexSearch(R"(sizeof\(.+\))", tok, m_re_result_temp))
                    continue;
                if (RegexSearch(R"(arraysize\(\w+\))", tok, m_re_result_temp))
                    continue;

                std::string tok_clean = StrLstrip(tok, '(');
                tok_clean = StrRstrip(tok_clean, ')');
                if (tok_clean.empty()) continue;
                if (RegexMatch(R"(\d+)", tok_clean, m_re_result_temp)) continue;
                if (RegexMatch("0[xX][0-9a-fA-F]+", tok_clean, m_re_result_temp)) continue;
                if (RegexMatch(R"(k[A-Z0-9]\w*)", tok_clean, m_re_result_temp)) continue;
                if (RegexMatch(R"((.+::)?k[A-Z0-9]\w*)", tok_clean, m_re_result_temp)) continue;
                if (RegexMatch("(.+::)?[A-Z][A-Z0-9_]*", tok_clean, m_re_result_temp)) continue;
                // A catch all for tricky sizeof cases, including 'sizeof expression',
                // 'sizeof(*type)', 'sizeof(const type)', 'sizeof(struct StructName)'
                // requires skipping the next token because we split on ' ' and '*'.
                if (tok_clean.starts_with("sizeof")) {
                    skip_next = true;
                    continue;
                }
                is_const = false;
                break;
            }
            if (!is_const) {
                Error(linenum, "runtime/arrays", 1,
                      "Do not use variable-length arrays.  Use an appropriately named "
                      "('k' followed by CamelCase) compile-time constant for the size.");
            }
        }
    }

    // Check for use of unnamed namespaces in header files.  Registration
    // macros are typically OK, so we allow use of "namespace {" on lines
    // that end with backslashes.
    static const regex_code RE_PATTERN_NAMESPACE_HEAD =
        RegexCompile(R"(\bnamespace\s*{)");
    if (is_header_extension &&
        RegexSearch(RE_PATTERN_NAMESPACE_HEAD, line, m_re_result_temp) &&
        line.back() != '\\') {
        Error(linenum, "build/namespaces_headers", 4,
              "Do not use unnamed namespaces in header files.  See "
              "https://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Namespaces"
              " for more information.");
    }
}

static const regex_code RE_PATTERN_FUNC_START =
    RegexCompile(R"(^([^()]*\w+)\()");
static const regex_code RE_PATTERN_OVERRIDE =
    RegexCompile(R"(\boverride\b)");

// Check if current line contains an inherited function.
static bool IsDerivedFunction(const CleansedLines& clean_lines, size_t linenum,
                              regex_match& re_result) {
    // Scan back a few lines for start of current function
    size_t min_line = (linenum >= 10) ? linenum - 10 : 0;
    for (size_t i = linenum;; i--) {
        const std::string& line = clean_lines.GetElidedAt(i);
        bool match = RegexMatch(RE_PATTERN_FUNC_START, line, re_result);
        if (match) {
            // Look for "override" after the matching closing parenthesis
            size_t closing_paren = GetMatchSize(re_result, 1);
            size_t pos = i;
            const std::string& close_line = CloseExpression(
                                        clean_lines, &pos, &closing_paren);
            return (closing_paren != INDEX_NONE &&
                    RegexSearch(RE_PATTERN_OVERRIDE, close_line.substr(closing_paren), re_result));
        }
        if (i == min_line) break;
    }
    return false;
}

// Check if current line contains an out-of-line method definition.
static bool IsOutOfLineMethodDefinition(const CleansedLines& clean_lines, size_t linenum,
                                        regex_match& re_result_temp) {
    // Scan back a few lines for start of current function
    size_t min_line = (linenum >= 10) ? linenum - 10 : 0;
    for (size_t i = linenum;; i--) {
        const std::string& line = clean_lines.GetElidedAt(i);
        if (RegexMatch(RE_PATTERN_FUNC_START, line, re_result_temp)) {
            return RegexMatch(R"(^[^()]*\w+::\w+\()", line, re_result_temp);
        }
        if (i == min_line) break;
    }
    return false;
}

// Check if current line is inside constructor initializer list.
static bool IsInitializerList(const CleansedLines& clean_lines, size_t linenum,
                              regex_match& re_result) {
    for (size_t i = linenum; i > 1; i--) {
        std::string line = clean_lines.GetElidedAt(i);

        if (i == linenum) {
            bool remove_function_body = RegexMatch(R"(^(.*)\{\s*$)", line, re_result);
            if (remove_function_body)
                line = GetMatchStr(re_result, line, 1);
        }
        static const regex_code RE_PATTERN_COLON =
            RegexCompile(R"(\s:\s*\w+[({])");
        if (RegexSearch(RE_PATTERN_COLON, line, re_result)) {
            // A lone colon tend to indicate the start of a constructor
            // initializer list.  It could also be a ternary operator, which
            // also tend to appear in constructor initializer lists as
            // opposed to parameter lists.
            return true;
        }
        static const regex_code RE_PATTERN_CLOSING_BRACE =
            RegexCompile(R"(\}\s*,\s*$)");
        if (RegexSearch(RE_PATTERN_CLOSING_BRACE, line, re_result)) {
            // A closing brace followed by a comma is probably the end of a
            // brace-initialized member in constructor initializer list.
            return true;
        }
        static const regex_code RE_PATTERN_BRACE =
            RegexCompile(R"([{};]\s*$)");
        if (RegexSearch(RE_PATTERN_BRACE, line, re_result)) {
            // Found one of the following:
            // - A closing brace or semicolon, probably the end of the previous
            //   function.
            // - An opening brace, probably the start of current class or namespace.
            //
            // Current line is probably not inside an initializer list since
            // we saw one of those things without seeing the starting colon.
            return false;
        }
    }

    // Got to the beginning of the file without seeing the start of
    // constructor initializer list.
    return false;
}

// Patterns for matching call-by-reference parameters.
//
// Supports nested templates up to 2 levels deep using this messy pattern:
//   < (?: < (?: < [^<>]*
//               >
//           |   [^<>] )*
//         >
//     |   [^<>] )*
//   >
#define RE_PATTERN_IDENT R"([_a-zA-Z]\w*)"
#define RE_PATTERN_TYPE \
        R"((?:const\s+)?(?:typename\s+|class\s+|struct\s+|union\s+|enum\s+)?)" \
        R"((?:\w|)" \
        R"(\s*<(?:<(?:<[^<>]*>|[^<>])*>|[^<>])*>|)" \
        R"(::)+)"

// A call-by-reference parameter ends with '& identifier'.
const regex_code RE_REF_PARAM = RegexCompile(
        "(" RE_PATTERN_TYPE R"((?:\s*(?:\bconst\b|[*]))*\s*)"
        R"(&\s*)" RE_PATTERN_IDENT R"()\s*(?:=[^,()]+)?[,)])");
// A call-by-const-reference parameter either ends with 'const& identifier'
// or looks like 'const type& identifier' when 'type' is atomic.
const regex_code RE_CONST_REF_PARAM = RegexCompile(
        R"((?:.*\s*\bconst\s*&\s*)" RE_PATTERN_IDENT
        R"(|const\s+)" RE_PATTERN_TYPE R"(\s*&\s*)" RE_PATTERN_IDENT ")");

// Stream types.
const regex_code RE_REF_STREAM_PARAM = RegexCompile(
        R"((?:.*stream\s*&\s*)" RE_PATTERN_IDENT ")");

const regex_code RE_PATTERN_ALLOWED_FUNCTIONS =
    RegexCompile(
        R"((?:[sS]wap(?:<\w:+>)?|)"
        R"(operator\s*[<>][<>]|)"
        R"(static_assert|COMPILE_ASSERT)"
        R"()\s*\()");

void FileLinter::CheckForNonConstReference(const CleansedLines& clean_lines,
                                           const std::string& elided_line, size_t linenum,
                                           NestingState* nesting_state) {
    // Do nothing if there is no '&' on current line.
    if (!StrContain(elided_line, '&'))
        return;

    // If a function is inherited, current function doesn't have much of
    // a choice, so any non-const references should not be blamed on
    // derived function.
    if (IsDerivedFunction(clean_lines, linenum, m_re_result))
        return;

    // Don't warn on out-of-line method definitions, as we would warn on the
    // in-line declaration, if it isn't marked with 'override'.
    if (IsOutOfLineMethodDefinition(clean_lines, linenum, m_re_result_temp))
        return;

    std::string line = elided_line;

    // Long type names may be broken across multiple lines, usually in one
    // of these forms:
    //   LongType
    //       ::LongTypeContinued &identifier
    //   LongType::
    //       LongTypeContinued &identifier
    //   LongType<
    //       ...>::LongTypeContinued &identifier
    //
    // If we detected a type split across two lines, join the previous
    // line to current line so that we can match const references
    // accordingly.
    //
    // Note that this only scans back one line, since scanning back
    // arbitrary number of lines would be expensive.  If you have a type
    // that spans more than 2 lines, please use a typedef.
    if (linenum > 1) {
        bool previous = false;
        static const regex_code RE_PATTERN_STARTS_WITH_COLONS =
            RegexCompile(R"(\s*::(?:[\w<>]|::)+\s*&\s*\S)");
        static const regex_code RE_PATTERN_STARTS_WITH_COLONS2 =
            RegexCompile(R"(\s*[a-zA-Z_]([\w<>]|::)+\s*&\s*\S)");
        if (RegexMatch(RE_PATTERN_STARTS_WITH_COLONS, line, m_re_result_temp)) {
            // previous_line\n + ::current_line
            previous = RegexSearch(
                            R"(\b((?:const\s*)?(?:[\w<>]|::)+[\w<>])\s*$)",
                            clean_lines.GetElidedAt(linenum - 1), m_re_result);
        } else if (RegexMatch(RE_PATTERN_STARTS_WITH_COLONS2, line, m_re_result_temp)) {
            // previous_line::\n + current_line
            previous = RegexSearch(
                            R"(\b((?:const\s*)?(?:[\w<>]|::)+::)\s*$)",
                            clean_lines.GetElidedAt(linenum - 1), m_re_result);
        }
        if (previous) {
            line = GetMatchStr(m_re_result,
                               clean_lines.GetElidedAt(linenum - 1), 1) + StrLstrip(line);
        } else {
            // Check for templated parameter that is split across multiple lines
            size_t endpos = line.rfind('>');
            if (endpos != std::string::npos) {
                size_t startline = linenum;
                size_t startpos = endpos;
                ReverseCloseExpression(clean_lines, &startline, &startpos);
                if (startpos != INDEX_NONE && startline < linenum) {
                    // Found the matching < on an earlier line, collect all
                    // pieces up to current line.
                    line = "";
                    for (size_t i = startline; i < linenum + 1; i++)
                        line += StrStrip(clean_lines.GetElidedAt(i));
                }
            }
        }
    }

    // Check for non-const references in function parameters.  A single '&' may
    // found in the following places:
    //   inside expression: binary & for bitwise AND
    //   inside expression: unary & for taking the address of something
    //   inside declarators: reference parameter
    // We will exclude the first two cases by checking that we are not inside a
    // function body, including one that was just introduced by a trailing '{'.
    // TODO(unknown): Doesn't account for 'catch(Exception& e)' [rare].
    BlockInfo* previous_top = nesting_state->PreviousStackTop();
    if (previous_top != nullptr &&
        !(previous_top->IsClassInfo() || previous_top->IsNamespaceInfo())) {
        // Not at toplevel, not within a class, and not within a namespace
        return;
    }

    // Avoid initializer lists.  We only need to scan back from the
    // current line for something that starts with ':'.
    //
    // We don't need to check the current line, since the '&' would
    // appear inside the second set of parentheses on the current line as
    // opposed to the first set.
    if (linenum > 0) {
        size_t min_line = (linenum >= 10) ? linenum - 10 : 0;
        for (size_t i = linenum - 1; i > min_line; i--) {
            const std::string& previous_line = clean_lines.GetElidedAt(i);
            if (!RegexSearch(R"([),]\s*$)", previous_line, m_re_result_temp))
                break;
            if (RegexMatch(R"(^\s*:\s+\S)", previous_line, m_re_result_temp))
                return;
        }
    }

    // Avoid preprocessors
    if (RegexSearch(R"(\\\s*$)", line, m_re_result_temp))
        return;

    // Avoid constructor initializer lists
    if (IsInitializerList(clean_lines, linenum, m_re_result))
        return;

    // We allow non-const references in a few standard places, like functions
    // called "swap()" or iostream operators like "<<" or ">>".  Do not check
    // those function parameters.
    //
    // We also accept & in static_assert, which looks like a function but
    // it's actually a declaration expression.

    if (RegexSearch(RE_PATTERN_ALLOWED_FUNCTIONS, line, m_re_result_temp)) {
        return;
    } else if (!RegexSearch(R"(\S+\([^)]*$)", line, m_re_result_temp)) {
        // Don't see an allowed function on this line.  Actually we
        // didn't see any function name on this line, so this is likely a
        // multi-line parameter list.  Try a bit harder to catch this case.
        for (size_t i = 0; i < 2; i++) {
            if (linenum > i &&
                RegexSearch(RE_PATTERN_ALLOWED_FUNCTIONS,
                            clean_lines.GetElidedAt(linenum - i - 1),
                            m_re_result_temp))
                return;
        }
    }

    static const regex_code RE_PATTERN_FUNC_BODY =
        RegexCompile("{[^}]*}");
    RegexReplace(RE_PATTERN_FUNC_BODY, " ", &line, m_re_result_temp);  // exclude function body
    while (true) {
        bool matched = RegexSearch(RE_REF_PARAM, line, m_re_result);
        if (!matched)
            break;
        std::string parameter = GetMatchStr(m_re_result, line, 1);
        if (!RegexMatch(RE_CONST_REF_PARAM, parameter, m_re_result_temp) &&
            !RegexMatch(RE_REF_STREAM_PARAM, parameter, m_re_result_temp)) {
            Error(linenum, "runtime/references", 2,
                  "Is this a non-const reference? "
                  "If so, make const or use a pointer: " +
                  RegexReplace(" *<", "<", parameter, m_re_result_temp));
        }
        line = line.substr(GetMatchEnd(m_re_result, 0));
    }
}

void FileLinter::CheckForNonStandardConstructs(const CleansedLines& clean_lines,
                                               const std::string& elided_line, size_t linenum,
                                               NestingState* nesting_state) {
    // Remove comments from the line, but leave in strings for now.
    const std::string& line = clean_lines.GetLineAt(linenum);

    // We don't need to search two regex patterns
    // if the line does not have printf.
    if (StrContain(line, "printf")) {
        static const regex_code RE_PATTERN_PRINTF_Q =
            RegexCompile(R"(printf\s*\(.*".*%[-+ ]?\d*q)");
        if (RegexSearch(RE_PATTERN_PRINTF_Q, line, m_re_result_temp)) {
            Error(linenum, "runtime/printf_format", 3,
                "%q in format strings is deprecated.  Use %ll instead.");
        }

        static const regex_code RE_PATTERN_PRINTF_N =
            RegexCompile(R"(printf\s*\(.*".*%\d+\$)");
        if (RegexSearch(RE_PATTERN_PRINTF_N, line, m_re_result_temp)) {
            Error(linenum, "runtime/printf_format", 2,
                "%N$ formats are unconventional.  Try rewriting to avoid them.");
        }
    }

    // Remove escaped backslashes before looking for undefined escapes.
    std::string line_removed_escape = StrReplaceAll(line, "\\\\", "");

    static const regex_code RE_PATTERN_PRINTF_UNESCAPE =
        RegexCompile(R"(("|\').*\\(%|\[|\(|{))");
    if (RegexSearch(RE_PATTERN_PRINTF_UNESCAPE, line_removed_escape, m_re_result_temp)) {
        Error(linenum, "build/printf_format", 3,
              "%, [, (, and { are undefined character escapes.  Unescape them.");
    }

    // For the rest, work with both comments and strings removed.
    const std::string& elided = elided_line;

    static const regex_code RE_PATTERN_STORAGE_CLASS =
        RegexCompile(R"(\b(const|volatile|void|char|short|int|long)"
                     "|float|double|signed|unsigned"
                     "|schar|u?int8|u?int16|u?int32|u?int64)"
                     R"(\s+(register|static|extern|typedef)\b)");
    if (RegexSearch(RE_PATTERN_STORAGE_CLASS, elided, m_re_result_temp)) {
        Error(linenum, "build/storage_class", 5,
              "Storage-class specifier (static, extern, typedef, etc) should be "
              "at the beginning of the declaration.");
    }

    static const regex_code RE_PATTERN_ENDIF_COMMENT =
        RegexCompile(R"(\s*#\s*endif\s*[^/\s]+)");
    if (RegexMatch(RE_PATTERN_ENDIF_COMMENT, elided, m_re_result_temp)) {
        Error(linenum, "build/endif_comment", 5,
              "Uncommented text after #endif is non-standard.  Use a comment.");
    }

    static const regex_code RE_PATTERN_FORWARD_DECL =
        RegexCompile(R"(\s*class\s+(\w+\s*::\s*)+\w+\s*;)");
    if (RegexMatch(RE_PATTERN_FORWARD_DECL, elided, m_re_result_temp)) {
        Error(linenum, "build/forward_decl", 5,
              "Inner-style forward declarations are invalid.  Remove this line.");
    }

    static const regex_code RE_PATTERN_DEPRECATED =
        RegexCompile(R"((\w+|[+-]?\d+(\.\d*)?)\s*(<|>)\?=?\s*(\w+|[+-]?\d+)(\.\d*)?)");
    if (RegexSearch(RE_PATTERN_DEPRECATED, elided, m_re_result_temp)) {
        Error(linenum, "build/deprecated", 3,
              ">? and <? (max and min) operators are non-standard and deprecated.");
    }

    static const regex_code RE_PATTERN_CONST_STR_MEMBER =
        RegexCompile(R"(^\s*const\s*string\s*&\s*\w+\s*;)");
    if (RegexSearch(RE_PATTERN_CONST_STR_MEMBER, elided, m_re_result_temp)) {
        // TODO(unknown): Could it be expanded safely to arbitrary references,
        // without triggering too many false positives? The first
        // attempt triggered 5 warnings for mostly benign code in the regtest, hence
        // the restriction.
        // Here's the original regexp, for the reference:
        // type_name = r'\w+((\s*::\s*\w+)|(\s*<\s*\w+?\s*>))?'
        // r'\s*const\s*' + type_name + '\s*&\s*\w+\s*;'
        Error(linenum, "runtime/member_string_references", 2,
              "const string& members are dangerous. It is much better to use "
              "alternatives, such as pointers or simple constants.");
    }

    // Everything else in this function operates on class declarations.
    // Return early if the top of the nesting stack is not a class, or if
    // the class head is not completed yet.
    ClassInfo* classinfo = nesting_state->InnermostClass();
    if (classinfo == nullptr || !classinfo->SeenOpenBrace())
        return;

    // The class may have been declared with namespace or classname qualifiers.
    // The constructor and destructor will not have those qualifiers.
    std::string base_classname = classinfo->Basename();

    if (!StrContain(elided, base_classname))
        return;

    // Look for single-argument constructors that aren't marked explicit.
    // Technically a valid construct, but against style.
    bool explicit_constructor_match = RegexMatch(
        R"(\s+(?:(?:inline|constexpr)\s+)*(explicit\s+)?)"
        R"((?:(?:inline|constexpr)\s+)*)" + base_classname + R"(\s*)"
        R"(\(((?:[^()]|\([^()]*\))*)\))", elided, m_re_result);

    if (!explicit_constructor_match)
        return;
    bool is_marked_explicit = IsMatched(m_re_result, 1);

    std::vector<std::string> constructor_args;
    if (GetMatchSize(m_re_result, 2) == 0)
        constructor_args = {};
    else
        constructor_args = StrSplitBy(GetMatchStr(m_re_result, elided, 2), ",");

    // collapse arguments so that commas in template parameter lists and function
    // argument parameter lists don't split arguments in two
    size_t i = 0;
    while (i < constructor_args.size()) {
        std::string constructor_arg = constructor_args[i];
        while (StrCount(constructor_arg, '<') > StrCount(constructor_arg, '>') ||
               StrCount(constructor_arg, '(') > StrCount(constructor_arg, ')')) {
            constructor_arg += "," + constructor_args[i + 1];
            constructor_arg.erase(constructor_arg.begin() + i + 1);
        }
        constructor_args[i] = constructor_arg;
        i++;
    }

    size_t variadic_args_count = 0;
    size_t defaulted_args_count = 0;
    for (const std::string& arg : constructor_args) {
        if (StrContain(arg, "&&..."))
            variadic_args_count++;
        if (StrContain(arg, '='))
            defaulted_args_count++;
    }
    bool noarg_constructor = (constructor_args.empty() ||  // empty arg list
                                // 'void' arg specifier
                                (constructor_args.size() == 1 &&
                                StrStrip(constructor_args[0]) == "void"));
    bool onearg_constructor = ((constructor_args.size() == 1 &&  // exactly one arg
                                    !noarg_constructor) ||
                                // all but at most one arg defaulted
                                (constructor_args.size() >= 1 &&
                                    !noarg_constructor &&
                                    defaulted_args_count >= constructor_args.size() - 1) ||
                                // variadic arguments with zero or one argument
                                (constructor_args.size() <= 2 &&
                                    variadic_args_count >= 1));

    static const regex_code RE_PATTERN_INITIALIZER_LIST =
        RegexCompile(R"(\bstd\s*::\s*initializer_list\b)");
    bool initializer_list_constructor =
            onearg_constructor &&
            RegexSearch(RE_PATTERN_INITIALIZER_LIST, constructor_args[0], m_re_result_temp);
    bool copy_constructor =
            onearg_constructor &&
            RegexMatch(R"(((const\s+(volatile\s+)?)?|(volatile\s+(const\s+)?))?)" +
                       base_classname + R"((\s*<[^>]*>)?(\s+const)?\s*(?:<\w+>\s*)?&)",
                       StrStrip(constructor_args[0]), m_re_result_temp);

    if (!is_marked_explicit &&
            onearg_constructor &&
            !initializer_list_constructor &&
            !copy_constructor) {
        if (defaulted_args_count > 0 || variadic_args_count > 0) {
            Error(linenum, "runtime/explicit", 4,
                  "Constructors callable with one argument "
                  "should be marked explicit.");
        } else {
            Error(linenum, "runtime/explicit", 4,
                  "Single-parameter constructors should be marked explicit.");
        }
    }
}

void FileLinter::CheckVlogArguments(const std::string& elided_line, size_t linenum) {
    static const regex_code RE_PATTERN_VLOG_ARG =
        RegexCompile(R"(\bVLOG\((INFO|ERROR|WARNING|DFATAL|FATAL)\))");
    if (RegexSearch(RE_PATTERN_VLOG_ARG, elided_line, m_re_result_temp)) {
        Error(linenum, "runtime/vlog", 5,
              "VLOG() should be used with numeric verbosity level.  "
              "Use LOG() if you want symbolic severity levels.");
    }
}

void FileLinter::CheckPosixThreading(const std::string& elided_line, size_t linenum) {
    // Additional pattern matching check to confirm that this is the
    // function we are looking for

    // (non-threadsafe name, thread-safe alternative, validation pattern)
    //
    // The validation pattern is used to eliminate false positives such as:
    //  _rand();               // false positive due to substring match.
    //  ->rand();              // some member function rand().
    //  ACMRandom rand(seed);  // some variable named rand.
    //  ISAACRandom rand();    // another variable named rand.
    //
    // Basically we require the return value of these functions to be used
    // in some expression context on the same line by matching on some
    // operator before the function name.  This eliminates constructors and
    // member function calls.

    static const regex_code RE_PATTERN_UNSAFE_FUNC =
        RegexCompile(R"((?:[-+*/=%^&|(<]\s*|>\s+))"
                    "(asctime|ctime|getgrgid|getgrnam|getlogin|getpwnam|"
                    "getpwuid|gmtime|localtime|rand|strtok|ttyname)"
                    R"(\([^)]*\))");

    bool match = RegexSearch(RE_PATTERN_UNSAFE_FUNC, elided_line, m_re_result);
    if (match) {
        std::string funcname = GetMatchStr(m_re_result, elided_line, 1);
        Error(linenum, "runtime/threadsafe_fn", 2,
                std::string("Consider using ") + funcname +
                "_r(...) instead of " + funcname +
                "(...) for improved thread safety.");
    }
}

void FileLinter::CheckInvalidIncrement(const std::string& elided_line, size_t linenum) {
    // Matches invalid increment: *count++, which moves pointer instead of
    // incrementing a value.
    static const regex_code RE_INVALID_INCREMENT = RegexCompile(R"(^\s*\*\w+(\+\+|--);)");
    if (RegexMatch(RE_INVALID_INCREMENT, elided_line, m_re_result_temp)) {
        Error(linenum, "runtime/invalid_increment", 5,
              "Changing pointer instead of value (or unused value of operator*).");
    }
}

void FileLinter::CheckMakePairUsesDeduction(const std::string& elided_line, size_t linenum) {
    static const regex_code RE_EXPLICIT_MAKEPAIR = RegexCompile(R"(\bmake_pair\s*<)");
    if (RegexSearch(RE_EXPLICIT_MAKEPAIR, elided_line, m_re_result_temp)) {
        Error(linenum, "build/explicit_make_pair",
              4,  // 4 = high confidence
              "For C++11-compatibility, omit template arguments from make_pair"
              " OR use pair directly OR if appropriate, construct a pair directly");
    }
}

void FileLinter::CheckRedundantVirtual(const CleansedLines& clean_lines,
                                       const std::string& elided_line, size_t linenum) {
    // Look for "virtual" on current line.
    static const regex_code RE_PATTERN_VIRTUAL =
        RegexCompile(R"(^(.*)(\bvirtual\b)(.*)$)");
    bool match = RegexMatch(RE_PATTERN_VIRTUAL, elided_line, m_re_result);
    if (!match) return;

    // Ignore "virtual" keywords that are near access-specifiers.  These
    // are only used in class base-specifier and do not apply to member
    // functions.
    if (RegexSearch(R"(\b(public|protected|private)\s+$)",
                    GetMatchStr(m_re_result, elided_line, 1), m_re_result_temp) ||
        RegexMatch(R"(^\s+(public|protected|private)\b)",
                   GetMatchStr(m_re_result, elided_line, 3), m_re_result_temp))
        return;

    // Ignore the "virtual" keyword from virtual base classes.  Usually
    // there is a column on the same line in these cases (virtual base
    // classes are rare in google3 because multiple inheritance is rare).
    if (RegexMatch(R"(^.*[^:]:[^:].*$)", elided_line, m_re_result_temp))
        return;

    // Look for the next opening parenthesis.  This is the start of the
    // parameter list (possibly on the next line shortly after virtual).
    // TODO(unknown): doesn't work if there are virtual functions with
    // decltype() or other things that use parentheses, but csearch suggests
    // that this is rare.
    size_t end_col = INDEX_NONE;
    size_t end_line = INDEX_NONE;
    size_t start_col = GetMatchSize(m_re_result, 2);
    for (size_t start_line = linenum;
            start_line < MIN(linenum + 3, clean_lines.NumLines()); start_line++) {
        std::string line = clean_lines.GetElidedAt(start_line).substr(start_col);
        bool parameter_list = RegexMatch(R"(^([^(]*)\()", line, m_re_result);
        if (parameter_list) {
            // Match parentheses to find the end of the parameter list
            end_line = start_line;
            end_col = start_col + GetMatchSize(m_re_result, 1);
            CloseExpression(clean_lines, &end_line, &end_col);
            break;
        }
        start_col = 0;
    }

    if (end_col == INDEX_NONE)
        return;  // Couldn't find end of parameter list, give up

    // Look for "override" or "final" after the parameter list
    // (possibly on the next few lines).
    for (size_t i = end_line;
            i < MIN(end_line + 3, clean_lines.NumLines()); i++) {
        std::string line = clean_lines.GetElidedAt(i).substr(end_col);
        match = RegexSearch(R"(\b(override|final)\b)", line, m_re_result);
        if (match) {
            Error(linenum, "readability/inheritance", 4,
                  "\"virtual\" is redundant since function is "
                  "already declared as \"" + GetMatchStr(m_re_result, line, 1) + "\"");
        }

        // Set end_col to check whole lines after we are done with the
        // first line.
        end_col = 0;
        if (RegexSearch(R"([^\w]\s*$)", line, m_re_result_temp))
            break;
    }
}

void FileLinter::CheckRedundantOverrideOrFinal(const CleansedLines& clean_lines,
                                               const std::string& elided_line, size_t linenum) {
    // Look for closing parenthesis nearby.  We need one to confirm where
    // the declarator ends and where the virt-specifier starts to avoid
    // false positives.
    const std::string& line = elided_line;
    size_t declarator_end = line.rfind(')');
    std::string fragment;
    if (declarator_end != std::string::npos) {
        fragment = line.substr(declarator_end);
    } else {
        if (linenum > 1 &&
            clean_lines.GetElidedAt(linenum - 1).rfind(')') != std::string::npos)
            fragment = line;
        else
            return;
    }

    // Check that at most one of "override" or "final" is present, not both
    if (RegexSearch(RE_PATTERN_OVERRIDE, fragment, m_re_result_temp) &&
        RegexSearch(R"(\bfinal\b)", fragment, m_re_result_temp)) {
        Error(linenum, "readability/inheritance", 4,
                              "\"override\" is redundant since function is "
                              "already declared as \"final\"");
    }
}

void FileLinter::CheckCxxHeaders(const std::string& elided_line, size_t linenum) {
    static const regex_code RE_PATTERN_CXX_HEADER =
        RegexCompile(R"(\s*#\s*include\s+[<"]([^<"]+)[">])");
    bool include = RegexMatch(RE_PATTERN_CXX_HEADER, elided_line, m_re_result);
    if (!include)
        return;

    std::string str1 = GetMatchStr(m_re_result, elided_line, 1);

    // Flag unapproved C++11 headers.
    if (InStrVec({ "cfenv", "fenv.h", "ratio" }, str1)) {
        Error(linenum, "build/c++11", 5,
              "<" + str1 + "> is an unapproved C++11 header.");
    }

    // filesystem is the only unapproved C++17 header
    if (str1 == "filesystem") {
        Error(linenum, "build/c++17", 5,
              "<filesystem> is an unapproved C++17 header.");
    }
}

void FileLinter::ProcessLine(bool is_header_extension,
                             const CleansedLines& clean_lines, size_t linenum,
                             IncludeState* include_state,
                             FunctionState* function_state,
                             NestingState* nesting_state) {
    const std::string& elided_line = clean_lines.GetElidedAt(linenum);
    nesting_state->Update(clean_lines, elided_line, linenum, this);
    CheckForNamespaceIndentation(clean_lines,
                                 elided_line, linenum, nesting_state);
    if (nesting_state->InAsmBlock()) return;
    CheckForFunctionLengths(clean_lines, linenum, function_state);
    CheckForMultilineCommentsAndStrings(elided_line, linenum);
    CheckStyle(clean_lines,
               elided_line, linenum, is_header_extension);
    CheckStyleWithState(clean_lines,
                        elided_line, linenum, nesting_state);
    CheckLanguage(clean_lines,
                  elided_line, linenum, is_header_extension,
                  include_state);
    CheckForNonConstReference(clean_lines,
                              elided_line, linenum, nesting_state);
    CheckForNonStandardConstructs(clean_lines,
                                  elided_line, linenum, nesting_state);
    CheckVlogArguments(elided_line, linenum);
    CheckPosixThreading(elided_line, linenum);
    CheckInvalidIncrement(elided_line, linenum);
    CheckMakePairUsesDeduction(elided_line, linenum);
    CheckRedundantVirtual(clean_lines, elided_line, linenum);
    CheckRedundantOverrideOrFinal(clean_lines, elided_line, linenum);
    CheckCxxHeaders(elided_line, linenum);
}

typedef std::vector<std::pair<std::string, regex_code>> header_patterns_t;

// Other scripts may reach in and modify this pattern.
static const header_patterns_t CompileContainingTemplatesPatterns() {
    const std::vector<std::pair<std::string, std::set<std::string>>>
    HEADERS_CONTAINING_TEMPLATES = {
        { "deque", { "deque", } },
        { "functional", { "unary_function", "binary_function",
                            "plus", "minus", "multiplies", "divides", "modulus",
                            "negate",
                            "equal_to", "not_equal_to", "greater", "less",
                            "greater_equal", "less_equal",
                            "logical_and", "logical_or", "logical_not",
                            "unary_negate", "not1", "binary_negate", "not2",
                            "bind1st", "bind2nd",
                            "pointer_to_unary_function",
                            "pointer_to_binary_function",
                            "ptr_fun",
                            "mem_fun_t", "mem_fun", "mem_fun1_t", "mem_fun1_ref_t",
                            "mem_fun_ref_t",
                            "const_mem_fun_t", "const_mem_fun1_t",
                            "const_mem_fun_ref_t", "const_mem_fun1_ref_t",
                            "mem_fun_ref", } },
        { "limits", { "numeric_limits", } },
        { "list", { "list", } },
        { "map", { "multimap", } },
        { "memory", { "allocator", "make_shared", "make_unique", "shared_ptr",
                        "unique_ptr", "weak_ptr" } },
        { "queue", { "queue", "priority_queue", } },
        { "set", { "set", "multiset", } },
        { "stack", { "stack", } },
        { "string", { "char_traits", "basic_string", } },
        { "tuple", { "tuple", } },
        { "unordered_map", { "unordered_map", "unordered_multimap" } },
        { "unordered_set", { "unordered_set", "unordered_multiset" } },
        { "utility", { "pair", } },
        { "vector", { "vector", } },

        // gcc extensions.
        // Note: std::hash is their hash, ::hash is our hash
        { "hash_map", { "hash_map", "hash_multimap", } },
        { "hash_set", { "hash_set", "hash_multiset", } },
        { "slist", { "slist", } },
    };
    header_patterns_t patterns;
    patterns.reserve(HEADERS_CONTAINING_TEMPLATES.size());
    for (const std::pair<std::string, std::set<std::string>>& p : HEADERS_CONTAINING_TEMPLATES) {
        std::string regex = SetToStr(p.second, R"(((^|(^|\s|((^|\W)::))std::)|[^>.:]\b)()", "|", R"()\s*\<)");
        patterns.emplace_back(p.first, std::move(RegexCompile(regex)));
    }
    return patterns;
}

static const header_patterns_t RE_PATTERNS_CONTAINING_TEMPLATES =
    CompileContainingTemplatesPatterns();

static const header_patterns_t CompileMaybeTemplatesPatterns() {
    const std::vector<std::pair<std::string, std::set<std::string>>>
    HEADERS_MAYBE_TEMPLATES = {
        { "algorithm", { "copy", "max", "min", "min_element", "sort", "transform" } },
        { "utility", { "forward", "make_pair", "move", "swap" } },
    };
    header_patterns_t patterns;
    patterns.reserve(HEADERS_MAYBE_TEMPLATES.size());
    for (const std::pair<std::string, std::set<std::string>>& p : HEADERS_MAYBE_TEMPLATES) {
        // Match max<type>(..., ...), max(..., ...), but not foo->max, foo.max or
        // 'type::max()'.
        std::string regex = SetToStr(p.second, R"(((\bstd::)|[^>.:])\b()", "|", R"()(<.*?>)?\([^\)])");
        patterns.emplace_back(p.first, std::move(RegexCompile(regex)));
    }
    return patterns;
}

static const header_patterns_t RE_PATTERNS_MAYBE_TEMPLATES =
    CompileMaybeTemplatesPatterns();

// Non templated types or global objects
static const header_patterns_t CompileTypesOrObjsPatterns() {
    const std::vector<std::pair<std::string, std::set<std::string>>>
    HEADERS_TYPES_OR_OBJS = {
        // String and others are special -- it is a non-templatized type in STL.
        { "string",  {"string"} },
        { "iostream", { "cin", "cout", "cerr", "clog", "wcin", "wcout",
                        "wcerr", "wclog" } },
        { "cstdio", { "FILE", "fpos_t" } },
    };
    header_patterns_t patterns;
    patterns.reserve(HEADERS_TYPES_OR_OBJS.size());
    for (const std::pair<std::string, std::set<std::string>>& p : HEADERS_TYPES_OR_OBJS) {
        std::string regex = SetToStr(p.second, "\\b(", "|", ")\\b");
        patterns.emplace_back(p.first, std::move(RegexCompile(regex)));
    }
    return patterns;
}

static const header_patterns_t RE_PATTERNS_TYPES_OR_OBJS =
    CompileTypesOrObjsPatterns();

// Non templated functions
static const regex_code CompileCstdioPatterns() {
    const std::set<std::string> HEADERS_CSTDIO_FUNCTIONS = {
        "fopen", "freopen",
        "fclose", "fflush", "setbuf", "setvbuf", "fread",
        "fwrite", "fgetc", "getc", "fgets", "fputc", "putc",
        "fputs", "getchar", "gets", "putchar", "puts", "ungetc",
        "scanf", "fscanf", "sscanf", "vscanf", "vfscanf",
        "vsscanf", "printf", "fprintf", "sprintf", "snprintf",
        "vprintf", "vfprintf", "vsprintf", "vsnprintf",
        "ftell", "fgetpos", "fseek", "fsetpos",
        "clearerr", "feof", "ferror", "perror",
        "tmpfile", "tmpnam"
    };

    std::string regex = SetToStr(HEADERS_CSTDIO_FUNCTIONS,
                                 R"(([^>.]|^)\b()", "|", R"()\([^\)])");

    return RegexCompile(regex);
}

static const regex_code RE_PATTERN_CSTDIO_FUNCTIONS =
    CompileCstdioPatterns();

void FileLinter::CheckForIncludeWhatYouUse(const CleansedLines& clean_lines,
                                           IncludeState* include_state) {
    // A map of header name to linenumber and the template entity.
    // Example of required: { '<functional>': (1219, 'less<>') }
    std::map<std::string, std::pair<size_t, std::string>> required = {};

    for (size_t linenum = 0; linenum < clean_lines.NumLines(); linenum++) {
        const std::string& line = clean_lines.GetElidedAt(linenum);
        if (line.empty() || line[0] == '#')
            continue;

        // Non templated types or global objects
        for (const std::pair<std::string, regex_code>& p : RE_PATTERNS_TYPES_OR_OBJS) {
            bool matched = RegexSearch(p.second, line, m_re_result);
            if (matched) {
                // Don't warn about strings in non-STL namespaces:
                // (We check only the first match per line; good enough.)
                std::string prefix = line.substr(0, GetMatchStart(m_re_result, 0));
                if (prefix.ends_with("std::") || !prefix.ends_with("::")) {
                    std::string func = GetMatchStr(m_re_result, line, 1);
                    required[p.first] = { linenum, func };
                }
            }
        }

        // Non templated functions
        {
            bool matched = RegexSearch(RE_PATTERN_CSTDIO_FUNCTIONS, line, m_re_result);
            if (matched) {
                // Don't warn about strings in non-STL namespaces:
                // (We check only the first match per line; good enough.)
                std::string prefix = line.substr(0, GetMatchStart(m_re_result, 0));
                if (prefix.ends_with("std::") || !prefix.ends_with("::")) {
                    std::string func = GetMatchStr(m_re_result, line, 2);
                    required["cstdio"] = { linenum, func };
                }
            }
        }

        for (const std::pair<std::string, regex_code>& p : RE_PATTERNS_MAYBE_TEMPLATES) {
            bool matched = RegexSearch(p.second, line, m_re_result);
            if (matched) {
                std::string func = GetMatchStr(m_re_result, line, 3);
                required[p.first] = { linenum, func };
            }
        }

        // The following function is just a speed up, no semantics are changed.
        if (!StrContain(line, '<'))  // Reduces the cpu time usage by skipping lines.
            continue;

        // Map is often overloaded. Only check, if it is fully qualified.
        // Match 'std::map<type>(...)', but not 'map<type>(...)''
        static const regex_code RE_PATTERNS_MAP_TEMPLATES =
            RegexCompile(R"((std\b::\bmap\s*\<)|(^(std\b::\b)map\b\(\s*\<))");
        if (RegexSearch(RE_PATTERNS_MAP_TEMPLATES, line, m_re_result_temp))
            required["map"] = { linenum, "map<>" };

        // Other scripts may reach in and modify this pattern.
        for (const std::pair<std::string, regex_code>& p : RE_PATTERNS_CONTAINING_TEMPLATES) {
            bool matched = RegexSearch(p.second, line, m_re_result);
            if (matched) {
                // Don't warn about IWYU in non-STL namespaces:
                // (We check only the first match per line; good enough.)
                std::string prefix = line.substr(0, GetMatchStart(m_re_result, 0));
                if (prefix.ends_with("std::") || !prefix.ends_with("::")) {
                    std::string func = GetMatchStr(m_re_result, line, 6) + "<>";
                    required[p.first] = { linenum, func };
                }
            }
        }
    }

    // All the lines have been processed, report the errors found.
    for (const auto& required_header_unstripped : required) {
        size_t linenum = required_header_unstripped.second.first;
        const std::string& func = required_header_unstripped.second.second;
        const std::string& header = required_header_unstripped.first;
        if (include_state->FindHeader(header) == INDEX_NONE) {
            Error(linenum,
                  "build/include_what_you_use", 4,
                  "Add #include <" + header + "> for " + func);
        }
    }
}

void FileLinter::CheckHeaderFileIncluded(IncludeState* include_state) {
    // Do not check test files
    std::string path_from_repo = m_file_from_repo.string();
    fs::path filedir = m_file.parent_path();
    std::string basename = m_file.filename().string();
    static const regex_code RE_PATTERN_TEST_SUFFIX =
        RegexCompile("(_test|_regtest|_unittest)$");
    if (RegexSearch(RE_PATTERN_TEST_SUFFIX, basename, m_re_result_temp))
        return;

    std::string message = "";
    size_t first_include = INDEX_NONE;
    std::string basefilename =
        basename.substr(0, basename.size() - m_file_extension.size());
    for (const std::string& ext : m_header_extensions) {
        std::string headerfile = basefilename + ext;
        fs::path headerpath = filedir / headerfile;
        if (!fs::is_regular_file(headerpath))
            continue;
        std::string headername =
            GetRelativeFromRepository(headerpath, m_options.Repository()).string();
        // Include path should not be Windows style.
        headername = StrReplaceAll(headername, "\\", "/");
        bool include_uses_unix_dir_aliases = false;
        for (const auto& section_list : include_state->IncludeList()) {
            for (const std::pair<std::string, size_t>&f : section_list) {
                const std::string& include_text = f.first;
                if (StrContain(include_text, "./"))
                    include_uses_unix_dir_aliases = true;
                if (StrContain(headername, include_text) ||
                    StrContain(include_text, headername))
                    return;
                if (first_include == INDEX_NONE)
                    first_include = f.second;
            }
        }

        message = path_from_repo + " should include its header file " + headername;
        if (include_uses_unix_dir_aliases)
            message += ". Relative paths like . and .. are not allowed.";
    }
    if (first_include == INDEX_NONE)
        first_include = 0;

    if (!message.empty())
        Error(first_include, "build/include", 5, message);
}

void FileLinter::CacheVariables() {
    m_file_from_repo = GetRelativeFromRepository(m_file, m_options.Repository());
    std::string ext = m_file.extension().string();
    if (ext.empty())
        m_file_extension = "";
    else
        m_file_extension = &(m_file.extension().string())[1];
    m_all_extensions = m_options.GetAllExtensions();
    m_header_extensions = m_options.GetHeaderExtensions();
    m_non_header_extensions = m_options.GetNonHeaderExtensions();
}

void FileLinter::CacheVariables(const fs::path& file) {
    m_file = file;
    m_filename = file.string();
    CacheVariables();
}

void FileLinter::ProcessFileData(std::vector<std::string>& lines) {
    IncludeState include_state = IncludeState();
    FunctionState function_state = FunctionState();
    NestingState nesting_state = NestingState();

    CheckForCopyright(lines);
    RemoveMultiLineComments(lines);
    CleansedLines clean_lines = CleansedLines(lines, m_options);

    {
        // Set error suppressions
        m_error_suppressions.Clear();
        size_t linenum = 0;
        for (const std::string& line : lines) {
            if (clean_lines.HasComment(linenum))
                ParseNolintSuppressions(line, linenum);
            linenum++;
        }
        if (m_error_suppressions.HasOpenBlock()) {
            Error(m_error_suppressions.GetOpenBlockStart(), "readability/nolint", 5,
                  "NONLINT block never ended");
        }
        ProcessGlobalSuppressions(lines);
    }

    bool is_header_extension = InStrSet(m_header_extensions, m_file_extension);

    if (is_header_extension) {
        m_cppvar = GetHeaderGuardCPPVariable();
        CheckForHeaderGuard(clean_lines);
    }

    for (size_t linenum = 0; linenum < clean_lines.NumLines(); linenum++) {
        ProcessLine(is_header_extension, clean_lines, linenum,
                    &include_state, &function_state, &nesting_state);
    }

    CheckForIncludeWhatYouUse(clean_lines, &include_state);

    // Check that the .cc file has included its header if it exists.
    if (m_options.IsSourceExtension(m_file_extension))
        CheckHeaderFileIncluded(&include_state);

    CheckForNewlineAtEOF(lines);
}

void FileLinter::ProcessFile() {
    if (!m_options.ProcessConfigOverrides(m_file, m_cpplint_state)) {
        return;
    }

    size_t lf_lines_count = 0;
    std::vector<size_t> crlf_lines = {};
    std::vector<size_t> bad_lines = {};
    std::vector<size_t> null_lines = {};
    std::vector<std::string> lines = {};

    if (StrIsChar(m_filename, '-')) {
        // TODO(matyalatte): support stdin code
        /*
        lines = codecs.StreamReaderWriter(sys.stdin,
                                        codecs.getreader('utf8'),
                                        codecs.getwriter('utf8'),
                                        'replace').read().split('\n');
        */
        m_cpplint_state->PrintError("Skipping input \"-\": stdin is unsupported yet.\n");
        return;
    } else {
        std::ifstream file(m_file, std::ios::binary);
        if (!file) {
            m_cpplint_state->PrintError(
                "Skipping input '" + m_filename + "': Can't open for reading\n");
        }
        std::string line;
        size_t linenum = 0;
        int status = LINE_OK;
        std::stringstream ss;
        // Note: We can't use getline cause it trims NUL bytes and a linefeed at EOF.
        while ((status & LINE_EOF) == 0) {
            line = GetLine(file, ss, &status);
            if (!line.empty() && line.back() == '\r') {
                // line ends with \r.
                crlf_lines.push_back(linenum + 1);
                line.pop_back();
            } else {
                lf_lines_count++;
            }
            if (status & LINE_BAD_RUNE) {
                // line contains bad runes.
                bad_lines.push_back(linenum + 1);
            }
            if (status & LINE_NULL) {
                // line contains null bytes.
                null_lines.push_back(linenum + 1);
            }
            lines.push_back(line);
            linenum++;
        }
    }

    CacheVariables();

    if (!StrIsChar(m_filename, '-') &&
        !m_all_extensions.contains(m_file_extension)) {
        m_cpplint_state->PrintError(
            "Ignoring " + m_filename + "; not a valid file name" +
            " (" + SetToStr(m_all_extensions) + ")\n");
    } else {
        // insert a comment line at the beginning of file.
        lines.insert(lines.begin(), "// marker so line numbers and indices both start at 1");
        // add a comment line to the end of file.
        lines.emplace_back("// marker so line numbers end in a known way");

        // Check lines
        ProcessFileData(lines);

        // If end-of-line sequences are a mix of LF and CR-LF, issue
        // warnings on the lines with CR.
        //
        // Don't issue any warnings if all lines are uniformly LF or CR-LF,
        // since critique can handle these just fine, and the style guide
        // doesn't dictate a particular end of line sequence.
        //
        // We can't depend on os.linesep to determine what the desired
        // end-of-line sequence should be, since that will return the
        // server-side end-of-line sequence.
        if (lf_lines_count > 0 && crlf_lines.size() > 0) {
            // Warn on every line with CR.  An alternative approach might be to
            // check whether the file is mostly CRLF or just LF, and warn on the
            // minority, we bias toward LF here since most tools prefer LF.
            for (size_t linenum : crlf_lines) {
                Error(linenum, "whitespace/newline", 1,
                      "Unexpected \\r (^M) found; better to use only \\n");
            }
        }
        for (size_t linenum : bad_lines) {
            Error(linenum, "readability/utf8", 5,
                  "Line contains invalid UTF-8 (or Unicode replacement character).");
        }
        for (size_t linenum : null_lines) {
            Error(linenum, "readability/nul", 5,
                  "Line contains NUL byte.");
        }
    }

    // Suppress printing anything if --quiet was passed unless the error
    // count has increased after processing this file.
    if (!m_cpplint_state->Quiet() || m_has_error)
        m_cpplint_state->PrintInfo("Done processing " + m_filename + "\n");
}
