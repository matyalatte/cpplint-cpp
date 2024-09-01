#include "glob_match.h"
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "regex_utils.h"

// We use modified version of glob.cpp to convert glob patterns to regex patterns.
// https://github.com/p-ranav/glob/blob/master/source/glob.cpp

static constexpr auto SPECIAL_CHARACTERS = std::string_view{"()[]{}?*+-|^$\\.&~# \t\n\r\v\f"};
static const auto ESCAPE_SET_OPER = RegexCompile(R"([&~|])");
static const auto ESCAPE_REPL_STR = std::string{R"(\\\1)"};

static bool string_replace(std::string &str, std::string_view from, std::string_view to) {
    std::size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

// Convert a glob pattern to a regex pattern.
// When match_with_parent is true,
// returned regex pattern will match with parent paths as well.
static std::string translate(std::string_view pattern, bool match_with_parent) {
    std::size_t i = 0, n = pattern.size();
    std::string result_string;

    while (i < n) {
        auto c = pattern[i];
        i += 1;
        if (c == '*') {
            if ((i <= 1 || pattern[i - 2] == '/' || pattern[i - 2] == '\\') &&
                    i < n && pattern[i] == '*' &&
                    (i + 1 == n || pattern[i + 1] == '/' || pattern[i + 1] == '\\')) {
                if (i + 1 == n) {
                    result_string += ".*";
                    break;
                } else {
                    result_string += R"((.*[\\/])?)";
                    i += 2;
                }
            } else {
                result_string += R"([^\\/]*)";
            }
        } else if (c == '?') {
            result_string += R"([^\\/])";
        } else if (c == '[') {
            auto j = i;
            if (j < n && pattern[j] == '!') {
                j += 1;
            }
            if (j < n && pattern[j] == ']') {
                j += 1;
            }
            while (j < n && pattern[j] != ']') {
                j += 1;
            }
            if (j >= n) {
                result_string += "\\[";
            } else {
                auto stuff = std::string(pattern.begin() + i, pattern.begin() + j);
                if (stuff.find("--") == std::string::npos) {
                    string_replace(stuff, std::string_view{"\\"}, std::string_view{R"(\\)"});
                } else {
                    std::vector<std::string> chunks;
                    std::size_t k = 0;
                    if (pattern[i] == '!') {
                        k = i + 2;
                    } else {
                        k = i + 1;
                    }

                    while (true) {
                        k = pattern.find("-", k, j);
                        if (k == std::string_view::npos) {
                            break;
                        }
                        chunks.push_back(std::string(pattern.begin() + i, pattern.begin() + k));
                        i = k + 1;
                        k = k + 3;
                    }

                    chunks.push_back(std::string(pattern.begin() + i, pattern.begin() + j));
                    // Escape backslashes and hyphens for set difference (--).
                    // Hyphens that create ranges shouldn't be escaped.
                    bool first = true;
                    for (auto &chunk : chunks) {
                        string_replace(chunk, std::string_view{"\\"}, std::string_view{R"(\\)"});
                        string_replace(chunk, std::string_view{"-"}, std::string_view{R"(\-)"});
                        if (first) {
                            stuff += chunk;
                            first = false;
                        } else {
                            stuff += "-" + chunk;
                        }
                    }
                }

                // Escape set operations (&&, ~~ and ||).
                RegexReplace(ESCAPE_SET_OPER, ESCAPE_REPL_STR, stuff);
                i = j + 1;
                if (stuff[0] == '!') {
                    stuff = R"(^\\/)" + std::string(stuff.begin() + 1, stuff.end());
                } else if (stuff[0] == '^' || stuff[0] == '[') {
                    stuff = "\\\\" + stuff;
                }
                result_string = result_string + "[" + stuff + "]";
            }
        } else if (c == '/' || c == '\\') {
            // Path separator
            result_string += R"([\\/])";
        } else {
            // SPECIAL_CHARS
            // closing ')', '}' and ']'
            // '-' (a range in character set)
            // '&', '~', (extended character set operations)
            // '#' (comment) and WHITESPACE (ignored) in verbose mode
            static std::map<int, std::string> special_characters_map;
            if (special_characters_map.empty()) {
                for (auto &&sc : SPECIAL_CHARACTERS) {
                    special_characters_map.emplace(
                        static_cast<int>(sc), std::string("\\") + std::string(1, sc));
                }
            }

            if (SPECIAL_CHARACTERS.find(c) != std::string_view::npos) {
                result_string += special_characters_map[static_cast<int>(c)];
            } else {
                result_string += c;
            }
        }
    }

    if (match_with_parent) {
        // GlobPattern::Match() should check parent paths as well.
        char c = pattern.back();
        if (c != '\\' && c != '/')
            result_string += R"(([\\/].*)?$)";
    } else {
        result_string.push_back('$');
    }
    return result_string;
}

GlobPattern::GlobPattern(const std::string& glob_pattern, bool match_with_parent) {
    std::string re_pattern_str = translate(glob_pattern, match_with_parent);
    m_re_pattern = RegexCompile(re_pattern_str);
}

bool GlobPattern::Match(const std::string& path) const {
    regex_match re_result = RegexCreateMatchData(m_re_pattern);
    return RegexMatch(m_re_pattern, path, re_result);
}
