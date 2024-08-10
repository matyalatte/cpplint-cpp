#include "line_utils.h"
#include <stack>
#include <string>
#include "cleanse.h"
#include "regex_utils.h"
#include "string_utils.h"

size_t GetIndentLevel(const std::string& line) {
    regex_match m;
    static const regex_code RE_PATTERN_INDENT =
        RegexCompile(R"(^( *)\S)");
    regex_match re_result = RegexCreateMatchData(RE_PATTERN_INDENT);
    bool indent = RegexMatch(RE_PATTERN_INDENT, line, re_result);
    if (indent)
        return GetMatchSize(re_result, 1);
    else
        return 0;
}

const regex_code RE_PATTERN_OPERATOR = RegexCompile(R"(\boperator\s*$)");

void FindEndOfExpressionInLine(const std::string& line,
                               size_t* startpos,
                               std::stack<char>* stack) {
    for (size_t i = *startpos; i < line.size(); i++) {
        char c = line[i];
        if (c == '(' || c == '[' || c == '{') {
            // Found start of parenthesized expression, push to expression stack
            stack->push(c);
        } else if (c == '<') {
            // Found potential start of template argument list
            if ((i > 0) && line[i - 1] == '<') {
                // Left shift operator
                if (!stack->empty() && stack->top() == '<') {
                    stack->pop();
                    if (stack->empty()) {
                        *startpos = SIZE_T_NONE;
                        *stack = {};
                        return;
                    }
                }
            } else if (i > 0 && RegexSearch(RE_PATTERN_OPERATOR, line.substr(0, i))) {
                // operator<, don't add to stack
                continue;
            } else {
                // Tentative start of template argument list
                stack->push('<');
            }
        } else if (c == ')' || c == ']' || c == '}') {
            // Found end of parenthesized expression.
            //
            // If we are currently expecting a matching '>', the pending '<'
            // must have been an operator.  Remove them from expression stack.
            while (!stack->empty() && stack->top() == '<')
                stack->pop();
            if (stack->empty()) {
                *startpos = SIZE_T_NONE;
                *stack = {};
                return;
            }
            if ((stack->top() == '(' && c == ')') ||
                (stack->top() == '[' && c == ']') ||
                (stack->top() == '{' && c == '}')) {
                stack->pop();
                if (stack->empty()) {
                    *startpos = i + 1;
                    *stack = {};
                    return;
                }
            } else {
                // Mismatched parentheses
                *startpos = SIZE_T_NONE;
                *stack = {};
                return;
            }
        } else if (c == '>') {
            // Found potential end of template argument list.

            // Ignore "->" and operator functions
            if (i > 0 &&
                (line[i - 1] == '-' || RegexSearch(RE_PATTERN_OPERATOR, line.substr(0, i - 1))))
                continue;

            // Pop the stack if there is a matching '<'.  Otherwise, ignore
            // this '>' since it must be an operator.
            if (!stack->empty()) {
                if (stack->top() == '<') {
                    stack->pop();
                    if (stack->empty()) {
                        *startpos = i + 1;
                        *stack = {};
                        return;
                    }
                }
            }
        } else if (c == ';') {
            // Found something that look like end of statements.  If we are currently
            // expecting a '>', the matching '<' must have been an operator, since
            // template argument list should not contain statements.
            while (!stack->empty() && stack->top() == '<')
                stack->pop();
            if (stack->empty()) {
                *startpos = SIZE_T_NONE;
                *stack = {};
                return;
            }
        }
    }

    // Did not find end of expression or unbalanced parentheses on this line
    *startpos = SIZE_T_NONE;
    return;
}

const std::string& CloseExpression(const CleansedLines& clean_lines, size_t* linenum, size_t* pos) {
    const std::string& line = clean_lines.GetElidedAt(*linenum);
    char c = line[*pos];
    std::string exp = line.substr(*pos);
    if (!(c == '(' || c == '{' || c == '[' || c == '<') ||
        (exp.starts_with("<<") || exp.starts_with("<="))) {
        *linenum = clean_lines.NumLines();
        *pos = SIZE_T_NONE;
        return line;
    }

    // Check first line
    std::stack<char> stack = {};
    size_t end_pos = *pos;
    FindEndOfExpressionInLine(line, &end_pos, &stack);
    if (end_pos != SIZE_T_NONE) {
        *pos = end_pos;
        return line;
    }

    // Continue scanning forward
    while (!stack.empty() && *linenum < clean_lines.NumLines() - 1) {
        (*linenum)++;
        const std::string& l = clean_lines.GetElidedAt(*linenum);
        end_pos = 0;
        FindEndOfExpressionInLine(l, &end_pos, &stack);
        if (end_pos != SIZE_T_NONE) {
            *pos = end_pos;
            return l;
        }
    }

    // Did not find end of expression before end of file, give up
    *linenum = clean_lines.NumLines() - 1;
    *pos = SIZE_T_NONE;
    return clean_lines.GetElidedAt(*linenum);
}

void FindStartOfExpressionInLine(const std::string& line,
                                 size_t* endpos,
                                 std::stack<char>* stack) {
    size_t i = *endpos;
    while (i != SIZE_T_NONE) {
        char c = line[i];
        if (c == ')' || c == ']' || c == '}') {
            // Found end of expression, push to expression stack
            stack->push(c);
        } else if (c == '>') {
            // Found potential end of template argument list.
            //
            // Ignore it if it's a "->" or ">=" or "operator>"
            if (i > 0 &&
                (line[i - 1] == '-' ||
                 RegexMatch(R"(\s>=\s)", line.substr(i - 1)) ||
                 RegexSearch(RE_PATTERN_OPERATOR, line.substr(0, i))))
                i--;
            else
                stack->push('>');
        } else if (c == '<') {
            // Found potential start of template argument list
            if (i > 0 && line[i - 1] == '<') {
                // Left shift operator
                i--;
            } else {
                // If there is a matching '>', we can pop the expression stack.
                // Otherwise, ignore this '<' since it must be an operator.
                if (!stack->empty() && stack->top() == '>') {
                    stack->pop();
                    if (stack->empty()) {
                        *endpos = i;
                        *stack = {};
                        return;
                    }
                }
            }
        } else if (c == '(' || c == '[' || c == '{') {
            // Found start of expression.
            //
            // If there are any unmatched '>' on the stack, they must be
            // operators.  Remove those.
            while (!stack->empty() && stack->top() == '>')
                stack->pop();
            if (stack->empty()) {
                *endpos = SIZE_T_NONE;
                *stack = {};
                return;
            }
            if ((c == '(' && stack->top() == ')') ||
                (c == '[' && stack->top() == ']') ||
                (c == '{' && stack->top() == '}')) {
                stack->pop();
                if (stack->empty()) {
                    *endpos = i;
                    *stack = {};
                    return;
                }
            } else {
                // Mismatched parentheses
                *endpos = SIZE_T_NONE;
                *stack = {};
                return;
            }
        } else if (c == ';') {
            // Found something that look like end of statements.  If we are currently
            // expecting a '<', the matching '>' must have been an operator, since
            // template argument list should not contain statements.
            while (!stack->empty() && stack->top() == '>')
                stack->pop();
            if (stack->empty()) {
                *endpos = SIZE_T_NONE;
                *stack = {};
                return;
            }
        }

        i--;
    }

    *endpos = SIZE_T_NONE;
}

const std::string& ReverseCloseExpression(const CleansedLines& clean_lines,
                                          size_t* linenum, size_t* pos) {
    const std::string& line = clean_lines.GetElidedAt(*linenum);
    char c = line[*pos];
    if (!(c == ')' || c == '}' || c == ']' || c == '>')) {
        *linenum = 0;
        *pos = SIZE_T_NONE;
        return line;
    }

    // Check last line
    size_t start_pos = *pos;
    std::stack<char> stack = {};
    FindStartOfExpressionInLine(line, &start_pos, &stack);
    if (start_pos != SIZE_T_NONE) {
        *pos = start_pos;
        return line;
    }

    // Continue scanning backward
    while (!stack.empty() && *linenum > 0) {
        (*linenum)--;
        const std::string& l = clean_lines.GetElidedAt(*linenum);
        start_pos = l.size() - 1;
        FindStartOfExpressionInLine(l, &start_pos, &stack);
        if (start_pos != SIZE_T_NONE) {
            *pos = start_pos;
            return l;
        }
    }

    // Did not find start of expression before beginning of file, give up
    *linenum = 0;
    *pos = SIZE_T_NONE;
    return clean_lines.GetElidedAt(*linenum);
}
