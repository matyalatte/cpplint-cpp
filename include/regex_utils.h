#pragma once
#include <memory>
#include <string>
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

// std::smatch.str(i) for pcre2
std::string GetMatchStr(regex_match& match, const std::string &subject, int i);

// std::smatch[i].matched
bool IsMatched(regex_match& match, int i) noexcept;

// std::smatch.position(i)
PCRE2_SIZE GetMatchStart(regex_match& match, int i) noexcept;

// std::smatch.position(i) + std::smatch.str(i).size()
PCRE2_SIZE GetMatchEnd(regex_match& match, int i) noexcept;

// std::smatch.str(i).size()
PCRE2_SIZE GetMatchSize(regex_match& match, int i) noexcept;

pcre2_code* RegexCompileBase(const std::string& regex,
                             uint32_t options = REGEX_OPTIONS_DEFAULT) noexcept;

inline regex_code RegexCompile(const std::string& regex,
                               uint32_t options = REGEX_OPTIONS_DEFAULT) noexcept {
    pcre2_code* ret = RegexCompileBase(regex, options);
    return regex_code(ret);
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

bool RegexSearch(const std::string& regex, const std::string& str,
                 uint32_t options = REGEX_OPTIONS_DEFAULT,
                 uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

bool RegexSearch(const regex_code& regex, const std::string& str,
                 uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

bool RegexSearch(const std::string& regex, const std::string& str,
                 regex_match& result,
                 uint32_t options = REGEX_OPTIONS_DEFAULT,
                 uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

bool RegexSearch(const regex_code& regex, const std::string& str,
                 regex_match& result,
                 uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept;

inline bool RegexMatch(const std::string& regex, const std::string& str,
                       uint32_t options = REGEX_OPTIONS_DEFAULT,
                       uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearch(regex, str, options, flags | PCRE2_ANCHORED);
}

inline bool RegexMatch(const regex_code& regex, const std::string& str,
                       uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearch(regex, str, flags | PCRE2_ANCHORED);
}

inline bool RegexMatch(const std::string& regex, const std::string& str,
                       regex_match& result,
                       uint32_t options = REGEX_OPTIONS_DEFAULT,
                       uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearch(regex, str, result, options, flags | PCRE2_ANCHORED);
}

inline bool RegexMatch(const regex_code& regex, const std::string& str,
                       regex_match& result,
                       uint32_t flags = REGEX_FLAGS_DEFAULT) noexcept {
    return RegexSearch(regex, str, result, flags | PCRE2_ANCHORED);
}

std::string RegexReplace(const std::string& regex, const std::string& fmt,
                         const std::string& str, bool replace_all = true);

std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                         const std::string& str, bool replace_all = true);

std::string RegexReplace(const std::string& regex, const std::string& fmt,
                         const std::string& str,
                         regex_match& match_data, bool replace_all = true);

std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                         const std::string& str,
                         regex_match& match_data, bool replace_all = true);

std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                         const std::string& str,
                         regex_match& match_data, bool* replaced, bool replace_all = true);


std::string RegexEscape(const std::string& str);

// Split a string by a regex pattern.
std::vector<std::string> RegexSplit(const std::string& regex, const std::string& str);
