#pragma once
#include <stack>
#include <string>
#include "cleanse.h"

// Return the number of leading spaces in line.
size_t GetIndentLevel(const std::string& line);

/*Find the position just after the end of current parenthesized expression.

Args:
    line: a CleansedLines line.
    startpos: start searching at this position.
    stack: nesting stack at startpos.

Returns:
    On finding matching end: (index just after matching end, None)
*/
void FindEndOfExpressionInLine(const std::string& line,
                               size_t* startpos,
                               std::stack<char>* stack);

/*If input points to ( or { or [ or <, finds the position that closes it.

If lines[linenum][pos] points to a '(' or '{' or '[' or '<', finds the
linenum/pos that correspond to the closing of the expression.

TODO(unknown): cpplint spends a fair bit of time matching parentheses.
Ideally we would want to index all opening and closing parentheses once
and have CloseExpression be just a simple lookup, but due to preprocessor
tricks, this is not so easy.

Args:
    clean_lines: A CleansedLines instance containing the file.
    linenum: The number of the line to check.
    pos: A position on the line.
*/
const std::string& CloseExpression(const CleansedLines& clean_lines,
                                   size_t* linenum, size_t* pos);

/*Find position at the matching start of current expression.

    This is almost the reverse of FindEndOfExpressionInLine, but note
    that the input position and returned position differs by 1.

    Args:
        line: a CleansedLines line.
        endpos: start searching at this position.
        stack: nesting stack at endpos.
*/
void FindStartOfExpressionInLine(const std::string& line,
                                 size_t* endpos,
                                 std::stack<char>* stack);

/*If input points to ) or } or ] or >, finds the position that opens it.

If lines[linenum][pos] points to a ')' or '}' or ']' or '>', finds the
linenum/pos that correspond to the opening of the expression.

Args:
    clean_lines: A CleansedLines instance containing the file.
    linenum: The number of the line to check.
    pos: A position on the line.
*/
const std::string& ReverseCloseExpression(const CleansedLines& clean_lines,
                                          size_t* linenum, size_t* pos);

/*
    Does line terminate so, that the next symbol is in string constant.
    This function does not consider single-line nor multi-line comments.
*/
bool IsCppString(const std::string& line);
