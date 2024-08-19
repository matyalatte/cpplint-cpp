#include "options.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "cpplint_state.h"
#include "error_suppressions.h"
#include "regex_utils.h"
#include "string_utils.h"
#include "version.h"

namespace fs = std::filesystem;

static const char* USAGE[] = {
    "Syntax: cpplint.cpp [--verbose=#] [--output=emacs|eclipse|vs7|junit|sed|gsed]\n"
    "                    [--filter=-x,+y,...]\n"
    "                    [--counting=total|toplevel|detailed] [--root=subdir]\n"
    "                    [--repository=path]\n"
    "                    [--linelength=digits] [--headers=x,y,...]\n"
    "                    [--recursive]\n"
    "                    [--exclude=path]\n"
    "                    [--extensions=hpp,cpp,...]\n"
    "                    [--includeorder=default|standardcfirst]\n"
    "                    [--config=filename]\n"
    "                    [--quiet]\n"
    "                    [--version]\n"
    "                    [--timing]\n"
    "                    [--threads=#]\n"
    "                    <file> [file] ...\n"
    "\n"
    "  Style checker for C/C++ source files.\n"
    "  This is a reimplementation of the Google style checker with minor extensions.\n"
    "\n"
    "  The style guidelines this tries to follow are those in\n"
    "\n"
    "  Every problem is given a confidence score from 1-5, with 5 meaning we are\n"
    "  certain of the problem, and 1 meaning it could be a legitimate construct.\n"
    "  This will miss some errors, and is not a substitute for a code review.\n"
    "\n"
    "  To suppress false-positive errors of certain categories, add a\n"
    "  'NOLINT(category[, category...])' comment to the line.  NOLINT or NOLINT(*)\n"
    "  suppresses errors of all categories on that line. To suppress categories\n"
    "  on the next line use NOLINTNEXTLINE instead of NOLINT. To suppress errors in\n"
    "  a block of code 'NOLINTBEGIN(category[, category...])' comment to a line at\n"
    "  the start of the block and to end the block add a comment with 'NOLINTEND'.\n"
    "  NOLINT blocks are inclusive so any statements on the same line as a BEGIN\n"
    "  or END will have the error suppression applied.\n"
    "\n"
    "  The files passed in will be linted; at least one file must be provided.\n"
    "  Default linted extensions are ", ".\n"
    "  Other file types will be ignored.\n"
    "  Change the extensions with the --extensions flag.\n"
    "\n"
    "  Flags:\n"
    "\n"
    "    output=emacs|eclipse|vs7|junit|sed|gsed\n"
    "      By default, the output is formatted to ease emacs parsing.  Visual Studio\n"
    "      compatible output (vs7) may also be used.  Further support exists for\n"
    "      eclipse (eclipse), and JUnit (junit). XML parsers such as those used\n"
    "      in Jenkins and Bamboo may also be used.\n"
    "      The sed format outputs sed commands that should fix some of the errors.\n"
    "      Note that this requires gnu sed. If that is installed as gsed on your\n"
    "      system (common e.g. on macOS with homebrew) you can use the gsed output\n"
    "      format. Sed commands are written to stdout, not stderr, so you should be\n"
    "      able to pipe output straight to a shell to run the fixes.\n"
    "\n"
    "    verbose=#\n"
    "      Specify a number 0-5 to restrict errors to certain verbosity levels.\n"
    "      Errors with lower verbosity levels have lower confidence and are more\n"
    "      likely to be false positives.\n"
    "\n"
    "    quiet\n"
    "      Don't print anything if no errors are found.\n"
    "\n"
    "    filter=-x,+y,...\n"
    "      Specify a comma-separated list of category-filters to apply: only\n"
    "      error messages whose category names pass the filters will be printed.\n"
    "      (Category names are printed with the message and look like\n"
    "      \"[whitespace/indent]\".)  Filters are evaluated left to right.\n"
    "      \"-FOO\" means \"do not print categories that start with FOO\".\n"
    "      \"+FOO\" means \"do print categories that start with FOO\".\n"
    "\n"
    "      Examples: --filter=-whitespace,+whitespace/braces\n"
    "                --filter=-whitespace,-runtime/printf,+runtime/printf_format\n"
    "                --filter=-,+build/include_what_you_use\n"
    "\n"
    "      To see a list of all the categories used in cpplint, pass no arg:\n"
    "         --filter=\n"
    "\n"
    "      Filters can directly be limited to files and also line numbers. The\n"
    "      syntax is category:file:line , where line is optional. The filter limitation\n"
    "      works for both + and - and can be combined with ordinary filters:\n"
    "\n"
    "      Examples: --filter=-whitespace:foo.h,+whitespace/braces:foo.h\n"
    "                --filter=-whitespace,-runtime/printf:foo.h:14,+runtime/printf_format:foo.h\n"
    "                --filter=-,+build/include_what_you_use:foo.h:321\n"
    "\n"
    "    counting=total|toplevel|detailed\n"
    "      The total number of errors found is always printed. If\n"
    "      'toplevel' is provided, then the count of errors in each of\n"
    "      the top-level categories like 'build' and 'whitespace' will\n"
    "      also be printed. If 'detailed' is provided, then a count\n"
    "      is provided for each category like 'legal/copyright'.\n"
    "\n"
    "    repository=path\n"
    "      The top level directory of the repository, used to derive the header\n"
    "      guard CPP variable. By default, this is determined by searching for a\n"
    "      path that contains .git, .hg, or .svn. When this flag is specified, the\n"
    "      given path is used instead. This option allows the header guard CPP\n"
    "      variable to remain consistent even if members of a team have different\n"
    "      repository root directories (such as when checking out a subdirectory\n"
    "      with SVN). In addition, users of non-mainstream version control systems\n"
    "      can use this flag to ensure readable header guard CPP variables.\n"
    "\n"
    "      Examples:\n"
    "        Assuming that Alice checks out ProjectName and Bob checks out\n"
    "        ProjectName/trunk and trunk contains src/chrome/ui/browser.h, then\n"
    "        with no --repository flag, the header guard CPP variable will be:\n"
    "\n"
    "        Alice => TRUNK_SRC_CHROME_BROWSER_UI_BROWSER_H_\n"
    "        Bob   => SRC_CHROME_BROWSER_UI_BROWSER_H_\n"
    "\n"
    "        If Alice uses the --repository=trunk flag and Bob omits the flag or\n"
    "        uses --repository=. then the header guard CPP variable will be:\n"
    "\n"
    "        Alice => SRC_CHROME_BROWSER_UI_BROWSER_H_\n"
    "        Bob   => SRC_CHROME_BROWSER_UI_BROWSER_H_\n"
    "\n"
    "    root=subdir\n"
    "      The root directory used for deriving header guard CPP variable.\n"
    "      This directory is relative to the top level directory of the repository\n"
    "      which by default is determined by searching for a directory that contains\n"
    "      .git, .hg, or .svn but can also be controlled with the --repository flag.\n"
    "      If the specified directory does not exist, this flag is ignored.\n"
    "\n"
    "      Examples:\n"
    "        Assuming that src is the top level directory of the repository (and\n"
    "        cwd=top/src), the header guard CPP variables for\n"
    "        src/chrome/browser/ui/browser.h are:\n"
    "\n"
    "        No flag => CHROME_BROWSER_UI_BROWSER_H_\n"
    "        --root=chrome => BROWSER_UI_BROWSER_H_\n"
    "        --root=chrome/browser => UI_BROWSER_H_\n"
    "        --root=.. => SRC_CHROME_BROWSER_UI_BROWSER_H_\n"
    "\n"
    "    linelength=digits\n"
    "      This is the allowed line length for the project. The default value is\n"
    "      80 characters.\n"
    "\n"
    "      Examples:\n"
    "        --linelength=120\n"
    "\n"
    "    recursive\n"
    "      Search for files to lint recursively. Each directory given in the list\n"
    "      of files to be linted is replaced by all files that descend from that\n"
    "      directory. Files with extensions not in the valid extensions list are\n"
    "      excluded.\n"
    "\n"
    "    exclude=path\n"
    "      Exclude the given path from the list of files to be linted. Relative\n"
    "      paths are evaluated relative to the current directory and shell globbing\n"
    "      is performed. This flag can be provided multiple times to exclude\n"
    "      multiple files.\n"
    "\n"
    "      Examples:\n"
    "        --exclude=one.cc\n"
    "        --exclude=src/*.cc\n"
    "        --exclude=src/*.cc --exclude=test/*.cc\n"
    "\n"
    "    extensions=extension,extension,...\n"
    "      The allowed file extensions that cpplint will check\n"
    "\n"
    "      Examples:\n"
    "        --extensions=", "\n"
    "\n"
    "    includeorder=default|standardcfirst\n"
    "      For the build/include_order rule, the default is to blindly assume angle\n"
    "      bracket includes with file extension are c-system-headers (default),\n"
    "      even knowing this will have false classifications.\n"
    "      The default is established at google.\n"
    "      standardcfirst means to instead use an allow-list of known c headers and\n"
    "      treat all others as separate group of \"other system headers\". The C headers\n"
    "      included are those of the C-standard lib and closely related ones.\n"
    "\n"
    "    config=filename\n"
    "      Search for config files with the specified name instead of CPPLINT.cfg\n"
    "\n"
    "    headers=x,y,...\n"
    "      The header extensions that cpplint will treat as .h in checks. Values are\n"
    "      automatically added to --extensions list.\n"
    "     (by default, only files with extensions ", " will be assumed to be headers)\n"
    "\n"
    "      Examples:\n"
    "        --headers=", "\n"
    "        --headers=hpp,hxx\n"
    "        --headers=hpp\n"
    "\n"
    "    timing\n"
    "      Display elapsed processing time.\n"
    "\n"
    "    threads=#\n"
    "      Specify a number of threads for multithreading.\n"
    "      You can use 0 or -1 for using all available threads."
    "\n"
    "      To see the number of available threads, pass no arg:\n"
    "         --threads=\n"
    "\n"
    "    cpplint.py supports per-directory configurations specified in CPPLINT.cfg\n"
    "    files. CPPLINT.cfg file can contain a number of key=value pairs.\n"
    "    Currently the following options are supported:\n"
    "\n"
    "      set noparent\n"
    "      filter=+filter1,-filter2,...\n"
    "      exclude_files=regex\n"
    "      linelength=80\n"
    "      root=subdir\n"
    "      headers=x,y,...\n"
    "\n"
    "    \"set noparent\" option prevents cpplint from traversing directory tree\n"
    "    upwards looking for more .cfg files in parent directories. This option\n"
    "    is usually placed in the top-level project directory.\n"
    "\n"
    "    The \"filter\" option is similar in function to --filter flag. It specifies\n"
    "    message filters in addition to the |_DEFAULT_FILTERS| and those specified\n"
    "    through --filter command-line flag.\n"
    "\n"
    "    \"exclude_files\" allows to specify a regular expression to be matched against\n"
    "    a file name. If the expression matches, the file is skipped and not run\n"
    "    through the linter.\n"
    "\n"
    "    \"linelength\" allows to specify the allowed line length for the project.\n"
    "\n"
    "    The \"root\" option is similar in function to the --root flag (see example\n"
    "    above). Paths are relative to the directory of the CPPLINT.cfg.\n"
    "\n"
    "    The \"headers\" option is similar in function to the --headers flag\n"
    "    (see example above).\n"
    "\n"
    "    CPPLINT.cfg has an effect on files in the same directory and all\n"
    "    sub-directories, unless overridden by a nested configuration file.\n"
    "\n"
    "      Example file:\n"
    "        filter=-build/include_order,+build/include_alpha\n"
    "        exclude_files=.*\\.cc\n"
    "\n"
    "    The above example disables build/include_order warning and enables\n"
    "    build/include_alpha as well as excludes all .cc from being\n"
    "    processed by linter, in the current directory (where the .cfg\n"
    "    file is located) and all sub-directories.\n"
};

void Options::PrintUsage(const std::string& message) {
    /*Prints a brief usage string and exits, optionally with an error message.

    Args:
        message: The optional error message.
    */

    std::string all_exts_as_list = SetToStr(GetAllExtensions());  // [h, hpp, cpp]
    std::string all_exts_as_opt = SetToStr(GetAllExtensions(), "", ",", "");  // h,hpp,cpp
    std::string header_exts_as_list = SetToStr(GetHeaderExtensions());  // [h, hpp]
    std::string header_exts_as_opt = SetToStr(GetHeaderExtensions(), "", ",", "");  // h,hpp

    std::ostream* ostream = &std::cout;
    int status = 0;

    if (message != "") {
        ostream = &std::cerr;
        status = 1;
    }

    *ostream << USAGE[0] << all_exts_as_list <<
                USAGE[1] << all_exts_as_opt <<
                USAGE[2] << header_exts_as_list <<
                USAGE[3] << header_exts_as_opt <<
                USAGE[4];
    if (message != "")
        *ostream << "\nFATAL ERROR: " << message << "\n";
    exit(status);
}

static void PrintVersion() {
    std::cout << "cpplint-cpp " << CPPLINT_VERSION <<
                 " " << PLATFORM_TAG << "\n"
                 "Reimplementation of cpplint.py " << ORIGINAL_VERSION << "\n";
    exit(0);
}

static void PrintCategories() {
    // Prints a list of all the error-categories used by error messages.
    // These are the categories used to filter messages via --filter.
    for (const char* const cat : ERROR_CATEGORIES) {
        std::cerr << "  " << cat << "\n";
    }
    exit(1);
}

static int GetNumThreads() {
    auto num = std::thread::hardware_concurrency();
    if (num < 1) {  // num can be zero.
        std::cout << "Warning: Failed to get the number of available threads.\n";
        num = 1;
    }
    return static_cast<int>(num);
}

static void PrintNumThreads() {
    std::cout << "Number of threads: " << GetNumThreads() << "\n";
    exit(0);
}

// Gets a value of "--opt=value" format.
// and removes leading and tailing double-quotations.
static std::string ArgToValue(const std::string& arg) {
    std::string val = StrAfterChar(arg, '=');
    return StrStrip(val, '"');
}

// Gets a value of "--opt=value" format as an integer.
static int ArgToIntValue(const std::string& arg) {
    std::string val = ArgToValue(arg);
    return static_cast<int>(StrToUint(val));
}

// Gets a value of "--opt=value" format as an unsigned integer.
static size_t ArgToUintValue(const std::string& arg) {
    std::string val = ArgToValue(arg);
    return StrToUint(val);
}

std::vector<fs::path> Options::ParseArguments(int argc, char** argv,
                                              CppLintState* cpplint_state) {
    int verbosity = cpplint_state->VerboseLevel();
    std::string output_format = "emacs";
    bool quiet = cpplint_state->Quiet();
    std::string counting_style = "";
    bool recursive = false;
    std::vector<fs::path> excludes = {};
    int num_threads = -1;
    m_filters = DEFAULT_FILTERS;

    char** argp = argv + 1;
    // parse "--*" options
    for (; argp < argv + argc; argp++) {
        std::string opt = argp[0];
        if (!opt.starts_with("--"))
            break;  // opt is not an option
        if (opt == "--help") {
            PrintUsage();
        } else if (opt == "--version") {
            PrintVersion();
        } else if (opt.starts_with("--output=")) {
            output_format = ArgToValue(opt);
            if (output_format == "junit") {
                PrintUsage("Sorry, cpplint.cpp does not support junit yet.");
            }
            if (!InStrVec({ "emacs", "vs7", "eclipse", "junit", "sed", "gsed" }, output_format)) {
                PrintUsage("The only allowed output formats are "
                           "emacs, vs7, eclipse, sed, gsed and junit.");
            }
        } else if (opt == "--quiet") {
            quiet = true;
        } else if (opt.starts_with("--verbose=") || opt.starts_with("--v=")) {
            verbosity = ArgToIntValue(opt);
            if (verbosity < 0)
                PrintUsage("Verbosity should be an integer. (" + opt + ")");
        } else if (opt.starts_with("--filter=")) {
            std::string filters = ArgToValue(opt);
            if (filters == "")
                PrintCategories();
            bool added = AddFilters(filters);
            if (!added) {
                PrintUsage("Every filter in --filters must start with + or -"
                           " (" + filters + ")");
            }
        } else if (opt.starts_with("--counting=")) {
            counting_style = ArgToValue(opt);
            if (!InStrVec({ "total", "toplevel", "detailed" }, counting_style)) {
                PrintUsage("Valid counting options are total, toplevel, and detailed");
            }
        } else if (opt.starts_with("--root=")) {
            m_root = ArgToValue(opt);
            if (!fs::exists(m_repository)) {
                PrintUsage("Root directory does not exist.(" + opt + ")");
            }
        } else if (opt.starts_with("--repository=")) {
            m_repository = ArgToValue(opt);
            if (!fs::exists(m_repository)) {
                PrintUsage("Repository path does not exist.(" + opt + ")");
            }
        } else if (opt.starts_with("--linelength=")) {
            m_line_length = ArgToUintValue(opt);
            if (m_line_length == INDEX_NONE)
                PrintUsage("Line length should be an integer. (" + opt + ")");
        } else if (opt.starts_with("--exclude=")) {
            std::string val = ArgToValue(opt);
            if (val != "") {
                excludes.emplace_back(
                    fs::weakly_canonical(fs::absolute(val)).make_preferred());
            }
        } else if (opt.starts_with("--extensions=")) {
            ProcessExtensionsOption(ArgToValue(opt));
        } else if (opt.starts_with("--headers=")) {
            ProcessHppHeadersOption(ArgToValue(opt));
        } else if (opt == "--recursive") {
            recursive = true;
        } else if (opt.starts_with("--includeorder=")) {
            ProcessIncludeOrderOption(ArgToValue(opt));
        } else if (opt.starts_with("--config=")) {
            m_config_filename = ArgToValue(opt);
            if (StrContain(m_config_filename, "\\") || StrContain(m_config_filename, "/"))
                PrintUsage("Config file name must not include directory components.");
        } else if (opt == "--timing") {
            m_timing = true;
        } else if (opt.starts_with("--threads=")) {
            std::string val = ArgToValue(opt);
            if (val.empty())
                PrintNumThreads();
            if (val != "-1" && val != "0") {
                num_threads = static_cast<int>(StrToUint(val));
                if (num_threads < 1)
                    PrintUsage("Number of threads should be a positive integer. (" + opt+ ")");
            }
        } else {
            PrintUsage("Invalid arguments. (" + opt + ")");
        }
    }

    // parse other args as file names
    std::vector<fs::path> filenames = {};
    for (; argp < argv + argc; argp++) {
        fs::path p = argp[0];
        if (!fs::exists(p)) {
            // TODO(unknown): Maybe make this have an exit code of 2 after all is done
            cpplint_state->PrintError("Skipping input '" + p.string() + "': Path not found.");
            continue;
        }
        filenames.emplace_back(fs::canonical(p).make_preferred());
    }

    if (filenames.size() == 0)
        PrintUsage("No files were specified.");

    if (recursive)
        filenames = ExpandDirectories(filenames);

    if (excludes.size() > 0)
        filenames = FilterExcludedFiles(std::move(filenames), excludes);

    if (num_threads == -1)
        num_threads = GetNumThreads();

    // Update options
    cpplint_state->SetOutputFormat(output_format);
    cpplint_state->SetQuiet(quiet);
    cpplint_state->SetVerboseLevel(verbosity);
    cpplint_state->SetCountingStyle(counting_style);
    cpplint_state->SetNumThreads(num_threads);

    // sort filenames
    std::sort(filenames.begin(), filenames.end());
    filenames.erase(std::unique(filenames.begin(), filenames.end()), filenames.end());
    return filenames;
}

void Options::ProcessExtensionsOption(const std::string& val) {
    m_valid_extensions = ParseCommaSeparetedList(val);
}

void Options::ProcessHppHeadersOption(const std::string& val) {
    m_hpp_headers = ParseCommaSeparetedList(val);
}

void Options::ProcessIncludeOrderOption(const std::string& val) {
    if (val == "" || val == "default")
        m_include_order = INCLUDE_ORDER_DEFAULT;
    else if (val == "standardcfirst")
        m_include_order = INCLUDE_ORDER_STDCFIRST;
    else
        PrintUsage("Invalid includeorder value " + val + ". Expected default|standardcfirst");
}

static bool ShouldBeExcluded(const fs::path& filename,
                             const std::vector<fs::path>& excludes) {
    for (const fs::path& exc : excludes) {
        // TODO(matyalatte): support glob patterns for --exclude
        if (filename == exc)  // same path
            return true;

        // Check if exc is a parent path of filename
        std::string exc_str = exc.string();
        if (exc_str.back() != fs::path::preferred_separator) {
            exc_str += fs::path::preferred_separator;
        }
        if (StrContain(filename.string(), exc_str))
            return true;
    }
    return false;
}

std::vector<fs::path> Options::FilterExcludedFiles(std::vector<fs::path> filenames,
                                                   const std::vector<fs::path>& excludes) {
    // remove matching exclude patterns from m_filenames
    auto new_end = std::remove_if(filenames.begin(), filenames.end(),
                                  [excludes](const fs::path& f)->bool {
                                      return ShouldBeExcluded(f, excludes);
                                  });
    filenames.erase(new_end, filenames.end());
    return filenames;
}

static void ExpandDirectoriesRec(const fs::path& root,
                          std::vector<fs::path>& filtered,
                          const std::set<std::string>& extensions) {
    if (!fs::is_directory(root)) {
        fs::path ext = root.extension();
        if (ext.empty())
            return;
        std::string root_ext = &(ext.string())[1];
        if (extensions.contains(root_ext))
            filtered.push_back(root);
        return;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(root)) {
        ExpandDirectoriesRec(entry.path(), filtered, extensions);
    }
}

std::vector<fs::path> Options::ExpandDirectories(const std::vector<fs::path>& filenames) {
    std::vector<fs::path> filtered = {};
    std::set<std::string> extensions = GetAllExtensions();
    for (const fs::path& f : filenames) {
        ExpandDirectoriesRec(f, filtered, extensions);
    }
    return filtered;
}

std::set<std::string> Options::GetAllExtensions() const {
    std::set<std::string> exts = GetHeaderExtensions();
    if (m_valid_extensions.size() > 0) {
        exts.insert(m_valid_extensions.begin(), m_valid_extensions.end());
    } else {
        exts.insert({ "c", "cc", "cpp", "cxx", "c++", "cu" });
    }
    return exts;
}

bool Options::IsHeaderExtension(const std::string& file_extension) const {
    return InStrSet(GetHeaderExtensions(), file_extension);
}

bool Options::IsSourceExtension(const std::string& file_extension) const {
    return InStrSet(GetNonHeaderExtensions(), file_extension);
}

std::set<std::string> Options::GetHeaderExtensions() const {
    if (m_hpp_headers.size() > 0) {
        return m_hpp_headers;
    } else if (m_valid_extensions.size() > 0) {
        std::set<std::string> exts = {};
        for (const std::string& e : exts) {
            if (StrContain(e, 'h'))
                exts.insert(e);
        }
        return exts;
    }
    return { "h", "hh", "hpp", "hxx", "h++", "cuh" };
}

std::set<std::string> Options::GetNonHeaderExtensions() const {
    std::set<std::string> all_exts = GetAllExtensions();
    std::set<std::string> header_exts = GetHeaderExtensions();
    std::set<std::string> difference;
    std::set_difference(
        all_exts.begin(), all_exts.end(),
        header_exts.begin(), header_exts.end(),
        std::inserter(difference, difference.begin()));
    return difference;
}

// Parses filters and append them to a vector.
// Returns false when the last filter does not start with + or -.
static bool ParseCommaSeparetedFilters(const std::string& filters,
                                       std::vector<Filter>& parsed) {
    const char* str_p = &filters[0];
    const char* start = str_p;

    while (*str_p != '\0') {
        if (*str_p == ',') {
            std::string item = StrStrip(start, str_p - 1);
            if (item.size() > 0) {
                if (item[0] != '+' && item[0] != '-')
                    return false;
                parsed.emplace_back(item);
            }
            start = str_p + 1;
        }
        str_p++;
    }
    std::string item = StrStrip(start, str_p - 1);
    if (item.size() > 0) {
        if (item[0] != '+' && item[0] != '-')
            return false;
        parsed.emplace_back(item);
    }
    return true;
}

class CfgFile {
 public:
    bool noparent;
    std::vector<Filter> filters;
    std::vector<std::string> exclude_files;
    size_t line_length;
    std::set<std::string> extensions;
    std::set<std::string> headers;
    std::string include_order;

    CfgFile() :
        noparent(false),
        filters({}),
        exclude_files({}),
        line_length(INDEX_NONE),
        extensions({}),
        headers({}),
        include_order("")
        {}

    bool ReadFile(const fs::path& file, CppLintState* cpplint_state) {
        std::ifstream cfg_file(file);
        if (!cfg_file) {
            cpplint_state->PrintError("Skipping config file '" +
                                      file.string() +
                                      "': Can't open for reading\n");
            return false;
        }

        // read .cfg file
        std::string line;
        while (std::getline(cfg_file, line)) {
            line = StrStrip(StrBeforeChar(line, '#'));
            if (line.size() == 0) continue;
            std::string name = StrStrip(StrBeforeChar(line, '='));
            std::string val = StrStrip(StrAfterChar(line, '='));
            if (name == "set noparent") {
                noparent = true;
            } else if (name == "filter") {
                bool result = ParseCommaSeparetedFilters(val, filters);
                if (!result) {
                    // The last filter does not start with + or -
                    cpplint_state->PrintError(
                        file.string() +
                        ": Every filter must start with + or -"
                        " (" + val + ")");
                }
            } else if (name == "exclude_files") {
                exclude_files.emplace_back(std::move(val));
            } else if (name == "linelength") {
                line_length = StrToUint(val);
                if (line_length == INDEX_NONE) {
                    cpplint_state->PrintError(
                        "Line length must be numeric in file (" + file.string() + ")\n");
                }
            } else if (name == "extensions") {
                extensions = ParseCommaSeparetedList(val);
            } else if (name == "headers") {
                headers = ParseCommaSeparetedList(val);
            } else if (name == "includeorder") {
                include_order = val;
            } else {
                cpplint_state->PrintError(
                    "Invalid configuration option (" + name +
                    ") in file " + file.string() + "\n");
            }
        }
        return true;
    }
};

std::map<fs::path, CfgFile> g_cfg_map = {};
std::mutex g_cfg_mtx;

CfgFile* GetCfg(const fs::path& file, CppLintState* cpplint_state) {
    std::lock_guard<std::mutex> lock(g_cfg_mtx);

    auto it = g_cfg_map.find(file);
    if (it != g_cfg_map.end())
        return &it->second;

    auto new_it = g_cfg_map.emplace(file, CfgFile());
    CfgFile* cfg = &(new_it.first->second);
    cfg->ReadFile(file, cpplint_state);
    return cfg;
}

bool Options::ProcessConfigOverrides(const fs::path& filename,
                                     CppLintState* cpplint_state) {
    fs::path path = filename;
    bool noparent = false;
    while (!noparent) {
        fs::path root = path.parent_path();
        fs::path basename = path.filename();
        if (root == path)
            break;
        fs::path cfg_path = root / m_config_filename;
        path = root;
        if (!fs::is_regular_file(cfg_path))
            continue;

        CfgFile* cfg = GetCfg(cfg_path, cpplint_state);
        if (!cfg)
            break;

        noparent = cfg->noparent;

        if (!cfg->filters.empty())
            ConcatVec(m_filters, cfg->filters);

        if (!cfg->exclude_files.empty()) {
            for (const std::string& exclude : cfg->exclude_files) {
                // When matching exclude_files pattern, use the base_name of
                // the current file name or the directory name we are processing.
                // For example, if we are checking for lint errors in /foo/bar/baz.cc
                // and we found the .cfg file at /foo/CPPLINT.cfg, then the config
                // file's "exclude_files" filter is meant to be checked against "bar"
                // and not "baz" nor "bar/baz.cc".
                std::string base_name = basename.string();
                if (base_name == "")
                    continue;
                bool match = RegexMatch(exclude, base_name);
                if (match) {
                    if (cpplint_state->Quiet()) {
                        // Suppress "Ignoring file" warning when using --quiet.
                        return false;
                    }
                    cpplint_state->PrintInfo(
                        "Ignoring \"" + filename.string() + "\": file excluded by \"" +
                        cfg_path.string() + "\". " +
                        "File path component " + base_name + " matches "
                        "pattern " + exclude + "\n");
                    return false;
                }
            }
        }

        if (cfg->line_length != INDEX_NONE)
            m_line_length = cfg->line_length;

        if (!cfg->extensions.empty())
            m_valid_extensions = cfg->extensions;

        if (!cfg->headers.empty())
            m_hpp_headers = cfg->headers;

        if (!cfg->include_order.empty())
            ProcessIncludeOrderOption(cfg->include_order);
    }

    return true;
}

bool Options::AddFilters(const std::string& filters) {
    return ParseCommaSeparetedFilters(filters, m_filters);
}

void Filter::ParseFilterSelector(const std::string& filter) {
    /*Parses the given command line parameter for file- and line-specific
    exclusions.
    readability/casting:file.cpp
    readability/casting:file.cpp:43

    Args:
        parameter: The parameter value of --filter

    Returns:
        [category, filename, line].
        Category is always given.
        Filename is either a filename or empty if all files are meant.
        Line is either a line in filename or -1 if all lines are meant.
    */
    if (filter[0] == '+') {
        m_sign = true;
    } else if (filter[0] == '-') {
        m_sign = false;
    } else {
        m_category = "";
        m_file = "";
        m_linenum = INDEX_NONE;
        return;
    }

    size_t colon_pos = filter.find(':', 1);
    if (colon_pos == std::string::npos) {
        m_category = filter.substr(1);
        m_file = "";
        m_linenum = INDEX_NONE;
        return;
    }
    m_category = filter.substr(1, colon_pos - 1);
    size_t second_colon_pos = filter.find(':', colon_pos + 1);
    if (second_colon_pos == std::string::npos) {
        m_file = filter.substr(colon_pos + 1, std::string::npos);
        m_linenum = INDEX_NONE;
        return;
    }
    m_file = filter.substr(colon_pos + 1, second_colon_pos - colon_pos);
    std::string line_str = filter.substr(second_colon_pos + 1, std::string::npos);
    m_linenum = StrToUint(line_str);
    return;
}

bool Options::ShouldPrintError(const std::string& category,
                               const std::string& filename, size_t linenum) const {
    bool is_filtered = false;
    for (const Filter& filter : Filters()) {
        if (filter.IsMatched(category, filename, linenum)) {
            // true with "-" filters, false with "+" filters
            is_filtered = !filter.IsPositive();
        }
    }
    return !is_filtered;
}
