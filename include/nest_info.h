#pragma once
#include <stack>
#include <string>
#include <utility>
#include <vector>
#include "cleanse.h"
#include "cpplint.h"
#include "regex_utils.h"


class FileLinter;

// These constants define the current inline assembly state
enum AssemblyState : int {
    NO_ASM = 0,       // Outside of inline assembly block
    INSIDE_ASM = 1,   // Inside inline assembly block
    END_ASM = 2,      // Last line of inline assembly block
    BLOCK_ASM = 3,    // The whole block is an inline assembly block
};

enum BlockType : int {
    BLOCK_INFO = 0,
    EXTERN_C_INFO,
    CLASS_INFO,
    NAMESPACE_INFO,
};

// Stores information about a generic block of code.
class BlockInfo {
 protected:
    size_t m_starting_linenum;
    bool m_seen_open_brace;
    int m_open_parentheses;
    int m_inline_asm;
    bool m_check_namespace_indentation;
    int m_block_type;

 public:
    BlockInfo(size_t linenum, bool seen_open_brace) :
        m_starting_linenum(linenum),
        m_seen_open_brace(seen_open_brace),
        m_open_parentheses(0),
        m_inline_asm(NO_ASM),
        m_check_namespace_indentation(false),
        m_block_type(BLOCK_INFO) {}

    virtual ~BlockInfo() {}

    /* Run checks that applies to text up to the opening brace.

       This is mostly for checking the text after the class identifier
       and the "{", usually where the base class is specified.  For other
       blocks, there isn't much to check, so we always pass.
    */
    virtual void CheckBegin(const CleansedLines& clean_lines,
                            size_t linenum) {
        UNUSED(clean_lines);
        UNUSED(linenum);
    }

    /* Run checks that applies to text after the closing brace.

       This is mostly used for checking end of namespace comments.
    */
    virtual void CheckEnd(const CleansedLines& clean_lines,
                          size_t linenum,
                          FileLinter* file_linter) {
        UNUSED(clean_lines);
        UNUSED(linenum);
        UNUSED(file_linter);
    }

    /* Returns true if this block is a _BlockInfo.

       This is convenient for verifying that an object is an instance of
       a _BlockInfo, but not an instance of any of the derived classes.
    */
    bool IsBlockInfo() const { return m_block_type == BLOCK_INFO; }
    bool IsExternCInfo() const { return m_block_type == EXTERN_C_INFO; }
    bool IsClassInfo() const { return m_block_type == CLASS_INFO; }
    bool IsNamespaceInfo() const { return m_block_type == NAMESPACE_INFO; }

    bool SeenOpenBrace() const { return m_seen_open_brace; }
    void SetSeenOpenBrace(bool seen_open_brace) { m_seen_open_brace = seen_open_brace; }

    int OpenParentheses() const { return m_open_parentheses; }
    void IncOpenParentheses(int inc) { m_open_parentheses += inc; }

    int InlineAsm() const { return m_inline_asm; }
    void SetInlineAsm(int inline_asm) { m_inline_asm = inline_asm; }

    size_t StartingLinenum() const { return m_starting_linenum; }
};

// Stores information about an 'extern "C"' block.
class ExternCInfo : public BlockInfo {
 public:
    explicit ExternCInfo(size_t linenum) : BlockInfo(linenum, true) {
        m_block_type = EXTERN_C_INFO;
    }
};

// Stores information about a class.
class ClassInfo : public BlockInfo {
 private:
    std::string m_name;
    std::string m_basename;
    bool m_is_derived;
    std::string m_access;
    bool m_is_struct;
    size_t m_last_line;
    size_t m_class_indent;
    regex_match m_re_result;

 public:
    ClassInfo(const std::string& name,
              const std::string& class_or_struct,
              const CleansedLines& clean_lines,
              size_t linenum);

    void CheckBegin(const CleansedLines& clean_lines,
                    size_t linenum) override;

    void CheckEnd(const CleansedLines& clean_lines,
                  size_t linenum,
                  FileLinter* file_linter) override;
    const std::string& Access() const { return m_access; }
    void SetAccess(const std::string& access) { m_access = access; }
    bool IsStruct() const { return m_is_struct; }
    size_t ClassIndent() const { return m_class_indent; }
    const std::string& Name() const { return m_name; }
    const std::string& Basename() const { return m_basename; }
    size_t LastLine() { return m_last_line; }
};

// Stores information about a namespace.
class NamespaceInfo : public BlockInfo {
 private:
    std::string m_name;

 public:
    NamespaceInfo(const std::string& name, size_t linenum) :
        BlockInfo(linenum, false) {
        m_block_type = NAMESPACE_INFO;
        m_name = name;
        m_check_namespace_indentation = true;
    }

    void CheckEnd(const CleansedLines& clean_lines,
                  size_t linenum,
                  FileLinter* file_linter) override;
};

// Stores checkpoints of nesting stacks when #if/#else is seen.
class PreprocessorInfo {
 private:
    bool m_seen_else;
    std::vector<BlockInfo*> m_stack_before_if;
    std::vector<BlockInfo*> m_stack_before_else;

 public:
    explicit PreprocessorInfo(const std::vector<BlockInfo*>& stack_before_if) {
        // The entire nesting stack before #if
        m_stack_before_if = stack_before_if;

        // The entire nesting stack up to #else
        m_stack_before_else = {};

        // Whether we have already seen #else or #elif
        m_seen_else = false;
    }

    bool SeenElse() { return m_seen_else; }
    void SetSeenElse(bool seen_else) { m_seen_else = seen_else; }

    const std::vector<BlockInfo*>& StackBeforeIf() {
        return m_stack_before_if;
    }

    const std::vector<BlockInfo*>& StackBeforeElse() {
        return m_stack_before_else;
    }

    void SetStackBeforeElse(const std::vector<BlockInfo*>& stack_before_else) {
        m_stack_before_else = stack_before_else;
    }
};
