#pragma once
#include <set>
#include <string>
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

// Split string by another string.
std::vector<std::string> StrSplitBy(const std::string &str, const std::string &delimiter);

// Split string by comma, strip white spaces, and remove duplicated items.
std::set<std::string> ParseCommaSeparetedList(const std::string& str);

// Concat vec2 to vec1.
inline void ConcatVec(std::vector<std::string>& vec1, std::vector<std::string>& vec2) {
    vec1.insert(vec1.end(), vec2.begin(), vec2.end());
}

// Check if a string is in a vector of strings
inline bool InStrVec(const std::vector<std::string>& str_vec, const std::string& str) {
    for (const std::string& s : str_vec) {
        if (s == str)
            return true;
    }
    return false;
}

// You can also use a null terminated array of char* instead of vector.
inline bool InStrVec(const char* const *str_vec, const std::string& str) {
    while (*str_vec != nullptr) {
        if (str == *str_vec)
            return true;
        str_vec++;
    }
    return false;
}

// Check if a character is in a vector of strings
inline bool InCharVec(const std::vector<char>& char_vec, const std::string& str) {
    if (str.size() != 1) return false;
    for (char c : char_vec) {
        if (str[0] == c)
            return true;
    }
    return false;
}

inline bool InStrSet(const std::set<std::string>& str_vec, const std::string& str) {
    for (const std::string& s : str_vec) {
        if (s == str)
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

inline bool StrIsChar(const std::string& str, char c) {
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
char GetFirstNonSpace(const std::string& str, size_t pos = 0) noexcept;

// Returns index to the first non-space character or INDEX_NONE.
size_t GetFirstNonSpacePos(const std::string& str, size_t pos = 0) noexcept;

// Returns true if the string is empty or consists of only white spaces.
inline bool StrIsBlank(const std::string& str) noexcept {
    return GetFirstNonSpace(str) == '\0';
}

// Returns true if the string consists of only digits.
bool StrIsDigit(const std::string& str) noexcept;
