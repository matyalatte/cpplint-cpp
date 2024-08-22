#include "regex_utils.h"
#include <ctype.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "common.h"

pcre2_code* RegexCompileBase(const char* regex, uint32_t options) noexcept {
    int error_number;
    PCRE2_SIZE error_offset;
    pcre2_code* code_ptr = pcre2_compile(
        reinterpret_cast<PCRE2_SPTR>(regex),
        PCRE2_ZERO_TERMINATED,
        options,
        &error_number,
        &error_offset,
        nullptr);

    if (!code_ptr) {
        std::cerr << "PCRE2 compilation failed. Offset: " << error_offset
            << ", Error: " << error_number << ", Pattern: " << regex << std::endl;
        exit(1);
    }

    return code_ptr;
}

static inline bool pcre2_match_priv(const pcre2_code* re, const std::string& str,
                                    PCRE2_SIZE startoffset, PCRE2_SIZE length,
                                    pcre2_match_data* match,
                                    uint32_t flags = REGEX_FLAGS_DEFAULT) {
    int rc = pcre2_match(
        re,
        reinterpret_cast<PCRE2_SPTR>(str.c_str()) + startoffset,
        length,
        0,
        flags,
        match,
        nullptr);
    return rc >= 0;
}

static bool GetMatchRange(regex_match& match, int i,
                          PCRE2_SIZE* start, PCRE2_SIZE* len) {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    if (i >= rc || ovector[2 * i] == PCRE2_UNSET || ovector[2 * i + 1] == PCRE2_UNSET)
        return false;

    *start = ovector[2 * i];
    PCRE2_SIZE end = ovector[2 * i + 1];
    *len = end - *start;
    return true;
}

std::string GetMatchStr(regex_match& match, const std::string &subject, int i, size_t startoffset) {
    PCRE2_SIZE start;
    PCRE2_SIZE len;
    bool valid = GetMatchRange(match, i, &start, &len);
    if (!valid) {
        // Return empty string if the group does not exist or is not matched
        return "";
    }
    // Return the substring as std::string
    return subject.substr(start + startoffset, len);
}

std::string_view GetMatchStrView(regex_match& match, const std::string &subject,
                                 int i, size_t startoffset) {
    PCRE2_SIZE start;
    PCRE2_SIZE len;
    bool valid = GetMatchRange(match, i, &start, &len);
    if (!valid) {
        // Return empty string if the group does not exist or is not matched
        return std::string_view(subject.data(), 0);
    }
    // Return the substring as std::string_view
    return std::string_view(subject.data() + start + startoffset, len);
}

bool IsMatched(regex_match& match, int i) noexcept {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    return i < rc &&
           ovector[2 * i] != PCRE2_UNSET &&
           ovector[2 * i + 1] != PCRE2_UNSET;
}

PCRE2_SIZE GetMatchStart(regex_match& match, int i, size_t startoffset) noexcept {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    if (i >= rc || ovector[2 * i] == PCRE2_UNSET || ovector[2 * i + 1] == PCRE2_UNSET)
        return INDEX_NONE;  // the group does not exist or is not matched

    PCRE2_SIZE start = ovector[2 * i];
    return start + startoffset;
}

PCRE2_SIZE GetMatchEnd(regex_match& match, int i, size_t startoffset) noexcept {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    if (i >= rc || ovector[2 * i] == PCRE2_UNSET || ovector[2 * i + 1] == PCRE2_UNSET)
        return INDEX_NONE;  // the group does not exist or is not matched

    PCRE2_SIZE end = ovector[2 * i + 1];
    return end + startoffset;
}

PCRE2_SIZE GetMatchSize(regex_match& match, int i) noexcept {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    if (i >= rc || ovector[2 * i] == PCRE2_UNSET || ovector[2 * i + 1] == PCRE2_UNSET)
        return INDEX_NONE;  // the group does not exist or is not matched

    PCRE2_SIZE start = ovector[2 * i];
    PCRE2_SIZE end = ovector[2 * i + 1];
    return end - start;
}

// We use this dummy buffer when we don't need results.
thread_local regex_match re_result_temp =
    RegexCreateMatchData(32);

bool RegexSearch(const regex_code& regex, const std::string& str,
                 uint32_t flags) noexcept {
    if (!regex) return false;
    return pcre2_match_priv(regex.get(), str, 0, str.size(),
                            re_result_temp.get(), flags);
}

bool RegexSearch(const regex_code& regex, const std::string& str,
                 regex_match& result,
                 uint32_t flags) noexcept {
    if (!regex) return false;
    return pcre2_match_priv(regex.get(), str, 0, str.size(), result.get(), flags);
}

bool RegexSearchWithRange(const regex_code& regex, const std::string& str,
                          size_t startoffset, size_t length,
                          uint32_t flags) noexcept {
    if (!regex) return false;
    return pcre2_match_priv(regex.get(), str, startoffset, length,
                            re_result_temp.get(), flags);
}

bool RegexSearchWithRange(const regex_code& regex, const std::string& str,
                          size_t startoffset, size_t length,
                          regex_match& result,
                          uint32_t flags) noexcept {
    if (!regex) return false;
    return pcre2_match_priv(regex.get(), str, startoffset, length,
                            result.get(), flags);
}

template<typename MatchFunc>
static void regex_replace_base(
        MatchFunc re_match,
        const pcre2_code* re, const char* fmt,
        const std::string& str,
        std::string* result_str,
        bool* replaced, bool replace_all) {
    *replaced = false;

    // Position to start the search
    PCRE2_SIZE start_offset = 0;

    // Get matched ranges and the length of replaced string
    std::vector<std::pair<PCRE2_SIZE, PCRE2_SIZE>>* matched_ranges = nullptr;
    size_t result_length = 0;
    size_t fmt_length = strlen(fmt);
    while (true) {
        int rc = re_match(
            re,
            (PCRE2_SPTR)str.c_str(),
            str.length(),
            start_offset,
            REGEX_OPTIONS_DEFAULT,
            re_result_temp.get(),
            nullptr);

        if (rc < 0) {
            if (rc == PCRE2_ERROR_NOMATCH) {
                result_length += str.length() - start_offset;
                break;  // No more matches
            } else {
                std::cerr << "PCRE2 matching error " << rc << std::endl;
                break;
            }
        }

        // Save the matched range.
        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(re_result_temp.get());
        PCRE2_SIZE match_start = ovector[0];
        PCRE2_SIZE match_end = ovector[1];
        if (!matched_ranges)
            matched_ranges = new std::vector<std::pair<PCRE2_SIZE, PCRE2_SIZE>>({});
        matched_ranges->emplace_back(match_start, match_end);
        result_length += match_start - start_offset + fmt_length;

        // Update previous_end and start_offset to continue after the current match
        start_offset = match_end;

        *replaced = true;

        // Handle empty matches by incrementing the offset
        if (match_start == match_end)
            start_offset++;

        if (start_offset >= str.length() || !replace_all)
            break;
    }

    if (!matched_ranges)
        return;  // Returns input string as it is.

    // Create return value
    std::string new_str;
    new_str.resize(result_length);
    char* result_p = new_str.data();
    const char* str_p = str.data();
    const char* fmt_p = fmt;
    PCRE2_SIZE prev_end = 0;

    for (const std::pair<PCRE2_SIZE, PCRE2_SIZE>& range : (*matched_ranges)) {
        memcpy(result_p, str_p + prev_end, range.first - prev_end);
        result_p += range.first - prev_end;
        memcpy(result_p, fmt_p, fmt_length);
        result_p += fmt_length;
        prev_end = range.second;
    }

    if (prev_end != str.length())
        memcpy(result_p, str_p + prev_end, str.length() - prev_end);

    *result_str = std::move(new_str);
    delete matched_ranges;
}

std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                         const std::string& str,
                         bool* replaced, bool replace_all) {
    if (!regex) return str;
    std::string result = str;
    regex_replace_base(pcre2_match, regex.get(), fmt.c_str(), str, &result, replaced, replace_all);
    return result;
}

void RegexReplace(const regex_code& regex, const char* fmt,
                  std::string* str,
                  bool* replaced, bool replace_all) {
    if (!regex) return;
    regex_replace_base(pcre2_match, regex.get(), fmt, *str, str, replaced, replace_all);
}

std::string RegexEscape(const std::string& str) {
    static const regex_code RE_PATTERN_ESCAPE = RegexCompile(R"([-[\]{}()*+?.,\^$|#\s])");
    return RegexReplace(RE_PATTERN_ESCAPE, R"(\$&)", str);
}

std::vector<std::string> RegexSplit(const std::string& regex, const std::string& str) {
    regex_code re = RegexCompile(regex);
    std::vector<std::string> result;

    if (!re) {
        result.push_back(str);
        return result;
    }

    // Position to start the search
    PCRE2_SIZE start_offset = 0;
    PCRE2_SIZE previous_end = 0;

    while (true) {
        int rc = pcre2_match(
            re.get(),
            (PCRE2_SPTR)str.c_str(),
            str.length(),
            start_offset,
            REGEX_OPTIONS_DEFAULT,
            re_result_temp.get(),
            nullptr);

        if (rc < 0) {
            if (rc == PCRE2_ERROR_NOMATCH) {
                // If no more matches, append the remaining string if any
                if (previous_end < str.size()) {
                    result.emplace_back(str.substr(previous_end));
                }
                break;  // No more matches
            } else {
                std::cerr << "PCRE2 matching error " << rc << std::endl;
                break;
            }
        }

        // Retrieve the starting position of the match
        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(re_result_temp.get());
        PCRE2_SIZE match_start = ovector[0];
        PCRE2_SIZE match_end = ovector[1];

        // Add the substring before the current match
        if (match_start > previous_end) {
            result.emplace_back(str.substr(previous_end, match_start - previous_end));
        }

        // Update previous_end and start_offset to continue after the current match
        previous_end = match_end;
        start_offset = match_end;

        // Handle empty matches by incrementing the offset
        if (match_start == match_end) {
            start_offset++;
            if (start_offset > str.length()) {
                break;
            }
        }
    }
    return result;
}

#ifdef SUPPORT_JIT
regex_code RegexJitCompile(const char* regex, uint32_t options) noexcept {
    pcre2_code* ret = RegexCompileBase(regex, options);

    int result = pcre2_jit_compile(ret, PCRE2_JIT_COMPLETE);
    if (result != 0) {
        std::cerr << "PCRE2 JIT compilation failed. " <<
            " Error: " << result << ", Pattern: " << regex << std::endl;
        exit(1);
    }

    return regex_code(ret);
}

static inline bool pcre2_jit_match_priv(const pcre2_code* re, const std::string& str,
                                        PCRE2_SIZE startoffset, PCRE2_SIZE length,
                                        pcre2_match_data* match,
                                        uint32_t flags = REGEX_FLAGS_DEFAULT) {
    int rc = pcre2_jit_match(
        re,
        reinterpret_cast<PCRE2_SPTR>(str.c_str()) + startoffset,
        length,
        0,
        flags,
        match,
        nullptr);
    return rc >= 0;
}

bool RegexJitSearch(const regex_code& regex, const std::string& str,
                    uint32_t flags) noexcept {
    if (!regex) return false;
    return pcre2_jit_match_priv(regex.get(), str, 0, str.size(),
                                re_result_temp.get(), flags);
}

bool RegexJitSearch(const regex_code& regex, const std::string& str,
                    regex_match& result,
                    uint32_t flags) noexcept {
    if (!regex) return false;
    return pcre2_jit_match_priv(regex.get(), str, 0, str.size(), result.get(), flags);
}

std::string RegexJitReplace(const regex_code& regex, const char* fmt,
                            const std::string& str,
                            bool* replaced, bool replace_all) {
    if (!regex) return str;
    std::string result = str;
    regex_replace_base(pcre2_jit_match, regex.get(), fmt,
                       str, &result, replaced, replace_all);
    return result;
}

void RegexJitReplace(const regex_code& regex, const char* fmt,
                     std::string* str,
                     bool* replaced, bool replace_all) {
    if (!regex) return;
    regex_replace_base(pcre2_jit_match, regex.get(), fmt,
                       *str, str, replaced, replace_all);
}
#endif
