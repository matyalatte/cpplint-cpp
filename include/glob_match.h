#pragma once
#include <string>
#include "common.h"
#include "regex_utils.h"

class GlobPattern {
 private:
    regex_code m_re_pattern;

 public:
    // When match_with_parent is true,
    // GlobPattern::Match() checks parent paths as well.
    explicit GlobPattern(const std::string& glob_pattern, bool match_with_parent = false);

    // Returns if a path matches with a glob pattern or not.
    [[nodiscard]] bool Match(const std::string& path) const;
};
