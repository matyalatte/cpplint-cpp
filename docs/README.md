# cpplint-cpp

[![License](https://img.shields.io/badge/License-BSD_3--Clause-green.svg)](https://opensource.org/licenses/BSD-3-Clause)

C++ reimplementation of [cpplint 2.0](https://github.com/cpplint/cpplint/tree/2.0.0)

## What is cpplint?

[Cpplint](https://github.com/cpplint/cpplint) is a command-line tool to check C/C++ files for style issues according to [Google's C++ style guide](http://google.github.io/styleguide/cppguide.html).
It used to be developed and maintained by Google Inc. One of its forks now maintains the project.

## Installation

You can install the `cpplint-cpp` command via pip.

```sh
pip install cpplint-cpp --no-index --find-links https://matyalatte.github.io/cpplint-cpp/packages.html
```

## cpplint-cpp vs. cpplint.py

Here is an analysis of the performance differences between `cpplint-cpp` and `cpplint.py` against two repositories:
[`googletest`](https://github.com/google/googletest) and `cpplint-cpp`.
Measurements were taken on an Ubuntu runner with [some scripts](BENCHMARK.md).

### Execution time

You can see `cpplint-cpp` has significantly better performance, being over 30 times faster than `cpplint.py`.

|             | googletest-1.14.0 (s) | cpplint-cpp (s) |
| ----------- | --------------------- | --------------- |
| cpplint-cpp | 0.439020              | 0.092547        |
| cpplint.py  | 21.639285             | 3.782867        |

### Memory usage

Despite using multithreading with 4 cores, `cpplint-cpp` has lower memory usage than `cpplint.py`.

|             | googletest-1.14.0 | cpplint-cpp |
| ----------- | ----------------- | ----------- |
| cpplint-cpp | 15.46 MiB         | 10.45 MiB   |
| cpplint.py  | 23.08 MiB         | 22.57 MiB   |

## Changes from cpplint.py

Basically, `cpplint-cpp` uses the same algorithm as `cpplint.py`, but some changes have been made to reduce processing time.

- Added concurrent file processing.
- Removed some redundant function calls.
- Used JIT compiler for some regex patterns.
- Combined some regex patterns.
- Added `--timing` option to display the execution time.
- Added `--threads=` option to specify the number of threads.
- And other minor changes for optimization...

## Unimplemented features

cpplint-cpp is a WIP project. Please note that the following features are not implemented yet.

- Glob patterns for `--exclude` option.
- JUnit style outputs.
- Multibyte characters in stdin on Windows.
- UNIX convention of using "-" for stdin.

## Building

### Requirements

- [Meson](https://github.com/mesonbuild/meson)
- C++20 compiler

### Debug build

You can build `cpplint-cpp` with the following commands.

```sh
meson setup build
meson compile -C build
meson test -C build
./build/cpplint-cpp --version
```

### Release build

You can use [`presets/release.ini`](../presets/release.ini) to enable options for release build.

```sh
meson setup build --native-file=presets/release.ini
meson compile -C build
```

### Build wheel package

You can make a pip package with the following commands.

```sh
mkdir dist
cp ./build/cpplint-cpp ./dist
cp ./build/version.h ./dist
pip install build
python -m build
```

## Submitting Feature Requests

I do not accept feature requests related to cpplint specifications, including the addition of new rules.
For such requests, please visit [the original cpplint project](https://github.com/cpplint/cpplint/issues) to submit your suggestions.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md)

## Credits

This software uses (or is inspired by) several open-source projects. I gratefully acknowledge the work of the following contributors:

- **[Google Style Guides](https://github.com/google/styleguide)** by Google Inc.
  - Contribution: Original implementation of `cpplint.py`.
  - License: [Apache License 2.0](https://github.com/google/styleguide/blob/gh-pages/LICENSE)

- **[cpplint/cpplint](https://github.com/cpplint/cpplint)**
  - Contribution: Latest updates of `cpplint.py`.
  - License: [BSD 3-Clause License](https://github.com/cpplint/cpplint/blob/master/LICENSE)

- **[pcre2](https://github.com/PCRE2Project/pcre2)**
  - Contribution: Regex matching.
  - License: [PCRE2 LICENCE](https://github.com/PCRE2Project/pcre2/blob/master/LICENCE)

- **[ThreadPool](https://github.com/progschj/ThreadPool)** by progschj
  - Contribution: Thread pool implementation
  - License: [zlib License](https://github.com/progschj/ThreadPool/blob/master/COPYING)

- **[widechar_width.h](https://github.com/ridiculousfish/widecharwidth/blob/master/widechar_width.h)** by ridiculousfish
  - Contribution: Reference tables for character widths.
  - License: [CC0 Public Domain](https://github.com/ridiculousfish/widecharwidth/blob/master/LICENSE)
