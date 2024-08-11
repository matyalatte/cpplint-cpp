#pragma once
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "common.h"

class CppLintState {
    // Maintains module-wide state..
 private:
    int m_verbose_level;  // global setting
    int m_error_count;  // global count of reported errors

    std::string m_counting;  // In what way are we counting errors?
    // string to int dict storing error counts
    std::map<std::string, int> m_errors_by_category;
    bool m_quiet;  // Suppress non-error messagess?

    /* output format:
     * "emacs" - format that emacs can parse (default)
     * "eclipse" - format that eclipse can parse
     * "vs7" - format that Microsoft Visual Studio 7 can parse
     * "junit" - format that Jenkins, Bamboo, etc can parse
     * "sed" - returns a gnu sed command to fix the problem
     * "gsed" - like sed, but names the command gsed, e.g. for macOS homebrew users
     */
    std::string m_output_format;

    // TODO(matyalatte): Support JUnit output
    // For JUnit output, save errors and failures until the end so that they
    // can be written into the XML
    // std::vector<std::string> m_junit_errors;
    // std::vector<JunitFailure> m_junit_failures;

    std::mutex m_mtx;

 public:
    CppLintState();

    const std::string& OutputFormat() const { return m_output_format; }
    void SetOutputFormat(const std::string& output_format) {
        // Sets the output format for errors.
        m_output_format = output_format;
    }

    bool Quiet() const { return m_quiet; }
    bool SetQuiet(bool quiet) {
        // Sets the module's quiet settings, and returns the previous setting.
        bool last_quiet = m_quiet;
        m_quiet = quiet;
        return last_quiet;
    }

    int VerboseLevel() const { return m_verbose_level; }
    int SetVerboseLevel(int level) {
        // Sets the module's verbosity, and returns the previous setting.
        int last_verbose_level = m_verbose_level;
        m_verbose_level = level;
        return last_verbose_level;
    }

    void SetCountingStyle(const std::string& counting_style) {
        // Sets the module's counting options.
        m_counting = counting_style;
    }

    void ResetErrorCounts() {
        // Sets the module's error statistic back to zero.
        m_error_count = 0;
        m_errors_by_category.clear();
    }

    int ErrorCount() const { return m_error_count; }
    int ErrorCount(const std::string& category) const;

    // Bumps the module's error statistic.
    void IncrementErrorCount(const std::string& category);

    // Outputs an error.
    // This should be called from FileLinter::Error to check filters
    void Error(const std::string& filename, size_t linenum,
               const std::string& category, int confidence,
               const std::string& message);

    // Print a summary of errors by category, and the total.
    void PrintErrorCounts();
    void PrintInfo(const std::string& message);
    void PrintError(const std::string& message);

    bool AddJUnitFailure(const std::string& filename,
                         size_t linenum,
                         const std::string& message,
                         const std::string& category,
                         int confidence) {
        UNUSED(filename);
        UNUSED(linenum);
        UNUSED(message);
        UNUSED(category);
        UNUSED(confidence);
        return false;
    }

    std::string FormatJUnitXML() { return ""; }
};
