#define _HAS_STREAM_INSERTION_OPERATORS_DELETED_IN_CXX20 1
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "cpplint_state.h"
#include "file_linter.h"
#include "options.h"

class FileLinterTest : public ::testing::Test {
 protected:
    FileLinter linter;
    Options options;
    CppLintState cpplint_state;
    std::string filename;
    std::string filters;

    FileLinterTest() = default;
    ~FileLinterTest() override = default;

    void SetUp() override {
        filename = "";
        filters = "-legal/copyright,-whitespace/ending_newline,+build/include_alpha";
    }

    void TearDown() override {
        cpplint_state.FlushThreadStream();
    }

    void ProcessFile() {
        ResetFilters();
        linter.ProcessFile();
    }

    void ResetFilters() {
        options = Options();
        options.AddFilters(filters);
        cpplint_state.SetCountingStyle("detailed");
        cpplint_state.SetVerboseLevel(0);
        cpplint_state.ResetErrorCounts();
        fs::path file = filename;
        linter = FileLinter(file, &cpplint_state, options);
    }

    void ExpectErrorStr(const char* expected, const char* file, int linenum) {
        std::string error_str = cpplint_state.GetErrorStreamAsStr();
        EXPECT_STREQ(expected, error_str.c_str()) << "  line: " << file << "(" << linenum << ")";
    }
};

#define EXPECT_ERROR_STR(expected) ExpectErrorStr(expected, __FILE__, __LINE__)

TEST_F(FileLinterTest, NullBytes) {
    filename = "./tests/test_files/nullbytes.c";
    ProcessFile();
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("readability/nul"));
    const char* expected =
        "./tests/test_files/nullbytes.c:1:  "
        "Line contains NUL byte."
        "  [readability/nul] [5]\n"
        "./tests/test_files/nullbytes.c:2:  "
        "Line contains NUL byte."
        "  [readability/nul] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(FileLinterTest, InvalidUTF) {
    filename = "./tests/test_files/invalid_utf.c";
    ProcessFile();
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("readability/utf8"));
    const char* expected =
        "./tests/test_files/invalid_utf.c:1:  "
        "Line contains invalid UTF-8 (or Unicode replacement character)."
        "  [readability/utf8] [5]\n"
        "./tests/test_files/invalid_utf.c:2:  "
        "Line contains invalid UTF-8 (or Unicode replacement character)."
        "  [readability/utf8] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(FileLinterTest, Crlf) {
    filename = "./tests/test_files/crlf.c";
    ProcessFile();
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
    const char* expected =
        "./tests/test_files/crlf.c:1:  "
        "Unexpected \\r (^M) found; better to use only \\n"
        "  [whitespace/newline] [1]\n";
    EXPECT_ERROR_STR(expected);
}
