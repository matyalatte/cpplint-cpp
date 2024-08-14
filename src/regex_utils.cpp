#include "regex_utils.h"
#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "common.h"

pcre2_code* RegexCompileBase(const std::string& regex, uint32_t options) noexcept {
    int error_number;
    PCRE2_SIZE error_offset;
    pcre2_code* code_ptr = pcre2_compile(
        reinterpret_cast<PCRE2_SPTR>(regex.c_str()),
        regex.length(),
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

#ifdef SUPPORT_JIT
regex_code RegexJitCompile(const std::string& regex, uint32_t options) noexcept {
    pcre2_code* ret = RegexCompileBase(regex, options);

    int result = pcre2_jit_compile(ret, PCRE2_JIT_COMPLETE);
    if (result != 0) {
        std::cerr << "PCRE2 JIT compilation failed. " <<
            " Error: " << result << ", Pattern: " << regex << std::endl;
        exit(1);
    }

    return regex_code(ret);
}
#endif

static inline bool pcre2_match_priv(const pcre2_code* re, const std::string& str,
                                    pcre2_match_data* match, uint32_t flags = REGEX_FLAGS_DEFAULT) {
    int rc = pcre2_match(
        re,
        reinterpret_cast<PCRE2_SPTR>(str.c_str()),
        str.length(),
        0,
        flags,
        match,
        nullptr);
    return rc >= 0;
}

std::string GetMatchStr(regex_match& match, const std::string &subject, int i) {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    if (i >= rc || ovector[2 * i] == PCRE2_UNSET || ovector[2 * i + 1] == PCRE2_UNSET)
        return "";  // Return empty string if the group does not exist or is not matched

    PCRE2_SIZE start = ovector[2 * i];
    PCRE2_SIZE end = ovector[2 * i + 1];
    PCRE2_SIZE len = end - start;

    // Return the substring as std::string
    return subject.substr(start, len);
}

bool IsMatched(regex_match& match, int i) noexcept {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    return i < rc &&
           ovector[2 * i] != PCRE2_UNSET &&
           ovector[2 * i + 1] != PCRE2_UNSET;
}

PCRE2_SIZE GetMatchStart(regex_match& match, int i) noexcept {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    if (i >= rc || ovector[2 * i] == PCRE2_UNSET || ovector[2 * i + 1] == PCRE2_UNSET)
        return INDEX_NONE;  // the group does not exist or is not matched

    PCRE2_SIZE start = ovector[2 * i];
    return start;
}

PCRE2_SIZE GetMatchEnd(regex_match& match, int i) noexcept {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match.get());
    int rc = pcre2_get_ovector_count(match.get());

    if (i >= rc || ovector[2 * i] == PCRE2_UNSET || ovector[2 * i + 1] == PCRE2_UNSET)
        return INDEX_NONE;  // the group does not exist or is not matched

    PCRE2_SIZE end = ovector[2 * i + 1];
    return end;
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

bool RegexSearch(const std::string& regex, const std::string& str,
                 uint32_t options, uint32_t flags) noexcept {
    regex_code re = RegexCompile(regex, options);
    if (!re) return false;
    regex_match result(
        pcre2_match_data_create_from_pattern(re.get(), nullptr));
    return pcre2_match_priv(re.get(), str, result.get(), flags);
}

bool RegexSearch(const regex_code& regex, const std::string& str,
                 uint32_t flags) noexcept {
    if (!regex) return false;
    regex_match result(
        pcre2_match_data_create_from_pattern(regex.get(), nullptr));
    return pcre2_match_priv(regex.get(), str, result.get(), flags);
}

bool RegexSearch(const std::string& regex, const std::string& str,
                 regex_match& result,
                 uint32_t options, uint32_t flags) noexcept {
    regex_code re = RegexCompile(regex, options);
    if (!re) return false;
    return pcre2_match_priv(re.get(), str, result.get(), flags);
}

bool RegexSearch(const regex_code& regex, const std::string& str,
                 regex_match& result,
                 uint32_t flags) noexcept {
    if (!regex) return false;
    return pcre2_match_priv(regex.get(), str, result.get(), flags);
}

static std::vector<char> regex_replace_base(
        const pcre2_code* re, const std::string& fmt,
        const std::string& str,
        pcre2_match_data* match_data,
        PCRE2_SIZE* outlength,
        bool* replaced, bool replace_all) {
    *replaced = false;
    uint32_t options = PCRE2_SUBSTITUTE_EXTENDED;
    if (replace_all)
        options |= PCRE2_SUBSTITUTE_GLOBAL;

    // Calculate the output size
    size_t buffer_size = MAX(str.length(), 1);  // Starting buffer size
    *outlength = buffer_size;
    std::vector<char> buffer(buffer_size);
    while (true) {
        int result = pcre2_substitute(
            re,
            reinterpret_cast<PCRE2_SPTR>(str.c_str()),
            str.length(),
            0,
            options,
            match_data,
            nullptr,
            reinterpret_cast<PCRE2_SPTR>(fmt.c_str()),
            fmt.length(),
            reinterpret_cast<PCRE2_UCHAR8*>(buffer.data()),
            outlength);

        if (result == PCRE2_ERROR_NOMEMORY) {
            // Increase buffer size and try again
            buffer_size *= 2;
            buffer.resize(buffer_size);
            *outlength = buffer_size;
        } else if (result >= 0) {
            // Success
            *replaced = result > 0;
            break;
        } else {
            // Substitution failed, return original string
            break;
        }
    }
    return buffer;
}

inline std::string regex_replace_base(const pcre2_code* re, const std::string& fmt,
                                      const std::string& str,
                                      pcre2_match_data* match_data,
                                      bool* replaced, bool replace_all) {
    PCRE2_SIZE outlength = 0;
    std::vector<char> buffer =
        regex_replace_base(re, fmt, str, match_data, &outlength, replaced, replace_all);
    if (*replaced)
        return std::string(buffer.data(), outlength);
    else
        return str;
}

inline std::string regex_replace_base(const pcre2_code* re, const std::string& fmt,
                                      const std::string& str,
                                      pcre2_match_data* match_data, bool replace_all) {
    bool replaced = false;
    PCRE2_SIZE outlength = 0;
    std::vector<char> buffer =
        regex_replace_base(re, fmt, str, match_data, &outlength, &replaced, replace_all);
    if (replaced)
        return std::string(buffer.data(), outlength);
    else
        return str;
}

inline void regex_replace_base(const pcre2_code* re, const std::string& fmt,
                               std::string* str,
                               pcre2_match_data* match_data, bool replace_all) {
    bool replaced = false;
    PCRE2_SIZE outlength = 0;
    std::vector<char> buffer =
        regex_replace_base(re, fmt, *str, match_data, &outlength, &replaced, replace_all);
    if (replaced)
        *str = std::string(buffer.data(), outlength);
}

inline void regex_replace_base(const pcre2_code* re, const std::string& fmt,
                               std::string* str,
                               pcre2_match_data* match_data,
                               bool* replaced, bool replace_all) {
    PCRE2_SIZE outlength = 0;
    std::vector<char> buffer =
        regex_replace_base(re, fmt, *str, match_data, &outlength, replaced, replace_all);
    if (*replaced)
        *str = std::string(buffer.data(), outlength);
}

std::string RegexReplace(const std::string& regex, const std::string& fmt,
                         const std::string& str, bool replace_all) {
    regex_code re = RegexCompile(regex);
    if (!re) return str;

    regex_match match_data(
        pcre2_match_data_create_from_pattern(re.get(), nullptr));
    return regex_replace_base(re.get(), fmt, str, match_data.get(), replace_all);
}

std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                         const std::string& str, bool replace_all) {
    if (!regex) return str;

    regex_match match_data(
        pcre2_match_data_create_from_pattern(regex.get(), nullptr));
    return regex_replace_base(regex.get(), fmt, str, match_data.get(), replace_all);
}

std::string RegexReplace(const std::string& regex, const std::string& fmt,
                         const std::string& str,
                         regex_match& match_data, bool replace_all) {
    regex_code re = RegexCompile(regex);
    if (!re) return str;
    return regex_replace_base(re.get(), fmt, str, match_data.get(), replace_all);
}

std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                         const std::string& str,
                         regex_match& match_data, bool replace_all) {
    if (!regex) return str;
    return regex_replace_base(regex.get(), fmt, str, match_data.get(), replace_all);
}

std::string RegexReplace(const regex_code& regex, const std::string& fmt,
                         const std::string& str,
                         regex_match& match_data, bool* replaced, bool replace_all) {
    if (!regex) return str;
    return regex_replace_base(regex.get(), fmt, str, match_data.get(), replaced, replace_all);
}

void RegexReplace(const regex_code& regex, const std::string& fmt,
                  std::string* str,
                  regex_match& match_data, bool replace_all) {
    if (!regex) return;
    regex_replace_base(regex.get(), fmt, str, match_data.get(), replace_all);
}

void RegexReplace(const regex_code& regex, const std::string& fmt,
                  std::string* str,
                  regex_match& match_data, bool* replaced, bool replace_all) {
    if (!regex) return;
    regex_replace_base(regex.get(), fmt, str, match_data.get(), replaced, replace_all);
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

    regex_match match_data(
        pcre2_match_data_create_from_pattern(re.get(), nullptr));

    // Position to start the search
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
            match_data.get(),
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
        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data.get());
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
