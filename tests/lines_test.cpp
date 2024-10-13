#define _HAS_STREAM_INSERTION_OPERATORS_DELETED_IN_CXX20 1
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "cpplint_state.h"
#include "file_linter.h"
#include "options.h"

class LinesLinterTest : public ::testing::Test {
 protected:
    FileLinter linter;
    Options options;
    CppLintState cpplint_state;
    std::string filename;

    LinesLinterTest() = default;
    ~LinesLinterTest() override = default;

    void SetUp() override {
        filename = "test/test.cpp";
        ResetFilters("-legal/copyright,-whitespace/ending_newline,"
                     "+build/include_alpha,+readability/fn_size");
    }

    void TearDown() override {
        cpplint_state.FlushThreadStream();
    }

    void ProcessLines(std::vector<std::string> lines) {
        linter.CacheVariables(filename);
        lines.insert(lines.begin(), "// marker so line numbers and indices both start at 1");
        lines.emplace_back("// marker so line numbers end in a known way");
        linter.ProcessFileData(lines);
    }

    void ResetFilters(const std::string filters) {
        options = Options();
        options.AddFilters(filters);
        cpplint_state.SetCountingStyle("detailed");
        cpplint_state.SetVerboseLevel(0);
        cpplint_state.ResetErrorCounts();
        linter = FileLinter(filename, &cpplint_state, options);
    }

    void ExpectErrorStr(const char* expected, const char* file, int linenum) {
        std::string error_str = cpplint_state.GetErrorStreamAsStr();
        EXPECT_STREQ(expected, error_str.c_str()) << "  line: " << file << "(" << linenum << ")";
    }
};

#define EXPECT_ERROR_STR(expected) ExpectErrorStr(expected, __FILE__, __LINE__)

// TODO(matyalatte): add more test cases

TEST_F(LinesLinterTest, EmptyFile) {
    ProcessLines({""});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CopyrightPass) {
    ResetFilters("-whitespace/ending_newline");
    ProcessLines({"// Copyright (c) 2024 matyalatte"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CopywriteNoCopywrite) {
    ResetFilters("-whitespace/ending_newline");
    ProcessLines({""});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("legal/copyright"));
    const char* expected =
        "test/test.cpp:0:  "
        "No copyright message found.  "
        "You should have a line: \"Copyright [year] <Copyright Owner>\""
        "  [legal/copyright] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CopywriteAfterTenLines) {
    // CheckForCopyright should check only the first 10 lines.
    ResetFilters("-whitespace/ending_newline");
    ProcessLines({
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "// Copyright (c) 2024 matyalatte"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("legal/copyright"));
    const char* expected =
        "test/test.cpp:0:  "
        "No copyright message found.  "
        "You should have a line: \"Copyright [year] <Copyright Owner>\""
        "  [legal/copyright] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, MultiLineCommentPass) {
    ProcessLines({
        "",
        "/*",
        "",
        "",
        "*/",
        "",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, MultiLineCommentComplex) {
    ProcessLines({
        "",
        "int a = 0; /*",
        "",
        "*/",
        "",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/multiline_comment"));
    const char* expected =
        "test/test.cpp:2:  "
        "Complex multi-line /*...*/-style comment found. "
        "Lint may give bogus warnings.  "
        "Consider replacing these with //-style comments, "
        "with #if 0...#endif, "
        "or with more clearly structured multi-line comments."
        "  [readability/multiline_comment] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, RemoveMultiLineCommentsPass) {
    std::vector<std::string> lines = {
        "/* This should be removed",
        "",
        "*/",
    };
    linter.RemoveMultiLineComments(lines);
    for (const std::string& line : lines) {
        EXPECT_STREQ("/**/", line.c_str());
    }
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, RemoveMultiLineCommentsFailed) {
    std::vector<std::string> lines = {
        "/* This should be removed",
        "",
    };
    linter.RemoveMultiLineComments(lines);

    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/multiline_comment"));
    const char* expected =
        "test/test.cpp:1:  "
        "Could not find end of multi-line comment"
        "  [readability/multiline_comment] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NewLinePass) {
    ResetFilters("-legal/copyright");
    ProcessLines({"// There is a new line at EOF", ""});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NewLineFail) {
    ResetFilters("-legal/copyright");
    ProcessLines({"// There is no new lines at EOF"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/ending_newline"));
    const char* expected =
        "test/test.cpp:1:  "
        "Could not find a newline character at the end of the file."
        "  [whitespace/ending_newline] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, HeaderGuardIgnore) {
    filename = "test.h";
    ProcessLines({"// NOLINT(build/header_guard)"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, HeaderGuardPragma) {
    filename = "test.h";
    ProcessLines({"#pragma once"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, HeaderGuardPass) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_H_",
        "#endif  // TEST_H_",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, HeaderGuardNoIfndef) {
    filename = "test.h";
    ProcessLines({
        "#define TEST_H_",
        "#endif  // TEST_H_",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
    const char* expected =
        "test.h:0:  "
        "No #ifndef header guard found, suggested CPP variable is: TEST_H_"
        "  [build/header_guard] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, HeaderGuardWrongIfndef) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_C_",
        "#define TEST_C_",
        "#endif  // TEST_H_",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
    const char* expected =
        "test.h:1:  "
        "#ifndef header guard has wrong style, please use: TEST_H_"
        "  [build/header_guard] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, HeaderGuardNoDefine) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#endif  // TEST_H_",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
    const char* expected =
        "test.h:0:  "
        "No #ifndef header guard found, suggested CPP variable is: TEST_H_"
        "  [build/header_guard] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, HeaderGuardWrongDefine) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_C_",
        "#endif  // TEST_H_",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
    const char* expected =
        "test.h:0:  "
        "No #ifndef header guard found, suggested CPP variable is: TEST_H_"
        "  [build/header_guard] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, HeaderGuardWrongEndif) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_H_",
        "#endif  // TEST_H__",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
    const char* expected =
        "test.h:3:  "
        "#endif line should be \"#endif  // TEST_H_\""
        "  [build/header_guard] [0]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, HeaderGuardEndifAnotherStyle) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_H_",
        "#endif  /* TEST_H_ */",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, HeaderGuardWrongEndif2) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_H_",
        "#endif  /* TEST_H__ */",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
    const char* expected =
        "test.h:3:  "
        "#endif line should be \"#endif  /* TEST_H_ */\""
        "  [build/header_guard] [0]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, HeaderGuardNoEndifComment) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_H_",
        "#endif"
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
    const char* expected =
        "test.h:3:  "
        "#endif line should be \"#endif  // TEST_H_\""
        "  [build/header_guard] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NamespaceIndentationPass) {
    ProcessLines({
        "namespace test {",
        "int a = 0;",
        "void func {",
        "    int b = 0;",
        "}",
        "#define macro \\",
        "    do { \\",
        "        something(); \\",
        "    } while (0)",
        "    // comment"
        "}"
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NamespaceIndentationFial) {
    ProcessLines({
        "namespace test {",
        "    int a = 0;",
        "void func {",
        "    int b = 0;",
        "}",
        "#define macro \\",
        "    do { \\",
        "        something(); \\",
        "    } while (0)",
        "}"
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/indent_namespace"));
    const char* expected =
        "test/test.cpp:2:  "
        "Do not indent within a namespace."
        "  [whitespace/indent_namespace] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, FunctionLengthPass) {
    int func_length = 250;
    std::vector<std::string> lines = { "void func(",
                                       "    int a) {" };
    for (int i = 0; i < func_length - 1; i++) {
        lines.emplace_back("    int val = 0;");
    }
    lines.emplace_back("}");
    ProcessLines(lines);
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, FunctionLengthNoStart) {
    std::vector<std::string> lines = { "void func()" };
    ProcessLines(lines);

    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/fn_size"));
    const char* expected =
        "test/test.cpp:1:  "
        "Lint failed to find start of function body."
        "  [readability/fn_size] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, FunctionLengthTooLong) {
    int func_length = 251;
    std::vector<std::string> lines = { "void func(",
                                       "    int a) {" };
    for (int i = 0; i < func_length - 1; i++) {
        lines.emplace_back("    int val = 0;");
    }
    lines.emplace_back("}");
    ProcessLines(lines);

    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/fn_size"));
    const char* expected =
        "test/test.cpp:253:  "
        "Small and focused functions are preferred: "
        "func() has 251 non-comment lines (error triggered by exceeding 250 lines)."
        "  [readability/fn_size] [0]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, FunctionLengthTestFuncPass) {
    // When the function name starts with "Test" or "TEST",
    // Cpplint allows 400 lines as a function block.
    int func_length = 400;
    std::vector<std::string> lines = { "void TEST() {" };
    for (int i = 0; i < func_length; i++) {
        lines.emplace_back("    int val = 0;");
    }
    lines.emplace_back("}");
    ProcessLines(lines);
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, FunctionLengthTestFuncTooLong) {
    int func_length = 401;
    std::vector<std::string> lines = { "void TEST() {" };
    for (int i = 0; i < func_length; i++) {
        lines.emplace_back("    int val = 0;");
    }
    lines.emplace_back("}");
    ProcessLines(lines);

    // Small and focused functions are preferred
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/fn_size"));
    const char* expected =
        "test/test.cpp:403:  "
        "Small and focused functions are preferred: "
        "TEST() has 401 non-comment lines (error triggered by exceeding 400 lines)."
        "  [readability/fn_size] [0]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, MultilineStringPass) {
    ProcessLines({
        "char test[] = \"multiline\"",
        "\"test\";",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, MultilineStringFail) {
    ProcessLines({
        "char test[] = \"multiline",
        "test\";",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("readability/multiline_string"));
    const char* expected =
        "test/test.cpp:1:  "
        "Multi-line string (\"...\") found.  This lint script doesn\'t "
        "do well with such strings, and may give bogus warnings.  "
        "Use C++11 raw strings or concatenation instead."
        "  [readability/multiline_string] [5]\n"
        "test/test.cpp:2:  "
        "Multi-line string (\"...\") found.  This lint script doesn\'t "
        "do well with such strings, and may give bogus warnings.  "
        "Use C++11 raw strings or concatenation instead."
        "  [readability/multiline_string] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BracePass) {
    ProcessLines({
        "void func() {",
        "    if (true) {",
        "        int a = 0;",
        "    } else if (true) {",
        "        int a = 0;",
        "    } else {",
        "        int a = 0;",
        "    }",
        "}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, BraceIfLineFeed) {
    ProcessLines({
        "void func() {",
        "    if (true)",
        "    {",
        "        int a = 0;",
        "    } else if (true) {",
        "        int a = 0;",
        "    } else {",
        "        int a = 0;",
        "    }",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
    const char* expected =
        "test/test.cpp:3:  "
        "{ should almost always be at the end of the previous line"
        "  [whitespace/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceElseIfLineFeed) {
    ProcessLines({
        "void func() {",
        "    if (true)",
        "        int a = 0;",
        "    else if (true)",
        "    {",
        "        int a = 0;",
        "    } else {",
        "        int a = 0;",
        "    }",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
    const char* expected =
        "test/test.cpp:5:  "
        "{ should almost always be at the end of the previous line"
        "  [whitespace/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceElseLineFeed) {
    ProcessLines({
        "void func() {",
        "    if (true)",
        "        int a = 0;",
        "    else if (true)",
        "        int a = 0;",
        "    else",
        "    {",
        "        int a = 0;",
        "    }",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
    const char* expected =
        "test/test.cpp:7:  "
        "{ should almost always be at the end of the previous line"
        "  [whitespace/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}


TEST_F(LinesLinterTest, BraceElseIfLineFeed2) {
    ProcessLines({
        "void func() {",
        "    if (true) {",
        "        int a = 0;",
        "    }",
        "    else if (true)",
        "        int a = 0;",
        "    else",
        "        int a = 0;",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
    const char* expected =
        "test/test.cpp:5:  "
        "An else should appear on the same line as the preceding }"
        "  [whitespace/newline] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceElseIfOneSide) {
    ProcessLines({
        "void func() {",
        "    if (true)",
        "        int a = 0;",
        "    } else if (true)",
        "        int a = 0;",
        "    else",
        "        int a = 0;",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:4:  "
        "If an else has a brace on one side, it should have it on both"
        "  [readability/braces] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceElseOneSide) {
    ProcessLines({
        "void func() {",
        "    if (true)",
        "        int a = 0;",
        "    } else if (true) {",
        "        int a = 0;",
        "    } else",
        "        int a = 0;",
        "}",
    });
    // If an else has a brace on one side
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:6:  "
        "If an else has a brace on one side, it should have it on both"
        "  [readability/braces] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceIfControled) {
    ProcessLines({"if (test) { hello; }"});
    // Controlled statements inside brackets of if clause should be on a separate line
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
    const char* expected =
        "test/test.cpp:1:  "
        "Controlled statements inside brackets of if clause should be on a separate line"
        "  [whitespace/newline] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceIfControledNoParen) {
    ProcessLines({
        "if (test) {",
        "    int a = 0;",
        "} else { hello; }",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
    const char* expected =
        "test/test.cpp:3:  "
        "Controlled statements inside brackets of else clause should be on a separate line"
        "  [whitespace/newline] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceIfMultiline) {
    ProcessLines({
        "if (test)",
        "    int a = 0;",
        "    int a = 0;",
    });
    // If/else bodies with multiple statements require braces
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:1:  "
        "If/else bodies with multiple statements require braces"
        "  [readability/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceIfMultiline2) {
    ProcessLines({
        "if (test)",
        "    int a = 0; int a = 0;",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:1:  "
        "If/else bodies with multiple statements require braces"
        "  [readability/braces] [4]\n"
        "test/test.cpp:2:  "
        "More than one command on the same line"
        "  [whitespace/newline] [0]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceIfMultiline3) {
    ProcessLines({
        "if (test)",
        "    int a = 0;",
        "else",
        "    int a = 0;",
        "    int a = 0;",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:3:  "
        "If/else bodies with multiple statements require braces"
        "  [readability/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceElseIndent) {
    ProcessLines({
        "if (test)",
        "    if (foo)",
        "        int a = 0;",
        "    else",
        "        int a = 0;",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:1:  "
        "Else clause should be indented at the same level as if. "
        "Ambiguous nested if/else chains require braces."
        "  [readability/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceElseIndent2) {
    ProcessLines({
        "if (test)",
        "    if (foo)",
        "        int a = 0;",
        "else",
        "    int a = 0;",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:2:  "
        "Else clause should be indented at the same level as if. "
        "Ambiguous nested if/else chains require braces."
        "  [readability/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, TrailingSemicolonPass) {
    ProcessLines({
        "for (;;) {}",
        "func = []() {",
        "    func();",
        "};",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, TrailingSemicolonFail) {
    ProcessLines({
        "for (;;) {};",
        "while (foo) {};",
        "switch (foo) {};",
        "Function() {};",
        "if (foo) {",
        "    hello;",
        "};",
    });
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:1:  "
        "You don't need a ; after a }"
        "  [readability/braces] [4]\n"
        "test/test.cpp:2:  "
        "You don't need a ; after a }"
        "  [readability/braces] [4]\n"
        "test/test.cpp:3:  "
        "You don't need a ; after a }"
        "  [readability/braces] [4]\n"
        "test/test.cpp:4:  "
        "You don't need a ; after a }"
        "  [readability/braces] [4]\n"
        "test/test.cpp:7:  "
        "You don't need a ; after a }"
        "  [readability/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, EmptyBlockPass) {
    ProcessLines({
        "while (true)",
        "    func();",
        "while (true) continue;",
        "do {",
        "    func();",
        "} while (true);",
        "if (true) {",
        "    func();",
        "}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, EmptyBlockConditional) {
    ProcessLines({"if (true);"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/empty_conditional_body"));
    const char* expected =
        "test/test.cpp:1:  "
        "Empty conditional bodies should use {}"
        "  [whitespace/empty_conditional_body] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, EmptyBlockLoop) {
    ProcessLines({
        "while (true);",
        "for (;;);",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/empty_loop_body"));
    const char* expected =
        "test/test.cpp:1:  "
        "Empty loop bodies should use {} or continue"
        "  [whitespace/empty_loop_body] [5]\n"
        "test/test.cpp:2:  "
        "Empty loop bodies should use {} or continue"
        "  [whitespace/empty_loop_body] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, EmptyBlockIf) {
    ProcessLines({
        "if (test,",
        "    func({})) {",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/empty_if_body"));
    const char* expected =
        "test/test.cpp:2:  "
        "If statement had no body and no else clause"
        "  [whitespace/empty_if_body] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CommentPass) {
    ProcessLines({
        "int a = 0;  // comment",
        "// TODO(me): test it",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CommentLeftSpaces) {
    ProcessLines({"int a = 0; // comment"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/comments"));
    const char* expected =
        "test/test.cpp:1:  "
        "At least two spaces is best between code and comments"
        "  [whitespace/comments] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CommentRightSpaces) {
    ProcessLines({"int a = 0;  //comment"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/comments"));
    const char* expected =
        "test/test.cpp:1:  "
        "Should have a space between // and comment"
        "  [whitespace/comments] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CommentTodoLeftSpaces) {
    ProcessLines({"int a = 0;  //  TODO(me): test"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/todo"));
    const char* expected =
        "test/test.cpp:1:  "
        "Too many spaces before TODO"
        "  [whitespace/todo] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CommentTodoName) {
    ProcessLines({"int a = 0;  // TODO: test"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/todo"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing username in TODO; it should look like "
        "\"// TODO(my_username): Stuff.\""
        "  [readability/todo] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CommentTodoRightSpaces) {
    ProcessLines({"int a = 0;  // TODO(me):test"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/todo"));
    const char* expected =
        "test/test.cpp:1:  "
        "TODO(my_username) should be followed by a space"
        "  [whitespace/todo] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BlankLinePass) {
    ProcessLines({
        "namespace {",
        "",
        "}  // namespace",
        "extern \"C\" {",
        "",
        "void Func() {}",
        "",
        "}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, BlankLineBlockStart) {
    ProcessLines({
        "if (foo) {",
        "",
        "    func();",
        "} else if (bar) {",
        "",
        "    func();",
        "} else {",
        "",
        "    func();",
        "}",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/blank_line"));
    const char* expected =
        "test/test.cpp:2:  "
        "Redundant blank line at the start of a code block "
        "should be deleted."
        "  [whitespace/blank_line] [2]\n"
        "test/test.cpp:5:  "
        "Redundant blank line at the start of a code block "
        "should be deleted."
        "  [whitespace/blank_line] [2]\n"
        "test/test.cpp:8:  "
        "Redundant blank line at the start of a code block "
        "should be deleted."
        "  [whitespace/blank_line] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BlankLineBlockEnd) {
    ProcessLines({
        "if (foo) {",
        "    func();",
        "",
        "} else if (bar) {",
        "    func();",
        "",
        "} else {",
        "    func();",
        "",  // Only the else block should raises an error.
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/blank_line"));
    const char* expected =
        "test/test.cpp:9:  "
        "Redundant blank line at the end of a code block "
        "should be deleted."
        "  [whitespace/blank_line] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BlankLineAfterSection) {
    ProcessLines({
        "class A {",
        " public:",
        "",
        " private:",
        "",
        "    struct B {",
        "     protected:",
        "",
        "        int foo;",
        "    };",
        "};",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/blank_line"));
    const char* expected =
        "test/test.cpp:3:  "
        "Do not leave a blank line after \"public:\""
        "  [whitespace/blank_line] [3]\n"
        "test/test.cpp:5:  "
        "Do not leave a blank line after \"private:\""
        "  [whitespace/blank_line] [3]\n"
        "test/test.cpp:8:  "
        "Do not leave a blank line after \"protected:\""
        "  [whitespace/blank_line] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingBeforeBracketsPass) {
    ProcessLines({
        "int a[] = { 1, 2, 3 };",
        "auto [abc, def] = func();",
        "#define NODISCARD [[nodiscard]]",
        "void foo(int param [[maybe_unused]]);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingBeforeBracketsFail) {
    ProcessLines({"int a [] = { 1, 2, 3 };"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
    const char* expected =
        "test/test.cpp:1:  "
        "Extra space before ["
        "  [whitespace/braces] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingForRangePass) {
    ProcessLines({
        "for (int i : numbers) {}",
        "for (std::size_t i : numbers) {}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingForRangeFail) {
    ProcessLines({
        "for (int i: numbers) {}",
        "for (int i :numbers) {}",
        "for (int i:numbers) {}",
        "for (std::size_t i: numbers) {}",
    });
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("whitespace/forcolon"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing space around colon in range-based for loop"
        "  [whitespace/forcolon] [2]\n"
        "test/test.cpp:2:  "
        "Missing space around colon in range-based for loop"
        "  [whitespace/forcolon] [2]\n"
        "test/test.cpp:3:  "
        "Missing space around colon in range-based for loop"
        "  [whitespace/forcolon] [2]\n"
        "test/test.cpp:4:  "
        "Missing space around colon in range-based for loop"
        "  [whitespace/forcolon] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingEqualsOperatorPass) {
    ProcessLines({
        "int tmp = a;",
        "a&=42;",
        "a|=42;",
        "a^=42;",
        "a+=42;",
        "a*=42;",
        "a/=42;",
        "a%=42;",
        "a>>=42;",
        "a<<=42;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingEqualsOperatorPass2) {
    ProcessLines({"bool Foo::operator==(const Foo& a) const = default;"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingEqualsOperatorFail) {
    ProcessLines({
        "int tmp= a;",
        "int tmp =a;",
        "int tmp=a;",
        "int tmp= 7;",
        "int tmp =7;",
        "int tmp=7;",
        "int* tmp=*p",
        "int* tmp= *p",
    });
    EXPECT_EQ(8, cpplint_state.ErrorCount());
    EXPECT_EQ(8, cpplint_state.ErrorCount("whitespace/operators"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing spaces around ="
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:2:  "
        "Missing spaces around ="
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:3:  "
        "Missing spaces around ="
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:4:  "
        "Missing spaces around ="
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:5:  "
        "Missing spaces around ="
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:6:  "
        "Missing spaces around ="
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:7:  "
        "Missing spaces around ="
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:8:  "
        "Missing spaces around ="
        "  [whitespace/operators] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingEqualsOperatorBoolFail) {
    ProcessLines({
        "bool result = a<=42",
        "bool result = a==42",
        "bool result = a!=42",
        "int a = b!=c",
    });
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("whitespace/operators"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing spaces around <="
        "  [whitespace/operators] [3]\n"
        "test/test.cpp:2:  "
        "Missing spaces around =="
        "  [whitespace/operators] [3]\n"
        "test/test.cpp:3:  "
        "Missing spaces around !="
        "  [whitespace/operators] [3]\n"
        "test/test.cpp:4:  "
        "Missing spaces around !="
        "  [whitespace/operators] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingShiftOperatorPass) {
    ProcessLines({
        "1<<20",
        "1024>>10",
        "Kernel<<<1, 2>>>()",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingShiftOperatorFail) {
    ProcessLines({
        "a<<b",
        "a>>b",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/operators"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing spaces around <<"
        "  [whitespace/operators] [3]\n"
        "test/test.cpp:2:  "
        "Missing spaces around >>"
        "  [whitespace/operators] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingCompOperatorPass) {
    ProcessLines({
        "if (foo < bar) return;",
        "if (foo > bar) return;",
        "if (foo < bar->baz) return;",
        "if (foo > bar->baz) return;",
        "CHECK_LT(x < 42)",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingCompOperatorFail) {
    ProcessLines({
        "if (foo<bar) return;",
        "if (foo>bar) return;",
        "if (foo<bar->baz) return;",
        "if (foo>bar->baz) return;",
        "CHECK_LT(x<42)",
    });
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("whitespace/operators"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing spaces around <"
        "  [whitespace/operators] [3]\n"
        "test/test.cpp:2:  "
        "Missing spaces around >"
        "  [whitespace/operators] [3]\n"
        "test/test.cpp:3:  "
        "Missing spaces around <"
        "  [whitespace/operators] [3]\n"
        "test/test.cpp:4:  "
        "Missing spaces around >"
        "  [whitespace/operators] [3]\n"
        "test/test.cpp:5:  "
        "Missing spaces around <"
        "  [whitespace/operators] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingUnaryOperatorFail) {
    ProcessLines({
        "i ++;",
        "i --;",
        "! flag;",
        "~ flag;",
    });
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("whitespace/operators"));
    const char* expected =
        "test/test.cpp:1:  "
        "Extra space for operator  ++;"
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:2:  "
        "Extra space for operator  --;"
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:3:  "
        "Extra space for operator ! "
        "  [whitespace/operators] [4]\n"
        "test/test.cpp:4:  "
        "Extra space for operator ~ "
        "  [whitespace/operators] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingParensNoSpaceBefore) {
    ProcessLines({
        "for(;;) {}",
        "if(true) return;",
        "while(true) continue;",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/parens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing space before ( in for("
        "  [whitespace/parens] [5]\n"
        "test/test.cpp:2:  "
        "Missing space before ( in if("
        "  [whitespace/parens] [5]\n"
        "test/test.cpp:3:  "
        "Missing space before ( in while("
        "  [whitespace/parens] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingParensInsideSpace) {
    ProcessLines({
        "if (foo ) {",
        "    func();",
        "}",
        "switch ( foo) {",
        "    func();",
        "}",
        "for (foo; ba; bar ) {",
        "    func();",
        "}",
    });
    // Mismatching spaces inside ()
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/parens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Mismatching spaces inside () in if"
        "  [whitespace/parens] [5]\n"
        "test/test.cpp:4:  "
        "Mismatching spaces inside () in switch"
        "  [whitespace/parens] [5]\n"
        "test/test.cpp:7:  "
        "Mismatching spaces inside () in for"
        "  [whitespace/parens] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingParensInsideSpace2) {
    ProcessLines({
        "while (  foo  ) {",
        "    func();",
        "}",
    });
    // Should have zero or one spaces inside ( and ) in while
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/parens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Should have zero or one spaces inside ( and ) in while"
        "  [whitespace/parens] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingCommaPass) {
    ProcessLines({
        "a = f(1, 2);",
        "int tmp = a, a = b, b = tmp;",
        "f(a, /* name */ b);",
        "f(a, /* name */b);",
        "f(a, /* name */-1);",
        "f(a, /* name */\"1\");",
        "f(1, /* name */, 2);",
        "f(1,, 2);",
        "operator,()",
        "operator,(a, b)",
    });
    // Missing space after ,
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingCommaFail) {
    ProcessLines({
        "a = f(1,2);",
        "int tmp = a,a = b,b = tmp;",
        "operator,(a,b)",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/comma"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing space after ,"
        "  [whitespace/comma] [3]\n"
        "test/test.cpp:2:  "
        "Missing space after ,"
        "  [whitespace/comma] [3]\n"
        "test/test.cpp:3:  "
        "Missing space after ,"
        "  [whitespace/comma] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingAfterSemicolon) {
    ProcessLines({
        "for (foo;bar;baz) {",
        "    func();a = b",
        "}",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/semicolon"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing space after ;"
        "  [whitespace/semicolon] [3]\n"
        "test/test.cpp:2:  "
        "Missing space after ;"
        "  [whitespace/semicolon] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingBeforeBracePass) {
    ProcessLines({
        "if (foo) {",
        "    func();",
        "}",
        "for (foo; bar; baz) {",
        "    func();",
        "}",
        "int8_t{3}",
        "int16_t{3}",
        "int32_t{3}",
        "uint64_t{12345}",
        "constexpr int64_t kBatchGapMicros = int64_t{7} * 24;  // 1 wk.",
        "MoveOnly(int i1, int i2) : ip1{new int{i1}}, ip2{new int{i2}} {}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}


TEST_F(LinesLinterTest, SpacingBeforeBraceFail) {
    ProcessLines({
        "if (foo){",
        "    func();",
        "}",
        "for (foo; bar; baz){",
        "    func();",
        "}",
        "blah{32}",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/braces"));
    const char* expected =
        "test/test.cpp:1:  "
        "Missing space before {"
        "  [whitespace/braces] [5]\n"
        "test/test.cpp:4:  "
        "Missing space before {"
        "  [whitespace/braces] [5]\n"
        "test/test.cpp:7:  "
        "Missing space before {"
        "  [whitespace/braces] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingBeforeElse) {
    ProcessLines({
        "if (foo) {",
        "    func();",
        "}else {",
        "    func();",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
    const char* expected =
        "test/test.cpp:3:  "
        "Missing space before else"
        "  [whitespace/braces] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingSemicolonEmptyState) {
    ProcessLines({"default:;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/semicolon"));
    const char* expected =
        "test/test.cpp:1:  "
        "Semicolon defining empty statement. Use {} instead."
        "  [whitespace/semicolon] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingSemicolonOnly) {
    ProcessLines({"    ;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/semicolon"));
    const char* expected =
        "test/test.cpp:1:  "
        "Line contains only semicolon. If this should be an empty statement, "
        "use {} instead."
        "  [whitespace/semicolon] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingBeforeSemicolon) {
    ProcessLines({
        "func() ;",
        "while (true) ;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/semicolon"));
    const char* expected =
        "test/test.cpp:1:  "
        "Extra space before last semicolon. If this should be an empty "
        "statement, use {} instead."
        "  [whitespace/semicolon] [5]\n"
        "test/test.cpp:2:  "
        "Extra space before last semicolon. If this should be an empty "
        "statement, use {} instead."
        "  [whitespace/semicolon] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingFuncCallAfterParensPass) {
    ProcessLines({
        "foo(  // comment",
        "    bar);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingFuncCallAfterParensFail) {
    ProcessLines({
        "EXPECT_LT( 42 < x);",
        "foo( bar);",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/parens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Extra space after ( in function call"
        "  [whitespace/parens] [4]\n"
        "test/test.cpp:2:  "
        "Extra space after ( in function call"
        "  [whitespace/parens] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingAfterParens) {
    ProcessLines({
        "( a + b)",
        "void operator=(  ) { }",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/parens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Extra space after ("
        "  [whitespace/parens] [2]\n"
        "test/test.cpp:2:  "
        "Extra space after ("
        "  [whitespace/parens] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingFuncCallBeforeParensPass) {
    ProcessLines({
        "foo (Foo::*bar)(",
        ");",
        "foo (x::y::*z)(",
        ");",
        "foo (*bar)(",
        ");",
        "sizeof (foo);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}


TEST_F(LinesLinterTest, SpacingFuncCallBeforeParensFail) {
    ProcessLines({
        "foo (bar);",
        "foo (Foo::bar)(",
        ");",
        "__VA_OPT__ (,)",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/parens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Extra space before ( in function call"
        "  [whitespace/parens] [4]\n"
        "test/test.cpp:2:  "
        "Extra space before ( in function call"
        "  [whitespace/parens] [4]\n"
        "test/test.cpp:4:  "
        "Extra space before ( in function call"
        "  [whitespace/parens] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingFuncCallClosingParensPass) {
    ProcessLines({
        "Func(1, 3);",
        "Other(Nest(1),",
        "      Nest(3));",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingFuncCallClosingParens) {
    ProcessLines({
        "Func(1, 3",
        "     );",
        "Other(Nest(1),",
        "      Nest(3",
        "      ));",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/parens"));
    const char* expected =
        "test/test.cpp:2:  "
        "Closing ) should be moved to the previous line"
        "  [whitespace/parens] [2]\n"
        "test/test.cpp:5:  "
        "Closing ) should be moved to the previous line"
        "  [whitespace/parens] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingFuncCallBeforeClosingParens) {
    ProcessLines({
        "Func(1, 3 );",
        "Func(1,",
        "     3 );",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/parens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Extra space before )"
        "  [whitespace/parens] [2]\n"
        "test/test.cpp:3:  "
        "Extra space before )"
        "  [whitespace/parens] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CheckPass) {
    ProcessLines({
        "CHECK(x);",
        "CHECK(x < 42 && x > 36);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CheckCheck) {
    ProcessLines({
        "CHECK(x == 42);",
        "CHECK(x != 42);",
        "CHECK(x >= 42);",
        "CHECK(x > 42);",
        "CHECK(x <= 42);",
        "CHECK(x < 42);",
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("readability/check"));
    const char* expected =
        "test/test.cpp:1:  "
        "Consider using CHECK_EQ instead of CHECK(a == b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:2:  "
        "Consider using CHECK_NE instead of CHECK(a != b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:3:  "
        "Consider using CHECK_GE instead of CHECK(a >= b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:4:  "
        "Consider using CHECK_GT instead of CHECK(a > b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:5:  "
        "Consider using CHECK_LE instead of CHECK(a <= b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:6:  "
        "Consider using CHECK_LT instead of CHECK(a < b)"
        "  [readability/check] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CheckDcheck) {
    ProcessLines({
        "DCHECK(x == 42);",
        "DCHECK(x != 42);",
        "DCHECK(x >= 42);",
        "DCHECK(x > 42);",
        "DCHECK(x <= 42);",
        "DCHECK(x < 42);",
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("readability/check"));
    const char* expected =
        "test/test.cpp:1:  "
        "Consider using DCHECK_EQ instead of DCHECK(a == b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:2:  "
        "Consider using DCHECK_NE instead of DCHECK(a != b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:3:  "
        "Consider using DCHECK_GE instead of DCHECK(a >= b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:4:  "
        "Consider using DCHECK_GT instead of DCHECK(a > b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:5:  "
        "Consider using DCHECK_LE instead of DCHECK(a <= b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:6:  "
        "Consider using DCHECK_LT instead of DCHECK(a < b)"
        "  [readability/check] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CheckExpect) {
    ProcessLines({
        "EXPECT_TRUE(x == 42);",
        "EXPECT_TRUE(x != 42);",
        "EXPECT_TRUE(x >= 42);",
        "EXPECT_FALSE(x == 42);",
        "EXPECT_FALSE(x != 42);",
        "EXPECT_FALSE(x >= 42);",
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("readability/check"));
    const char* expected =
        "test/test.cpp:1:  "
        "Consider using EXPECT_EQ instead of EXPECT_TRUE(a == b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:2:  "
        "Consider using EXPECT_NE instead of EXPECT_TRUE(a != b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:3:  "
        "Consider using EXPECT_GE instead of EXPECT_TRUE(a >= b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:4:  "
        "Consider using EXPECT_NE instead of EXPECT_FALSE(a == b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:5:  "
        "Consider using EXPECT_EQ instead of EXPECT_FALSE(a != b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:6:  "
        "Consider using EXPECT_LT instead of EXPECT_FALSE(a >= b)"
        "  [readability/check] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CheckAssert) {
    ProcessLines({
        "ASSERT_TRUE(x == 42);",
        "ASSERT_TRUE(x != 42);",
        "ASSERT_TRUE(x >= 42);",
        "ASSERT_FALSE(x == 42);",
        "ASSERT_FALSE(x != 42);",
        "ASSERT_FALSE(x >= 42);",
        "ASSERT_TRUE(obj.func() == 42);",
    });
    // Consider using ASSERT_EQ instead of ASSERT_TRUE(a == b)
    EXPECT_EQ(7, cpplint_state.ErrorCount());
    EXPECT_EQ(7, cpplint_state.ErrorCount("readability/check"));
    const char* expected =
        "test/test.cpp:1:  "
        "Consider using ASSERT_EQ instead of ASSERT_TRUE(a == b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:2:  "
        "Consider using ASSERT_NE instead of ASSERT_TRUE(a != b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:3:  "
        "Consider using ASSERT_GE instead of ASSERT_TRUE(a >= b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:4:  "
        "Consider using ASSERT_NE instead of ASSERT_FALSE(a == b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:5:  "
        "Consider using ASSERT_EQ instead of ASSERT_FALSE(a != b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:6:  "
        "Consider using ASSERT_LT instead of ASSERT_FALSE(a >= b)"
        "  [readability/check] [2]\n"
        "test/test.cpp:7:  "
        "Consider using ASSERT_EQ instead of ASSERT_TRUE(a == b)"
        "  [readability/check] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, AltTokensPass) {
    ProcessLines({
        "#include \"base/false-and-false.h\"",
        "true nand true;",
        "true nor true;",
        "#error false or false",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, AltTokensFail) {
    ProcessLines({
        "true or true;",
        "true and true;",
        "if (not true) return;",
        "1 bitor 1;",
        "1 xor 1;",
        "x = compl 1;",
        "x and_eq y;",
        "x or_eq y;",
        "x xor_eq y;",
        "x not_eq y;",
        "if (true and(foo)) return;",
    });
    EXPECT_EQ(11, cpplint_state.ErrorCount());
    EXPECT_EQ(11, cpplint_state.ErrorCount("readability/alt_tokens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Use operator || instead of or"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:2:  "
        "Use operator && instead of and"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:3:  "
        "Use operator ! instead of not"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:4:  "
        "Use operator | instead of bitor"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:5:  "
        "Use operator ^ instead of xor"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:6:  "
        "Use operator ~ instead of compl"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:7:  "
        "Use operator &= instead of and_eq"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:8:  "
        "Use operator |= instead of or_eq"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:9:  "
        "Use operator ^= instead of xor_eq"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:10:  "
        "Use operator != instead of not_eq"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:11:  "
        "Use operator && instead of and"
        "  [readability/alt_tokens] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, AltTokensMultipleFails) {
    ProcessLines({
        "if (true or true and (not true)) return;",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("readability/alt_tokens"));
    const char* expected =
        "test/test.cpp:1:  "
        "Use operator || instead of or"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:1:  "
        "Use operator && instead of and"
        "  [readability/alt_tokens] [2]\n"
        "test/test.cpp:1:  "
        "Use operator ! instead of not"
        "  [readability/alt_tokens] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingSectionPass) {
    // No errors for small classes
    ProcessLines({
        "class Foo {",
        " public:",
        " protected:",
        " private:",
        "    struct B {",
        "     public:",
        "     private:",
        "    };",
        "};",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SpacingSectionFail) {
    std::vector<std::string> lines = {
        "class Foo {",
        " public:",
        " protected:",
        " private:",
        "    struct B {",
        "     public:",
        "     private:"
        "    };",
        "};",
    };
    for (int i = 0; i < 22; i++) {
        lines.insert(lines.begin() + 6, "        int a;");
    }
    ProcessLines(lines);

    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/blank_line"));
    const char* expected =
        "test/test.cpp:3:  "
        "\"protected:\" should be preceded by a blank line"
        "  [whitespace/blank_line] [3]\n"
        "test/test.cpp:4:  "
        "\"private:\" should be preceded by a blank line"
        "  [whitespace/blank_line] [3]\n"
        "test/test.cpp:29:  "
        "\"private:\" should be preceded by a blank line"
        "  [whitespace/blank_line] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IndentTab) {
    ProcessLines({"    \tfoo;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/tab"));
    const char* expected =
        "test/test.cpp:1:  "
        "Tab found; better to use spaces"
        "  [whitespace/tab] [1]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IndentOddPass) {
    ProcessLines({
        "  int two_space;",
        "    int four_space;",
        " public:",
        "   private:",
        " protected: \\",
        "    int a;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}


TEST_F(LinesLinterTest, IndentOddFail) {
    ProcessLines({
        " int one_space;",
        "   int three_space;",
        " char* one_space = \"public:\";",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/indent"));
    const char* expected =
        "test/test.cpp:1:  "
        "Weird number of spaces at line-start.  "
        "Are you using a 2-space indent?"
        "  [whitespace/indent] [3]\n"
        "test/test.cpp:2:  "
        "Weird number of spaces at line-start.  "
        "Are you using a 2-space indent?"
        "  [whitespace/indent] [3]\n"
        "test/test.cpp:3:  "
        "Weird number of spaces at line-start.  "
        "Are you using a 2-space indent?"
        "  [whitespace/indent] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SpacingEndOfLine) {
    ProcessLines({
        "int foo; ",
        "// Hello there  ",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/end_of_line"));
    const char* expected =
        "test/test.cpp:1:  "
        "Line ends in whitespace.  Consider deleting these extra spaces."
        "  [whitespace/end_of_line] [4]\n"
        "test/test.cpp:2:  "
        "Line ends in whitespace.  Consider deleting these extra spaces."
        "  [whitespace/end_of_line] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, LineLengthPass) {
    std::string normal_line = "// x  ";
    for (int i = 0; i < 37; i++) {
        normal_line += "xx";
    }

    // three-byte utf-8 characters
    std::string utf_3b_line = "// x  ";
    for (int i = 0; i < 37; i++) {
        utf_3b_line += "\xE3\x81\x82";  // あ
    }

    // four-byte utf-8 characters
    std::string utf_4b_line = "// x  ";
    for (int i = 0; i < 37; i++) {
        utf_4b_line += "\xF0\x9F\x98\x80";  // 😀
    }

    // combining characters
    std::string utf_cb_line = "// x  ";
    for (int i = 0; i < 37; i++) {
        utf_cb_line += "A\xCC\x80\x41\xCC\x80";  // ÀÀ
    }

    // Ignore ending Paths
    std::string path_line = "// //some/path/to/f";
    for (int i = 0; i < 50; i++) {
        path_line += "ile";
    }

    // Ignore ending URLs
    std::string url_line = "// Read http://g";
    for (int i = 0; i < 50; i++) {
        url_line += "oo";
    }
    url_line += "gle.com/";

    ProcessLines({
        normal_line,
        utf_3b_line,
        utf_4b_line,
        utf_cb_line,
        path_line,
        url_line
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, LineLengthFail) {
    std::string normal_line = "// x  ";
    for (int i = 0; i < 38; i++) {
        normal_line += "xx";
    }

    std::string utf_3b_line = "// x  ";
    for (int i = 0; i < 38; i++) {
        utf_3b_line += "\xE3\x81\x82";  // あ
    }

    std::string utf_4b_line = "// x  ";
    for (int i = 0; i < 38; i++) {
        utf_4b_line += "\xF0\x9F\x98\x80";  // 😀
    }

    std::string utf_cb_line = "// x  ";
    for (int i = 0; i < 38; i++) {
        utf_cb_line += "A\xCC\x80\x41\xCC\x80";  // ÀÀ
    }

    std::string path_line = "// //some/path/to/f";
    for (int i = 0; i < 50; i++) {
        path_line += "ile";
    }
    path_line += " and comment";

    std::string url_line = "// Read http://g";
    for (int i = 0; i < 50; i++) {
        url_line += "oo";
    }
    url_line += "gle.com/ and comment";

    ProcessLines({
        normal_line,
        utf_3b_line,
        utf_4b_line,
        utf_cb_line,
        path_line,
        url_line
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("whitespace/line_length"));
    std::string expected;
    for (int i = 1; i <= 6; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "Lines should be <= 80 characters long"
            "  [whitespace/line_length] [2]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, MultipleCommandsPass) {
    ProcessLines({
        "switch (x) {",
        "     case 0: func(); break;",
        "}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, MultipleCommandsFail) {
    ProcessLines({"int foo; int bar;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
    const char* expected =
        "test/test.cpp:1:  "
        "More than one command on the same line"
        "  [whitespace/newline] [0]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IncludeSubdirPass) {
    ProcessLines({
        "#include <string>",
        "#include \"baz.aa\"",
        "#include \"dir/foo.h\"",
        "#include \"lua.h\"",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IncludeSubdirFail) {
    ProcessLines({
        "#include \"bar.hh\"",
        "#include \"foo.h\"",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("build/include_subdir"));
    const char* expected =
        "test/test.cpp:1:  "
        "Include the directory when naming header files"
        "  [build/include_subdir] [4]\n"
        "test/test.cpp:2:  "
        "Include the directory when naming header files"
        "  [build/include_subdir] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IncludeDuplication) {
    ProcessLines({
        "#include <string>",
        "#include <string>",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include"));
    const char* expected =
        "test/test.cpp:2:  "
        "\"string\" already included at test/test.cpp:1"
        "  [build/include] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IncludeOtherPackages) {
    ProcessLines({"#include \"other/package.c\""});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include"));
    const char* expected =
        "test/test.cpp:1:  "
        "Do not include .c files from other packages"
        "  [build/include] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IncludeOrderPass) {
    ProcessLines({
        "#include \"test/test.h\"",
        "#include <stdio.h>",
        "#include <string>",
        "#include \"dir/foo.h\"",
        "",
        "#include \"bar/baz.hpp\"",
        "",
        "#include MACRO",
        "#include \"car/b.h\"",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IncludeOrderPassWithMacro) {
    ProcessLines({
        "#include <string.h>",
        "#include \"dir/foo.h\"",
        "#ifdef LANG_CXX11",
        "#include <initializer_list>",
        "#endif  // LANG_CXX11",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IncludeOrderCAfterCpp) {
    ProcessLines({
        "#include <string>",
        "#include <stdio.h>",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include_order"));
    const char* expected =
        "test/test.cpp:2:  "
        "Found C system header after C++ system header. "
        "Should be: test.h, c system, c++ system, other."
        "  [build/include_order] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IncludeOrderCppAfterOther) {
    ProcessLines({
        "#include <string.h>",
        "#include \"dir/foo.h\"",
        "#include <initializer_list>",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include_order"));
    const char* expected =
        "test/test.cpp:3:  "
        "Found C++ system header after other header. "
        "Should be: test.h, c system, c++ system, other."
        "  [build/include_order] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IncludeOrderCppAfterOtherWithMacro) {
    ProcessLines({
        "#include <string.h>",
        "#ifdef LANG_CXX11",
        "#include \"dir/foo.h\"",
        "#include <initializer_list>",
        "#endif  // LANG_CXX11",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include_order"));
    const char* expected =
        "test/test.cpp:4:  "
        "Found C++ system header after other header. "
        "Should be: test.h, c system, c++ system, other."
        "  [build/include_order] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IncludeAlphabetOrderPass) {
    ProcessLines({
        "#include <string>"
        "#include \"foo/b.h\"",
        "#include \"foo/c.h\"",
        "#include \"foo/e.h\"",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IncludeAlphabetOrderPassInl) {
    // Ignore -inl.h files
    ProcessLines({
        "#include \"foo/b-inl.h\"",
        "#include \"foo/b.h\"",
        "#include \"foo/e.h\"",
        "#include \"foo/e-inl.h\"",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IncludeAlphabetOrderFail) {
    ProcessLines({
        "#include \"foo/e.h\"",
        "#include \"foo/b.h\"",
        "#include \"foo/c.h\"",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include_alpha"));
    const char* expected =
        "test/test.cpp:2:  "
        "Include \"foo/b.h\" not in alphabetical order"
        "  [build/include_alpha] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CastCstylePass) {
    ProcessLines({
        "u a = (u)NULL;",
        "uint a = (uint)NULL;",
        "typedef MockCallback<int(int)> CallbackType;",
        "scoped_ptr< MockCallback<int(int)> > callback_value;",
        "std::function<int(bool)>",
        "x = sizeof(int)",
        "x = alignof(int)",
        "alignas(int) char x[42]",
        "alignas(alignof(x)) char y[42]",
        "void F(int (func)(int));",
        "void F(int (func)(int*));",
        "void F(int (Class::member)(int));",
        "void F(int (Class::member)(int*));",
        "void F(int (Class::member)(int), int param);",
        "void F(int (Class::member)(int*), int param);",
        "X Class::operator++(int)",
        "X Class::operator--(int)",
        "[](int/*unused*/) -> bool {",
        "}",
        "[](int /*unused*/) -> bool {",
        "}",
        "auto f = [](MyStruct* /*unused*/)->int {",
        "}",
        "[](int) -> bool {",
        "}",
        "auto f = [](MyStruct*)->int {",
        "}",
        "int64_t{4096} * 1000 * 1000",
        "size_t{4096} * 1000 * 1000",
        "uint_fast64_t{4096} * 1000 * 1000",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CastCstyleFail) {
    ProcessLines({
        "int a = (int)1.0;",
        "int a = (int)-1.0;",
        "int *a = (int *)NULL;",
        "uint16_t a = (uint16_t)1.0;",
        "int32_t a = (int32_t)1.0;",
        "uint64_t a = (uint64_t)1.0;",
        "size_t a = (size_t)1.0;",
        "char *a = (char *) \"foo\";",
    });

    EXPECT_EQ(8, cpplint_state.ErrorCount());
    EXPECT_EQ(8, cpplint_state.ErrorCount("readability/casting"));
    const char* expected =
        "test/test.cpp:1:  "
        "Using C-style cast.  Use static_cast<int>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:2:  "
        "Using C-style cast.  Use static_cast<int>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:3:  "
        "Using C-style cast.  Use reinterpret_cast<int *>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:4:  "
        "Using C-style cast.  Use static_cast<uint16_t>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:5:  "
        "Using C-style cast.  Use static_cast<int32_t>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:6:  "
        "Using C-style cast.  Use static_cast<uint64_t>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:7:  "
        "Using C-style cast.  Use static_cast<size_t>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:8:  "
        "Using C-style cast.  Use const_cast<char *>(...) instead"
        "  [readability/casting] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CastDeprecatedPass) {
    ProcessLines({
        "#include <set>",
        "int a = int();",
        "X::X() : a(int()) {}",
        "operator bool();",
        "new int64_t(123);",
        "new   int64_t(123);",
        "new const int(42);",
        "using a = bool(int arg);",
        "x = bit_cast<double(*)[3]>(y);",
        "void F(const char(&src)[N]);",
        // Placement new
        "new(field_ptr) int(field->default_value_enum()->number());",
        // C++11 function wrappers
        "std::function<int(bool)>",
        "std::function<const int(bool)>",
        "std::function< int(bool) >",
        "mfunction<int(bool)>",
        "typedef std::function<",
        "    bool(int)> F;",
        // Return types for function pointers
        "typedef bool(FunctionPointer)();",
        "typedef bool(FunctionPointer)(int param);",
        "typedef bool(MyClass::*MemberFunctionPointer)();",
        "typedef bool(MyClass::* MemberFunctionPointer)();",
        "typedef bool(MyClass::*MemberFunctionPointer)() const;",
        "void Function(bool(FunctionPointerArg)());",
        "void Function(bool(FunctionPointerArg)()) {}",
        "typedef set<int64_t, bool(*)(int64_t, int64_t)> SortedIdSet",
        "bool TraverseNode(T *Node, bool(VisitorBase:: *traverse) (T *t)) {}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CastDeprecatedFail) {
    ProcessLines({
        "int a = int(1.0);",
        "bool a = bool(1);",
        "double a = double(1);",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("readability/casting"));
    const char* expected =
        "test/test.cpp:1:  "
        "Using deprecated casting style.  "
        "Use static_cast<int>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:2:  "
        "Using deprecated casting style.  "
        "Use static_cast<bool>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:3:  "
        "Using deprecated casting style.  "
        "Use static_cast<double>(...) instead"
        "  [readability/casting] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CastMockIgnore) {
    ProcessLines({
        "#include <string>",
        "MOCK_METHOD0(method,",
        "             int());",
        "MOCK_CONST_METHOD1(method1,",
        "                   float(string));",
        "MOCK_CONST_METHOD2_T(method2, double(float, float));",
        "MOCK_CONST_METHOD1(method3, SomeType(int));",
        "MOCK_METHOD1(method4, int(bool));",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CastRuntimePass) {
    ProcessLines({
        "#include <string>"
        "BudgetBuckets&(BudgetWinHistory::*BucketFn)(void) const;",
        "&(*func_ptr)(arg)",
        "Compute(arg, &(*func_ptr)(i, j));",
        "int* x = reinterpret_cast<int *>(&foo);",
        "auto x = implicit_cast<string &(*)(int)>(&foo);"
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CastRuntimeFail) {
    ProcessLines({
        "int* x = &static_cast<int*>(foo);",
        "int* x = &reinterpret_cast<int *>(foo);",
        "int* x = &(int*)foo;",
    });
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/casting"));
    const char* expected =
        "test/test.cpp:1:  "
        "Are you taking an address of a cast?  "
        "This is dangerous: could be a temp var.  "
        "Take the address before doing the cast, rather than after"
        "  [runtime/casting] [4]\n"
        "test/test.cpp:2:  "
        "Are you taking an address of a cast?  "
        "This is dangerous: could be a temp var.  "
        "Take the address before doing the cast, rather than after"
        "  [runtime/casting] [4]\n"
        "test/test.cpp:3:  "
        "Using C-style cast.  Use reinterpret_cast<int*>(...) instead"
        "  [readability/casting] [4]\n"
        "test/test.cpp:3:  "
        "Are you taking an address of a cast?  "
        "This is dangerous: could be a temp var.  "
        "Take the address before doing the cast, rather than after"
        "  [runtime/casting] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, CastRuntimeAltPass) {
    ProcessLines({
        "int* x = &(down_cast<Obj*>(obj)->member_);",
        "int* x = &(down_cast<Obj*>(obj)[index]);",
        "int* x = &(down_cast<Obj*>(obj)\n->member_);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, CastRuntimeAltFail) {
    ProcessLines({
        "int* x = &down_cast<Obj*>(obj)->member_;",
        "int* x = &down_cast<Obj*>(obj)[index];",
        "int* x = &down_cast<Obj*>(obj)\n->member_;",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("readability/casting"));
    const char* expected =
        "test/test.cpp:1:  "
        "Are you taking an address of something dereferenced "
        "from a cast?  Wrapping the dereferenced expression in "
        "parentheses will make the binding more obvious"
        "  [readability/casting] [4]\n"
        "test/test.cpp:2:  "
        "Are you taking an address of something dereferenced "
        "from a cast?  Wrapping the dereferenced expression in "
        "parentheses will make the binding more obvious"
        "  [readability/casting] [4]\n"
        "test/test.cpp:3:  "
        "Are you taking an address of something dereferenced "
        "from a cast?  Wrapping the dereferenced expression in "
        "parentheses will make the binding more obvious"
        "  [readability/casting] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, GlobalStringPass) {
    ProcessLines({
        "#include <string>",
        "#include <vector>",
        "string* foo;",
        "string *foo;",
        "string* pointer = Func();",
        "string *pointer = Func();",
        "const string* foo;",
        "const string *foo;",
        "const string* pointer = Func();",
        "const string *pointer = Func();",
        "string const* foo;",
        "string const *foo;",
        "string const* pointer = Func();",
        "string const *pointer = Func();",
        "string* const foo;",
        "string *const foo;",
        "string* const pointer = Func();",
        "string *const pointer = Func();",
        "string Foo::bar() {}",
        "string Foo::operator*() {}",
        "    string foo;",
        "string Func() { return \"\"; }",
        "string Func () { return \"\"; }",
        "string const& FileInfo::Pathname() const;",
        "string const &FileInfo::Pathname() const;",
        "string VeryLongNameFunctionSometimesEndsWith(",
        "    VeryLongNameType very_long_name_variable) {}",
        "template<>",
        "string FunctionTemplateSpecialization<SomeType>(",
        "      int x) { return \"\"; }",
        "template<>",
        "string FunctionTemplateSpecialization<vector<A::B>* >(",
        "      int x) { return \"\"; }",
        "string Class<Type>::Method() const {",
        "  return \"\";",
        "}",
        "string Class<Type>::Method(",
        "    int arg) const {",
        "  return \"\";",
        "}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, GlobalString) {
    ProcessLines({
        "#include <string>",
        "string foo;",
        "string kFoo = \"hello\";  // English",
        "static string foo;",
        "string Foo::bar;",
        "std::string foo;",
        "std::string kFoo = \"hello\";  // English",
        "static std::string foo;",
    });
    EXPECT_EQ(7, cpplint_state.ErrorCount());
    EXPECT_EQ(7, cpplint_state.ErrorCount("runtime/string"));
    std::string expected;
    for (int i = 2; i <= 8; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "Static/global string variables are not permitted."
            "  [runtime/string] [4]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, GlobalString2) {
    ProcessLines({
        "#include <string>",
        "std::string Foo::bar;",
        "::std::string foo;",
        "::std::string kFoo = \"hello\";  // English",
        "static ::std::string foo;",
        "::std::string Foo::bar;",
        "string foo(\"foobar\");"
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("runtime/string"));
    std::string expected;
    for (int i = 2; i <= 7; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "Static/global string variables are not permitted."
            "  [runtime/string] [4]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, GlobalStringConst) {
    ProcessLines({
        "#include <string>",
        "static const string foo;",
        "static const std::string foo;",
        "static const ::std::string foo;",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/string"));
    const char* expected =
        "test/test.cpp:2:  "
        "For a static/global string constant, use a C style string instead: "
        "\"static const char foo[]\"."
        "  [runtime/string] [4]\n"
        "test/test.cpp:3:  "
        "For a static/global string constant, use a C style string instead: "
        "\"static const char foo[]\"."
        "  [runtime/string] [4]\n"
        "test/test.cpp:4:  "
        "For a static/global string constant, use a C style string instead: "
        "\"static const char foo[]\"."
        "  [runtime/string] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, GlobalStringMultiline) {
    ProcessLines({
        "#include <string>",
        "const string Class",
        "::static_member_variable1;",
        "const string Class::",
        "static_member_variable2;",
        "string Class::",
        "static_member_variable3;",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/string"));
    const char* expected =
        "test/test.cpp:2:  "
        "For a static/global string constant, use a C style string instead: "
        "\"const char Class::static_member_variable1[]\"."
        "  [runtime/string] [4]\n"
        "test/test.cpp:4:  "
        "For a static/global string constant, use a C style string instead: "
        "\"const char Class::static_member_variable2[]\"."
        "  [runtime/string] [4]\n"
        "test/test.cpp:6:  "
        "Static/global string variables are not permitted."
        "  [runtime/string] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, SelfinitPass) {
    ProcessLines({
        "Foo::Foo(Bar r, Bel l) : r_(r), l_(l) { }",
        "Foo::Foo(Bar r) : r_(r), l_(r_), ll_(l_) { }",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, SelfinitFail) {
    ProcessLines({
        "Foo::Foo(Bar r, Bel l) : r_(r_), l_(l_) { }",
        "Foo::Foo(Bar r, Bel l) : r_(CHECK_NOTNULL(r_)) { }",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/init"));
    const char* expected =
        "test/test.cpp:1:  "
        "You seem to be initializing a member variable with itself."
        "  [runtime/init] [4]\n"
        "test/test.cpp:2:  "
        "You seem to be initializing a member variable with itself."
        "  [runtime/init] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, PrintfSnprintfSizePass) {
    ProcessLines({
        "#include <cstdio>",
        "vsnprintf(NULL, 0, format);",
        "snprintf(fisk, sizeof(fisk), format);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, PrintfSnprintfSizeFail) {
    ProcessLines({
        "#include <cstdio>",
        "snprintf(fisk, 1, format);"
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/printf"));
    const char* expected =
        "test/test.cpp:2:  "
        "If you can, use sizeof(fisk) instead of 1 as the 2nd arg to snprintf."
        "  [runtime/printf] [3]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, PrintfSprintf) {
    ProcessLines({
        "#include <cstdio>",
        "i = sprintf(foo, bar, 3);"
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/printf"));
    const char* expected =
        "test/test.cpp:2:  "
        "Never use sprintf. Use snprintf instead."
        "  [runtime/printf] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, PrintfStrcpyStrcat) {
    ProcessLines({
        "foo = strcpy(foo, bar);",
        "foo = strcat(foo, bar);",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/printf"));
    const char* expected =
        "test/test.cpp:1:  "
        "Almost always, snprintf is better than strcpy"
        "  [runtime/printf] [4]\n"
        "test/test.cpp:2:  "
        "Almost always, snprintf is better than strcat"
        "  [runtime/printf] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IntPortPass) {
    ProcessLines({"unsigned short port;"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IntPortFail) {
    ProcessLines({"short port;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/int"));
    const char* expected =
        "test/test.cpp:1:  "
        "Use \"unsigned short\" for ports, not \"short\""
        "  [runtime/int] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IntDeclarationPass) {
    ProcessLines({
        "long double b = 65.0;",
        "int64_t a = 65;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IntDeclarationFail) {
    ProcessLines({
        "long long aa = 6565;",
        "long a = 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
    const char* expected =
        "test/test.cpp:1:  "
        "Use int16_t/int64_t/etc, rather than the C type long"
        "  [runtime/int] [4]\n"
        "test/test.cpp:2:  "
        "Use int16_t/int64_t/etc, rather than the C type long"
        "  [runtime/int] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, OperatorUnaryPass) {
    ProcessLines({
        "void operator=(const Myclass&);",
        "void operator&(int a, int b);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, OperatorUnaryFail) {
    ProcessLines({
        "void operator&() { }",
        "void operator & () { }",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/operator"));
    const char* expected =
        "test/test.cpp:1:  "
        "Unary operator& is dangerous.  Do not use it."
        "  [runtime/operator] [4]\n"
        "test/test.cpp:2:  "
        "Unary operator& is dangerous.  Do not use it."
        "  [runtime/operator] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, BraceBeforeIf) {
    ProcessLines({
        "if (foo) {",
        "    int a;",
        "} if (foo) {",
        "    int a;",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
    const char* expected =
        "test/test.cpp:3:  "
        "Did you mean \"else if\"? If not, start a new line for \"if\"."
        "  [readability/braces] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, PrintFormatPass) {
    ProcessLines({
        "#include <cstdio>",
        "printf(\"foo\");",
        "printf(\"foo: %s\", foo);",
        "DocidForPrintf(docid);",
        "printf(format, value);",
        "printf(__VA_ARGS__);",
        "printf(format.c_str(), value);",
        "printf(format(index).c_str(), value);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, PrintFormatFail) {
    ProcessLines({
        "#include <cstdio>",
        "printf(foo);",
        "printf(foo.c_str());",
        "printf(foo->c_str());",
        "StringPrintf(foo->c_str());",
    });
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("runtime/printf"));
    const char* expected =
        "test/test.cpp:2:  "
        "Potential format string bug. Do printf(\"%s\", foo) instead."
        "  [runtime/printf] [4]\n"
        "test/test.cpp:3:  "
        "Potential format string bug. Do printf(\"%s\", foo.c_str()) instead."
        "  [runtime/printf] [4]\n"
        "test/test.cpp:4:  "
        "Potential format string bug. Do printf(\"%s\", foo->c_str()) instead."
        "  [runtime/printf] [4]\n"
        "test/test.cpp:5:  "
        "Potential format string bug. Do StringPrintf(\"%s\", foo->c_str()) instead."
        "  [runtime/printf] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, MemsetPass) {
    ProcessLines({
        "  memset(buf, 0, sizeof(buf));",
        "  memset(buf, 0, xsize * ysize);",
        "  memset(buf, 'y', 0);",
        "  memset(buf, 4, 0);",
        "  memset(buf, -1, 0);",
        "  memset(buf, 0xF1, 0);",
        "  memset(buf, 0xcd, 0);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, MemsetFail) {
    ProcessLines({
        "  memset(buf, sizeof(buf), 0);",
        "  memset(buf, xsize * ysize, 0);",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/memset"));
    const char* expected =
        "test/test.cpp:1:  "
        "Did you mean \"memset(buf, 0, sizeof(buf))\"?"
        "  [runtime/memset] [4]\n"
        "test/test.cpp:2:  "
        "Did you mean \"memset(buf, 0, xsize * ysize)\"?"
        "  [runtime/memset] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NamespaceUsingPass) {
    ProcessLines({"using foo;"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NamespaceUsingFail) {
    ProcessLines({"using namespace foo;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/namespaces"));
    const char* expected =
        "test/test.cpp:1:  "
        "Do not use namespace using-directives.  "
        "Use using-declarations instead."
        "  [build/namespaces] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NamespaceUsingLiteralsPass) {
    ProcessLines({
        "using std::literals;",
        "using std::literals::chrono_literals;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NamespaceUsingLiteralsFail) {
    ProcessLines({
        "using namespace std::literals;",
        "using namespace std::literals::chrono_literals;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("build/namespaces_literals"));
    const char* expected =
        "test/test.cpp:1:  "
        "Do not use namespace using-directives.  "
        "Use using-declarations instead."
        "  [build/namespaces_literals] [5]\n"
        "test/test.cpp:2:  "
        "Do not use namespace using-directives.  "
        "Use using-declarations instead."
        "  [build/namespaces_literals] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, ArrayLengthPass) {
    ProcessLines({
        "int a[64];",
        "int a[0xFF];",
        "int first[256], second[256];",
        "int array_name[kCompileTimeConstant];",
        "char buf[somenamespace::kBufSize];",
        "int array_name[ALL_CAPS];",
        "AClass array1[foo::bar::ALL_CAPS];",
        "int a[kMaxStrLen + 1];",
        "int a[sizeof(foo)];",
        "int a[sizeof(*foo)];",
        "int a[sizeof foo];",
        "int a[sizeof(struct Foo)];",
        "int a[128 - sizeof(const bar)];",
        "int a[(sizeof(foo) * 4)];",
        "int a[(arraysize(fixed_size_array)/2) << 1];",
        "delete a[some_var];",
        "return a[some_var];",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, ArrayLengthFail) {
    ProcessLines({
        "int a[any_old_variable];",
        "int doublesize[some_var * 2];",
        "int a[afunction()];",
        "int a[function(kMaxFooBars)];",
        "bool a_list[items_->size()];",
        "namespace::Type buffer[len+1];",
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("runtime/arrays"));

    std::string expected;
    for (int i = 1; i <= 6; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "Do not use variable-length arrays.  Use an appropriately named "
            "('k' followed by CamelCase) compile-time constant for the size."
            "  [runtime/arrays] [1]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, NamespaceInHeaderPass) {
    ProcessLines({"namespace {}"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}


TEST_F(LinesLinterTest, NamespaceInHeaderFail) {
    filename = "test.h";
    ProcessLines({
        "#pragma once",
        "namespace {}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/namespaces_headers"));
    const char* expected =
        "test.h:2:  "
        "Do not use unnamed namespaces in header files.  See "
        "https://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Namespaces"
        " for more information."
        "  [build/namespaces_headers] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NonConstReferencePass) {
    ProcessLines({
        "#include <iostream>",
        "#include <string>",
        "#include <utility>",
        "#include <vector>",
        // Allow use of non-const references in a few specific cases
        "stream& operator>>(stream& s, Foo& f);",
        "stream& operator<<(stream& s, Foo& f);",
        "void swap(Bar& a, Bar& b);",
        "ostream& LogFunc(ostream& s);",
        "ostringstream& LogFunc(ostringstream& s);",
        "istream& LogFunc(istream& s);",
        "istringstream& LogFunc(istringstream& s);",
        // Returning a non-const reference from a function is OK.
        "int& g();",
        // Passing a const reference to a struct (using the struct keyword) is OK.
        "void foo(const struct tm& tm);",
        // Passing a const reference to a typename is OK.
        "void foo(const typename tm& tm);",
        // Const reference to a pointer type is OK.
        "void foo(const Bar* const& p) {",
        "}",
        "void foo(Bar const* const& p) {",
        "}",
        "void foo(Bar* const& p) {",
        "}",
        // Const reference to a templated type is OK.
        "void foo(const std::vector<std::string>& v);",
        // Returning an address of something is not prohibited.
        "return &something;",
        "if (condition) return &something;",
        "if (condition) address = &something;",
        "if (condition) result = lhs&rhs;",
        "if (condition) result = lhs & rhs;",
        "a = (b+c) * sizeof &f;",
        "a = MySize(b) * sizeof &f;",
        // We don't get confused by C++11 range-based for loops.
        "for (const string& s : c)",
        "    continue;",
        "for (auto& r : c)",
        "    continue;",
        "for (typename Type& a : b)",
        "    continue;",
        // We don't get confused by some other uses of '&'.
        "T& operator=(const T& t);",
        "int g() { return (a & b); }",
        "T& r = (T&)*(vp());",
        "T& r = v;",
        "static_assert((kBits & kMask) == 0, \"text\");",
        "COMPILE_ASSERT((kBits & kMask) == 0, text);",
        // Derived member functions are spared from override check
        "void Func(X& x) override;",
        "void Func(X& x) override {",
        "}",
        "void Func(X& x) const override;",
        "void Func(X& x) const override {",
        "}",
        // Don't warn on out-of-line method definitions.
        "void NS::Func(X& x) {",
        "}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NonConstReferencePass2) {
    // Other potential false positives.  These need full parser
    // state to reproduce as opposed to just TestLint.
    ProcessLines({
        "#include <string>",
        "#include <utility>",
        "void swap(int &x,",
        "          int &y) {",
        "}",
        "void swap(",
        "    sparsegroup<T, GROUP_SIZE, Alloc> &x,",
        "    sparsegroup<T, GROUP_SIZE, Alloc> &y) {",
        "}",
        "ostream& operator<<(",
        "    ostream& out",
        "    const dense_hash_set<Value, Hash, Equals, Alloc>& seq) {",
        "}",
        "class A {",
        "  void Function(",
        "      string &x) override {",
        "  }",
        "};",
        "void Derived::Function(",
        "    string &x) {",
        "}",
        "#define UNSUPPORTED_MASK(_mask) \\",
        "  if (flags & _mask) { \\",
        "    LOG(FATAL) << \"Unsupported flag: \" << #_mask; \\",
        "  }",
        "Constructor::Constructor()",
        "    : initializer1_(a1 & b1),",
        "      initializer2_(a2 & b2) {",
        "}",
        "Constructor::Constructor()",
        "    : initializer1_{a3 & b3},",
        "      initializer2_(a4 & b4) {",
        "}",
        "Constructor::Constructor()",
        "    : initializer1_{a5 & b5},",
        "      initializer2_(a6 & b6) {}",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NonConstReferencePassBadTemplate) {
    // A peculiar false positive due to bad template argument parsing
    ProcessLines({
        "inline RCULocked<X>::ReadPtr::ReadPtr(const RCULocked* rcu) {",
        "  DCHECK(!(data & kFlagMask)) << \"Error\";",
        "}",
        "",
        "RCULocked<X>::WritePtr::WritePtr(RCULocked* rcu)",
        "    : lock_(&rcu_->mutex_) {",
        "}"
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}


TEST_F(LinesLinterTest, NonConstReferenceFail) {
    ProcessLines({
        "bool operator>(Foo& s, Foo& f);",
        "bool operator+(Foo& s, Foo& f);",
        "int len(Foo& s);",
        // Non-const reference to a templated type is not OK.
        "void foo(std::vector<int>& p);",
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("runtime/references"));
    const char* expected =
        "test/test.cpp:1:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Foo& s"
        "  [runtime/references] [2]\n"
        "test/test.cpp:1:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Foo& f"
        "  [runtime/references] [2]\n"
        "test/test.cpp:2:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Foo& s"
        "  [runtime/references] [2]\n"
        "test/test.cpp:2:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Foo& f"
        "  [runtime/references] [2]\n"
        "test/test.cpp:3:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Foo& s"
        "  [runtime/references] [2]\n"
        "test/test.cpp:4:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: std::vector<int>& p"
        "  [runtime/references] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NonConstReferenceFailPointer) {
    ProcessLines({
        // Non-const reference to a pointer type is not OK.
        "void foo(Bar*& p);",
        "void foo(const Bar*& p);",
        "void foo(Bar const*& p);",
        "void foo(struct Bar*& p);",
        "void foo(const struct Bar*& p);",
        "void foo(struct Bar const*& p);",
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("runtime/references"));
    const char* expected =
        "test/test.cpp:1:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Bar*& p"
        "  [runtime/references] [2]\n"
        "test/test.cpp:2:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: const Bar*& p"
        "  [runtime/references] [2]\n"
        "test/test.cpp:3:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Bar const*& p"
        "  [runtime/references] [2]\n"
        "test/test.cpp:4:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: struct Bar*& p"
        "  [runtime/references] [2]\n"
        "test/test.cpp:5:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: const struct Bar*& p"
        "  [runtime/references] [2]\n"
        "test/test.cpp:6:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: struct Bar const*& p"
        "  [runtime/references] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NonConstReferenceFailMemberFunc) {
    ProcessLines({
        // Derived member functions are spared from override check
        "void Func(X& x);",
        "void Func(X& x) {}",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/references"));
    const char* expected =
        "test/test.cpp:1:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: X& x"
        "  [runtime/references] [2]\n"
        "test/test.cpp:2:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: X& x"
        "  [runtime/references] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NonConstReferenceFailTemplateArg) {
    ProcessLines({
        // Spaces before template arguments.  This is poor style, but
        // happens 0.15% of the time.
        "#include <vector>",
        "void Func(const vector <int> &const_x, "
        "vector <int> &nonconst_x) {",
        "}",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/references"));
    const char* expected =
        "test/test.cpp:2:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: vector<int> &nonconst_x"
        "  [runtime/references] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NonConstReferenceFailMultiline) {
    ProcessLines({
        "void Func(const Outer::",
        "              Inner& const_x,",
        "          const Outer",
        "              ::Inner& const_y,",
        "          const Outer<",
        "              int>::Inner& const_z,",
        "          Outer::",
        "              Inner& nonconst_x,",
        "          Outer",
        "              ::Inner& nonconst_y,",
        "          Outer<",
        "              int>::Inner& nonconst_z) {",
        "}",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/references"));
    const char* expected =
        "test/test.cpp:8:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Outer::Inner& nonconst_x"
        "  [runtime/references] [2]\n"
        "test/test.cpp:10:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Outer::Inner& nonconst_y"
        "  [runtime/references] [2]\n"
        "test/test.cpp:12:  "
        "Is this a non-const reference? "
        "If so, make const or use a pointer: Outer<int>::Inner& nonconst_z"
        "  [runtime/references] [2]\n";
    EXPECT_ERROR_STR(expected);
}

// NOLINTBEGIN(runtime/printf_format)

TEST_F(LinesLinterTest, PrintFormatQ) {
    ProcessLines({
        "#include <cstdio>",
        "fprintf(file, \"%q\", value);",
        "aprintf(file, \"The number is %12q\", value);",
        "printf(file, \"The number is\" \"%-12q\", value);",
        "printf(file, \"The number is\" \"%+12q\", value);",
        "printf(file, \"The number is\" \"% 12q\", value);",
    });
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("runtime/printf_format"));
    std::string expected;
    for (int i = 2; i <= 6; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "%q in format strings is deprecated.  Use %ll instead."
            "  [runtime/printf_format] [3]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, PrintFormatN) {
    ProcessLines({
        "#include <cstdio>",
        "snprintf(file, \"Never mix %d and %1$d parameters!\", value);"
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/printf_format"));
    const char* expected =
        "test/test.cpp:2:  "
        "%N$ formats are unconventional.  Try rewriting to avoid them."
        "  [runtime/printf_format] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, PrintFormatEscapePass) {
    ProcessLines({
        "#include <cstdio>",
        "printf(\"\\\\%%%d\", value);",
        "printf(R\"(\\[)\");",
        "printf(R\"(\\[%s)\", R\"(\\])\");",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, PrintFormatEscapeFail) {
    ProcessLines({
        "#include <cstdio>",
        "printf(\"\\%%d\", value);",
        "snprintf(buffer, sizeof(buffer), \"\\[%d\", value);",
        "fprintf(file, \"\\(%d\", value);",
        "vsnprintf(buffer, sizeof(buffer), \"\\\\\\{%d\", ap);",
    });
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("build/printf_format"));

    std::string expected;
    for (int i = 2; i <= 5; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "%, [, (, and { are undefined character escapes.  Unescape them."
            "  [build/printf_format] [3]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

// NOLINTEND

TEST_F(LinesLinterTest, StorageClass) {
    ProcessLines({
        "const int static foo = 5;",
        "char static foo;",
        "double const static foo = 2.0;",
        "uint64_t typedef unsigned_long_long;",
        "int register foo = 0;"
    });
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("build/storage_class"));

    std::string expected;
    for (int i = 1; i <= 5; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "Storage-class specifier (static, extern, typedef, etc) should be "
            "at the beginning of the declaration."
            "  [build/storage_class] [5]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, EndifComment) {
    ProcessLines({
        "#if 0",
        "#endif Not a comment",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/endif_comment"));
    const char* expected =
        "test/test.cpp:2:  "
        "Uncommented text after #endif is non-standard.  Use a comment."
        "  [build/endif_comment] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, ForwardDecl) {
    ProcessLines({"class Foo::Goo;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/forward_decl"));
    const char* expected =
        "test/test.cpp:1:  "
        "Inner-style forward declarations are invalid.  Remove this line."
        "  [build/forward_decl] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, OperatorDeprecated) {
    ProcessLines({
        "int a = a >? c;",
        "int a = a <? c;",
        "int a >?= b;",
        "int a <?= b;",
    });
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("build/deprecated"));

    std::string expected;
    for (int i = 1; i <= 4; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            ">? and <? (max and min) operators are non-standard and deprecated."
            "  [build/deprecated] [3]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, MemberStringReferencesPass) {
    std::vector<std::string> lines = {
        "#include <string>",
        "void f(const string&);",
        "const string& f(const string& a, const string& b);",
        "typedef const string& A;",
    };
    const char* const MEMBER_DECLS[] = {
        "const string& church",
        "const string &turing",
        "const string & godel"
    };
    for (const char* const decl : MEMBER_DECLS) {
        lines.push_back(std::string(decl) + " = b;");
        lines.push_back(std::string(decl) + "     =");
        lines.push_back("  b;");
    }
    ProcessLines({lines});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, MemberStringReferencesFail) {
    ProcessLines({
        "#include <string>",
        "const string& church;",
        "const string &turing;",
        "const string & godel;",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/member_string_references"));
    std::string expected;
    for (int i = 2; i <= 4; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "const string& members are dangerous. It is much better to use "
            "alternatives, such as pointers or simple constants."
            "  [runtime/member_string_references] [2]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, ExplicitSingleParamPass) {
    ProcessLines({
        "class Foo {",
        "    Foo();",
        "    explicit Foo(int f);",
        "    explicit Foo (int f);",
        "    explicit Foo(int f);  // simpler than Foo(blar, blar)",
        "    inline explicit Foo(int f);",
        "    explicit inline Foo(int f);",
        "    constexpr explicit Foo(int f);",
        "    explicit constexpr Foo(int f);",
        "    inline constexpr explicit Foo(int f);",
        "    explicit inline constexpr Foo(int f);",
        "};",
        "class Qualifier::AnotherOne::Foo {",
        "    explicit Foo(int f);",
        "};",
        "struct Foo {",
        "    explicit Foo(int f);",
        "};",
        "template<typename T> class Foo {",
        "    explicit Foo(int f);",
        "    explicit inline Foo(int f);",
        "    inline explicit Foo(int f);",
        "};",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount("runtime/explicit"));
}

TEST_F(LinesLinterTest, ExplicitSingleParamFail) {
    ProcessLines({
        "class Foo {",
        "    Foo(int f);",
        "    Foo(int f);  // simpler than Foo(blar, blar)",
        "    inline Foo(int f);",
        "    constexpr Foo(int f);",
        "    inline constexpr Foo(int f);",
        "};",
        "class Qualifier::AnotherOne::Foo {",
        "    Foo(int f);",
        "};",
        "struct Foo {",
        "    Foo(int f);",
        "};",
        "template<typename T> class Foo {",
        "    Foo(int f);",
        "    inline Foo(int f);",
        "};",
    });
    EXPECT_EQ(9, cpplint_state.ErrorCount());
    EXPECT_EQ(9, cpplint_state.ErrorCount("runtime/explicit"));
    std::string expected;
    for (int i : { 2, 3, 4, 5, 6, 9, 12, 15, 16 }) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "Single-parameter constructors should be marked explicit."
            "  [runtime/explicit] [4]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, ExplicitSingleParamFail2) {
    ProcessLines({
        "class Foo {",
        "    Foo (int f);",
        "};",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/explicit"));
    const char* expected =
        "test/test.cpp:2:  "
        "Extra space before ( in function call"
        "  [whitespace/parens] [4]\n"
        "test/test.cpp:2:  "
        "Single-parameter constructors should be marked explicit."
        "  [runtime/explicit] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, ExplicitSingleParamWithTemplatePass) {
    ProcessLines({
        "class Foo {",
        "    explicit Foo(A<B, C> d);",
        "};",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount("runtime/explicit"));
}

TEST_F(LinesLinterTest, ExplicitSingleParamWithTemplateFail) {
    ProcessLines({
        "class Foo {",
        "    Foo(A<B, C> d);",
        "};",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/explicit"));
    const char* expected =
        "test/test.cpp:2:  "
        "Single-parameter constructors should be marked explicit."
        "  [runtime/explicit] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, ExplicitCallableWithSingleParamPass) {
    ProcessLines({
        "class Foo {",
        "    explicit Foo(int f = 0);",
        "    explicit Foo(int f, int g = 0);",
        "    explicit Foo(int f = 0, int g = 0);",
        "};",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, ExplicitCallableWithSingleParamFail) {
    ProcessLines({
        "class Foo {",
        "    Foo(int f = 0);",
        "    Foo(int f, int g = 0);",
        "    Foo(int f = 0, int g = 0);",
        "};",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/explicit"));
    std::string expected;
    for (int i = 2; i <= 4; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "Constructors callable with one argument "
            "should be marked explicit."
            "  [runtime/explicit] [4]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, VlogPass) {
    ProcessLines({
        "VLOG(1)",
        "VLOG(99)",
        "LOG(ERROR)",
        "LOG(INFO)",
        "LOG(WARNING)",
        "LOG(FATAL)",
        "LOG(DFATAL)",
        "VLOG(SOMETHINGWEIRD)",
        "MYOWNVLOG(ERROR)",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, VlogFail) {
    ProcessLines({
        "VLOG(ERROR)",
        "VLOG(INFO)",
        "VLOG(WARNING)",
        "VLOG(FATAL)",
        "VLOG(DFATAL)",
    });
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("runtime/vlog"));
    std::string expected;
    for (int i = 1; i <= 5; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "VLOG() should be used with numeric verbosity level.  "
              "Use LOG() if you want symbolic severity levels."
            "  [runtime/vlog] [5]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, VlogWithIndentFail) {
    ProcessLines({
        "    VLOG(ERROR)",
        "    VLOG(INFO)",
        "    VLOG(WARNING)",
        "    VLOG(FATAL)",
        "    VLOG(DFATAL)",
    });
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("runtime/vlog"));
    std::string expected;
    for (int i = 1; i <= 5; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "VLOG() should be used with numeric verbosity level.  "
              "Use LOG() if you want symbolic severity levels."
            "  [runtime/vlog] [5]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, PosixThreadPass) {
    ProcessLines({
        "var = sctime_r()",
        "var = strtok_r()",
        "var = strtok_r(foo, ba, r)",
        "var = brand()",
        "_rand();",
        ".rand()",
        "->rand()",
        "ACMRandom rand(seed);",
        "ISAACRandom rand();",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, PosixThreadFail) {
    ProcessLines({
        "var = rand()",
        "var = strtok(str, delim)",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/threadsafe_fn"));
    const char* expected =
        "test/test.cpp:1:  "
        "Consider using rand_r(...) instead of rand(...)"
        " for improved thread safety."
        "  [runtime/threadsafe_fn] [2]\n"
        "test/test.cpp:2:  "
        "Consider using strtok_r(...) instead of strtok(...)"
        " for improved thread safety."
        "  [runtime/threadsafe_fn] [2]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, InvalidIncrementPass) {
    ProcessLines({"(*count)++;"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, InvalidIncrementFail) {
    ProcessLines({"*count++;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/invalid_increment"));
    const char* expected =
        "test/test.cpp:1:  "
        "Changing pointer instead of value (or unused value of operator*)."
        "  [runtime/invalid_increment] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, ExplicitMakePairPass) {
    ProcessLines({
        "make_pair",
        "make_pair(42, 42);",
        "my_make_pair<int, int>",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, ExplicitMakePairFail) {
    ProcessLines({
        "make_pair<int, int>",
        "make_pair <int, int>",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("build/explicit_make_pair"));
    const char* expected =
        "test/test.cpp:1:  "
        "For C++11-compatibility, omit template arguments from make_pair"
        " OR use pair directly OR if appropriate, construct a pair directly"
        "  [build/explicit_make_pair] [4]\n"
        "test/test.cpp:2:  "
        "For C++11-compatibility, omit template arguments from make_pair"
        " OR use pair directly OR if appropriate, construct a pair directly"
        "  [build/explicit_make_pair] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, RedundantVirtualPass) {
    ProcessLines({
        "virtual void F()",
        "virtual void F();",
        "virtual void F() {}",
        "struct A : virtual B {",
        "    ~A() override;",
        "};",
        "class C",
        "    : public D,",
        "      public virtual E {",
        "    void Func() override;",
        "};",
        "void Finalize(AnnotationProto *final) override;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, RedundantVirtualFail) {
    ProcessLines({
        "virtual int F() override",
        "    final;",
        "virtual int F() override;",
        "virtual int F() override {}",
        "virtual int F() final",
        "    override;",
        "virtual int F() final;",
        "virtual int F() final {}",
    });
    EXPECT_EQ(8, cpplint_state.ErrorCount());
    EXPECT_EQ(8, cpplint_state.ErrorCount("readability/inheritance"));
    const char* expected =
        "test/test.cpp:1:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"override\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:1:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"final\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:3:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"override\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:4:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"override\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:5:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"final\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:5:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"override\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:7:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"final\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:8:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"final\""
        "  [readability/inheritance] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, RedundantVirtualMultilineFail) {
    ProcessLines({
        "virtual void F(int a,",
        "               int b) override;",
        "virtual void F(int a,",
        "               int b) LOCKS_EXCLUDED(lock) override;",
        "virtual void F(int a,",
        "               int b)",
        "    LOCKS_EXCLUDED(lock) override;",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("readability/inheritance"));
    const char* expected =
        "test/test.cpp:1:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"override\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:3:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"override\""
        "  [readability/inheritance] [4]\n"
        "test/test.cpp:5:  "
        "\"virtual\" is redundant since function is "
        "already declared as \"override\""
        "  [readability/inheritance] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, RedundantOverrideFail) {
    ProcessLines({
        "int F() override final",
        "int F() override final;",
        "int F() override final {}",
        "int F() final override",
        "int F() final override;",
        "int F() final override {}",
    });
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("readability/inheritance"));
    std::string expected;
    for (int i = 1; i <= 6; i++) {
        expected +=
            "test/test.cpp:" + std::to_string(i) + ":  "
            "\"override\" is redundant since function is "
            "already declared as \"final\""
            "  [readability/inheritance] [4]\n";
    }
    EXPECT_ERROR_STR(expected.c_str());
}

TEST_F(LinesLinterTest, DeprecatedHeaderCpp11) {
    ProcessLines({
        "#include <fenv.h>",
        "",
        "#include <cfenv>",
        "#include <ratio>",
    });
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("build/c++11"));
    const char* expected =
        "test/test.cpp:1:  "
        "<fenv.h> is an unapproved C++11 header."
        "  [build/c++11] [5]\n"
        "test/test.cpp:3:  "
        "<cfenv> is an unapproved C++11 header."
        "  [build/c++11] [5]\n"
        "test/test.cpp:4:  "
        "<ratio> is an unapproved C++11 header."
        "  [build/c++11] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, DeprecatedHeaderCpp17) {
    ProcessLines({"#include <filesystem>"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/c++17"));
    const char* expected =
        "test/test.cpp:1:  "
        "<filesystem> is an unapproved C++17 header."
        "  [build/c++17] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, IncludeWhatYouUsePass) {
    ProcessLines({
        "DECLARE_string(foobar);",
        "DEFINE_string(foobar, \"\", \"\");",
        "base::hash_map<int, int> foobar;",
        "void a(const my::string &foobar);",
        "foo->swap(0, 1);",
        "foo.swap(0, 1);",
        // False positive for std::map
        "template <typename T>",
        "struct Foo {",
        "    T t;",
        "};",
        "template <typename T>",
        "Foo<T> map(T t) {",
        "    return Foo<T>{ t };",
        "},",
        "struct Bar {",
        "};",
        "auto res = map<Bar>();",
        // False positive for boost::container::set
        "boost::container::set<int> foo;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IncludeWhatYouUsePassIncluded) {
    ProcessLines({
        "#include <limits>",
        "#include <memory>",
        "#include <queue>",
        "#include <utility>",
        "void a(const std::priority_queue<int> &foobar);",
        "int i = numeric_limits<int>::max();",
        "std::unique_ptr<int> x;",
        "std::swap(a, b);",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IncludeWhatYouUseFail) {
    ProcessLines({
        "std::string* str;",
        "std::vector<int> vec;",
        "std::pair<int, int> pair;",
        "std::map<int, int> map;",
    });
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("build/include_what_you_use"));
    const char* expected =
        "test/test.cpp:4:  "
        "Add #include <map> for map<>"
        "  [build/include_what_you_use] [4]\n"
        "test/test.cpp:1:  "
        "Add #include <string> for string"
        "  [build/include_what_you_use] [4]\n"
        "test/test.cpp:3:  "
        "Add #include <utility> for pair<>"
        "  [build/include_what_you_use] [4]\n"
        "test/test.cpp:2:  "
        "Add #include <vector> for vector<>"
        "  [build/include_what_you_use] [4]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NolintBlockPass) {
    ProcessLines({
        "// NOLINTBEGIN(build/include)",
        "// NOLINTEND",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NolintBlockSuppressAll) {
    ProcessLines({
        "// NOLINTBEGIN",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NolintBlockSuppressAllInBlock) {
    ProcessLines({
        "// NOLINTBEGIN",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
        "// NOLINTEND",
        "long a = (int64_t) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/casting"));
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, NolintNoEnd) {
    ProcessLines({
        "// NOLINTBEGIN(build/include)",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
    const char* expected =
        "test/test.cpp:1:  "
        "NOLINT block never ended"
        "  [readability/nolint] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NolintNoBegin) {
    ProcessLines({"// NOLINTEND"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
    const char* expected =
        "test/test.cpp:1:  "
        "Not in a NOLINT block"
        "  [readability/nolint] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NolintBlockDefined) {
    ProcessLines({
        "// NOLINTBEGIN(build/include)",
        "// NOLINTBEGIN(build/include)",
        "// NOLINTEND",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
    const char* expected =
        "test/test.cpp:2:  "
        "NOLINT block already defined on line 1"
        "  [readability/nolint] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NolintEndWithCategory) {
    ProcessLines({
        "// NOLINTBEGIN(build/include)",
        "// NOLINTEND(build/include)",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
    const char* expected =
        "test/test.cpp:2:  "
        "NOLINT categories not supported in block END: build/include"
        "  [readability/nolint] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NolintUnknownCategory) {
    ProcessLines({
        "// NOLINT(unknown/category)",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
    const char* expected =
        "test/test.cpp:1:  "
        "Unknown NOLINT error category: unknown/category"
        "  [readability/nolint] [5]\n";
    EXPECT_ERROR_STR(expected);
}

TEST_F(LinesLinterTest, NolintLineSuppressAll) {
    ProcessLines({
        "long a = (int64_t) 65;  // NOLINT(*)",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NolintLineSuppressOneCategory) {
    ProcessLines({
        "long a = (int64_t) 65;  // NOLINT(runtime/int)",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/casting"));
}

TEST_F(LinesLinterTest, NolintNextLine) {
    ProcessLines({
        "// NOLINTNEXTLINE",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/casting"));
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, LintCFile) {
    ProcessLines({
        // This suppress readability/casting
        "// LINT_C_FILE",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, LintCFileMultiline) {
    ProcessLines({
        // This suppress readability/casting
        "/* LINT_C_FILE",
        "*/",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, VimMode) {
    ProcessLines({
        // This suppress readability/casting
        "// vim: sw=8 filetype=c ts=8",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, VimMode2) {
    ProcessLines({
        // This suppresses readability/casting
        "// vi: sw=8 filetype=c ts=8",
        "long a = (int64_t) 65;",
        "long a = (int64_t) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, LintKernelFile) {
    ProcessLines({
        // This suppresses whitespace/tab
        "// LINT_KERNEL_FILE",
        "\t\tint a = 0;",
        "\t\tlong a = (int64_t) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/casting"));
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/int"));
}
