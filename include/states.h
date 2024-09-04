#pragma once
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>
#include "cleanse.h"
#include "nest_info.h"
#include "regex_utils.h"

class FileLinter;

// These constants define types of headers for use with
// IncludeState.CheckNextIncludeOrder().
enum HeaderType : int {
    C_SYS_HEADER = 1,
    CPP_SYS_HEADER = 2,
    OTHER_SYS_HEADER = 3,
    LIKELY_MY_HEADER = 4,
    POSSIBLE_MY_HEADER = 5,
    OTHER_HEADER = 6,
};

class IncludeState {
    /*Tracks line numbers for includes, and the order in which includes appear.

    include_list contains list of lists of (header, line number) pairs.
    It's a lists of lists rather than just one flat list to make it
    easier to update across preprocessor boundaries.

    Call CheckNextIncludeOrder() once for each header in the file, passing
    in the type constants defined above. Calls in an illegal order will
    raise an _IncludeError with an appropriate error message.
    */

 private:
    int m_section;
    std::string m_last_header;
    std::vector<std::vector<std::pair<std::string, size_t>>> m_include_list;

 public:
    IncludeState() :
        m_section(0),
        m_last_header(),
        m_include_list({{}}) {
        ResetSection("");
    }

    // Check if a header has already been included.
    // It returns line number of previous occurrence,
    // or -1 if the header has not been seen before.
    size_t FindHeader(const std::string& header);

    // Reset section checking for preprocessor directive.
    void ResetSection(const std::string& directive);

    void SetLastHeader(const std::string& header_path) {
        m_last_header = header_path;
    }

    /* Returns a path canonicalized for alphabetical comparison.

        - replaces "-" with "_" so they both cmp the same.
        - removes '-inl' since we don't require them to be after the main header.
        - lowercase everything, just in case.
    */
    static std::string CanonicalizeAlphabeticalOrder(const std::string& header_path);

    // Check if a header is in alphabetical order with the previous header.
    bool IsInAlphabeticalOrder(const CleansedLines& clean_lines,
                               size_t linenum,
                               const std::string& header_path);

    /* Returns a non-empty error message if the next header is out of order.

        This function also updates the internal state to be ready to check
        the next include.
    */
    std::string CheckNextIncludeOrder(int header_type);

    std::vector<std::pair<std::string, size_t>>& LastIncludeList() { return m_include_list.back(); }
    [[nodiscard]] const auto& IncludeList() const { return m_include_list; }
    std::set<std::string> GetIncludes() {
        std::set<std::string> includes = {};
        for (const auto& sublist : m_include_list) {
            for (const std::pair<std::string, size_t>& p : sublist) {
                includes.insert(p.first);
            }
        }
        return includes;
    }
};

// Tracks current function name and the number of lines in its body.
class FunctionState {
 private:
    bool m_in_a_function;
    int m_lines_in_function;
    std::string m_current_function;

 public:
    FunctionState() :
        m_in_a_function(false),
        m_lines_in_function(0),
        m_current_function() {}

    // Start analyzing function body.
    void Begin(const std::string& function_name) {
        m_in_a_function = true;
        m_lines_in_function = 0;
        m_current_function = function_name;
    }

    // Count line in current function body.
    void Count() {
        if (m_in_a_function)
            m_lines_in_function++;
    }

    // Report if too many lines in function body.
    void Check(FileLinter* file_linter,
               size_t linenum, int verbose_level);

    // Stop analyzing function body.
    void End() {
        m_in_a_function = false;
    }
};

// Holds states related to parsing braces.
class NestingState {
 private:
    // Store all BlockInfo objects here to free them with destructor.
    std::stack<BlockInfo*> m_block_info_buffer;

    // Stack for tracking all braces.  An object is pushed whenever we
    // see a "{", and popped when we see a "}".  Only 3 types of
    // objects are possible:
    // - _ClassInfo: a class or struct.
    // - _NamespaceInfo: a namespace.
    // - _BlockInfo: some other type of block.
    std::vector<BlockInfo*> m_stack;

    // Top of the previous stack before each Update().
    //
    // Because the nesting_stack is updated at the end of each line, we
    // had to do some convoluted checks to find out what is the current
    // scope at the beginning of the line.  This check is simplified by
    // saving the previous top of nesting stack.
    //
    // We could save the full stack, but we only need the top.  Copying
    // the full nesting stack would slow down cpplint by ~10%.
    BlockInfo* m_previous_stack_top;

    // Stack of _PreprocessorInfo objects.
    std::stack<PreprocessorInfo> m_pp_stack;
    regex_match m_re_result;

 public:
    NestingState() :
        m_stack({}),
        m_previous_stack_top(nullptr),
        m_pp_stack(),
        m_re_result(RegexCreateMatchData(16)) {}

    ~NestingState() {
        // Free block info objects
        while (!m_block_info_buffer.empty()) {
            BlockInfo* block = m_block_info_buffer.top();
            delete block;
            m_block_info_buffer.pop();
        }
    }

    bool SeenOpenBrace() {
        /*Check if we have seen the opening brace for the innermost block.

        Returns:
            True if we have seen the opening brace, False if the innermost
            block is still expecting an opening brace.
        */
        return m_stack.empty() || m_stack.back()->SeenOpenBrace();
    }

    bool InNamespaceBody() {
        // Check if we are currently one level inside a namespace body.
        return !m_stack.empty() && m_stack.back()->IsNamespaceInfo();
    }

    bool InExternC() {
        // Check if we are currently one level inside an 'extern "C"' block.
        return !m_stack.empty() && m_stack.back()->IsExternCInfo();
    }

    bool InClassDeclaration() {
        // Check if we are currently one level inside a class or struct declaration.
        return !m_stack.empty() && m_stack.back()->IsClassInfo();
    }

    bool InAsmBlock() {
        // Check if we are currently one level inside an inline ASM block.
        return !m_stack.empty() && (m_stack.back()->InlineAsm() != NO_ASM);
    }

    // Check if current position is inside template argument list.
    static bool InTemplateArgumentList(const CleansedLines& clean_lines, size_t linenum, size_t pos);

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
    void UpdatePreprocessor(const std::string& line);

    // TODO(unknown): Update() is too long, but we will refactor later.
    // Update nesting state with current line.
    void Update(const CleansedLines& clean_lines,
                const std::string& elided_line,
                size_t linenum,
                FileLinter* file_linter);

    // Get class info on the top of the stack.
    ClassInfo* InnermostClass();

    bool IsNamespaceIndentInfo() {
        return m_stack.size() >= 1 &&
               (m_stack.back()->IsNamespaceInfo() ||
                (m_previous_stack_top != nullptr &&
                 m_previous_stack_top->IsNamespaceInfo()));
    }

    // Returns true if we are at a new block, and it is directly
    // inside of a namespace.
    bool IsBlockInNameSpace(bool is_forward_declaration) {
        if (is_forward_declaration) {
            return m_stack.size() >= 1 &&
                   m_stack.back()->IsNamespaceInfo();
        }

        if (m_stack.size() >= 1) {
            if (m_stack.back()->IsNamespaceInfo()) {
                return true;
            } else if (m_stack.size() > 1 &&
                        m_previous_stack_top != nullptr &&
                        m_previous_stack_top->IsNamespaceInfo() &&
                        m_stack[m_stack.size() - 2]->IsNamespaceInfo()) {
                return true;
            }
        }
        return false;
    }

    const BlockInfo* GetStackAt(size_t i) {
        return m_stack[i];
    }

    size_t GetStackSize() { return m_stack.size(); }

    BlockInfo* PreviousStackTop() { return m_previous_stack_top; }
};
