#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#ifndef PCRE2_STATIC
#define PCRE2_STATIC
#endif
#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#endif
#include "pcre2.h"

// Custom deleter for pcre2_code
struct Pcre2CodeDeleter {
    void operator()(pcre2_code* code) const {
        if (code) pcre2_code_free(code);
    }
};

// Custom deleter for pcre2_match_data
struct Pcre2MatchDataDeleter {
    void operator()(pcre2_match_data* match_data) const {
        if (match_data) pcre2_match_data_free(match_data);
    }
};

typedef std::unique_ptr<pcre2_code, Pcre2CodeDeleter> regex_code;
typedef std::unique_ptr<pcre2_match_data, Pcre2MatchDataDeleter> regex_match;

// Note: We don't care if the strings are UTF-8 or not in the regex module.
//       UTF-8 validation is done when reading files.
inline constexpr uint32_t REGEX_OPTIONS_DEFAULT = 0;
// inline constexpr uint32_t REGEX_OPTIONS_UTF = PCRE2_UCP | PCRE2_UTF;
inline constexpr uint32_t REGEX_OPTIONS_ICASE = PCRE2_CASELESS | REGEX_OPTIONS_DEFAULT;
inline constexpr uint32_t REGEX_OPTIONS_MULTILINE = PCRE2_MULTILINE | REGEX_OPTIONS_DEFAULT;

inline constexpr uint32_t REGEX_FLAGS_DEFAULT = 0;

// Note: Templated functions with "typename STR" should support std::string and std::string_view.

// std::smatch.str(i) for pcre2
template <typename STR>
std::string GetMatchStr(regex_match& match, const STR&subject, int i,
                        size_t startoffset = 0);

template <typename STR>
std::string_view GetMatchStrView(regex_match& match, const STR&subject, int i,
                                 size_t startoffset = 0);

// std::smatch[i].matched
bool IsMatched(regex_match& match, int i) noexcept;

// std::smatch.position(i)
PCRE2_SIZE GetMatchStart(regex_match& match, int i,
                         size_t startoffset = 0) noexcept;

// std::smatch.position(i) + std::smatch.str(i).size()
PCRE2_SIZE GetMatchEnd(regex_match& match, int i,
                       size_t startoffset = 0) noexcept;

// std::smatch.str(i).size()
PCRE2_SIZE GetMatchSize(regex_match& match, int i) noexcept;

pcre2_code* RegexCompileBase(const char* regex,
                             uint32_t options = REGEX_OPTIONS_DEFAULT) noexcept;

// get pcre2_code as a unique pointer
inline regex_code RegexCompile(const char* regex,
                               uint32_t options = REGEX_OPTIONS_DEFAULT) noexcept {
    pcre2_code* ret = RegexCompileBase(regex, options);
    return regex_code(ret);
}

inline regex_code RegexCompile(const std::string& regex,
                               uint32_t options = REGEX_OPTIONS_DEFAULT) noexcept {
    return RegexCompile(regex.c_str(), options);
}

// ovecsize is the number of groups plus one.
inline regex_match RegexCreateMatchData(uint32_t ovecsize) noexcept {
    pcre2_match_data* ret = pcre2_match_data_create(ovecsize, nullptr);
    return regex_match(ret);
}

inline regex_match RegexCreateMatchData(const regex_code& regex) noexcept {
    if (!regex) return regex_match(nullptr);
    pcre2_match_data* ret = pcre2_match_data_create_from_pattern(regex.get(), nullptr);
    return regex_match(ret);
}

template <typename STR>
bool RegexSearch(const regex_code& regex, const STR& str,
                 uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

template <typename STR>
bool RegexSearch(const regex_code& regex, const STR& str,
                 regex_match& result,
                 uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

template <typename STR>
inline bool RegexSearch(
        const std::string& regex, const STR& str,
        uint32_t options = REGEX_OPTIONS_DEFAULT,
        uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    regex_code re = RegexCompile(regex, options);
    return RegexSearch(re, str, flags);
}

template <typename STR>
inline bool RegexSearch(
        const std::string& regex, const STR& str,
        regex_match& result,
        uint32_t options = REGEX_OPTIONS_DEFAULT,
        uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    regex_code re = RegexCompile(regex, options);
    return RegexSearch(re, str, result, flags);
}

// RegexSearch(regex, str.substr(startoffset, length))
// without creating string objects for substr
template <typename STR>
bool RegexSearchWithRange(const regex_code& regex, const STR& str,
                          size_t startoffset, size_t length,
                          uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

template <typename STR>
bool RegexSearchWithRange(const regex_code& regex, const STR& str,
                          size_t startoffset, size_t length,
                          regex_match& result,
                          uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

template <typename STR>
inline bool RegexSearchWithRange(
        const std::string& regex, const STR& str,
        size_t startoffset, size_t length,
        uint32_t options = REGEX_OPTIONS_DEFAULT,
        uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    regex_code re = RegexCompile(regex, options);
    return RegexSearchWithRange(re, str, startoffset, length, flags);
}

template <typename STR>
inline bool RegexSearchWithRange(
        const std::string& regex, const STR& str,
        size_t startoffset, size_t length,
        regex_match& result,
        uint32_t options = REGEX_OPTIONS_DEFAULT,
        uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    regex_code re = RegexCompile(regex, options);
    return RegexSearchWithRange(re, str, startoffset, length, result, flags);
}

template <typename STR>
inline bool RegexMatch(const std::string& regex, const STR& str,
                       uint32_t options = REGEX_OPTIONS_DEFAULT,
                       uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearch(regex, str, options, flags | PCRE2_ANCHORED);
}

template <typename STR>
inline bool RegexMatch(const regex_code& regex, const STR& str,
                       uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearch(regex, str, flags | PCRE2_ANCHORED);
}

template <typename STR>
inline bool RegexMatch(const std::string& regex, const STR& str,
                       regex_match& result,
                       uint32_t options = REGEX_OPTIONS_DEFAULT,
                       uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearch(regex, str, result, options, flags | PCRE2_ANCHORED);
}

template <typename STR>
inline bool RegexMatch(const regex_code& regex, const STR& str,
                       regex_match& result,
                       uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearch(regex, str, result, flags | PCRE2_ANCHORED);
}

// RegexMatch(regex, str.substr(startoffset, length))
// without creating string objects for substr
template <typename STR>
inline bool RegexMatchWithRange(const regex_code& regex, const STR& str,
                                size_t startoffset, size_t length,
                                uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearchWithRange(regex, str, startoffset, length,
                                flags | PCRE2_ANCHORED);
}

template <typename STR>
inline bool RegexMatchWithRange(const regex_code& regex, const STR& str,
                                size_t startoffset, size_t length,
                                regex_match& result,
                                uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearchWithRange(regex, str, startoffset, length,
                                result, flags | PCRE2_ANCHORED);
}

template <typename STR>
inline bool RegexMatchWithRange(const std::string& regex, const STR& str,
                                size_t startoffset, size_t length,
                                uint32_t options = REGEX_OPTIONS_DEFAULT,
                                uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearchWithRange(regex, str, startoffset, length,
                                options, flags | PCRE2_ANCHORED);
}

template <typename STR>
inline bool RegexMatchWithRange(const std::string& regex, const STR& str,
                                size_t startoffset, size_t length,
                                regex_match& result,
                                uint32_t options = REGEX_OPTIONS_DEFAULT,
                                uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearchWithRange(regex, str, startoffset, length,
                                result, options, flags | PCRE2_ANCHORED);
}

std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                         const std::string& str,
                         bool* replaced, bool replace_all = true);

inline std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                                const std::string& str, bool replace_all = true) {
    bool replaced = false;
    return RegexReplace(regex, fmt, str, &replaced, replace_all);
}

inline std::string RegexReplace(const std::string& regex, const std::string& fmt,
                                const std::string& str, bool replace_all = true) {
    regex_code re = RegexCompile(regex);
    return RegexReplace(re, fmt, str, replace_all);
}

// This version is faster than others but strings should be mutable.
void RegexReplace(const regex_code& regex, const char* fmt,
                  std::string* str,
                  bool* replaced, bool replace_all = true);

inline void RegexReplace(const regex_code& regex, const std::string& fmt,
                         std::string* str,
                         bool* replaced, bool replace_all = true) {
    RegexReplace(regex, fmt.c_str(), str, replaced, replace_all);
}

inline void RegexReplace(const regex_code& regex, const char* fmt,
                         std::string* str,
                         bool replace_all = true) {
    bool replaced = false;
    RegexReplace(regex, fmt, str, &replaced, replace_all);
}

inline void RegexReplace(const regex_code& regex, const std::string& fmt,
                         std::string* str,
                         bool replace_all = true) {
    RegexReplace(regex, fmt.c_str(), str, replace_all);
}

std::string RegexEscape(const std::string& str);

inline std::string RegexEscape(const std::string_view& str) {
    return RegexEscape(std::string(str));
}

// Split a string by a regex pattern.
std::vector<std::string> RegexSplit(const std::string& regex, const std::string& str);

#ifdef SUPPORT_JIT
// Uses jit compiler for regex
// It makes matching faster when using complex patterns in RegexSearch.
regex_code RegexJitCompile(const char* regex,
                           uint32_t options = REGEX_OPTIONS_DEFAULT) noexcept;

inline regex_code RegexJitCompile(const std::string& regex,
                                  uint32_t options = REGEX_OPTIONS_DEFAULT) noexcept {
    return RegexJitCompile(regex.c_str(), options);
}

// This function is 10% faster than RegexSearch
// but it crashes when regex_code is not JIT compiled.
template <typename STR>
bool RegexJitSearch(const regex_code& regex, const STR& str,
                    uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

template <typename STR>
bool RegexJitSearch(const regex_code& regex, const STR& str,
                    regex_match& result,
                    uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

void RegexJitReplace(const regex_code& regex, const char* fmt,
                    std::string* str,
                    bool* replaced, bool replace_all = true);

inline void RegexJitReplace(const regex_code& regex, const char* fmt,
                            std::string* str,
                            bool replace_all = true) {
    bool replaced = false;
    RegexJitReplace(regex, fmt, str, &replaced, replace_all);
}
#else
#define RegexJitCompile(...) RegexCompile(__VA_ARGS__)
#define RegexJitSearch(...) RegexSearch(__VA_ARGS__)
#define RegexJitReplace(...) RegexReplace(__VA_ARGS__)
#endif
