# Changelog

## 0.3.0 (2024-10-19)

- cpplint-cpp now conforms to [cpplint 2.0](https://github.com/cpplint/cpplint/tree/2.0.0).
  - Disabled `readability/fn_size` by default. (https://github.com/matyalatte/cpplint-cpp/pull/14)
  - Cast checks now uses standard fixed-width typenames. (https://github.com/matyalatte/cpplint-cpp/pull/15)
  - Fixed false positives on concept declaration. (https://github.com/matyalatte/cpplint-cpp/pull/16)
- Fixed a bug where `latch` and `numbers` were not considered as c++ headers, courtesy of @GermanAizek. (https://github.com/matyalatte/cpplint-cpp/pull/6)
- `--exclude` now supports glob patterns. (https://github.com/matyalatte/cpplint-cpp/pull/9)
- Added support for unix convention of using `-` for stdin. (https://github.com/matyalatte/cpplint-cpp/pull/13)
- Fixed a compile error on Clang, courtesy of @GermanAizek. (https://github.com/matyalatte/cpplint-cpp/pull/5)
- Fixed a compile error on Ubuntu20.04 with an old version of GCC. (https://github.com/matyalatte/cpplint-cpp/pull/12)

## 0.2.1 (2024-08-25)

- Initial release for cpplint-cpp.
