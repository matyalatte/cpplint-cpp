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

    virtual void SetUp() {
        filename = "test/test.cpp";
        ResetFilters("-legal/copyright,-whitespace/ending_newline,+build/include_alpha");
    }

    virtual void TearDown() {}

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
};


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
    // Complex multi-line /*...*/-style comment found.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/multiline_comment"));
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

    // Could not find end of multi-line comment
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/multiline_comment"));
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
    // No #ifndef header guard found
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
}

TEST_F(LinesLinterTest, HeaderGuardWrongIfndef) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_C_",
        "#define TEST_C_",
        "#endif  // TEST_H_",
    });
    // #ifndef header guard has wrong style
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
}

TEST_F(LinesLinterTest, HeaderGuardNoDefine) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#endif  // TEST_H_",
    });
    // No #ifndef header guard found
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
}

TEST_F(LinesLinterTest, HeaderGuardWrongDefine) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_C_",
        "#endif  // TEST_H_",
    });
    // No #ifndef header guard found
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
}

TEST_F(LinesLinterTest, HeaderGuardWrongEndif) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_H_",
        "#endif  // TEST_H__",
    });
    // #endif line should be "#endif  //...
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
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
    // #endif line should be "#endif  //...
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
}

TEST_F(LinesLinterTest, HeaderGuardNoEndifComment) {
    filename = "test.h";
    ProcessLines({
        "#ifndef TEST_H_",
        "#define TEST_H_",
        "#endif"
    });
    // #endif line should be "#endif  //...
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/header_guard"));
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

    // Lint failed to find start of function body
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/fn_size"));
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

    // Small and focused functions are preferred
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/fn_size"));
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
    // { should almost always be at the end of the previous line
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
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
    // { should almost always be at the end of the previous line
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
}

TEST_F(LinesLinterTest, BraceElseLineFeed) {
    ProcessLines({
        "void func() {",
        "    if (true)",
        "        int a = 0;",
        "    else if (true)"
        "        int a = 0;",
        "    else",
        "    {",
        "        int a = 0;",
        "    }",
        "}",
    });
    // { should almost always be at the end of the previous line
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
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
    // An else should appear on the same line as the preceding }
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
}

TEST_F(LinesLinterTest, BraceElseIfOneSide) {
    ProcessLines({
        "void func() {",
        "    if (true)"
        "        int a = 0;",
        "    } else if (true)",
        "        int a = 0;",
        "    else",
        "        int a = 0;",
        "}",
    });
    // If an else has a brace on one side
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
}

TEST_F(LinesLinterTest, BraceElseOneSide) {
    ProcessLines({
        "void func() {",
        "    if (true)"
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
}

TEST_F(LinesLinterTest, BraceIfControled) {
    ProcessLines({"if (test) { hello; }"});
    // Controlled statements inside brackets of if clause should be on a separate line
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
}

TEST_F(LinesLinterTest, BraceIfControledNoParen) {
    ProcessLines({
        "if (test) {",
        "    int a = 0;",
        "} else { hello; }",
    });
    // Controlled statements inside brackets of if clause should be on a separate line
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
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
}

TEST_F(LinesLinterTest, BraceIfMultiline2) {
    ProcessLines({
        "if (test)",
        "    int a = 0; int a = 0;",
    });
    // If/else bodies with multiple statements require braces
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
}

TEST_F(LinesLinterTest, BraceIfMultiline3) {
    ProcessLines({
        "if (test)",
        "    int a = 0;",
        "else",
        "    int a = 0;",
        "    int a = 0;",
    });
    // If/else bodies with multiple statements require braces
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
}

TEST_F(LinesLinterTest, BraceElseIndent) {
    ProcessLines({
        "if (test)",
        "    if (foo)",
        "        int a = 0;",
        "    else",
        "        int a = 0;",
    });
    // Else clause should be indented at the same level as if.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
}

TEST_F(LinesLinterTest, BraceElseIndent2) {
    ProcessLines({
        "if (test)",
        "    if (foo)",
        "        int a = 0;",
        "else",
        "    int a = 0;",
    });
    // Else clause should be indented at the same level as if.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
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
        "    hellow;",
        "};",
    });
    // You don't need a ; after a }
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("readability/braces"));
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
    // Empty conditional bodies should use {}
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/empty_conditional_body"));
}

TEST_F(LinesLinterTest, EmptyBlockLoop) {
    ProcessLines({
        "while (true);",
        "for (;;);",
    });
    // Empty loop bodies should use {} or continue
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/empty_loop_body"));
}

TEST_F(LinesLinterTest, EmptyBlockIf) {
    ProcessLines({
        "if (test,",
        "    func({})) {",
        "}",
    });
    // If statement had no body and no else clause
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/empty_if_body"));
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
    // At least two spaces is best between code and comments
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/comments"));
}

TEST_F(LinesLinterTest, CommentRightSpaces) {
    ProcessLines({"int a = 0;  //comment"});
    // Should have a space between // and comment
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/comments"));
}

TEST_F(LinesLinterTest, CommentTodoLeftSpaces) {
    ProcessLines({"int a = 0;  //  TODO(me): test"});
    // Too many spaces before TODO
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/todo"));
}

TEST_F(LinesLinterTest, CommentTodoName) {
    ProcessLines({"int a = 0;  // TODO: test"});
    // Missing username in TODO; it should look like
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/todo"));
}

TEST_F(LinesLinterTest, CommentTodoRightSpaces) {
    ProcessLines({"int a = 0;  // TODO(me):test"});
    // TODO(my_username) should be followed by a space
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/todo"));
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
    // Redundant blank line at the start of a code block should be deleted.
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/blank_line"));
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
    // Redundant blank line at the end of a code block should be deleted.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/blank_line"));
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
    // Do not leave a blank line after public:, private:, protected:
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/blank_line"));
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
    // Missing spaces around =
    EXPECT_EQ(8, cpplint_state.ErrorCount());
    EXPECT_EQ(8, cpplint_state.ErrorCount("whitespace/operators"));
}

TEST_F(LinesLinterTest, SpacingEqualsOperatorBoolFail) {
    ProcessLines({
        "bool result = a<=42",
        "bool result = a==42",
        "bool result = a!=42",
        "int a = b!=c",
    });
    // Missing spaces around =
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("whitespace/operators"));
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
    // Missing spaces around << or >>
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/operators"));
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
    // Missing spaces around < or >
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("whitespace/operators"));
}

TEST_F(LinesLinterTest, SpacingUnaryOperatorFail) {
    ProcessLines({
        "i ++;",
        "i --;",
        "! flag;",
        "~ flag;",
    });
    // Extra space for ++, --, !, ~
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("whitespace/operators"));
}

TEST_F(LinesLinterTest, SpacingParensNoSpaceBefore) {
    ProcessLines({
        "for(;;) {}",
        "if(true) return;",
        "while(true) continue;",
    });
    // Missing space before ( in for, if, while
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/parens"));
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
    // Missing space after ,
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/comma"));
}

TEST_F(LinesLinterTest, SpacingAfterSemicolon) {
    ProcessLines({
        "for (foo;bar;baz) {",
        "    func();a = b",
        "}",
    });
    // Missing space after ;
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/semicolon"));
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
    // Missing space before {
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/braces"));
}

TEST_F(LinesLinterTest, SpacingBeforeElse) {
    ProcessLines({
        "if (foo) {",
        "    func();",
        "}else {",
        "    func();",
        "}",
    });
    // Missing space before else
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/braces"));
}

TEST_F(LinesLinterTest, SpacingSemicolonEmptyState) {
    ProcessLines({"default:;"});
    // Semicolon defining empty statement.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/semicolon"));
}

TEST_F(LinesLinterTest, SpacingSemicolonOnly) {
    ProcessLines({"    ;"});
    // Line contains only semicolon.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/semicolon"));
}

TEST_F(LinesLinterTest, SpacingBeforeSemicolon) {
    ProcessLines({
        "func() ;",
        "while (true) ;",
    });
    // Line contains only semicolon.
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/semicolon"));
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
    // Extra space after ( in function call
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/parens"));
}

TEST_F(LinesLinterTest, SpacingAfterParens) {
    ProcessLines({
        "( a + b)",
        "void operator=(  ) { }",
    });
    // Extra space after (
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/parens"));
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
    // Extra space before ( in function call
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/parens"));
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
    // Closing ) should be moved to the previous line
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/parens"));
}

TEST_F(LinesLinterTest, SpacingFuncCallBeforeClosingParens) {
    ProcessLines({
        "Func(1, 3 );",
        "Func(1,",
        "     3 );",
    });
    // Extra space before )
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/parens"));
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
    // Consider using CHECK_EQ instead of CHECK(a == b)
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("readability/check"));
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
    // Consider using DCHECK_EQ instead of DCHECK(a == b)
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("readability/check"));
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
    // Consider using EXPECT_EQ instead of EXPECT_TRUE(a == b)
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("readability/check"));
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
    // Use operator || instead of or
    EXPECT_EQ(11, cpplint_state.ErrorCount());
    EXPECT_EQ(11, cpplint_state.ErrorCount("readability/alt_tokens"));
}

TEST_F(LinesLinterTest, AltTokensMultipleFails) {
    ProcessLines({
        "if (true or true and (not true)) return;",
    });
    // Use operator || instead of or
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("readability/alt_tokens"));
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

    // "private:" should be preceded by a blank line
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/blank_line"));
}

TEST_F(LinesLinterTest, IndentTab) {
    ProcessLines({"    \tfoo;"});
    // Tab found; better to use spaces
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/tab"));
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
    // Weird number of spaces at line-start.
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("whitespace/indent"));
}

TEST_F(LinesLinterTest, SpacingEndOfLine) {
    ProcessLines({
        "int foo; ",
        "// Hello there  ",
    });
    // Line ends in whitespace.
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("whitespace/end_of_line"));
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
    // More than one command on the same line
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("whitespace/newline"));
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
    // Include the directory when naming header files
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("build/include_subdir"));
}

TEST_F(LinesLinterTest, IncludeDuplication) {
    ProcessLines({
        "#include <string>",
        "#include <string>",
    });
    // "string" already included at test.cpp:1
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include"));
}

TEST_F(LinesLinterTest, IncludeOtherPackages) {
    ProcessLines({"#include \"other/package.c\""});
    // Do not include .c files from other packages
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include"));
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
    // Found C system header after C++ system header.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include_order"));
}

TEST_F(LinesLinterTest, IncludeOrderCppAfterOther) {
    ProcessLines({
        "#include <string.h>",
        "#include \"dir/foo.h\"",
        "#include <initializer_list>",
    });
    // Found C++ system header after other header.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include_order"));
}

TEST_F(LinesLinterTest, IncludeOrderCppAfterOtherWithMacro) {
    ProcessLines({
        "#include <string.h>",
        "#ifdef LANG_CXX11",
        "#include \"dir/foo.h\"",
        "#include <initializer_list>",
        "#endif  // LANG_CXX11",
    });
    // Found C++ system header after other header.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include_order"));
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
    // Include "foo/b.h" not in alphabetical order
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/include_alpha"));
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
        "uint16 a = (uint16)1.0;",
        "int32 a = (int32)1.0;",
        "uint64 a = (uint64)1.0;",
        "size_t a = (size_t)1.0;",
        "char *a = (char *) \"foo\";",
    });

    // Using C-style cast.
    EXPECT_EQ(8, cpplint_state.ErrorCount());
    EXPECT_EQ(8, cpplint_state.ErrorCount("readability/casting"));
}

TEST_F(LinesLinterTest, CastDeprecatedPass) {
    ProcessLines({
        "#include <set>",
        "int a = int();",
        "X::X() : a(int()) {}",
        "operator bool();",
        "new int64(123);",
        "new   int64(123);",
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
        "typedef set<int64, bool(*)(int64, int64)> SortedIdSet",
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
    // Using deprecated casting style.
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("readability/casting"));
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
    // Are you taking an address of a cast?
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/casting"));
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
    // Are you taking an address of something dereferenced from a cast?
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("readability/casting"));
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
        "std::string Foo::bar;",
        "::std::string foo;",
        "::std::string kFoo = \"hello\";  // English",
        "static ::std::string foo;",
        "::std::string Foo::bar;",
        "string foo(\"foobar\");"
    });
    // Static/global string variables are not permitted.
    EXPECT_EQ(13, cpplint_state.ErrorCount());
    EXPECT_EQ(13, cpplint_state.ErrorCount("runtime/string"));
}

TEST_F(LinesLinterTest, GlobalStringConst) {
    ProcessLines({
        "#include <string>",
        "static const string foo;",
        "static const std::string foo;",
        "static const ::std::string foo;",
    });
    // For a static/global string constant, use a C style
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/string"));
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
    // For a static/global string constant, use a C style
    // Static/global string variables are not permitted.
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/string"));
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
    // You seem to be initializing a member variable with itself.
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/init"));
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
    // If you can, use sizeof(fisk) instead of 1 as the 2nd arg to snprintf
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/printf"));
}

TEST_F(LinesLinterTest, PrintfSprintf) {
    ProcessLines({
        "#include <cstdio>",
        "i = sprintf(foo, bar, 3);"
    });
    // Never use sprintf. Use snprintf instead.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/printf"));
}

TEST_F(LinesLinterTest, PrintfStrcpyStrcat) {
    ProcessLines({
        "foo = strcpy(foo, bar);",
        "foo = strcat(foo, bar);",
    });
    // Almost always, snprintf is better than strcpy/strcat
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/printf"));
}

TEST_F(LinesLinterTest, IntPortPass) {
    ProcessLines({"unsigned short port;"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IntPortFail) {
    ProcessLines({"short port;"});
    // Use "unsigned short" for ports, not "short"
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, IntDeclarationPass) {
    ProcessLines({
        "long double b = 65.0;",
        "int64 a = 65;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, IntDeclarationFail) {
    ProcessLines({
        "long long aa = 6565;",
        "long a = 65;",
    });
    // Use int16/int64/etc, rather than the C type long
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
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
    // Unary operator& is dangerous.
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/operator"));
}

TEST_F(LinesLinterTest, BraceBeforeIf) {
    ProcessLines({
        "if (foo) {",
        "    int a;",
        "} if (foo) {",
        "    int a;",
        "}",
    });
    // Did you mean "else if"? If not, start a new line for "if".
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/braces"));
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
    // Potential format string bug. Do printf("%s", foo) instead.
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("runtime/printf"));
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
    // Did you mean "memset(buf, 0, sizeof(buf))"
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/memset"));
}

TEST_F(LinesLinterTest, NamespaceUsingPass) {
    ProcessLines({"using foo;"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NamespaceUsingFail) {
    ProcessLines({"using namespace foo;"});
    // Do not use namespace using-directives.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/namespaces"));
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
    // Do not use namespace using-directives.
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("build/namespaces_literals"));
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
    // Do not use variable-length arrays.
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("runtime/arrays"));
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
    // Do not use unnamed namespaces in header files.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/namespaces_headers"));
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
}

TEST_F(LinesLinterTest, NonConstReferenceFailMemberFunc) {
    ProcessLines({
        // Derived member functions are spared from override check
        "void Func(X& x);",
        "void Func(X& x) {}",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/references"));
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
    // %q in format strings is deprecated.
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("runtime/printf_format"));
}

TEST_F(LinesLinterTest, PrintFormatN) {
    ProcessLines({
        "#include <cstdio>",
        "snprintf(file, \"Never mix %d and %1$d parameters!\", value);"
    });
    // %N$ formats are unconventional.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/printf_format"));
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
    // %, [, (, and { are undefined character escapes.
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("build/printf_format"));
}

// NOLINTEND

TEST_F(LinesLinterTest, StorageClass) {
    ProcessLines({
        "const int static foo = 5;",
        "char static foo;",
        "double const static foo = 2.0;",
        "uint64 typedef unsigned_long_long;",
        "int register foo = 0;"
    });
    // Storage-class specifier (static, extern, typedef, etc) should be
    // at the beginning of the declaration
    EXPECT_EQ(5, cpplint_state.ErrorCount());
    EXPECT_EQ(5, cpplint_state.ErrorCount("build/storage_class"));
}

TEST_F(LinesLinterTest, EndifComment) {
    ProcessLines({
        "#if 0",
        "#endif Not a comment",
    });
    // Uncommented text after #endif is non-standard.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/endif_comment"));
}

TEST_F(LinesLinterTest, ForwardDecl) {
    ProcessLines({"class Foo::Goo;"});
    // Inner-style forward declarations are invalid.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/forward_decl"));
}

TEST_F(LinesLinterTest, OperatorDeprecated) {
    ProcessLines({
        "int a = a >? c;",
        "int a = a <? c;",
        "int a >?= b;",
        "int a <?= b;",
    });
    // >? and <? (max and min) operators are non-standard and deprecated.
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("build/deprecated"));
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
    // const string& members are dangerous.
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/member_string_references"));
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
        "    Foo (int f);",
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
    // Single-parameter constructors should be marked explicit.
    EXPECT_EQ(10, cpplint_state.ErrorCount("runtime/explicit"));
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
    // Constructors callable with one argument should be marked explicit.
    EXPECT_EQ(3, cpplint_state.ErrorCount("runtime/explicit"));
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
        "    VLOG(ERROR)",
        "    VLOG(INFO)",
        "    VLOG(WARNING)",
        "    VLOG(FATAL)",
        "    VLOG(DFATAL)",
    });
    EXPECT_EQ(10, cpplint_state.ErrorCount());
    EXPECT_EQ(10, cpplint_state.ErrorCount("runtime/vlog"));
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
}

TEST_F(LinesLinterTest, InvalidIncrementPass) {
    ProcessLines({"(*count)++;"});
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, InvalidIncrementFail) {
    ProcessLines({"*count++;"});
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/invalid_increment"));
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
    // For C++11-compatibility, omit template arguments from
    // make_pair OR use pair directly OR if appropriate,
    // construct a pair directly
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("build/explicit_make_pair"));
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
        "virtual void F(int a,",
        "               int b) override;",
        "virtual void F(int a,",
        "               int b) LOCKS_EXCLUDED(lock) override;",
        "virtual void F(int a,",
        "               int b)",
        "    LOCKS_EXCLUDED(lock) override;",
    });
    // virtual is redundant since function is already
    // declared as override/final
    EXPECT_EQ(11, cpplint_state.ErrorCount());
    EXPECT_EQ(11, cpplint_state.ErrorCount("readability/inheritance"));
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
    // override is redundant since function is already
    // declared as final
    EXPECT_EQ(6, cpplint_state.ErrorCount());
    EXPECT_EQ(6, cpplint_state.ErrorCount("readability/inheritance"));
}

TEST_F(LinesLinterTest, DeprecatedHeaderCpp11) {
    ProcessLines({
        "#include <fenv.h>",
        "",
        "#include <cfenv>",
        "#include <ratio>",
    });
    // <cfenv> is an unapproved C++11 header.
    EXPECT_EQ(3, cpplint_state.ErrorCount());
    EXPECT_EQ(3, cpplint_state.ErrorCount("build/c++11"));
}

TEST_F(LinesLinterTest, DeprecatedHeaderCpp17) {
    ProcessLines({"#include <filesystem>"});
    // <filesystem> is an unapproved C++17 header.
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("build/c++17"));
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
    // Add Add #include <...> for ...
    EXPECT_EQ(4, cpplint_state.ErrorCount());
    EXPECT_EQ(4, cpplint_state.ErrorCount("build/include_what_you_use"));
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
        "long a = (int64) 65;",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NolintBlockSuppressAllInBlock) {
    ProcessLines({
        "// NOLINTBEGIN",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
        "// NOLINTEND",
        "long a = (int64) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/casting"));
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, NolintNoEnd) {
    ProcessLines({
        "// NOLINTBEGIN(build/include)",
    });
    // NONLINT block never ended
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
}

TEST_F(LinesLinterTest, NolintNoBegin) {
    ProcessLines({"// NOLINTEND"});
    // Not in a NOLINT block
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
}

TEST_F(LinesLinterTest, NolintBlockDefined) {
    ProcessLines({
        "// NOLINTBEGIN(build/include)",
        "// NOLINTBEGIN(build/include)",
        "// NOLINTEND",
    });
    // NONLINT block already defined on line
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
}

TEST_F(LinesLinterTest, NolintEndWithCategory) {
    ProcessLines({
        "// NOLINTBEGIN(build/include)",
        "// NOLINTEND(build/include)",
    });
    // NOLINT categories not supported in block END
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
}

TEST_F(LinesLinterTest, NolintUnknownCategory) {
    ProcessLines({
        "// NOLINT(unknown/category)",
    });
    // Unknown NOLINT error category
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/nolint"));
}

TEST_F(LinesLinterTest, NolintLineSuppressAll) {
    ProcessLines({
        "long a = (int64) 65;  // NOLINT(*)",
    });
    EXPECT_EQ(0, cpplint_state.ErrorCount());
}

TEST_F(LinesLinterTest, NolintLineSuppressOneCategory) {
    ProcessLines({
        "long a = (int64) 65;  // NOLINT(runtime/int)",
    });
    EXPECT_EQ(1, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/casting"));
}

TEST_F(LinesLinterTest, NolintNextLine) {
    ProcessLines({
        "// NOLINTNEXTLINE",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/casting"));
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, LintCFile) {
    ProcessLines({
        // This surpress readability/casting
        "// LINT_C_FILE",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, VimMode) {
    ProcessLines({
        // This surpress readability/casting
        "// vim: sw=8 filetype=c ts=8",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, VimMode2) {
    ProcessLines({
        // This surpresses readability/casting
        "// vi: sw=8 filetype=c ts=8",
        "long a = (int64) 65;",
        "long a = (int64) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(2, cpplint_state.ErrorCount("runtime/int"));
}

TEST_F(LinesLinterTest, LintKernelFile) {
    ProcessLines({
        // This surpresses whitespace/tab
        "// LINT_KERNEL_FILE",
        "\t\tint a = 0;",
        "\t\tlong a = (int64) 65;",
    });
    EXPECT_EQ(2, cpplint_state.ErrorCount());
    EXPECT_EQ(1, cpplint_state.ErrorCount("readability/casting"));
    EXPECT_EQ(1, cpplint_state.ErrorCount("runtime/int"));
}
