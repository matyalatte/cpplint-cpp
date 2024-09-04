#include "states.h"
#include <cassert>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include "cleanse.h"
#include "file_linter.h"
#include "line_utils.h"
#include "nest_info.h"
#include "regex_utils.h"
#include "string_utils.h"

const char* const TYPE_NAMES[] = {
    "",
    "C system header",  // C_SYS_HEADER
    "C++ system header",  // CPP_SYS_HEADER
    "other system header",  // OTHER_SYS_HEADER
    "header this file implements",  // LIKELY_MY_HEADER
    "header this file may implement",  // POSSIBLE_MY_HEADER
    "other header",  // OTHER_HEADER
};

// m_section will move monotonically through this set. If it ever
// needs to move backwards, CheckNextIncludeOrder will raise an error.
enum SectionType : int {
    INITIAL_SECTION = 0,
    MY_H_SECTION = 1,
    C_SECTION = 2,
    CPP_SECTION = 3,
    OTHER_SYS_SECTION = 4,
    OTHER_H_SECTION = 5,
};

const char* const SECTION_NAMES[] = {
    "... nothing. (This can't be an error.)",  // INITIAL_SECTION
    "a header this file implements",  // MY_H_SECTION
    "C system header",  // C_SECTION
    "C++ system header",  // CPP_SECTION
    "other system header",  // OTHER_SYS_SECTION
    "other header",  // OTHER_H_SECTION
};

size_t IncludeState::FindHeader(const std::string& header) {
    for (const std::vector<std::pair<std::string, size_t>>& section_list : m_include_list) {
        for (const std::pair<std::string, size_t>&f : section_list) {
            if (f.first == header)
                return f.second;
        }
    }
    return INDEX_NONE;
}

void IncludeState::ResetSection(const std::string& directive) {
    // The name of the current section.
    m_section = INITIAL_SECTION;
    // The path of last found header.
    m_last_header.clear();

    // Update list of includes.  Note that we never pop from the
    // include list.
    if (InStrVec({ "if", "ifdef", "ifndef" }, directive))
        m_include_list.emplace_back();
    else if (directive == "else" || directive == "elif")
        m_include_list.back() = {};
}

std::string IncludeState::CanonicalizeAlphabeticalOrder(const std::string& header_path) {
    std::string ret = StrReplaceAll(header_path, "-inl.h", ".h");
    ret = StrReplaceAll(ret, "-", "_");
    return StrToLower(ret);
}

bool IncludeState::IsInAlphabeticalOrder(const CleansedLines& clean_lines,
                           size_t linenum,
                           const std::string& header_path) {
    // If previous section is different from current section, m_last_header will
    // be reset to empty string, so it's always less than current header.
    //
    // If previous line was a blank line, assume that the headers are
    // intentionally sorted the way they are.
    static const regex_code RE_PATTERN_INCLUDE_ORDER =
        RegexCompile(R"(^\s*#\s*include\b)");
    if ((m_last_header.compare(header_path) > 0) &&
        RegexMatch(RE_PATTERN_INCLUDE_ORDER,
                   clean_lines.GetElidedAt(linenum - 1)))
        return false;
    return true;
}

std::string IncludeState::CheckNextIncludeOrder(int header_type) {
    std::string error_message = std::string("Found ") + TYPE_NAMES[header_type] +
                                " after " + SECTION_NAMES[m_section];

    int last_section = m_section;

    if (header_type == C_SYS_HEADER) {
        if (m_section <= C_SECTION) {
            m_section = C_SECTION;
        } else {
            m_last_header.clear();
            return error_message;
        }
    } else if (header_type == CPP_SYS_HEADER) {
        if (m_section <= CPP_SECTION) {
            m_section = CPP_SECTION;
        } else {
            m_last_header.clear();
            return error_message;
        }
    } else if (header_type == OTHER_SYS_HEADER) {
        if (m_section <= OTHER_SYS_SECTION) {
            m_section = OTHER_SYS_SECTION;
        } else {
            m_last_header.clear();
            return error_message;
        }
    } else if (header_type == LIKELY_MY_HEADER) {
        if (m_section <= MY_H_SECTION)
            m_section = MY_H_SECTION;
        else
            m_section = OTHER_H_SECTION;
    } else if (header_type == POSSIBLE_MY_HEADER) {
        if (m_section <= MY_H_SECTION) {
            m_section = MY_H_SECTION;
        } else {
            // This will always be the fallback because we're not sure
            // enough that the header is associated with this file.
            m_section = OTHER_H_SECTION;
        }
    } else {
        assert(header_type == OTHER_HEADER);
        m_section = OTHER_H_SECTION;
    }

    if (last_section != m_section)
        m_last_header.clear();

    return "";
}

const int NORMAL_TRIGGER = 250;  // for --v=0, 500 for --v=1, etc.
const int TEST_TRIGGER = 400;    // about 50% more than _NORMAL_TRIGGER.

static inline int IntPow(int base, int exp) {
    int n = 1;
    for (int i = 0; i < exp; i++)
        n *= base;
    return n;
}

void FunctionState::Check(FileLinter* file_linter,
                          size_t linenum, int verbose_level) {
    if (!m_in_a_function)
        return;

    int base_trigger;
    if (m_current_function.starts_with("TEST") || m_current_function.starts_with("Test"))
        base_trigger = TEST_TRIGGER;
    else
        base_trigger = NORMAL_TRIGGER;
    int trigger = base_trigger * IntPow(2, verbose_level);

    if (m_lines_in_function > trigger) {
        int error_level = static_cast<int>(
                            std::log2(static_cast<double>(m_lines_in_function) / base_trigger));
        // 50 => 0, 100 => 1, 200 => 2, 400 => 3, 800 => 4, 1600 => 5, ...
        if (error_level > 5)
            error_level = 5;
        file_linter->Error(linenum, "readability/fn_size", error_level,
              "Small and focused functions are preferred: " +
              m_current_function + " has " +
              std::to_string(m_lines_in_function) + " non-comment lines"
              " (error triggered by exceeding " +
              std::to_string(trigger) + " lines).");
    }
}

bool NestingState::InTemplateArgumentList(const CleansedLines& clean_lines,
                                          size_t linenum, size_t pos) {
    while (linenum < clean_lines.NumLines()) {
        // Find the earliest character that might indicate a template argument
        std::string line = clean_lines.GetElidedAt(linenum).substr(pos);
        static const regex_code RE_PATTERN_TEMPLATE_ARG =
            RegexCompile(R"(^[^{};=\[\]\.<>]*(.))");
        thread_local regex_match re_result =
            RegexCreateMatchData(RE_PATTERN_TEMPLATE_ARG);
        bool match = RegexMatch(RE_PATTERN_TEMPLATE_ARG, line, re_result);
        if (!match) {
            linenum++;
            pos = 0;
            continue;
        }
        std::string_view token = GetMatchStrView(re_result, line, 1);
        pos += GetMatchSize(re_result, 0);

        // These things do not look like template argument list:
        //   class Suspect {
        //   class Suspect x; }
        if (InCharVec({ '{', '}', ';' }, token))
            return false;

        // These things look like template argument list:
        //   template <class Suspect>
        //   template <class Suspect = default_value>
        //   template <class Suspect[]>
        //   template <class Suspect...>
        if (InCharVec({ '>', '=', '[', ']', '.' }, token))
            return true;

        // Check if token is an unmatched '<'.
        // If not, move on to the next character.
        if (!StrIsChar(token, '<')) {
            pos += 1;
            if (pos >= line.size()) {
                linenum++;
                pos = 0;
            }
            continue;
        }

        // We can't be sure if we just find a single '<', and need to
        // find the matching '>'.
        size_t end_line = linenum;
        size_t end_pos = pos - 1;
        CloseExpression(clean_lines, &end_line, &end_pos);
        if (end_pos == INDEX_NONE) {
            // Not sure if template argument list or syntax error in file
            return false;
        }
        linenum = end_line;
        pos = end_pos;
    }
    return false;
}

void NestingState::UpdatePreprocessor(const std::string& line) {
    /*Update preprocessor stack.

    We need to handle preprocessors due to classes like this:
        #ifdef SWIG
        struct ResultDetailsPageElementExtensionPoint {
        #else
        struct ResultDetailsPageElementExtensionPoint : public Extension {
        #endif

    We make the following assumptions (good enough for most files):
    - Preprocessor condition evaluates to true from #if up to first
        #else/#elif/#endif.

    - Preprocessor condition evaluates to false from #else/#elif up
        to #endif.  We still perform lint checks on these lines, but
        these do not affect nesting stack.
    */
    if (GetFirstNonSpace(line) != '#')
        return;  // Not a macro

    static const regex_code RE_PATTERN_IF_MACRO =
        RegexCompile(R"(^\s*#\s*(if|ifdef|ifndef)\b)");
    static const regex_code RE_PATTERN_ELSE_MACRO =
        RegexCompile(R"(^\s*#\s*(else|elif)\b)");
    static const regex_code RE_PATTERN_ENDIF_MACRO =
        RegexCompile(R"(^\s*#\s*endif\b)");
    if (RegexMatch(RE_PATTERN_IF_MACRO, line)) {
        // Beginning of #if block, save the nesting stack here.  The saved
        // stack will allow us to restore the parsing state in the #else case.
        m_pp_stack.emplace(m_stack);
    } else if (RegexMatch(RE_PATTERN_ELSE_MACRO, line)) {
        // Beginning of #else block
        if (!m_pp_stack.empty()) {
            PreprocessorInfo& pp = m_pp_stack.top();
            if (!pp.SeenElse()) {
                // This is the first #else or #elif block.  Remember the
                // whole nesting stack up to this point.  This is what we
                // keep after the #endif.
                pp.SetSeenElse(true);
                pp.SetStackBeforeElse(m_stack);
            }

            // Restore the stack to how it was before the #if
            m_stack = pp.StackBeforeIf();
        } else {
            // TODO(unknown): unexpected #else, issue warning?
        }
    } else if (RegexMatch(RE_PATTERN_ENDIF_MACRO, line)) {
        // End of #if or #else blocks.
        if (!m_pp_stack.empty()) {
            PreprocessorInfo& pp = m_pp_stack.top();
            // If we saw an #else, we will need to restore the nesting
            // stack to its former state before the #else, otherwise we
            // will just continue from where we left off.
            if (pp.SeenElse()) {
                // Here we can just use a shallow copy since we are the last
                // reference to it.
                m_stack = pp.StackBeforeElse();
            }
            // Drop the corresponding #if
            m_pp_stack.pop();
        } else {
            // TODO(unknown): unexpected #endif, issue warning?
        }
    }
}

const regex_code RE_PATTERN_ASM = RegexCompile(
    R"(^\s*(?:asm|_asm|__asm|__asm__))"
    R"((?:\s+(volatile|__volatile__))?)"
    R"(\s*[{(])");

void NestingState::Update(const CleansedLines& clean_lines,
                          const std::string& elided_line,
                          size_t linenum,
                          FileLinter* file_linter) {
    std::string line = elided_line;

    // Remember top of the previous nesting stack.
    //
    // The stack is always pushed/popped and not modified in place, so
    // we can just do a shallow copy instead of copy.deepcopy.  Using
    // deepcopy would slow down cpplint by ~28%.
    if (!m_stack.empty())
        m_previous_stack_top = m_stack.back();
    else
        m_previous_stack_top = nullptr;

    // Update pp_stack
    UpdatePreprocessor(line);

    // Count parentheses.  This is to avoid adding struct arguments to
    // the nesting stack.
    if (!m_stack.empty()) {
        BlockInfo* inner_block = m_stack.back();
        int depth_change = StrCount(line, '(') - StrCount(line, ')');
        inner_block->IncOpenParentheses(depth_change);

        // Also check if we are starting or ending an inline assembly block.
        int inline_asm = inner_block->InlineAsm();
        if (inline_asm == NO_ASM || inline_asm == END_ASM) {
            if (depth_change != 0 &&
                inner_block->OpenParentheses() == 1 &&
                RegexMatch(RE_PATTERN_ASM, line)) {
                // Enter assembly block
                inner_block->SetInlineAsm(INSIDE_ASM);
            } else {
                // Not entering assembly block.  If previous line was _END_ASM,
                // we will now shift to NO_ASM state.
                inner_block->SetInlineAsm(NO_ASM);
            }
        } else if (inline_asm == INSIDE_ASM &&
                   inner_block->OpenParentheses() == 0) {
            // Exit assembly block
            inner_block->SetInlineAsm(END_ASM);
        }
    }

    // Consume namespace declaration at the beginning of the line.  Do
    // this in a loop so that we catch same line declarations like this:
    //   namespace proto2 { namespace bridge { class MessageSet; } }
    while (1) {
        // Match start of namespace.  The "\b\s*" below catches namespace
        // declarations even if it weren't followed by a whitespace, this
        // is so that we don't confuse our namespace checker.  The
        // missing spaces will be flagged by CheckSpacing.
        static const regex_code RE_PATTERN_START_OF_NAMESPACE =
            RegexCompile(R"(^\s*namespace\b\s*([:\w]+)?(.*)$)");
        bool match = RegexMatch(RE_PATTERN_START_OF_NAMESPACE, line, m_re_result);
        if (!match)
            break;

        m_block_info_buffer.push(new NamespaceInfo(GetMatchStr(m_re_result, line, 1), linenum));
        m_stack.push_back(m_block_info_buffer.top());

        line = GetMatchStr(m_re_result, line, 2);
        size_t pos = line.find('{');
        if (pos != std::string::npos) {
            m_stack.back()->SetSeenOpenBrace(true);
            line = line.substr(pos + 1);
        }
    }

    // Look for a class declaration in whatever is left of the line
    // after parsing namespaces.  The regexp accounts for decorated classes
    // such as in:
    //   class LOCKABLE API Object {
    //   };
    static const regex_code RE_PATTERN_CLASS_DECL =
        RegexCompile(R"(^(\s*(?:template\s*<[\w\s<>,:=]*>\s*)?)"
                     R"((class|struct)\s+(?:[a-zA-Z0-9_]+\s+)*(\w+(?:::\w+)*)))"
                     R"((.*)$)");
    bool match = RegexMatch(RE_PATTERN_CLASS_DECL, line, m_re_result);
    if (match &&
        (m_stack.empty() || m_stack.back()->OpenParentheses() == 0)) {
        // We do not want to accept classes that are actually template arguments:
        //   template <class Ignore1,
        //             class Ignore2 = Default<Args>,
        //             template <Args> class Ignore3>
        //   void Function() {};
        //
        // To avoid template argument cases, we scan forward and look for
        // an unmatched '>'.  If we see one, assume we are inside a
        // template argument list.
        size_t end_declaration = GetMatchSize(m_re_result, 1);
        if (!InTemplateArgumentList(clean_lines, linenum, end_declaration)) {
            m_block_info_buffer.push(
                new ClassInfo(
                        GetMatchStr(m_re_result, line, 3),
                        GetMatchStr(m_re_result, line, 2),
                        clean_lines, linenum));
            m_stack.push_back(m_block_info_buffer.top());
            line = GetMatchStr(m_re_result, line, 4);
        }
    }

    // If we have not yet seen the opening brace for the innermost block,
    // run checks here.
    if (!SeenOpenBrace())
        m_stack.back()->CheckBegin(clean_lines, linenum);

    // Update access control if we are inside a class/struct
    if (!m_stack.empty() && m_stack.back()->IsClassInfo()) {
        ClassInfo* classinfo = static_cast<ClassInfo*>(m_stack.back());
        static const regex_code RE_PATTERN_CLASS_ACCESS =
            RegexCompile(R"(^(.*)\b(public|private|protected|signals)(\s+(?:slots\s*)?)?)"
                         R"(:(?:[^:]|$))");
        match = RegexMatch(RE_PATTERN_CLASS_ACCESS, line, m_re_result);
        if (match) {
            classinfo->SetAccess(GetMatchStr(m_re_result, line, 2));

            // Check that access keywords are indented +1 space.  Skip this
            // check if the keywords are not preceded by whitespaces.
            std::string indent = GetMatchStr(m_re_result, line, 1);
            if ((indent.size() != (classinfo->ClassIndent() + 1)) &&
                StrIsBlank(indent)) {
                std::string parent;
                if (classinfo->IsStruct())
                    parent = "struct " + classinfo->Name();
                else
                    parent = "class " + classinfo->Name();
                std::string slots = "";
                if (!GetMatchStrView(m_re_result, line, 3).empty())
                    slots = GetMatchStr(m_re_result, line, 3);
                file_linter->Error(linenum, "whitespace/indent", 3,
                    GetMatchStr(m_re_result, line, 2) + slots + ":" +
                    " should be indented +1 space inside " + parent);
            }
        }
    }

    // Consume braces or semicolons from what's left of the line
    size_t pos = 0;
    while (pos < line.size()) {
        // Match first brace, semicolon, or closed parenthesis.
        static const regex_code RE_PATTERN_TOKEN =
            RegexCompile("^[^{;)}]*([{;)}])(.*)$");
        size_t length = line.size() - pos;
        bool matched = RegexMatchWithRange(RE_PATTERN_TOKEN, line,
                                           pos, length, m_re_result);
        if (!matched)
            break;

        const char token = line[GetMatchStart(m_re_result, 1, pos)];
        assert(GetMatchSize(m_re_result, 1) == 1);
        if (token == '{') {
            // If namespace or class hasn't seen a opening brace yet, mark
            // namespace/class head as complete.  Push a new block onto the
            // stack otherwise.
            static const regex_code RE_PATTERN_EXTERN =
                RegexCompile(R"(^extern\s*"[^"]*"\s*\{)");
            if (!SeenOpenBrace()) {
                m_stack.back()->SetSeenOpenBrace(true);
            } else if (RegexMatchWithRange(RE_PATTERN_EXTERN, line, pos, length)) {
                m_block_info_buffer.push(new ExternCInfo(linenum));
                m_stack.push_back(m_block_info_buffer.top());
            } else {
                m_block_info_buffer.push(new BlockInfo(linenum, true));
                m_stack.push_back(m_block_info_buffer.top());
                if (RegexMatchWithRange(RE_PATTERN_ASM, line, pos, length))
                    m_stack.back()->SetInlineAsm(BLOCK_ASM);
            }
        } else if (token == ';' || token == ')') {
            // If we haven't seen an opening brace yet, but we already saw
            // a semicolon, this is probably a forward declaration.  Pop
            // the stack for these.
            //
            // Similarly, if we haven't seen an opening brace yet, but we
            // already saw a closing parenthesis, then these are probably
            // function arguments with extra "class" or "struct" keywords.
            // Also pop these stack for these.
            if (!SeenOpenBrace())
                m_stack.pop_back();
        } else {  // token == '}'
            // Perform end of block checks and pop the stack.
            if (!m_stack.empty()) {
                m_stack.back()->CheckEnd(clean_lines, linenum, file_linter);
                m_stack.pop_back();
            }
        }
        pos = GetMatchStart(m_re_result, 2, pos);
    }
}

ClassInfo* NestingState::InnermostClass() {
    if (m_stack.empty())
        return nullptr;

    for (size_t i = m_stack.size() - 1;; i--) {
        BlockInfo* blockinfo = m_stack[i];
        if (blockinfo->IsClassInfo())
            return static_cast<ClassInfo*>(blockinfo);
        if (i == 0)
            break;
    }
    return nullptr;
}
