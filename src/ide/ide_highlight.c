/* Syntax highlighting for DiScript source. */

#include "ide/ide_highlight.h"

#include <string.h>

static const char *const k_keywords[] = {
    "if",     "elif",  "else",   "for",    "in",      "while",  "break",  "continue",
    "def",    "class", "return", "import", "from",    "as",     "pass",   "del",
    "global", "lambda", "try",   "except", "finally", "raise",  "with",   "yield",
    "and",    "or",    "not",    "is",     "assert",  "chunk",  "true",   "false",
    "none",   "nil",   "null",   "self",   NULL
};

static const char *const k_builtins[] = {
    "print",  "len",    "range",  "str",    "int",     "float",  "bool",   "list",
    "dict",   "type",   "abs",    "min",    "max",     "sum",    "sorted", "input",
    "open",   "read",   "write",  "append", "pop",     "push",   "insert", "remove",
    "keys",   "values", "items",  "get",    "update",  "copy",   "clear",  "split",
    "join",   "upper",  "lower",  "strip",  "replace", "startswith", "endswith",
    "format", "random", "math",   "love",   "console", "time",   "fs",     NULL
};

static int is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int is_ident(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}
static int is_digit(char c) { return c >= '0' && c <= '9'; }

static int in_list(const char *const *list, const char *s, size_t n) {
    for (int i = 0; list[i]; i++) {
        if (strlen(list[i]) == n && memcmp(list[i], s, n) == 0)
            return 1;
    }
    return 0;
}

static int push(IDEToken *out, int max, int count, size_t start, size_t end, int kind) {
    if (out && count < max)
        out[count].start = start, out[count].end = end, out[count].kind = (unsigned char)kind;
    return count + 1;
}

/* Offset of the next occurrence of `needle`, or `from` if absent. */
static size_t find_seq(const char *t, size_t len, size_t from, const char *needle) {
    size_t n = strlen(needle);
    if (n == 0)
        return from;
    for (size_t i = from; i + n <= len; i++)
        if (memcmp(t + i, needle, n) == 0)
            return i;
    return from;
}

int ide_highlight(const char *text, size_t len, IDEToken *out, int max_tokens) {
    int count = 0;
    size_t i = 0;
    /* The previous non-space token was `def` / `class`? Then the next name is
     * painted as a function or class name. */
    int after_def = 0, after_class = 0;

    while (i < len) {
        char c = text[i];

        /* Comments: `#` to end of line. */
        if (c == '#') {
            size_t e = i;
            while (e < len && text[e] != '\n')
                e++;
            count = push(out, max_tokens, count, i, e, IDE_TK_COMMENT);
            i = e;
            continue;
        }

        /* Strings: triple-quoted first, then single/double with escapes. */
        if (c == '"' || c == '\'') {
            int triple = 0;
            if (i + 2 < len && text[i + 1] == c && text[i + 2] == c)
                triple = 1;
            size_t e = i + (triple ? 3 : 1);
            if (triple) {
                while (e + 2 < len) {
                    if (text[e] == '\\') {
                        e += 2;
                        continue;
                    }
                    if (text[e] == c && text[e + 1] == c && text[e + 2] == c) {
                        e += 3;
                        break;
                    }
                    e++;
                }
                if (e + 2 >= len)
                    e = len;
            } else {
                while (e < len && text[e] != c && text[e] != '\n') {
                    if (text[e] == '\\' && e + 1 < len)
                        e += 2;
                    else
                        e++;
                }
                if (e < len && text[e] == c)
                    e++;
            }
            count = push(out, max_tokens, count, i, e, IDE_TK_STRING);
            i = e;
            after_def = after_class = 0;
            continue;
        }

        /* Numbers, including hex and floats. */
        if (is_digit(c) || (c == '.' && i + 1 < len && is_digit(text[i + 1]))) {
            size_t e = i;
            if (c == '0' && i + 1 < len && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
                e = i + 2;
                while (e < len && ((text[e] >= '0' && text[e] <= '9') || (text[e] >= 'a' && text[e] <= 'f') ||
                                   (text[e] >= 'A' && text[e] <= 'F')))
                    e++;
            } else {
                while (e < len && (is_digit(text[e]) || text[e] == '.' || text[e] == 'e' || text[e] == 'E' ||
                                   ((text[e] == '+' || text[e] == '-') && e > i &&
                                    (text[e - 1] == 'e' || text[e - 1] == 'E'))))
                    e++;
            }
            count = push(out, max_tokens, count, i, e, IDE_TK_NUMBER);
            i = e;
            after_def = after_class = 0;
            continue;
        }

        /* Identifiers and keywords. */
        if (is_ident_start(c)) {
            size_t e = i;
            while (e < len && is_ident(text[e]))
                e++;
            size_t n = e - i;
            int kind = IDE_TK_DEFAULT;
            if (after_def) {
                kind = IDE_TK_FUNCTION;
            } else if (after_class) {
                kind = IDE_TK_CLASS;
            } else if (in_list(k_keywords, text + i, n)) {
                kind = IDE_TK_KEYWORD;
            } else if (in_list(k_builtins, text + i, n)) {
                kind = IDE_TK_BUILTIN;
            }
            after_def = (kind == IDE_TK_KEYWORD && n == 3 && memcmp(text + i, "def", 3) == 0);
            after_class = (kind == IDE_TK_KEYWORD && n == 5 && memcmp(text + i, "class", 5) == 0);
            if (kind != IDE_TK_DEFAULT)
                count = push(out, max_tokens, count, i, e, kind);
            i = e;
            continue;
        }

        /* Operators worth colouring. */
        if (strchr("+-*/%=<>!&|^~", c)) {
            size_t e = i + 1;
            while (e < len && strchr("+-*/%=<>!&|^~", text[e]))
                e++;
            count = push(out, max_tokens, count, i, e, IDE_TK_OPERATOR);
            i = e;
            after_def = after_class = 0;
            continue;
        }

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }
        after_def = after_class = 0;
        i++;
    }
    (void)find_seq;
    return count;
}
