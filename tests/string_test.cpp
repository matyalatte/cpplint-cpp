#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "string_utils.h"

// TODO(matyalatte): add more test cases

TEST(StringTest, StrBeforeChar) {
    std::string res = StrBeforeChar("test@foo@bar", '@');
    EXPECT_STREQ("test", res.c_str());
}

TEST(StringTest, StrAfterChar) {
    std::string res = StrAfterChar("test@foo@bar", '@');
    EXPECT_STREQ("foo@bar", res.c_str());
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
    std::string res = StrStripChar("@@@test@@@", '@');
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

TEST(StringTest, CheckFirstNonSpace) {
    bool res = CheckFirstNonSpace(" \t\r\n\v\fa", 'a');
    EXPECT_EQ(true, res);
    res = CheckFirstNonSpace(" \t\r\n\v\fa", ';');
    EXPECT_EQ(false, res);
    res = CheckFirstNonSpace("", ';');
    EXPECT_EQ(false, res);
}

TEST(StringTest, StrIsBlank) {
    bool res = StrIsBlank("  \t\r\n\v\f");
    EXPECT_EQ(true, res);
    res = StrIsBlank("");
    EXPECT_EQ(true, res);
    res = StrIsBlank(" a");
    EXPECT_EQ(false, res);
}
