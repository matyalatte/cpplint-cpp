#include <gtest/gtest.h>
#include <string>
#include "glob_match.h"

struct GlobCase {
    const char* pattern;
    const std::string str;
    bool expected;
    // expected value when parent matching is enabled.
    bool expected_parent;
};

class GlobMatchTest : public ::testing::TestWithParam<GlobCase> {
};

const GlobCase glob_cases[] = {
    // literal
    { "/foo/bar.h", "/foo/bar.h", true, true },
    { "/foo/bar.h", "/foo/bar-h", false, false },
    // any characters
    { "/foo/*h", "/foo/bar-h", true, true },
    { "/foo/bar/*h", "/foo/bar-h", false, false },
    { "/foo/bar/*h", "/foo/bar/test.h", true, true },
    { "/*/test.h", "/foo/test.h", true, true },
    { "/*/test.h", "/foo/bar/test.h", false, false },
    { "foo/*", "foo/test.h", true, true },
    { "foo/*", "foo/bar/test.h", false, true },
    // recursive
    { "/**/test.h", "/foo/test.h", true, true },
    { "/**/test.h", "/foo/bar/test.h", true, true },
    { "/**bar/test.h", "/foo/bar/test.h", false, false },
    { "**/test.h", "/foo/test.h", true, true },
    { "**/test.h", "/foo/bar/test.h", true, true },
    { "**/test.h", "test.h", true, true },
    { "**bar/test.h", "/foo/bar/test.h", false, false },
    { "foo/**", "foo/test.h", true, true },
    { "foo/**", "foo/bar/test.h", true, true },
    // any single character
    { "/foo/bar?h", "/foo/bar.h", true, true },
    { "/foo/bar?h", "/foo/bar..h", false, false },
    { "/foo/bar?h", "/foo/bar/h", false, false },
    // list
    { "/foo/[abc].h", "/foo/b.h", true, true },
    { "/foo/[abc].h", "/foo/d.h", false, false },
    // negative list
    { "/foo/[!abc].h", "/foo/b.h", false, false },
    { "/foo/[!abc].h", "/foo/d.h", true, true },
    { "/foo/[!abc].h", "/foo//.h", false, false },
    // range
    { "/foo/[a-c].h", "/foo/b.h", true, true },
    { "/foo/[a-c].h", "/foo/d.h", false, false },
    // negative range
    { "/foo/[!a-c].h", "/foo/b.h", false, false },
    { "/foo/[!a-c].h", "/foo/d.h", true, true },
    { "/foo/[!a-c].h", "/foo//.h", false, false },
    // compare with parent matching
    { "/foo/bar", "/foo/bar/baz", false, true },
    { "/foo/*", "/foo/bar/baz", false, true },
    { "/foo/*/", "/foo/bar/baz", false, true },
    { "/foo/**/test", "/foo/bar/baz/test/a.cpp", false, true },
    // windows paths
    { "C:/foo/bar.h", "C:\\foo\\bar.h", true, true },
    { "C:/foo/bar", "C:\\foo\\bar\\baz", false, true },
    { "C:\\foo\\bar", "C:/foo/bar/baz", false, true },
};

INSTANTIATE_TEST_SUITE_P(GlobMatchTestInstantiation,
    GlobMatchTest,
    ::testing::ValuesIn(glob_cases));

TEST_P(GlobMatchTest, GlobMatch) {
    const GlobCase test_case = GetParam();
    GlobPattern glob(test_case.pattern);
    bool match = glob.Match(test_case.str);
    EXPECT_EQ(test_case.expected, match) <<
        "  pattern: " << test_case.pattern << "\n" <<
        "  str: " << test_case.str;
}

TEST_P(GlobMatchTest, GlobMatchParentMatching) {
    const GlobCase test_case = GetParam();
    // enable parent matching
    GlobPattern glob(test_case.pattern, true);
    bool match = glob.Match(test_case.str);
    EXPECT_EQ(test_case.expected_parent, match) <<
        "  pattern: " << test_case.pattern << "\n" <<
        "  str: " << test_case.str;
}
