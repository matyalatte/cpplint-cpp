#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include "cpplint_state.h"

namespace fs = std::filesystem;

// The default state of the category filter. This is overridden by the --filter=
// flag. By default all errors are on, so only add here categories that should be
// off by default (i.e., categories that must be enabled by the --filter= flags).
// All entries here should start with a '-' or '+', as in the --filter= flag.
const std::vector<std::string> DEFAULT_FILTERS = {
    "-build/include_alpha",
};

class Options {
 private:
    fs::path m_root;
    fs::path m_repository;
    size_t m_line_length;
    std::string m_config_filename;
    std::set<std::string> m_valid_extensions;
    std::set<std::string> m_hpp_headers;
    std::string m_include_order;

    // filters to apply when emitting error messages
    std::vector<std::string> m_filters;

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
        m_include_order("default"),
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

    const std::string& IncludeOrder() const { return m_include_order; }

    const std::vector<std::string>& Filters() const { return m_filters; }

    // Adds filters to the existing list of error-message filters.
    bool AddFilters(const std::string& filters);

    // Checks if the error is filtered or not.
    bool ShouldPrintError(const std::string& category,
                          const std::string& filename, size_t linenum);
};
