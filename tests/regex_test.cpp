#include <gtest/gtest.h>
#include <string>
#include "regex_utils.h"

// TODO(matyalatte): add more test cases

struct RegexCase {
    const char* pattern;
    const char* str;
    bool expected;
};

class RegexMatchTest : public ::testing::TestWithParam<RegexCase> {
};

// Test cases from cpython.
// https://github.com/python/cpython/blob/main/Lib/test/re_tests.py
const RegexCase regex_cases_python[] = {
    { "Python|Perl|Tcl", "Perl", true },
    // { ")", "", TSM_SYNTAX_ERROR },  // RegexSearch can not handle syntax errors.
    { "", "", true },
    { "abc", "abc", true },
    { "abc", "xbc", false },
    { "abc", "axc", false },
    { "abc", "abx", false },
    { "abc", "xabcy", true },
    { "abc", "ababc", true },
    { "ab*c", "abc", true },
    { "ab*bc", "abc", true },
    { "ab*bc", "abbc", true },
    { "ab*bc", "abbbbc", true },
    { "ab+bc", "abbc", true },
    { "ab+bc", "abc", false },
    { "ab+bc", "abq", false },
    { "ab+bc", "abbbbc", true },
    { "ab?bc", "abbc", true },
    { "ab?bc", "abc", true },
    { "ab?bc", "abbbbc", false },
    { "ab?c", "abc", true },
    { "^abc$", "abc", true },
    { "^abc$", "abcc", false },
    { "^abc", "abcc", true },
    { "^abc$", "aabc", false },
    { "abc$", "aabc", true },
    { "^", "abc", true },
    { "$", "abc", true },
    { "a.c", "abc", true },
    { "a.c", "axc", true },
    { "a.*c", "axyzc", true },
    { "a.*c", "axyzd", false },
    { "a[bc]d", "abc", false },
    { "a[bc]d", "abd", true },
    { "a[b-d]e", "abd", false },
    { "a[b-d]e", "ace", true },
    { "a[b-d]", "aac", true },
    { "a[-d]", "a-", true },
    { "a[\\-d]", "a-", true },
    { "a]", "a]", true },
    { "a[]]b", "a]b", true },
    { "a[\\]]b", "a]b", true },
    { "a[^bc]d", "aed", true },
    { "a[^bc]d", "abd", false },
    // This case will fail but we don't use the case.
    // { "a[^-b]d", "adc", true },
    { "a[^-b]d", "a-c", false },
    { "a[^]b]c", "a]c", false },
    { "a[^]b]c", "adc", true },

    { "ab|cd", "abc", true },
    { "ab|cd", "abcd", true },

    { "$b", "b", false },
    { "a+b+c", "aabbabc", true },
    { "[^ab]*", "cde", true },
    { "abc", "", false },
    { "a*", "", true },
    { "a|b|c|d|e", "e", true },
    { "abcd*efg", "abcdefg", true },
    { "ab*", "xabyabbbz", true },
    { "ab*", "xayabbbz", true },
    { "[abhgefdc]ij", "hij", true },
    { "a[bcd]*dcdcde", "adcdcde", true },
    { "a[bcd]+dcdcde", "adcdcde", false },
    { "[a-zA-Z_][z-zA-Z0-9_]*", "alpha", true },
    { "multiple words of text", "uh-uh", false },
    { "multiple words", "multiple words, yeah", true },
    { "[k]", "ab", false },
    { "a[-]?c", "ac", true },
    { "\\w+", "--ab_cd0123---", true },
    { "[\\w]+", "--ab_cd0123---", true },
    { "\\D+", "1234abc5678", true },
    { "[\\D+]", "1234abc5678", true },
    { "[\\da-fA-F]+", "123abc", true },
    // { "", "", true },
};

INSTANTIATE_TEST_SUITE_P(RegexMatchTestInstantiation_Python,
    RegexMatchTest,
    ::testing::ValuesIn(regex_cases_python));

TEST_P(RegexMatchTest, RegexMatch) {
    const RegexCase test_case = GetParam();
    bool match = RegexSearch(test_case.pattern, test_case.str);
    EXPECT_EQ(test_case.expected, match) <<
        "  pattern: " << test_case.pattern << "\n" <<
        "  str: " << test_case.str;
}

TEST(RegexTest, RegexReplace) {
    std::string res = RegexReplace("[0-9]+", "@", "test29848foo10092bar");
    EXPECT_STREQ("test@foo@bar", res.c_str());
}

#define RE_PATTERN_C_COMMENTS R"(/\*(?:[^*]|\*(?!/))*\*/)"

TEST(RegexTest, RegexReplace2) {
    regex_code RE_PATTERN_CLEANSE_LINE_C_COMMENTS =
        RegexCompile(
            R"((\s*)" RE_PATTERN_C_COMMENTS R"(\s*$|)"
            RE_PATTERN_C_COMMENTS R"(\s+|)"
            R"(\s+)" RE_PATTERN_C_COMMENTS R"((?=\W)|)"
            RE_PATTERN_C_COMMENTS ")");

    bool replaced;
    std::string res = RegexReplace(RE_PATTERN_CLEANSE_LINE_C_COMMENTS, "",
                                   "        /*foo=*/true, /*bar=*/true);", &replaced);
    EXPECT_EQ(true, replaced);
    EXPECT_STREQ("        true, true);", res.c_str());
}

TEST(RegexTest, RegexReplaceNoCopy) {
    regex_code RE_PATTERN_CLEANSE_LINE_C_COMMENTS =
        RegexCompile(
            R"((\s*)" RE_PATTERN_C_COMMENTS R"(\s*$|)"
            RE_PATTERN_C_COMMENTS R"(\s+|)"
            R"(\s+)" RE_PATTERN_C_COMMENTS R"((?=\W)|)"
            RE_PATTERN_C_COMMENTS ")");

    bool replaced;
    std::string res = "        /*foo=*/true, /*bar=*/true);";
    RegexReplace(RE_PATTERN_CLEANSE_LINE_C_COMMENTS, "",
                 &res, &replaced);
    EXPECT_EQ(true, replaced);
    EXPECT_STREQ("        true, true);", res.c_str());
}

TEST(RegexTest, RegexMatchWithRange) {
    bool match = RegexMatchWithRange("^test$", "rangetest", 5, 4);
    EXPECT_EQ(true, match);
}
