#pragma once
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "common.h"

enum : int {
    COUNT_TOTAL,
    COUNT_TOPLEVEL,
    COUNT_DETAILED,
    COUNT_MAX,
};

enum : int {
    OUTPUT_EMACS,
    OUTPUT_VS7,
    OUTPUT_ECLIPSE,
    OUTPUT_JUNIT,
    OUTPUT_SED,
    OUTPUT_GSED,
    OUTPUT_MAX,
};

class CppLintState {
    // Maintains module-wide state..
 private:
    int m_verbose_level;  // global setting
    int m_error_count;  // global count of reported errors

    int m_counting;  // In what way are we counting errors?
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
    int m_output_format;

    // TODO(matyalatte): Support JUnit output
    // For JUnit output, save errors and failures until the end so that they
    // can be written into the XML
    // std::vector<std::string> m_junit_errors;
    // std::vector<JunitFailure> m_junit_failures;

    std::mutex m_mtx;

    int m_num_threads;

 public:
    CppLintState();

    [[nodiscard]] int OutputFormat() const { return m_output_format; }
    void SetOutputFormat(const std::string& output_format) {
        // Sets the output format for errors.
        if (output_format == "vs7")
            m_output_format = OUTPUT_VS7;
        else if (output_format == "eclipse")
            m_output_format = OUTPUT_ECLIPSE;
        else if (output_format == "junit")
            m_output_format = OUTPUT_JUNIT;
        else if (output_format == "sed")
            m_output_format = OUTPUT_SED;
        else if (output_format == "gsed")
            m_output_format = OUTPUT_GSED;
        else
            m_output_format = OUTPUT_EMACS;
    }

    [[nodiscard]] bool Quiet() const { return m_quiet; }
    bool SetQuiet(bool quiet) {
        // Sets the module's quiet settings, and returns the previous setting.
        bool last_quiet = m_quiet;
        m_quiet = quiet;
        return last_quiet;
    }

    [[nodiscard]] int VerboseLevel() const { return m_verbose_level; }
    int SetVerboseLevel(int level) {
        // Sets the module's verbosity, and returns the previous setting.
        int last_verbose_level = m_verbose_level;
        m_verbose_level = level;
        return last_verbose_level;
    }

    void SetCountingStyle(const std::string& counting_style) {
        // Sets the module's counting options.
        if (counting_style == "toplevel")
            m_counting = COUNT_TOPLEVEL;
        else if (counting_style == "detailed")
            m_counting = COUNT_DETAILED;
        else
            m_counting = COUNT_TOTAL;
    }

    void ResetErrorCounts() {
        // Sets the module's error statistic back to zero.
        m_error_count = 0;
        m_errors_by_category.clear();
    }

    [[nodiscard]] int ErrorCount() const { return m_error_count; }
    [[nodiscard]] int ErrorCount(const std::string& category) const;

    void SetNumThreads(int num_threads) { m_num_threads = num_threads; }
    [[nodiscard]] int GetNumThreads() const { return m_num_threads; }

    // Bumps the module's error statistic.
    void IncrementErrorCount(const std::string& category);

    // Outputs an error.
    // This should be called from FileLinter::Error to check filters
    void Error(const std::string& filename, size_t linenum,
               const std::string& category, int confidence,
               const std::string& message);

    // Flush buffers for cout and cerr
    void FlushThreadStream();

    // Get error buffer as string
    std::string GetErrorStreamAsStr();

    // Print a summary of errors by category, and the total.
    void PrintErrorCounts();
    void PrintInfo(const std::string& message) const;
    void PrintError(const std::string& message) const;

    static bool AddJUnitFailure(const std::string& filename,
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

    static std::string FormatJUnitXML() { return ""; }
};
