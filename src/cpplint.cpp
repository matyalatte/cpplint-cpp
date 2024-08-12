#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>
#include "cpplint_state.h"
#include "file_linter.h"
#include "options.h"
#include "ThreadPool.h"

namespace fs = std::filesystem;

static void ProcessFile(const fs::path& filename,
                        CppLintState* cpplint_state,
                        const Options& global_options) {
    FileLinter linter(filename, cpplint_state, global_options);
    linter.ProcessFile();
}

int main(int argc, char** argv) {
    // We don't use cstdio
    std::ios_base::sync_with_stdio(false);

    std::chrono::system_clock::time_point  start, end;
    start = std::chrono::system_clock::now();

    std::vector<fs::path> filenames;
    Options global_options = Options();
    CppLintState cpplint_state = CppLintState();

    // Parse argv
    filenames = global_options.ParseArguments(argc, argv, &cpplint_state);

    // Generate a future for each file
    ThreadPool pool(std::thread::hardware_concurrency());
    std::vector<std::future<void>> futures;
    for (const fs::path& filename : filenames) {
        futures.push_back(pool.enqueue([&filename, &cpplint_state, &global_options]() {
            ProcessFile(filename, &cpplint_state, global_options);
        }));
    }

    // Wait for all futures to complete
    for (auto&& future : futures) {
        future.get();
    }

    // If --quiet is passed, suppress printing error count unless there are errors.
    if (!cpplint_state.Quiet() || cpplint_state.ErrorCount() > 0)
        cpplint_state.PrintErrorCounts();

    if (cpplint_state.OutputFormat() == OUTPUT_JUNIT)
        std::cerr << cpplint_state.FormatJUnitXML();

    if (global_options.Timing()) {
        end = std::chrono::system_clock::now();
        std::chrono::milliseconds::rep elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        double elapsed_sec = static_cast<double>(elapsed_ms) / 1000;
        cpplint_state.PrintInfo(
            "Runtime: " + std::to_string(elapsed_sec) + "(s)\n");
    }

    return cpplint_state.ErrorCount() > 0;
}
