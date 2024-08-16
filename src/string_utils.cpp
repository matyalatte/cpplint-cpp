#include "string_utils.h"
#include <ctype.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "common.h"

#define IS_SPACE(c) isspace((uint8_t)(c))
#define IS_DIGIT(c) isdigit((uint8_t)(c))

std::string StrBeforeChar(const std::string& str, char c) {
    const char* str_p = &str[0];
    const char* start = str_p;
    const char* end = &str[str.size()];
    while (*str_p != '\0') {
        if (*str_p == c) {
            end = str_p;
            break;
        }
        str_p++;
    }
    return std::string(start, end);
}

std::string StrAfterChar(const std::string& str, char c) {
    const char* str_p = &str[0];
    while (*str_p != '\0') {
        if (*str_p == c)
            return str_p + 1;
        str_p++;
    }
    return "";
}

std::string StrStrip(const std::string &str) {
    if (str.empty()) return str;
    const char* start = &str[0];
    while (IS_SPACE(*start))
        start++;
    const char* end = &str[str.size() - 1];
    while (start <= end && IS_SPACE(*end))
        end--;
    if (end < start)
        return "";
    return std::string(start, end + 1);
}

std::string StrStrip(const std::string &str, char c) {
    if (str.empty()) return str;
    const char* start = &str[0];
    while (*start == c)
        start++;
    const char* end = &str[str.size() - 1];
    while (start < end && *end == c)
        end--;
    return std::string(start, end + 1);
}

std::string StrStrip(const char* start, const char* end) {
    if (start > end) return "";
    while (IS_SPACE(*start))
        start++;
    while (start <= end && IS_SPACE(*end))
        end--;
    if (end < start)
        return "";
    return std::string(start, end + 1);
}

std::string StrRstrip(const std::string &str) {
    if (str.empty()) return str;
    const char* start = &str[0];
    const char* end = &str[str.size() - 1];
    while (start <= end && IS_SPACE(*end))
        end--;
    if (end < start)
        return "";
    return std::string(start, end + 1);
}

std::string StrRstrip(const std::string &str, char c) {
    if (str.empty()) return str;
    const char* start = &str[0];
    const char* end = &str[str.size() - 1];
    while (start <= end && *end == c)
        end--;
    if (end < start)
        return "";
    return std::string(start, end + 1);
}

std::string StrLstrip(const std::string &str) {
    const char* start = &str[0];
    while (IS_SPACE(*start))
        start++;
    const char* end = &str[str.size()];
    return std::string(start, end);
}

std::string StrLstrip(const std::string &str, char c) {
    const char* start = &str[0];
    while (*start == c)
        start++;
    const char* end = &str[str.size()];
    return std::string(start, end);
}

size_t StrLstripSize(const std::string &str) {
    const char* start = &str[0];
    while (IS_SPACE(*start))
        start++;
    return &str[0] + str.size() - start;
}

std::vector<std::string> StrSplit(const std::string& str, size_t max_size) {
    std::vector<std::string> split = {};
    const char* str_p = &str[0];
    const char* start = str_p;
    // skip white spaces

    while (*str_p != '\0') {
        // skip white spaces
        while (IS_SPACE(*str_p)) {
            str_p++;
        }
        if (*str_p == '\0')
            break;
        start = str_p;
        // find the next white space
        while (!IS_SPACE(*str_p)) {
            if (*str_p == '\0')
                break;
            str_p++;
        }
        const char* end = str_p;
        split.emplace_back(start, end);
        if (split.size() >= max_size) {
            break;
        }
    }
    return split;
}

std::string StrSplitLast(const std::string& str) {
    if (str.empty())
        return "";

    const char* start = &str[0];
    const char* str_p = &str[str.size() - 1];
    // skip white spaces
    while (IS_SPACE(*str_p) && str_p >= start)
        str_p--;

    if (str_p < start)
        return "";  // All characters are white spaces.

    const char* end = str_p + 1;

    while (!IS_SPACE(*str_p) && str_p >= start)
        str_p--;

    return std::string(str_p + 1, end);
}

std::vector<std::string> StrSplitBy(const std::string &str, const std::string &delimiter) {
    std::string copied = str;
    std::vector<std::string> tokens = {};
    size_t pos = 0;
    size_t delimiter_length = delimiter.size();

    while ((pos = copied.find(delimiter)) != std::string::npos) {
        tokens.emplace_back(copied.substr(0, pos));
        copied = copied.substr(pos + delimiter_length);
    }
    tokens.emplace_back(copied);  // Add the remaining part of the string

    return tokens;
}

std::set<std::string> ParseCommaSeparetedList(const std::string& str) {
    std::set<std::string> set = {};
    const char* str_p = &str[0];
    const char* start = str_p;

    while (*str_p != '\0') {
        if (*str_p == ',') {
            std::string item = StrStrip(start, str_p - 1);
            if (item.size() > 0)
                set.insert(item);
            start = str_p + 1;
        }
        str_p++;
    }
    std::string item = StrStrip(start, str_p - 1);
    if (item.size() > 0)
        set.insert(item);
    return set;
}

std::string SetToStr(const std::set<std::string>& set,
                     const std::string& prefix,
                     const std::string& delim,
                     const std::string& suffix) {
    std::string ret = prefix;
    size_t i = 0;
    for (const std::string& s : set) {
        ret += s;
        if (i < (set.size() - 1)) {
            ret += delim;
        }
        i++;
    }
    ret += suffix;
    return ret;
}

#define IS_DIGIT(c) isdigit((uint8_t)(c))

// Converts a string to an int. returns -1 when failed to parse.
size_t StrToUint(const std::string& val) noexcept {
    const char* val_p = &val[0];
    size_t ret = 0;
    while (*val_p != '\0') {
        if (!IS_DIGIT(*val_p))
            return INDEX_NONE;  // negative or non-numeric
        if (ret > INDEX_MAX / 10)
            return INDEX_NONE;  // overflow
        ret *= 10;
        size_t digit = TO_SIZE(*val_p - '0');
        if (INDEX_MAX - ret < digit)
            return INDEX_NONE;  // overflow
        ret += digit;
        val_p++;
    }
    return ret;
}

int StrCount(const std::string& str, const std::string& target) noexcept {
    if (target.empty()) return 0;
    int occurrences = 0;
    std::string::size_type pos = 0;
    while ((pos = str.find(target, pos)) != std::string::npos) {
        ++occurrences;
        pos += target.length();
    }
    return occurrences;
}

int StrCount(const std::string& str, char c) noexcept {
    int occurrences = 0;
    const char* str_p = &str[0];
    while (*str_p != '\0') {
        if (*str_p == c)
            ++occurrences;
        str_p++;
    }
    return occurrences;
}

std::string StrReplaceAll(const std::string &str, const std::string& from, const std::string& to) {
    std::string copied = str;
    size_t pos = 0;
    size_t to_len = to.length();

    if (from.empty())
        return copied;

    while ((pos = copied.find(from, pos)) != std::string::npos) {
        copied.replace(pos, from.length(), to);
        pos += to_len;
    }
    return copied;
}

std::string StrToLower(const std::string &str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return lower;
}

std::string StrToUpper(const std::string &str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(),
        [](unsigned char c){ return std::toupper(c); });
    return upper;
}

char GetFirstNonSpace(const std::string& str, size_t pos) noexcept {
    const char* start = &str[pos];
    while (IS_SPACE(*start))
        start++;
    return *start;
}

size_t GetFirstNonSpacePos(const std::string& str, size_t pos) noexcept {
    const char* start = &str[pos];
    while (IS_SPACE(*start))
        start++;
    if (*start == '\0')
        return INDEX_NONE;
    return TO_SIZE(start - &str[0]);
}

bool StrIsDigit(const std::string& str) noexcept {
    if (str.empty()) return false;
    const char* start = &str[0];
    while (IS_DIGIT(*start))
        start++;
    return *start == '\0';
}
