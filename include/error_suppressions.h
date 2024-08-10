#pragma once
#include <climits>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include "string_utils.h"

// We categorize each error message we print.  Here are the categories.
// We want an explicit list so we can list them all in cpplint --filter=.
// If you add a new error message with a new category, add it to the list
// here!  cpplint_unittest.py should tell you if you forget to do this.
inline const char* const ERROR_CATEGORIES[] = {
    "build/c++11",
    "build/c++17",
    "build/deprecated",
    "build/endif_comment",
    "build/explicit_make_pair",
    "build/forward_decl",
    "build/header_guard",
    "build/include",
    "build/include_subdir",
    "build/include_alpha",
    "build/include_order",
    "build/include_what_you_use",
    "build/namespaces_headers",
    "build/namespaces_literals",
    "build/namespaces",
    "build/printf_format",
    "build/storage_class",
    "legal/copyright",
    "readability/alt_tokens",
    "readability/braces",
    "readability/casting",
    "readability/check",
    "readability/constructors",
    "readability/fn_size",
    "readability/inheritance",
    "readability/multiline_comment",
    "readability/multiline_string",
    "readability/namespace",
    "readability/nolint",
    "readability/nul",
    "readability/strings",
    "readability/todo",
    "readability/utf8",
    "runtime/arrays",
    "runtime/casting",
    "runtime/explicit",
    "runtime/int",
    "runtime/init",
    "runtime/invalid_increment",
    "runtime/member_string_references",
    "runtime/memset",
    "runtime/operator",
    "runtime/printf",
    "runtime/printf_format",
    "runtime/references",
    "runtime/string",
    "runtime/threadsafe_fn",
    "runtime/vlog",
    "whitespace/blank_line",
    "whitespace/braces",
    "whitespace/comma",
    "whitespace/comments",
    "whitespace/empty_conditional_body",
    "whitespace/empty_if_body",
    "whitespace/empty_loop_body",
    "whitespace/end_of_line",
    "whitespace/ending_newline",
    "whitespace/forcolon",
    "whitespace/indent",
    "whitespace/indent_namespace",
    "whitespace/line_length",
    "whitespace/newline",
    "whitespace/operators",
    "whitespace/parens",
    "whitespace/semicolon",
    "whitespace/tab",
    "whitespace/todo",
    nullptr,
};

inline bool InErrorCategories(const std::string& category) {
    return InStrVec(ERROR_CATEGORIES, category);
}

// These error categories are no longer enforced by cpplint, but for backwards-
// compatibility they may still appear in NOLINT comments.
inline const char* const LEGACY_ERROR_CATEGORIES[] = {
    "build/class",
    "readability/streams",
    "readability/function",
    nullptr,
};

inline bool InLegacyErrorCategories(const std::string& category) {
    return InStrVec(LEGACY_ERROR_CATEGORIES, category);
}

// These prefixes for categories should be ignored since they relate to other
// tools which also use the NOLINT syntax, e.g. clang-tidy.
inline const char* const OTHER_NOLINT_CATEGORY_PREFIXES[] = {
    "clang-analyzer-",
    "abseil-",
    "altera-",
    "android-",
    "boost-",
    "bugprone-",
    "cert-",
    "concurrency-",
    "cppcoreguidelines-",
    "darwin-",
    "fuchsia-",
    "google-",
    "hicpp-",
    "linuxkernel-",
    "llvm-",
    "llvmlibc-",
    "misc-",
    "modernize-",
    "mpi-",
    "objc-",
    "openmp-",
    "performance-",
    "portability-",
    "readability-",
    "zircon-",
};

inline bool InOtherNolintCategories(const std::string& category) {
    for (const char* const c : OTHER_NOLINT_CATEGORY_PREFIXES) {
        if (category.starts_with(c))
            return true;
    }
    return false;
}

class LineRange {
 private:
    size_t m_begin;
    size_t m_end;

 public:
    LineRange(size_t begin, size_t end) {
        m_begin = begin;
        m_end = end;
    }

    std::string ToStr() const {
        return "[" + std::to_string(m_begin) + "-" + std::to_string(m_end) +"]";
    }

    bool Contain(size_t linenum) const {
        return m_begin <= linenum && linenum <= m_end;
    }

    bool ContainRange(const LineRange& other) const {
        return m_begin <= other.m_begin && other.m_end <= m_end;
    }

    size_t GetBegin() {
        return m_begin;
    }

    void SetEnd(size_t end) {
        m_end = end;
    }
};

class ErrorSuppressions {
 private:
    std::map<std::string, std::vector<LineRange>> m_suppressions;
    std::stack<LineRange*> m_open_block_suppressions;

 public:
    ErrorSuppressions() :
        m_suppressions({}), m_open_block_suppressions() {}

    void Clear() {
        m_suppressions.clear();
        m_open_block_suppressions = {};
    }

    LineRange* AddSuppression(const std::string& category, const LineRange& line_range) {
        std::vector<LineRange>& suppressed = m_suppressions[category];
        if (suppressed.empty() || !suppressed.back().ContainRange(line_range)) {
            suppressed.push_back(line_range);
            return &suppressed.back();
        }
        return nullptr;
    }

    void AddGlobalSuppression(const std::string& category) {
        AddSuppression(category, LineRange(0, SIZE_T_MAX));
    }

    void AddLineSuppression(const std::string& category, size_t linenum) {
        AddSuppression(category, LineRange(linenum, linenum));
    }

    void StartBlockSuppression(const std::string& category, size_t linenum) {
        LineRange* open_block = AddSuppression(category, LineRange(linenum, SIZE_T_MAX));
        m_open_block_suppressions.push(open_block);
    }

    void EndBlockSuppression(size_t linenum) {
        while (HasOpenBlock()) {
            LineRange* open_block = m_open_block_suppressions.top();
            if (open_block)
                open_block->SetEnd(linenum);
            m_open_block_suppressions.pop();
        }
    }

    size_t GetOpenBlockStart() {
        while (HasOpenBlock()) {
            LineRange* open_block = m_open_block_suppressions.top();
            if (open_block)
                return open_block->GetBegin();
            m_open_block_suppressions.pop();
        }
        return SIZE_T_NONE;
    }

    bool IsSuppressed(const std::string& category, size_t linenum) {
        // Check the non-category suppressions
        for (const LineRange& lr : m_suppressions[""]) {
            if (lr.Contain(linenum))
                return true;
        }

        // Check a category
        auto it = m_suppressions.find(category);
        if (it == m_suppressions.end())
            return false;
        const std::vector<LineRange>& suppressed = it->second;
        for (const LineRange& lr : suppressed) {
            if (lr.Contain(linenum))
                return true;
        }
        return false;
    }

    bool IsSuppressed(size_t linenum) {
        // Check the non-category suppressions
        for (const LineRange& lr : m_suppressions[""]) {
            if (lr.Contain(linenum))
                return true;
        }
        return false;
    }

    bool HasOpenBlock() { return !m_open_block_suppressions.empty(); }

    void AddDefaultCSuppressions() {
        static const std::vector<std::string> DEFAULT_C_SUPPRESSED_CATEGORIES = {
            "readability/casting",
        };
        for (const std::string& category : DEFAULT_C_SUPPRESSED_CATEGORIES)
            AddGlobalSuppression(category);
    }

    void AddDefaultKernelSuppressions() {
        static const std::vector<std::string> DEFAULT_KERNEL_SUPPRESSED_CATEGORIES = {
            "whitespace/tab",
        };
        for (const std::string& category : DEFAULT_KERNEL_SUPPRESSED_CATEGORIES)
            AddGlobalSuppression(category);
    }
};
