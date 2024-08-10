#pragma once
#include <string>
#include <vector>
#include "options.h"
#include "regex_utils.h"
#include "string_utils.h"

extern const regex_code RE_PATTERN_INCLUDE;

extern const regex_code RE_PATTERN_ALT_TOKEN_REPLACEMENT;

// ALT_TOKEN_REPLACEMENT[alt_token]
const std::string& AltTokenToToken(const std::string& alt_token);

/*Does line terminate so, that the next symbol is in string constant.

  This function does not consider single-line nor multi-line comments.

  Args:
    line: is a partial line of code starting from the 0..n.

  Returns:
    True, if next character appended to 'line' is inside a
    string constant.
*/
bool IsCppString(const std::string& line);

// Removes //-comments and single-line C-style /* */ comments.
std::string CleanseComments(const std::string& line, bool* is_comment,
                            regex_match& re_result_temp);

class CleansedLines {
    /*Holds 4 copies of all lines with different preprocessing applied to them.

    1) elided member contains lines without strings and comments.
    2) lines member contains lines without comments.
    3) raw_lines member contains all the lines without processing.
    4) lines_without_raw_strings member is same as raw_lines, but with C++11 raw
        strings removed.
    All these members are of <type 'list'>, and of the same length.
    */
 private:
    std::vector<std::string> m_elided;
    std::vector<std::string> m_lines;
    std::vector<std::string>& m_raw_lines;
    std::vector<std::string> m_lines_without_raw_strings;
    std::vector<bool> m_has_comment;
    regex_match m_re_result;

 public:
    CleansedLines(std::vector<std::string>& lines,
                  const Options& options);

    /*Removes C++11 raw strings from lines.

    Before:
      static const char kData[] = R"(
          multi-line string
          )";

    After:
      static const char kData[] = ""
          (replaced by blank line)
          "";
    */
    std::vector<std::string> CleanseRawStrings(const std::vector<std::string>& raw_lines);

    /*
      Collapses strings and chars on a line to simple "" or '' blocks.
      We nix strings first so we're not fooled by text like '"http://"'
    */
    std::string CollapseStrings(const std::string& elided);

    /*
    Replace any alternate token by its original counterpart.

    In order to comply with the google rule stating that unary operators should
    never be followed by a space, an exception is made for the 'not' and 'compl'
    alternate tokens. For these, any trailing space is removed during the
    conversion.
    */
    std::string ReplaceAlternateTokens(const std::string& line);

    size_t NumLines() const { return m_lines.size(); }

    const std::string& GetLineAt(size_t id) const { return m_lines[id]; }
    const std::string& GetElidedAt(size_t id) const { return m_elided[id]; }
    const std::string& GetRawLineAt(size_t id) const { return m_raw_lines[id]; }
    const std::string& GetLineWithoutRawStringAt(size_t id) const {
        return m_lines_without_raw_strings[id];
    }
    const std::vector<std::string>& GetLinesWithoutRawStrings() const {
        return m_lines_without_raw_strings;
    }

    bool HasComment(size_t id) const { return m_has_comment[id]; }
};
