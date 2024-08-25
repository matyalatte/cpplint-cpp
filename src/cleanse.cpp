#include "cleanse.h"
#include <cassert>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "cpplint_state.h"
#include "line_utils.h"
#include "regex_utils.h"
#include "string_utils.h"

/*
  Alternative tokens and their replacements.  For full list, see section 2.5
  Alternative tokens [lex.digraph] in the C++ standard.

  Digraphs (such as "%:") are not included here since it's a mess to
  match those on a word boundary.
*/
const std::map<std::string, const std::string> ALT_TOKEN_REPLACEMENT = {
    { "and", "&&" },
    { "bitor", "|" },
    { "or", "||" },
    { "xor", "^" },
    { "compl", "~" },
    { "bitand", "&" },
    { "and_eq", "&=" },
    { "or_eq", "|=" },
    { "xor_eq", "^=" },
    { "not", "!" },
    { "not_eq", "!=" },
};

const std::string& AltTokenToToken(const std::string& alt_token) {
    auto it = ALT_TOKEN_REPLACEMENT.find(alt_token);
    assert(it != ALT_TOKEN_REPLACEMENT.end());
    return it->second;
}

/*
  Regular expression that matches all the above keywords.  The "[ =()]"
  bit is meant to avoid matching these keywords outside of boolean expressions.

  False positives include C-style multi-line comments and multi-line strings
  but those have always been troublesome for cpplint.
*/
std::string GetReAltTokenReplacement() {
    std::string pattern = "([ =()])(";
    for (auto& item : ALT_TOKEN_REPLACEMENT) {
        pattern += item.first + "|";
    }
    pattern.pop_back();
    pattern += ")([ (]|$)";
    return pattern;
}

const regex_code RE_PATTERN_INCLUDE = RegexCompile(R"(^\s*#\s*include\s*([<"])([^>"]*)[>"].*$)");
static const regex_code RE_PATTERN_ALT_TOKEN_REPLACEMENT =
    RegexCompile(GetReAltTokenReplacement());

std::vector<std::string>
CleansedLines::CleanseRawStrings(const std::vector<std::string>& raw_lines) {
    std::string delimiter = "";
    std::vector<std::string> lines_without_raw_strings;
    lines_without_raw_strings.reserve(raw_lines.size());

    for (const std::string& line : raw_lines) {
        std::string new_line = line;
        if (!delimiter.empty()) {
            // Inside a raw string, look for the end
            size_t end = line.find(delimiter);
            if (end != std::string::npos) {
                // Found the end of the string, match leading space for this
                // line and resume copying the original lines, and also insert
                // a "" on the last line.
                RegexMatch(R"(^(\s*)\S)", line, m_re_result);
                new_line = GetMatchStr(m_re_result, line, 1) + "\"\"" +
                           line.substr(end + delimiter.size());
                delimiter = "";
            } else {
                // Haven't found the end yet, append a blank line.
                new_line = "\"\"";
            }
        }

        // Look for beginning of a raw string, and replace them with
        // empty strings.  This is done in a loop to handle multiple raw
        // strings on the same line.
        while (delimiter.empty()) {
            /*  Look for beginning of a raw string.
                See 2.14.15 [lex.string] for syntax.
                Once we have matched a raw string, we check the prefix of the
                line to make sure that the line is not part of a single line
                comment.  It's done this way because we remove raw strings
                before removing comments as opposed to removing comments
                before removing raw strings.  This is because there are some
                cpplint checks that requires the comments to be preserved, but
                we don't want to check comments that are inside raw strings.
            */
            static const regex_code RE_PATTERN_RAW_STR =
                RegexJitCompile(R"---(^(.*?)\b(?:R|u8R|uR|UR|LR)"([^\s\\()]*)\((.*)$)---");
            static const regex_code RE_PATTERN_RAW_STR_SINGLE_LINE =
                RegexCompile(R"---(^([^\'"]|\'(\\.|[^\'])*\'|"(\\.|[^"])*")*//)---");

            bool matched = RegexJitSearch(RE_PATTERN_RAW_STR,
                                          new_line, m_re_result);
            if (matched) {
                std::string match_str_1 = GetMatchStr(m_re_result, new_line, 1);
                if (RegexMatch(RE_PATTERN_RAW_STR_SINGLE_LINE, match_str_1))
                    break;

                delimiter = ")" + GetMatchStr(m_re_result, new_line, 2) + "\"";
                std::string_view match_str_3 = GetMatchStrView(m_re_result, new_line, 3);
                size_t end = match_str_3.find(delimiter);
                if (end != std::string::npos) {
                    // Raw string ended on same line
                    new_line = match_str_1 + "\"\"" +
                               std::string(match_str_3.substr(end + delimiter.size()));
                    delimiter = "";
                } else {
                    // Start of a multi-line raw string
                    new_line = match_str_1 + "\"\"";
                }
            } else {
                break;
            }
        }

        lines_without_raw_strings.emplace_back(std::move(new_line));
    }

  // TODO(unknown): if delimiter is not None here, we might want to
  // emit a warning for unterminated string.
  return lines_without_raw_strings;
}

// Match a single C style comment on the same line.
#define RE_PATTERN_C_COMMENTS R"(/\*(?:[^*]|\*(?!/))*\*/)"

std::string CleanseComments(const std::string& line, bool* is_comment) {
    size_t commentpos = line.find("//");
    std::string new_line = line;
    if (commentpos != std::string::npos) {
        std::string sub_line = line.substr(0, commentpos);
        if (!IsCppString(sub_line)) {
            new_line = StrRstrip(sub_line);
            *is_comment = true;
        }
    }

    /* Matches multi-line C style comments.
    This RE is a little bit more complicated than one might expect, because we
    have to take care of space removals tools so we can handle comments inside
    statements better.
    The current rule is: We only clear spaces from both sides when we're at the
    end of the line. Otherwise, we try to remove spaces from the right side,
    if this doesn't work we try on left side but only if there's a non-character
    on the right.
    */
    static const regex_code RE_PATTERN_CLEANSE_LINE_C_COMMENTS =
        RegexCompile(
            R"((\s*)" RE_PATTERN_C_COMMENTS R"(\s*$|)"
            RE_PATTERN_C_COMMENTS R"(\s+|)"
            R"(\s+)" RE_PATTERN_C_COMMENTS R"((?=\W)|)"
            RE_PATTERN_C_COMMENTS ")");

    // get rid of /* ... */
    bool replaced = false;
    RegexReplace(RE_PATTERN_CLEANSE_LINE_C_COMMENTS, "", &new_line, &replaced);
    if (replaced)
        *is_comment = true;
    return new_line;
}

std::string CleansedLines::ReplaceAlternateTokens(const std::string& line) {
    std::string str = line;
    std::string ret = line;
    while (!str.empty()) {
        bool match = RegexMatch(RE_PATTERN_ALT_TOKEN_REPLACEMENT, str, m_re_result);
        if (!match)
            break;  // replaced all tokens
        const std::string& key = GetMatchStr(m_re_result, str, 2);
        const std::string& token = AltTokenToToken(key);
        std::string tail = ((key == "not" || key == "compl") &&
                            StrIsChar(GetMatchStrView(m_re_result, str, 3), ' ')) ? "" : "\\3";
        // replace the found token
        RegexReplace(RE_PATTERN_ALT_TOKEN_REPLACEMENT,
                     std::string("\\1") + token + tail, &ret, false);
        // remove the replaced part from str
        str = str.substr(GetMatchEnd(m_re_result, 0));
    }
    return ret;
}

std::string CleansedLines::CollapseStrings(const std::string& elided) {
    if (RegexMatch(RE_PATTERN_INCLUDE, elided))
        return elided;

    // Matches standard C++ escape sequences per 2.13.2.3 of the C++ standard.
    static const regex_code RE_PATTERN_CLEANSE_LINE_ESCAPES =
        RegexCompile(R"(\\([abfnrtv?"\\\']|\d+|x[0-9a-fA-F]+))");

    std::string new_elided = elided;
    // Remove escaped characters first to make quote/single quote collapsing
    // basic.  Things that look like escaped characters shouldn't occur
    // outside of strings and chars.
    RegexReplace(RE_PATTERN_CLEANSE_LINE_ESCAPES, "", &new_elided);

    // Replace quoted strings and digit separators.  Both single quotes
    // and double quotes are processed in the same loop, otherwise
    // nested quotes wouldn't work.
    std::string collapsed = "";
    while (1) {
        // Find the first quote character
        static const regex_code RE_PATTERN_QUOTE =
            RegexCompile(R"(^([^\'"]*)([\'"])(.*)$)");
        bool match = RegexMatch(RE_PATTERN_QUOTE, new_elided, m_re_result);
        if (!match) {
            collapsed += new_elided;
            break;
        }
        std::string_view head = GetMatchStrView(m_re_result, new_elided, 1);
        bool has_double_quote = new_elided[GetMatchStart(m_re_result, 2)] == '"';
        std::string_view tail = GetMatchStrView(m_re_result, new_elided, 3);

        if (has_double_quote) {
            // Collapse double quoted strings
            size_t second_quote = tail.find('"');
            if (second_quote != std::string::npos) {
                collapsed += std::string(head) + R"("")";
                new_elided = tail.substr(second_quote + 1);
            } else {
                // Unmatched double quote, don't bother processing the rest
                // of the line since this is probably a multiline string.
                collapsed += new_elided;
                break;
            }
        } else {
            /*Found single quote, check nearby text to eliminate digit separators.
            There is no special handling for floating point here, because
            the integer/fractional/exponent parts would all be parsed
            correctly as long as there are digits on both sides of the
            separator.  So we are fine as long as we don't see something
            like "0.'3" (gcc 4.9.0 will not allow this literal).
            */
            static const regex_code RE_PATTERN_DIGIT =
                RegexJitCompile(R"(\b(?:0[bBxX]?|[1-9])[0-9a-fA-F]*$)");
            if (RegexJitSearch(RE_PATTERN_DIGIT, head)) {
                std::string subject = "'" + std::string(tail);
                static const regex_code RE_PATTERN_DIGIT2 =
                    RegexCompile(R"(^((?:\'?[0-9a-zA-Z_])*)(.*)$)");
                RegexMatch(RE_PATTERN_DIGIT2, subject, m_re_result);
                collapsed += std::string(head) +
                             StrReplaceAll(GetMatchStr(m_re_result, subject, 1), "'", "");
                new_elided = GetMatchStr(m_re_result, subject, 2);
            } else {
                size_t second_quote = tail.find("\'");
                if (second_quote != std::string::npos) {
                    collapsed += std::string(head) + "''";
                    new_elided = tail.substr(second_quote + 1);
                } else {
                    // Unmatched single quote
                    collapsed += new_elided;
                    break;
                }
            }
        }
    }
    return collapsed;
}

CleansedLines::CleansedLines(std::vector<std::string>& lines,
                             const Options& options) :
                             m_elided({}),
                             m_lines({}),
                             m_raw_lines(lines),
                             m_has_comment(lines.size(), false),
                             m_re_result(RegexCreateMatchData(16)) {
    if (!options.ShouldPrintError("readability/alt_tokens", "", INDEX_NONE)) {
        for (std::string& line : m_raw_lines) {
            line = ReplaceAlternateTokens(line);
        }
    }
    m_lines.reserve(lines.size());
    m_elided.reserve(lines.size());
    m_lines_without_raw_strings = CleanseRawStrings(m_raw_lines);
    size_t linenum = 0;
    for (const std::string& line : m_lines_without_raw_strings) {
        bool is_comment = false;
        std::string comment_removed = CleanseComments(line, &is_comment);
        if (is_comment) {
            m_has_comment[linenum] = true;
        }
        std::string elided = CollapseStrings(comment_removed);
        m_lines.emplace_back(std::move(comment_removed));
        m_elided.emplace_back(std::move(elided));
        linenum++;
    }
}
