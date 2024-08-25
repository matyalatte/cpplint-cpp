# cpplint-cpp

[![License](https://img.shields.io/badge/License-BSD_3--Clause-green.svg)](https://opensource.org/licenses/BSD-3-Clause)

C++ reimplementation of [cpplint 1.7](https://github.com/cpplint/cpplint/tree/ab7335bcc734f6d21226631060888bfb77bbc9d7)

## What is cpplint?

[Cpplint](https://github.com/cpplint/cpplint) is a command-line tool to check C/C++ files for style issues according to [Google's C++ style guide](http://google.github.io/styleguide/cppguide.html).
It used to be developed and maintained by Google Inc. One of its forks now maintains the project.

## Installation

You can install the `cpplint-cpp` command via pip.

```sh
pip install cpplint-cpp --find-links https://github.com/matyalatte/cpplint-cpp/releases/latest
```

## cpplint-cpp vs. cpplint.py

Here is an analysis of the performance differences between `cpplint-cpp` and `cpplint.py` against two repositories:
[`googletest`](https://github.com/google/googletest) and `cpplint-cpp`.
Measurements were taken on an Ubuntu runner with [`benchmark.yml`](.github/workflows/benchmark.yml).

### Execution time

You can see `cpplint-cpp` has significantly better performance, being over 30 times faster than `cpplint.py`.

|             | googletest-1.14.0 (s) | cpplint-cpp (s) |
| ----------- | --------------------- | --------------- |
| cpplint-cpp | 0.443100              | 0.090062        |
| cpplint.py  | 25.826862             | 3.865572        |

### Memory usage

Despite using multithreading with 4 cores, `cpplint-cpp` has lower memory usage than `cpplint.py`.

|             | googletest-1.14.0 | cpplint-cpp |
| ----------- | ----------------- | ----------- |
| cpplint-cpp | 15.55 MiB         | 10.32 MiB   |
| cpplint.py  | 23.01 MiB         | 22.43 MiB   |

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

You can use [`presets/release.ini`](./presets/release.ini) to enable options for release build.

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

- **[widechar_width.h](https://github.com/ridiculousfish/widecharwidth/blob/master/widechar_width.h)** by ridiculousfish
  - Contribution: Reference tables for character widths.
  - License: [CC0 Public Domain](https://github.com/ridiculousfish/widecharwidth/blob/master/LICENSE)
