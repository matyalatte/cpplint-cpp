#include "cpplint_state.h"
#include <cassert>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "string_utils.h"

CppLintState::CppLintState() :
    m_verbose_level(1),
    m_error_count(0),
    m_counting(COUNT_TOTAL),
    m_errors_by_category({}),
    m_quiet(false),
    m_output_format(OUTPUT_EMACS) {}

void CppLintState::IncrementErrorCount(const std::string& category) {
    std::string cat = category;
    m_error_count += 1;
    if (m_counting == COUNT_TOTAL)
        return;  // No need for detailed error counts.
    if (m_counting == COUNT_TOPLEVEL)
        cat = StrBeforeChar(cat, '/');
    auto it = m_errors_by_category.find(cat);
    if (it == m_errors_by_category.end())
        m_errors_by_category[cat] = 1;
    else
        it->second += 1;
}

int CppLintState::ErrorCount(const std::string& category) const {
    std::string cat = category;
    if (m_counting == COUNT_TOTAL)
        return 0;
    if (m_counting == COUNT_TOPLEVEL)
        cat = StrBeforeChar(cat, '/');
    auto it = m_errors_by_category.find(cat);
    if (it == m_errors_by_category.end())
        return 0;
    else
        return it->second;
}

void CppLintState::PrintErrorCounts() {
    for (const auto& item : m_errors_by_category) {
        PrintInfo("Category \'" + item.first +
                  "\' errors found: " + std::to_string(item.second) + "\n");
    }
    if (m_error_count > 0) {
        PrintInfo("Total errors found: " + std::to_string(m_error_count) + "\n");
    }
}

void CppLintState::PrintInfo(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mtx);

    // _quiet does not represent --quiet flag.
    // Hide infos from stdout to keep stdout pure for machine consumption
    if (m_output_format != OUTPUT_JUNIT &&
        m_output_format != OUTPUT_SED &&
        m_output_format != OUTPUT_GSED)
        std::cout << message;
}

void CppLintState::PrintError(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mtx);

    if (m_output_format == OUTPUT_JUNIT) {
        // m_junit_errors.push_back(message);
    } else {
        std::cerr << message;
    }
}

const std::map<std::string, std::string> SED_FIXUPS = {
    { "Remove spaces around =", R"(s/ = /=/)" },
    { "Remove spaces around !=", R"(s/ != /!=/)" },
    { "Remove space before ( in if (", R"(s/if (/if(/)" },
    { "Remove space before ( in for (", R"(s/for (/for(/)" },
    { "Remove space before ( in while (", R"(s/while (/while(/)" },
    { "Remove space before ( in switch (", R"(s/switch (/switch(/)" },
    { "Should have a space between // and comment", R"(s/\/\//\/\/ /)" },
    { "Missing space before {", R"(s/\([^ ]\){/\1 {/)" },
    { "Tab found, replace by spaces", R"(s/\t/  /g)" },
    { "Line ends in whitespace.  Consider deleting these extra spaces.", R"(s/\s*$//)" },
    { "You don't need a ; after a }", R"(s/};/}/)" },
    { "Missing space after ,", R"(s/,\([^ ]\)/, \1/g)" },
};

void CppLintState::Error(const std::string& filename, size_t linenum,
           const std::string& category, int confidence,
           const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mtx);

    IncrementErrorCount(category);
    if (m_output_format == OUTPUT_VS7) {
        std::cerr << filename << "(" << linenum << "): error cpplint: [" <<
                     category << "] " << message << " [" << confidence << "]\n";
    } else if (m_output_format == OUTPUT_ECLIPSE) {
        std::cerr << filename << ":" << linenum << ": warning: " <<
                     message << "  [" << category << "] [" << confidence << "]\n";
    } else if (m_output_format == OUTPUT_JUNIT) {
        bool res = AddJUnitFailure(filename, linenum, message, category, confidence);
        if (!res)
            std::cerr << "Failed to add a JUnit failure\n";
    } else if (m_output_format == OUTPUT_SED ||
               m_output_format == OUTPUT_GSED) {
        auto it = SED_FIXUPS.find(message);
        if (it == SED_FIXUPS.end()) {
            if (m_output_format == OUTPUT_SED)
                std::cout << "sed";
            else
                std::cout << "gsed";
            std::cout << " -i" <<
                         " '" << linenum << it->second << "' " << filename <<
                         " # " << message << "  [" << category << "] [" << confidence << "]\n";
        } else {
            std::cerr << "# " << filename << ":" << linenum << ": " <<
                         " \"" << message << "\"  [" << category << "] [" << confidence << "]\n";
        }
    } else {
        std::cerr << filename << ":" << linenum << ":  " << message << "  [" <<
                     category << "] [" << confidence << "]\n";
    }
}

