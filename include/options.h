#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include "cpplint_state.h"

namespace fs = std::filesystem;

// Class for filters options e.g., "-build/c++11"
class Filter {
    bool m_sign;  // + or -
    std::string m_category;
    std::string m_file;
    size_t m_linenum;

 public:
    Filter() :
        m_sign(false),
        m_category(""),
        m_file(""),
        m_linenum(INDEX_NONE) {}

    explicit Filter(const std::string& filter) {
        ParseFilterSelector(filter);
    }

    void ParseFilterSelector(const std::string& filter);

    bool IsPositive() const { return m_sign; }

    bool IsMatched(const std::string& category,
                   const std::string& file,
                   size_t linenum) const {
        return category.starts_with(m_category) &&
               (m_file.empty() || m_file == file) &&
               (m_linenum == linenum || m_linenum == INDEX_NONE);
    }
};

// The default state of the category filter. This is overridden by the --filter=
// flag. By default all errors are on, so only add here categories that should be
// off by default (i.e., categories that must be enabled by the --filter= flags).
// All entries here should start with a '-' or '+', as in the --filter= flag.
const std::vector<Filter> DEFAULT_FILTERS = {
    Filter("-build/include_alpha"),
};

enum : int {
    INCLUDE_ORDER_DEFAULT,
    INCLUDE_ORDER_STDCFIRST,
    INCLUDE_ORDER_MAX,
};

class Options {
 private:
    fs::path m_root;
    fs::path m_repository;
    size_t m_line_length;
    std::string m_config_filename;
    std::set<std::string> m_valid_extensions;
    std::set<std::string> m_hpp_headers;
    int m_include_order;
    bool m_timing;

    // filters to apply when emitting error messages
    std::vector<Filter> m_filters;

    // Parse --extensions option
    void ProcessExtensionsOption(const std::string& val);
    // Parse --headers option
    void ProcessHppHeadersOption(const std::string& val);
    // Parse --includeorder option
    void ProcessIncludeOrderOption(const std::string& val);

    // Searches a list of filenames and replaces directories in the list with
    // all files descending from those directories. Files with extensions not in
    // the valid extensions list are excluded.
    std::vector<fs::path> ExpandDirectories(const std::vector<fs::path>& filenames);

    // Filters out files listed in the --exclude command line switch. File paths
    // in the switch are evaluated relative to the current working directory
    std::vector<fs::path> FilterExcludedFiles(std::vector<fs::path> filenames,
                                              const std::vector<fs::path>& excludes);

 public:
    Options() :
        m_root(""),
        m_repository(""),
        m_line_length(80),
        m_config_filename("CPPLINT.cfg"),
        m_valid_extensions({}),
        m_hpp_headers({}),
        m_include_order(INCLUDE_ORDER_DEFAULT),
        m_timing(false),
        m_filters(DEFAULT_FILTERS)
        {}

    /*Parses the command line arguments.
      This may set the output format and verbosity level as side-effects.
    */
    std::vector<fs::path> ParseArguments(int argc, char** argv,
                                         CppLintState* cpplint_state);

    const fs::path& Root() const { return m_root; }
    const fs::path& Repository() const { return m_repository; }
    size_t LineLength() const { return m_line_length; }

    std::set<std::string> GetAllExtensions() const;
    bool IsHeaderExtension(const std::string& file_extension) const;
    bool IsSourceExtension(const std::string& file_extension) const;
    std::set<std::string> GetHeaderExtensions() const;
    std::set<std::string> GetNonHeaderExtensions() const;

    bool ProcessConfigOverrides(const fs::path& filename,
                                CppLintState* cpplint_state);

    void PrintUsage(const std::string& message = "");

    int IncludeOrder() const { return m_include_order; }

    const std::vector<Filter>& Filters() const { return m_filters; }

    // Adds filters to the existing list of error-message filters.
    bool AddFilters(const std::string& filters);

    // Checks if the error is filtered or not.
    bool ShouldPrintError(const std::string& category,
                          const std::string& filename, size_t linenum) const;

    bool Timing() const { return m_timing; }
};
