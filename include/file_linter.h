#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include "cleanse.h"
#include "cpplint_state.h"
#include "error_suppressions.h"
#include "options.h"
#include "regex_utils.h"
#include "states.h"

namespace fs = std::filesystem;

// A worker for a file
class FileLinter {
 private:
    CppLintState* m_cpplint_state;
    Options m_options;
    ErrorSuppressions m_error_suppressions;
    std::set<std::string> m_all_extensions;
    std::set<std::string> m_header_extensions;
    std::set<std::string> m_non_header_extensions;
    fs::path m_file;
    std::string m_filename;
    std::string m_file_extension;
    fs::path m_file_from_repo;  // relative path from repository
    std::string m_cppvar;
    regex_match m_re_result;
    regex_match m_re_result_temp;  // use this when we dont need results
    bool m_has_error;

 public:
    FileLinter() {}
    FileLinter(const fs::path& file, CppLintState* state, const Options& options) :
                m_cpplint_state(state),
                m_options(options),
                m_error_suppressions(),
                m_all_extensions({}),
                m_header_extensions({}),
                m_non_header_extensions({}),
                m_file(file),
                m_filename(file.string()),
                m_file_extension(),
                m_file_from_repo(),
                m_cppvar(),
                m_re_result(RegexCreateMatchData(16)),
                m_re_result_temp(RegexCreateMatchData(16)),
                m_has_error(false) {}

    fs::path GetRelativeFromRepository(const fs::path& file, const fs::path& repository);
    fs::path GetRelativeFromSubdir(const fs::path& file, const fs::path& subdir);

    // Logs an error if no Copyright message appears at the top of the file.
    void CheckForCopyright(const std::vector<std::string>& lines);

    // Remove multiline (c-style) comments from lines.
    void RemoveMultiLineComments(std::vector<std::string>& lines);

    // Logs an error if there is no newline char at the end of the file.
    void CheckForNewlineAtEOF(const std::vector<std::string>& lines);

    std::string GetHeaderGuardCPPVariable();

    /*
      Checks that the file contains a header guard.
      Logs an error if no #ifndef header guard is present.  For other
      headers, checks that the full pathname is used.
     */
    void CheckForHeaderGuard(const CleansedLines& clean_lines);

    bool IsForwardClassDeclaration(const std::string& elided_line);

    bool IsMacroDefinition(const CleansedLines& clean_lines,
                           const std::string& elided_line, size_t linenum);

    void CheckForNamespaceIndentation(const CleansedLines& clean_lines,
                                      const std::string& elided_line, size_t linenum,
                                      NestingState* nesting_state);

    /*
    Reports for long function bodies.

    For an overview why this is done, see:
    https://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Write_Short_Functions

    Uses a simplistic algorithm assuming other style guidelines
    (especially spacing) are followed.
    Only checks unindented functions, so class members are unchecked.
    Trivial bodies are unchecked, so constructors with huge initializer lists
    may be missed.
    Blank/comment lines are not counted so as to avoid encouraging the removal
    of vertical space and comments just to get through a lint check.
    NOLINT *on the last line of a function* disables this check.
    */
    void CheckForFunctionLengths(const CleansedLines& clean_lines, size_t linenum,
                                 FunctionState* function_state);

    // Logs an error if we see /* ... */ or "..." that extend past one line.
    // /* ... */ comments are legit inside macros, for one line.
    /* Otherwise, we prefer // comments, so it's ok to warn about the
       other.  Likewise, it's ok for strings to extend across multiple
       lines, as long as a line continuation character (backslash)
       terminates each line. Although not currently prohibited by the C++
       style guide, it's ugly and unnecessary. We don't do well with either
       in this lint program, so we warn about both.
    */
    void CheckForMultilineCommentsAndStrings(const std::string& elided_line,
                                             size_t linenum);

    // Looks for misplaced braces (e.g. at the end of line).
    void CheckBraces(const CleansedLines& clean_lines,
                     const std::string& elided_line, size_t linenum);

    // Looks for redundant trailing semicolon.
    void CheckTrailingSemicolon(const CleansedLines& clean_lines,
                                const std::string& elided_line, size_t linenum);
    void CheckEmptyBlockBody(const CleansedLines& clean_lines,
                             const std::string& elided_line, size_t linenum);

    // Checks for common mistakes in comments.
    void CheckComment(const std::string& line,
                      size_t linenum, size_t next_line_start);

    /*Checks for the correctness of various spacing issues in the code.

    Things we check for: spaces around operators, spaces after
    if/for/while/switch, no spaces around parens in function calls, two
    spaces between code and comment, don't start a block with a blank
    line, don't end a function with a blank line, don't add a blank line
    after public/protected/private, don't have too many blank lines in a row.
    */
    void CheckSpacing(const CleansedLines& clean_lines,
                      const std::string& elided_line, size_t linenum,
                      NestingState* nesting_state);

    // Checks for horizontal spacing around operators.
    void CheckOperatorSpacing(const CleansedLines& clean_lines,
                              const std::string& elided_line, size_t linenum);

    // Checks for horizontal spacing around parentheses.
    void CheckParenthesisSpacing(const std::string& elided_line, size_t linenum);

    // Checks for horizontal spacing near commas and semicolons.
    void CheckCommaSpacing(const CleansedLines& clean_lines,
                           const std::string& elided_line, size_t linenum);

    // Checks for horizontal spacing near commas.
    void CheckBracesSpacing(const CleansedLines& clean_lines,
                            const std::string& elided_line, size_t linenum,
                            NestingState* nesting_state);

    // Checks for the correctness of various spacing around function calls.
    void CheckSpacingForFunctionCall(const std::string& elided_line, size_t linenum);
    void CheckSpacingForFunctionCallBase(const std::string& line,
                                         const std::string& fncall, size_t linenum);

    // Checks the use of CHECK and EXPECT macros.
    void CheckCheck(const CleansedLines& clean_lines,
                    const std::string& elided_line, size_t linenum);

    // Check alternative keywords being used in boolean expressions.
    void CheckAltTokens(const std::string& elided_line, size_t linenum);

    // Checks for additional blank line issues related to sections.
    // Currently the only thing checked here is blank line before protected/private.
    void CheckSectionSpacing(const CleansedLines& clean_lines,
                             ClassInfo* classinfo, size_t linenum);

    /*
    Checks rules from the 'C++ style rules' section of cppguide.html.

    Most of these rules are hard to test (naming, comment style), but we
    do what we can.  In particular we check for 2-space indents, line lengths,
    tab usage, spaces inside code, etc.
    */
    void CheckStyle(const CleansedLines& clean_lines,
                    const std::string& elided_line,
                    size_t linenum,
                    bool is_header_extension);

    // Checks C++ style rules that require NestingState.
    void CheckStyleWithState(const CleansedLines& clean_lines,
                             const std::string& elided_line,
                             size_t linenum,
                             NestingState* nesting_state);

    /*
    Drops common suffixes like _test.cc or -inl.h from filename.

    For example:
        >>> _DropCommonSuffixes('foo/foo-inl.h')
        'foo/foo'
        >>> _DropCommonSuffixes('foo/bar/foo.cc')
        'foo/bar/foo'
        >>> _DropCommonSuffixes('foo/foo_internal.h')
        'foo/foo'
        >>> _DropCommonSuffixes('foo/foo_unusualinternal.h')
        'foo/foo_unusualinternal'
    */
    fs::path DropCommonSuffixes(const fs::path& file);

    /* Figures out what kind of header 'include' is.
    For example:
        >>> ClassifyInclude(FileInfo('foo/foo.cc'), 'stdio.h', True)
        _C_SYS_HEADER
        >>> ClassifyInclude(FileInfo('foo/foo.cc'), 'string', True)
        _CPP_SYS_HEADER
        >>> ClassifyInclude(FileInfo('foo/foo.cc'), 'foo/foo.h', True, "standardcfirst")
        _OTHER_SYS_HEADER
        >>> ClassifyInclude(FileInfo('foo/foo.cc'), 'foo/foo.h', False)
        _LIKELY_MY_HEADER
        >>> ClassifyInclude(FileInfo('foo/foo_unknown_extension.cc'),
        ...                  'bar/foo_other_ext.h', False)
        _POSSIBLE_MY_HEADER
        >>> _ClassifyInclude(FileInfo('foo/foo.cc'), 'foo/bar.h', False)
        _OTHER_HEADER
    */
    int ClassifyInclude(const fs::path& path_from_repo,
                        const fs::path& include,
                        bool used_angle_brackets);

    /*
    Check rules that are applicable to #include lines.

    Strings on #include lines are NOT removed from elided line, to make
    certain tasks easier. However, to prevent false positives, checks
    applicable to #include lines in CheckLanguage must be put here.
    */
    void CheckIncludeLine(const CleansedLines& clean_lines, size_t linenum,
                          IncludeState* include_state);

    // Checks whether where function type arguments are expected.
    bool ExpectingFunctionArgs(const CleansedLines& clean_lines,
                               const std::string& elided_line, size_t linenum);

    // Checks for a C-style cast by looking for the pattern.
    // Returns true if an error was emitted. false otherwise.
    bool CheckCStyleCast(const CleansedLines& clean_lines,
                         const std::string& elided_line, size_t linenum,
                         const std::string& cast_type,
                         const regex_code& pattern);

    // Various cast related checks.
    void CheckCasts(const CleansedLines& clean_lines,
                    const std::string& elided_line, size_t linenum);

    // Check for unsafe global or static objects.
    void CheckGlobalStatic(const std::string& elided_line, size_t linenum);

    // Check for printf related issues.
    void CheckPrintf(const std::string& elided_line, size_t linenum);

    /*
    Checks rules from the 'C++ language rules' section of cppguide.html.

    Some of these rules are hard to test (function overloading, using
    uint32 inappropriately), but we do the best we can.
    */
    void CheckLanguage(const CleansedLines& clean_lines,
                       const std::string& elided_line, size_t linenum,
                       bool is_header_extension,
                       IncludeState* include_state);

    /*
    Check for non-const references.

    Separate from CheckLanguage since it scans backwards from current
    line, instead of scanning forward.
    */
    void CheckForNonConstReference(const CleansedLines& clean_lines,
                                   const std::string& elided_line, size_t linenum,
                                   NestingState* nesting_state);

    /*
    Logs an error if we see certain non-ANSI constructs ignored by gcc-2.

    Complain about several constructs which gcc-2 accepts, but which are
    not standard C++.  Warning about these in lint is one way to ease the
    transition to new compilers.
    - put storage class first (e.g. "static const" instead of "const static").
    - "%lld" instead of %qd" in printf-type functions.
    - "%1$d" is non-standard in printf-type functions.
    - "\%" is an undefined character escape sequence.
    - text after #endif is not allowed.
    - invalid inner-style forward declaration.
    - >? and <? operators, and their >?= and <?= cousins.

    Additionally, check for constructor/destructor style violations and reference
    members, as it is very convenient to do so while checking for
    gcc-2 compliance.
    */
    void CheckForNonStandardConstructs(const CleansedLines& clean_lines,
                                       const std::string& elided_line, size_t linenum,
                                       NestingState* nesting_state);

    /*
    Checks that VLOG() is only used for defining a logging level.

    For example, VLOG(2) is correct. VLOG(INFO), VLOG(WARNING), VLOG(ERROR), and
    VLOG(FATAL) are not.
    */
    void CheckVlogArguments(const std::string& elided_line, size_t linenum);

    /*
    Checks for calls to thread-unsafe functions.

    Much code has been originally written without consideration of
    multi-threading. Also, engineers are relying on their old experience;
    they have learned posix before threading extensions were added. These
    tests guide the engineers to use thread-safe functions (when using
    posix directly).
    */
    void CheckPosixThreading(const std::string& elided_line, size_t linenum);

    /*
    Checks for invalid increment *count++.

    For example following function:
    void increment_counter(int* count) {
        *count++;
    }
    is invalid, because it effectively does count++, moving pointer, and should
    be replaced with ++*count, (*count)++ or *count += 1.
    */
    void CheckInvalidIncrement(const std::string& elided_line, size_t linenum);

    /*
    Check that make_pair's template arguments are deduced.

    G++ 4.6 in C++11 mode fails badly if make_pair's template arguments are
    specified explicitly, and such use isn't intended in any case.
    */
    void CheckMakePairUsesDeduction(const std::string& elided_line, size_t linenum);

    // Check if line contains a redundant "virtual" function-specifier.
    void CheckRedundantVirtual(const CleansedLines& clean_lines,
                               const std::string& elided_line, size_t linenum);

    // Check if line contains a redundant "override" or "final" virt-specifier.
    void CheckRedundantOverrideOrFinal(const CleansedLines& clean_lines,
                                       const std::string& elided_line, size_t linenum);

    // Flag C++ headers that the styleguide restricts.
    void CheckCxxHeaders(const std::string& elided_line, size_t linenum);

    /*
    Reports for missing stl includes.

    This function will output warnings to make sure you are including the headers
    necessary for the stl containers and functions that you use. We only give one
    reason to include a header. For example, if you use both equal_to<> and
    less<> in a .h file, only one (the latter in the file) of these will be
    reported as a reason to include the <functional>.
    */
    void CheckForIncludeWhatYouUse(const CleansedLines& clean_lines,
                                   IncludeState* include_state);

    // Logs an error if a source file does not include its header.
    void CheckHeaderFileIncluded(IncludeState* include_state);

    /*
    Updates the global list of line error-suppressions.

    Parses any NOLINT comments on the current line, updating the global
    error_suppressions store.  Reports an error if the NOLINT comment
    was malformed.
    */
    void ParseNolintSuppressions(const std::string& raw_line,
                                 size_t linenum);

    // Updates the list of global error suppressions.
    // Parses any lint directives in the file that have global effect.
    void ProcessGlobalSuppressions(const std::string& lines);

    // Calculates some member variables
    void CacheVariables();
    void CacheVariables(const fs::path& file);


    // Gets lines from a file and executes ProcessFileData
    void ProcessFile();

    // Process lines in the file
    void ProcessFileData(std::vector<std::string>& lines);

    // Processes a single line in the file.
    void ProcessLine(bool is_header_extension,
                     const CleansedLines& clean_lines,
                     const std::string& elided_line, size_t linenum,
                     IncludeState* include_state,
                     FunctionState* function_state,
                     NestingState* nesting_state);

    void Error(size_t linenum,
               const std::string& category, int confidence,
               const std::string& message) {
        if (m_error_suppressions.IsSuppressed(category, linenum) ||
            !m_options.ShouldPrintError(category, m_filename, linenum) ||
            confidence < m_cpplint_state->VerboseLevel()) {
            // The error is suppressed with NOLINT comments,
            // or filtered with --filter options,
            // or verbose level is higher than confidence.
            return;
        }
        m_has_error = true;
        m_cpplint_state->Error(m_filename, linenum, category, confidence, message);
    }
};
