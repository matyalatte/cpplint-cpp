#include "nest_info.h"
#include <string>
#include "cleanse.h"
#include "file_linter.h"
#include "line_utils.h"
#include "regex_utils.h"
#include "string_utils.h"

ClassInfo::ClassInfo(const std::string& name,
                     const std::string& class_or_struct,
                     const CleansedLines& clean_lines,
                     size_t linenum) :
                     BlockInfo(linenum, false),
                     m_name(name),
                     m_basename(),
                     m_is_derived(false),
                     m_re_result(RegexCreateMatchData(2)) {
    m_block_type = CLASS_INFO;
    m_check_namespace_indentation = true;
    if (class_or_struct == "struct") {
        m_access = "public";
        m_is_struct = true;
    } else {
        m_access = "private";
        m_is_struct = false;
    }

    m_basename = RegexEscape(StrSplitBy(m_name, "::").back());

    // Remember initial indentation level for this class.  Using raw_lines here
    // instead of elided to account for leading comments.
    m_class_indent = GetIndentLevel(clean_lines.GetRawLineAt(linenum));

    // Try to find the end of the class.  This will be confused by things like:
    //   class A {
    //   } *x = { ...
    //
    // But it's still good enough for CheckSectionSpacing.
    m_last_line = 0;
    int depth = 0;
    for (size_t i = linenum; i < clean_lines.NumLines(); i++) {
        const std::string& line = clean_lines.GetElidedAt(i);
        depth += StrCount(line, "{") - StrCount(line, "}");
        if (depth == 0) {
            m_last_line = i;
            break;
        }
    }
}

void ClassInfo::CheckBegin(const CleansedLines& clean_lines,
                           size_t linenum) {
    static const regex_code RE_PATTERN_CHECK_BEGIN =
        RegexCompile("(^|[^:]):($|[^:])");

    // Look for a bare ':'
    if (RegexSearch(RE_PATTERN_CHECK_BEGIN, clean_lines.GetElidedAt(linenum)))
        m_is_derived = true;
}

void ClassInfo::CheckEnd(const CleansedLines& clean_lines,
                         size_t linenum,
                         FileLinter* file_linter) {
    // If there is a DISALLOW macro, it should appear near the end of
    // the class.
    bool seen_last_thing_in_class = false;
    for (size_t i = linenum - 1; i > m_starting_linenum; i--) {
        const std::string& elided = clean_lines.GetElidedAt(i);
        if (StrContain(elided, "DISALLOW_")) {
            bool match = RegexSearch(
                            R"(\b(DISALLOW_COPY_AND_ASSIGN|DISALLOW_IMPLICIT_CONSTRUCTORS)\()" +
                            RegexEscape(m_name) + R"(\))",
                            elided, m_re_result);
            if (match) {
                if (seen_last_thing_in_class)
                    file_linter->Error(i, "readability/constructors", 3,
                                    GetMatchStr(m_re_result, elided, 1) +
                                    " should be the last thing in the class");
                break;
            }
        }

        if (!StrIsBlank(elided))
            seen_last_thing_in_class = true;
    }

    // Check that closing brace is aligned with beginning of the class.
    // Only do this if the closing brace is indented by only whitespaces.
    // This means we will not check single-line class definitions.
    bool indent = RegexMatch(R"(^( *)\})",
                             clean_lines.GetElidedAt(linenum), m_re_result);
    if (indent && GetMatchSize(m_re_result, 1) != m_class_indent) {
        std::string parent;
        if (m_is_struct)
            parent = "struct " + m_name;
        else
            parent = "class " + m_name;
        file_linter->Error(linenum, "whitespace/indent", 3,
                           "Closing brace should be aligned with beginning of " + parent);
    }
}

void NamespaceInfo::CheckEnd(const CleansedLines& clean_lines,
                             size_t linenum,
                             FileLinter* file_linter) {
    // Check end of namespace comments.
    const std::string& line = clean_lines.GetRawLineAt(linenum);

    // Check how many lines is enclosed in this namespace.  Don't issue
    // warning for missing namespace comments if there aren't enough
    // lines.  However, do apply checks if there is already an end of
    // namespace comment and it's incorrect.
    //
    // TODO(unknown): We always want to check end of namespace comments
    // if a namespace is large, but sometimes we also want to apply the
    // check if a short namespace contained nontrivial things (something
    // other than forward declarations).  There is currently no logic on
    // deciding what these nontrivial things are, so this check is
    // triggered by namespace size only, which works most of the time.
    static const regex_code RE_PATTERN_NAME_SPACE_END =
        RegexCompile(R"(^\s*};*\s*(//|/\*).*\bnamespace\b)");
    thread_local regex_match re_result = RegexCreateMatchData(2);
    if (linenum - m_starting_linenum < 10
            && !RegexMatch(RE_PATTERN_NAME_SPACE_END, line, re_result))
        return;

    // Look for matching comment at end of namespace.
    //
    // Note that we accept C style "/* */" comments for terminating
    // namespaces, so that code that terminate namespaces inside
    // preprocessor macros can be cpplint clean.
    //
    // We also accept stuff like "// end of namespace <name>." with the
    // period at the end.
    //
    // Besides these, we don't accept anything else, otherwise we might
    // get false negatives when existing comment is a substring of the
    // expected namespace.
    if (!m_name.empty()) {
        // Named namespace
        if (!RegexMatch(R"(^\s*};*\s*(//|/\*).*\bnamespace\s+)" +
                            RegexEscape(m_name) + R"([\*/\.\\\s]*$)",
                            line, re_result)) {
            file_linter->Error(linenum, "readability/namespace", 5,
                               "Namespace should be terminated with \"// namespace "
                               + m_name + "\"");
        }
    } else {
        // Anonymous namespace
        if (!RegexMatch(R"(^\s*};*\s*(//|/\*).*\bnamespace[\*/\.\\\s]*$)", line, re_result)) {
            // If "// namespace anonymous" or "// anonymous namespace (more text)",
            // mention "// anonymous namespace" as an acceptable form
            if (RegexMatch(R"(^\s*}.*\b(namespace anonymous|anonymous namespace)\b)", line, re_result)) {
                file_linter->Error(linenum, "readability/namespace", 5,
                    "Anonymous namespace should be terminated with \"// namespace\""
                    " or \"// anonymous namespace\"");
            } else {
                file_linter->Error(
                    linenum, "readability/namespace", 5,
                    "Anonymous namespace should be terminated with \"// namespace\"");
            }
        }
    }
}
