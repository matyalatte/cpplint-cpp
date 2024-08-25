#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "common.h"

// Gets a substring before a character.
std::string StrBeforeChar(const std::string& str, char c);

// Gets a substring after a character.
std::string StrAfterChar(const std::string& str, char c);

// Strips leading and tailing white spaces from a string.
std::string StrStrip(const std::string &str);

// Strips leading and tailing characters from a string.
std::string StrStrip(const std::string &str, char c);

// Strips leading spaces from start and tailing spaces from end.
std::string StrStrip(const char* start, const char* end);

// Strips tailing white spaces from a string.
std::string StrRstrip(const std::string &str);

// Strips tailing characters from a string.
std::string StrRstrip(const std::string &str, char c);

// Strips leading white spaces from a string.
std::string StrLstrip(const std::string &str);

// Strips leading characters from a string.
std::string StrLstrip(const std::string &str, char c);

// StrLstrip(str).size()
size_t StrLstripSize(const std::string &str);

// Split string by white spaces. Empty strings are removed from return value.
std::vector<std::string> StrSplit(const std::string& str, size_t max_size = INDEX_MAX);

// StrSplit().back()
template <typename STR>
STR StrSplitLast(const STR& str);

// Split string by another string.
std::vector<std::string> StrSplitBy(const std::string &str, const std::string &delimiter);

// Split string by comma, strip white spaces, and remove duplicated items.
template <typename STR>
std::set<std::string> ParseCommaSeparetedList(const STR& str);

// Concat vec2 to vec1.
template <typename T>
inline void ConcatVec(std::vector<T>& vec1, std::vector<T>& vec2) {
    vec1.insert(vec1.end(), vec2.begin(), vec2.end());
}

// Check if a string is in a vector of strings
template <typename STR>
inline bool InStrVec(const std::vector<std::string>& str_vec, const STR& str) {
    for (const std::string& s : str_vec) {
        if (s == str)
            return true;
    }
    return false;
}

// You can also use a null terminated array of char* instead of vector.
template <typename STR>
inline bool InStrVec(const char* const *str_vec, const STR& str) {
    while (*str_vec != nullptr) {
        if (str == *str_vec)
            return true;
        str_vec++;
    }
    return false;
}

// Check if a character is in a vector of strings
template <typename STR>
inline bool InCharVec(const std::vector<char>& char_vec, const STR& str) {
    if (str.size() != 1) return false;
    for (char c : char_vec) {
        if (str[0] == c)
            return true;
    }
    return false;
}

inline bool StrContain(const std::string& str, const std::string& target) {
    return str.find(target) != std::string::npos;
}

inline bool StrContain(const std::string& str, const char* target) {
    return str.find(target) != std::string::npos;
}

inline bool StrContain(const std::string& str, const char c) {
    return str.find(c) != std::string::npos;
}

inline bool StrContain(const std::string& str, const char c, size_t pos) {
    return str.find(c, pos) != std::string::npos;
}

template <typename STR>
inline bool StrIsChar(const STR& str, char c) {
    return str.size() == 1 && str[0] == c;
}

std::string SetToStr(const std::set<std::string>& set,
                     const std::string& prefix = "[",
                     const std::string& delim = ", ",
                     const std::string& suffix = "]");

// Converts a string to an int. returns -1 when failed to parse.
size_t StrToUint(const std::string& val) noexcept;

// Counts occurrences
int StrCount(const std::string& str, const std::string& target) noexcept;
int StrCount(const std::string& str, char c) noexcept;

std::string StrReplaceAll(const std::string &str,
                          const std::string& from, const std::string& to);

std::string StrToLower(const std::string &str);

std::string StrToUpper(const std::string &str);

// Returns the first non-space character or a null terminator.
// You can replace RegexMatch(R"(\s*x)", line) with GetFirstNoneSpace(line) == 'x'
char GetFirstNonSpace(const std::string& str, size_t pos = 0) noexcept;

// Returns index to the first non-space character or INDEX_NONE.
size_t GetFirstNonSpacePos(const std::string& str, size_t pos = 0) noexcept;

// Returns true if the string is empty or consists of only white spaces.
inline bool StrIsBlank(const std::string& str) noexcept {
    return GetFirstNonSpace(str) == '\0';
}

// Returns the last non-space character or a null terminator.
// You can replace RegexSearch(R"(x\s*$)", line) with GetLastNoneSpace(line) == 'x'
char GetLastNonSpace(const std::string& str) noexcept;

// Returns index to the last non-space character or INDEX_NONE.
size_t GetLastNonSpacePos(const std::string& str) noexcept;

// Returns true if the string consists of only digits.
bool StrIsDigit(const std::string& str) noexcept;
bool StrIsDigit(const std::string_view& str) noexcept;

// Remove items of set2 from set1.
std::set<std::string> SetDiff(const std::set<std::string>& set1,
                              const std::set<std::string>& set2);
