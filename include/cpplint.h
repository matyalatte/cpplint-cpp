#pragma once
#include <string>

// You can suppress warnings about unused variables with this macro.
#define UNUSED(x) (void)(x)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define TO_SIZE(n) static_cast<size_t>(n)

// Use SIZE_MAX as an invalid value
constexpr size_t INDEX_NONE = SIZE_MAX;

// Ensures that npos is SIZE_MAX
static_assert(std::string::npos == SIZE_MAX,
              "std::string::npos should be equal to SIZE_MAX");

// Ensures that npos is -1
static_assert(std::string::npos == -1,
              "std::string::npos should be equal to -1");

// Maximum valid value
constexpr size_t INDEX_MAX = SIZE_MAX - 1;
