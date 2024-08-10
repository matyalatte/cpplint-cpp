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

    virtual void SetUp() {
        filename = "";
        filters = "-legal/copyright,-whitespace/ending_newline,+build/include_alpha";
    }

    virtual void TearDown() {}

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
};

TEST_F(FileLinterTest, NullBytes) {
    filename = "./tests/test_files/nullbytes.c";
    ProcessFile();
    // Line contains NUL byte.
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("readability/nul"));
}

TEST_F(FileLinterTest, InvalidUTF) {
    filename = "./tests/test_files/invalid_utf.c";
    ProcessFile();
    // Line contains invalid UTF-8
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("readability/utf8"));
}

TEST_F(FileLinterTest, Crlf) {
    filename = "./tests/test_files/crlf.c";
    ProcessFile();
    // Unexpected \r (^M) found
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
}
