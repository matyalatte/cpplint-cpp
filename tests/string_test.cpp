#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "string_utils.h"

// TODO(matyalatte): add more test cases

TEST(StringTest, StrBeforeChar) {
    std::string res = StrBeforeChar("test@foo@bar", '@');
    EXPECT_STREQ("test", res.c_str());
}

TEST(StringTest, StrBeforeCharNostrip) {
    std::string res = StrBeforeChar("test", '@');
    EXPECT_STREQ("test", res.c_str());
}

TEST(StringTest, StrAfterChar) {
    std::string res = StrAfterChar("test@foo@bar", '@');
    EXPECT_STREQ("foo@bar", res.c_str());
}

TEST(StringTest, StrAfterCharNostrip) {
    std::string res = StrAfterChar("test", '@');
    EXPECT_STREQ("", res.c_str());
}

TEST(StringTest, StrStrip) {
    std::string res = StrStrip("   test   ");
    EXPECT_STREQ("test", res.c_str());
}

TEST(StringTest, StrStripEmpty) {
    std::string res = StrStrip("   ");
    EXPECT_STREQ("", res.c_str());
}

TEST(StringTest, StrStripNostrip) {
    std::string res = StrStrip("a");
    EXPECT_STREQ("a", res.c_str());
}

TEST(StringTest, StrStripWithChar) {
    std::string res = StrStrip("@@@test@@@", '@');
    EXPECT_STREQ("test", res.c_str());
}

TEST(StringTest, StrLstrip) {
    std::string res = StrLstrip("   test   ");
    EXPECT_STREQ("test   ", res.c_str());
}

TEST(StringTest, StrLstripEmpty) {
    std::string res = StrLstrip("   ");
    EXPECT_STREQ("", res.c_str());
}

TEST(StringTest, StrLstripNostrip) {
    std::string res = StrLstrip("a");
    EXPECT_STREQ("a", res.c_str());
}

TEST(StringTest, StrLstripWithChar) {
    std::string res = StrLstrip("@@@test@@@", '@');
    EXPECT_STREQ("test@@@", res.c_str());
}

TEST(StringTest, StrLstripEmptyWithChar) {
    std::string res = StrLstrip("@@@", '@');
    EXPECT_STREQ("", res.c_str());
}

TEST(StringTest, StrRstrip) {
    std::string res = StrRstrip("   test   ");
    EXPECT_STREQ("   test", res.c_str());
}

TEST(StringTest, StrRstripEmpty) {
    std::string res = StrRstrip("   ");
    EXPECT_STREQ("", res.c_str());
}

TEST(StringTest, StrRstripNostrip) {
    std::string res = StrRstrip("a");
    EXPECT_STREQ("a", res.c_str());
}

TEST(StringTest, StrRstripWithChar) {
    std::string res = StrRstrip("@@@test@@@", '@');
    EXPECT_STREQ("@@@test", res.c_str());
}

TEST(StringTest, StrRstripEmptyWithChar) {
    std::string res = StrRstrip("@@@", '@');
    EXPECT_STREQ("", res.c_str());
}

TEST(StringTest, StrSplit) {
    const std::vector<std::string> expected =
        { "test", "foo", "bar" };
    std::vector<std::string> res = StrSplit("test foo bar");
    EXPECT_EQ(3, res.size());
    for (size_t i = 0; i < res.size(); i++) {
        EXPECT_STREQ(expected[i].c_str(), res[i].c_str());
    }
}

TEST(StringTest, StrSplitTwo) {
    const std::vector<std::string> expected =
        { "test", "foo" };
    std::vector<std::string> res = StrSplit("test foo bar", 2);
    EXPECT_EQ(2, res.size());
    for (size_t i = 0; i < res.size(); i++) {
        EXPECT_STREQ(expected[i].c_str(), res[i].c_str());
    }
}

TEST(StringTest, StrSplitBy) {
    const std::vector<std::string> expected =
        { "test", "foo", "bar" };
    std::vector<std::string> res = StrSplitBy("testbyfoobybar", "by");
    EXPECT_EQ(3, res.size());
    for (size_t i = 0; i < res.size(); i++) {
        EXPECT_STREQ(expected[i].c_str(), res[i].c_str());
    }
}

TEST(StringTest, StrReplaceAll) {
    std::string res = StrReplaceAll("testreplacefooreplacebar", "replace", "@");
    EXPECT_STREQ("test@foo@bar", res.c_str());
}

TEST(StringTest, StrReplaceAllFromEmpty) {
    std::string res = StrReplaceAll("testreplacefooreplacebar", "", "@");
    EXPECT_STREQ("testreplacefooreplacebar", res.c_str());
}

TEST(StringTest, ParseCommaSeparetedList) {
    std::set<std::string> expected =
        { "a", "b", "see", "d"};
    std::set<std::string> actual =
        ParseCommaSeparetedList("a,b, see ,,d");
    EXPECT_EQ(expected.size(), actual.size());
    auto ex_it = expected.begin();
    auto ac_it = actual.begin();
    while (ex_it != expected.end()) {
        EXPECT_STREQ((*ex_it).c_str(), (*ac_it).c_str());
        ++ex_it;
        ++ac_it;
    }
}

TEST(StringTest, SetToStr) {
    std::string str = SetToStr({ "a", "foo", "bar" });
    EXPECT_STREQ("[a, bar, foo]", str.c_str());
    str = SetToStr({});
    EXPECT_STREQ("[]", str.c_str());
}

TEST(StringTest, SetToStrCustom) {
    std::string str = SetToStr(
        { "a", "foo", "bar" },
        "prefix(",
        " | ",
        ").end()");
    EXPECT_STREQ("prefix(a | bar | foo).end()", str.c_str());
}

TEST(StringTest, StrToUint) {
    size_t res = StrToUint("12");
    EXPECT_EQ(12, res);
    // input should be positive
    res = StrToUint("-1");
    EXPECT_EQ(INDEX_NONE, res);
    // input should be numeric
    res = StrToUint("a");
    EXPECT_EQ(INDEX_NONE, res);
    // input should be numeric
    res = StrToUint("a");
    EXPECT_EQ(INDEX_NONE, res);
    // overflow
    std::string index_max_str = std::to_string(INDEX_MAX);
    res = StrToUint(index_max_str + "0");
    EXPECT_EQ(INDEX_NONE, res);
    index_max_str.pop_back();
    res = StrToUint(index_max_str + "9");
    EXPECT_EQ(INDEX_NONE, res);
}

TEST(StringTest, StrCount) {
    int res = StrCount("hello world hello", "hello");
    EXPECT_EQ(2, res);
    res = StrCount("hello world hello", "");
    EXPECT_EQ(0, res);
    res = StrCount("", "hello");
    EXPECT_EQ(0, res);
}

TEST(StringTest, StrCountChar) {
    int res = StrCount("hello world hello", 'l');
    EXPECT_EQ(5, res);
    res = StrCount("", 'l');
    EXPECT_EQ(0, res);
}

TEST(StringTest, GetFirstNonSpace) {
    char res = GetFirstNonSpace(" \t\r\n\v\fa");
    EXPECT_EQ('a', res);
    res = GetFirstNonSpace("");
    EXPECT_EQ('\0', res);
}

TEST(StringTest, GetFirstNonSpacePos) {
    size_t res = GetFirstNonSpacePos(" \t\r\n\v\fa");
    EXPECT_EQ(6, res);
    res = GetFirstNonSpacePos("");
    EXPECT_EQ(INDEX_NONE, res);
}

TEST(StringTest, StrIsBlank) {
    bool res = StrIsBlank("  \t\r\n\v\f");
    EXPECT_EQ(true, res);
    res = StrIsBlank("");
    EXPECT_EQ(true, res);
    res = StrIsBlank(" a");
    EXPECT_EQ(false, res);
}
