# cpplint-cpp

[![License](https://img.shields.io/badge/License-BSD_3--Clause-green.svg)](https://opensource.org/licenses/BSD-3-Clause)

C++ reimplementation of [cpplint 1.7](https://github.com/cpplint/cpplint/commit/ab7335bcc734f6d21226631060888bfb77bbc9d7)

## What is cpplint?

[Cpplint](https://github.com/cpplint/cpplint) is a command-line tool to check C/C++ files for style issues according to [Google's C++ style guide](http://google.github.io/styleguide/cppguide.html).
It used to be developed and maintained by Google Inc. One of its forks now maintains the project.

## Building

> [!Warning]
> There is no installers yet. You need to build cpplint-cpp from the source and setup the environment by yourself.

### Requirements

- [Meson](https://github.com/mesonbuild/meson)
- C++20 compiler

### Debug build

You can build `cpplint-cpp` with the following commands.

```sh
meson setup build
meson compile -C build
meson test -C build
./build/cpplint-cpp --recursive .
```

### Release build

You can use [`presets/release.ini`](./presets/release.ini) to enable options for release build.

```sh
meson setup build --native-file=presets/release.ini
meson compile -C build
```

## Changes from cpplint.py

Basically, `cpplint-cpp` uses the same algorithm as `cpplint.py`, but some changes have been made to reduce processing time.

- Added concurrent file processing. (Removed mutable variables from the global scope.)
- Removed some redundant function calls.
- Combined some regex patterns.
- And other minor changes for optimization...

## Unimplemented features

cpplint-cpp is a WIP project. Please note that the following features are not implemented yet.

- Glob patterns for `--exclude` option.
- JUnit style outputs.
- Multibyte characters in stdin on Windows.
- UNIX convention of using "-" for stdin.
- Installation by package managers.

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
