#pragma once
#include <iostream>
#include <sstream>
#include <string>

enum LineStatus : int {
    LINE_OK = 0,
    LINE_EOF = 1,  // A line terminated with EOF
    LINE_BAD_RUNE = 2,  // A line contains bad runes
    LINE_NULL = 4,  // A line contains null bytes
};

/**
 * Reads a line from istream.
 * It converts null or broken bytes to bad rune characters (U+FFFD).
 * It also reads an empty line after the linefeed at EOF.
 *
 * @param stream An istream object. (e.g., std::cin, std::ifstream)
 * @returns Bitwise OR of LineStatus values.
 */
std::string GetLine(std::istream& stream, std::string* buffer, int* status);

// Gets the number of characters in a line that was read with GetLine().
// It might crash when the line has broken bytes.
size_t GetLineWidth(const std::string& line) noexcept;
