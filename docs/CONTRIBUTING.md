# Contributing to cpplint-cpp

cpplint-cpp is an open-source project that warmly welcomes contributions. We appreciate your help in improving the project!

## Unacceptable Patches

The following types of changes are not acceptable. Before you start contributing, ensure that your work does not involve the following:

- Adding support for new coding rules that cpplint.py does not support.
- Breaking compatibility with [cpplint.py 2.0](https://github.com/cpplint/cpplint/tree/2.0.0).
- Making extensive algorithm changes that would make it difficult for cpplint.py users to read the source code.
- Implementing platform-specific optimizations (e.g., using SIMD extensions).

## Coding Style

### Common Rules

cpplint-cpp follows [Google's C++ Style Guide](http://google.github.io/styleguide/cppguide.html) with the following modifications:

- Line length should not exceed 100 characters.
- Use 4 spaces for indentation.
- No copyright notice or author line is required in source files.
- `std::filesystem` is allowed.

### Naming Conventions

It is preferable to use the same names for functions and classes as in cpplint.py (or leave comments indicating the equivalent parts of cpplint.py in your code.) Keep in mind that the source code is compared to cpplint.py.

### ThreadPool.h

[ThreadPool.h](../include/ThreadPool.h) does not follow the style guide because it is a third-party file. This file should not be modified unless absolutely necessary. If you do need to edit it, please note the changes in the comment section at the beginning of the file.

## Compatibility with cpplint.py

cpplint-cpp should maintain compatibility with cpplint.py. For the same file, the number of errors per category should be consistent between cpplint-cpp and cpplint.py. However, it is not required to reproduce issues from cpplint.py in cpplint-cpp.

## CI Workflow

No merge request may be merged until it passes the following code checks:

- Linting by cpplint.py 2.0 (or cpplint-cpp)
- Typo check by [codespell](https://github.com/codespell-project/codespell)
- Unit tests with `meson test -C build`

It is recommended to run test and lint after building, as follows.

```sh
meson setup build
meson compile -C build
meson test -C build -v
./build/cpplint-cpp --recursive --quiet .
```
