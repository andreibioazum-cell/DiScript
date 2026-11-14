/* Pythonic Indentation + F-String + UTF-8 Lexer for DiScript */

#include "discript/ds_lexer.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ts_alloc_str(DSTokenStream *ts, const char *s, size_t n) {
    char *buf = (char *)malloc(n + 1);
    if (!buf)
        return NULL;
    if (n > 0)
        memcpy(buf, s, n);
    buf[n] = '\0';
    if (ts->owned_count == ts->owned_cap) {
        int ncap = ts->owned_cap < 64 ? 64 : ts->owned_cap * 2;
        char **nb = (char **)realloc(ts->owned_bufs, (size_t)ncap * sizeof(char *));
        if (!nb) {
            free(buf);
            return NULL;
        }
        ts->owned_bufs = nb;
        ts->owned_cap = ncap;
    }
    ts->owned_bufs[ts->owned_count++] = buf;
    return buf;
}

static void ts_emit(DSTokenStream *ts, DSToken tok) {
    if (ts->count == ts->cap) {
        int ncap = ts->cap < 128 ? 128 : ts->cap * 2;
        DSToken *nt = (DSToken *)realloc(ts->tokens, (size_t)ncap * sizeof(DSToken));
        if (!nt)
            return;
        ts->tokens = nt;
        ts->cap = ncap;
    }
    ts->tokens[ts->count++] = tok;
}

static void ts_emit_simple(DSTokenStream *ts, DSTokenKind k, int line, int col) {
    DSToken t;
    memset(&t, 0, sizeof(t));
    t.kind = k;
    t.line = line;
    t.col = col;
    ts_emit(ts, t);
}

static void ts_emit_str(DSTokenStream *ts, DSTokenKind k, const char *s, size_t len, int line, int col) {
    DSToken t;
    memset(&t, 0, sizeof(t));
    t.kind = k;
    t.line = line;
    t.col = col;
    t.str_ptr = ts_alloc_str(ts, s, len);
    t.str_len = (uint32_t)len;
    ts_emit(ts, t);
}

static void ts_error(DSTokenStream *ts, int line, const char *fmt, ...) {
    if (ts->had_error)
        return;
    ts->had_error = 1;
    ts->error_line = line;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ts->error_msg, sizeof(ts->error_msg), fmt, ap);
    va_end(ap);
}

static int is_ident_start(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
}

static int is_ident_cont(uint8_t c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static DSTokenKind lookup_keyword(const char *s, size_t n) {
    switch (n) {
    case 2:
        if (memcmp(s, "if", 2) == 0) return TOK_IF;
        if (memcmp(s, "in", 2) == 0) return TOK_IN;
        if (memcmp(s, "or", 2) == 0) return TOK_OR;
        if (memcmp(s, "as", 2) == 0) return TOK_AS;
        break;
    case 3:
        if (memcmp(s, "def", 3) == 0) return TOK_DEF;
        if (memcmp(s, "for", 3) == 0) return TOK_FOR;
        if (memcmp(s, "and", 3) == 0) return TOK_AND;
        if (memcmp(s, "not", 3) == 0) return TOK_NOT;
        if (memcmp(s, "del", 3) == 0) return TOK_DEL;
        if (memcmp(s, "try", 3) == 0) return TOK_TRY;
        if (memcmp(s, "nil", 3) == 0) return TOK_NONE;
        break;
    case 4:
        if (memcmp(s, "elif", 4) == 0) return TOK_ELIF;
        if (memcmp(s, "else", 4) == 0) return TOK_ELSE;
        if (memcmp(s, "True", 4) == 0 || memcmp(s, "true", 4) == 0) return TOK_TRUE;
        if (memcmp(s, "None", 4) == 0 || memcmp(s, "null", 4) == 0) return TOK_NONE;
        if (memcmp(s, "pass", 4) == 0) return TOK_PASS;
        if (memcmp(s, "from", 4) == 0) return TOK_FROM;
        break;
    case 5:
        if (memcmp(s, "while", 5) == 0) return TOK_WHILE;
        if (memcmp(s, "break", 5) == 0) return TOK_BREAK;
        if (memcmp(s, "False", 5) == 0 || memcmp(s, "false", 5) == 0) return TOK_FALSE;
        if (memcmp(s, "class", 5) == 0) return TOK_CLASS;
        break;
    case 6:
        if (memcmp(s, "return", 6) == 0) return TOK_RETURN;
        if (memcmp(s, "import", 6) == 0) return TOK_IMPORT;
        if (memcmp(s, "global", 6) == 0) return TOK_GLOBAL;
        if (memcmp(s, "lambda", 6) == 0) return TOK_LAMBDA;
        if (memcmp(s, "assert", 6) == 0) return TOK_ASSERT;
        if (memcmp(s, "except", 6) == 0) return TOK_EXCEPT;
        break;
    case 8:
        if (memcmp(s, "continue", 8) == 0) return TOK_CONTINUE;
        break;
    default:
        break;
    }
    return TOK_IDENT;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

typedef struct LexBuf {
    char *data;
    size_t len;
    size_t cap;
} LexBuf;

static void lb_push(LexBuf *b, char c) {
    if (b->len + 1 >= b->cap) {
        size_t ncap = b->cap < 64 ? 64 : b->cap * 2;
        char *nd = (char *)realloc(b->data, ncap);
        if (!nd)
            return;
        b->data = nd;
        b->cap = ncap;
    }
    b->data[b->len++] = c;
}

static int lex_internal(const char *src, size_t len, DSTokenStream *ts, int base_line, int expr_mode);

/* Lexes f"..." / f'...' / f"""...""" into ("chunk" + TOK_FSTR_FMT(expr, fmt) + ...) */
static size_t lex_fstring(const char *src, size_t pos, size_t len, DSTokenStream *ts,
                          char quote, int triple, int *io_line, int col) {
    int start_line = *io_line;
    ts_emit_simple(ts, TOK_LPAREN, start_line, col);

    int part_count = 0;
    LexBuf lit = {NULL, 0, 0};

    while (pos < len) {
        /* Check closing quote(s) */
        if (triple) {
            if (pos + 2 < len && src[pos] == quote && src[pos + 1] == quote && src[pos + 2] == quote) {
                pos += 3;
                break;
            }
        } else {
            if (src[pos] == quote) {
                pos++;
                break;
            }
            if (src[pos] == '\n') {
                ts_error(ts, *io_line, "незакрытая f-строка");
                free(lit.data);
                return pos;
            }
        }

        /* Escaped {{ or }} */
        if (src[pos] == '{' && pos + 1 < len && src[pos + 1] == '{') {
            lb_push(&lit, '{');
            pos += 2;
            continue;
        }
        if (src[pos] == '}' && pos + 1 < len && src[pos + 1] == '}') {
            lb_push(&lit, '}');
            pos += 2;
            continue;
        }

        if (src[pos] == '{') {
            /* Flush accumulated literal segment if non-empty */
            if (lit.len > 0) {
                if (part_count > 0)
                    ts_emit_simple(ts, TOK_PLUS, *io_line, col);
                ts_emit_str(ts, TOK_STRING, lit.data, lit.len, *io_line, col);
                lit.len = 0;
                part_count++;
            }
            pos++; /* consume '{' */
            int expr_line = *io_line;
            size_t expr_start = pos;
            int depth = 1;
            int colon_idx = -1;
            char in_q = 0;
            while (pos < len && depth > 0) {
                char ch = src[pos];
                if (ch == '\n')
                    (*io_line)++;
                if (in_q) {
                    if (ch == '\\' && pos + 1 < len) {
                        pos += 2;
                        continue;
                    }
                    if (ch == in_q)
                        in_q = 0;
                } else {
                    if (ch == '\'' || ch == '"')
                        in_q = ch;
                    else if (ch == '{' || ch == '(' || ch == '[')
                        depth++;
                    else if (ch == '}' || ch == ')' || ch == ']') {
                        depth--;
                        if (depth == 0)
                            break;
                    } else if (ch == ':' && depth == 1 && colon_idx < 0) {
                        /* Make sure it's not !=, := or dict literal at depth > 1 */
                        colon_idx = (int)pos;
                    }
                }
                pos++;
            }
            if (depth != 0) {
                ts_error(ts, expr_line, "незакрытая '{' в f-строке");
                free(lit.data);
                return pos;
            }
            size_t expr_end = (colon_idx >= 0) ? (size_t)colon_idx : pos;
            const char *fmt_ptr = (colon_idx >= 0) ? (src + colon_idx + 1) : "";
            size_t fmt_len = (colon_idx >= 0) ? (pos - (size_t)colon_idx - 1) : 0;
            pos++; /* consume '}' */

            if (part_count > 0)
                ts_emit_simple(ts, TOK_PLUS, expr_line, col);

            /* Emit: TOK_FSTR_FMT (str_ptr = fmt) '(' <sub-tokens> ')' */
            ts_emit_str(ts, TOK_FSTR_FMT, fmt_ptr, fmt_len, expr_line, col);
            ts_emit_simple(ts, TOK_LPAREN, expr_line, col);
            if (!lex_internal(src + expr_start, expr_end - expr_start, ts, expr_line, 1)) {
                free(lit.data);
                return pos;
            }
            ts_emit_simple(ts, TOK_RPAREN, expr_line, col);
            part_count++;
            continue;
        }

        if (src[pos] == '\\' && pos + 1 < len) {
            pos++;
            char esc = src[pos++];
            switch (esc) {
            case 'n': lb_push(&lit, '\n'); break;
            case 't': lb_push(&lit, '\t'); break;
            case 'r': lb_push(&lit, '\r'); break;
            case '\\': lb_push(&lit, '\\'); break;
            case '\'': lb_push(&lit, '\''); break;
            case '"': lb_push(&lit, '"'); break;
            case '0': lb_push(&lit, '\0'); break;
            case 'x':
                if (pos + 1 < len && hex_val(src[pos]) >= 0 && hex_val(src[pos + 1]) >= 0) {
                    lb_push(&lit, (char)((hex_val(src[pos]) << 4) | hex_val(src[pos + 1])));
                    pos += 2;
                } else {
                    lb_push(&lit, 'x');
                }
                break;
            default:
                lb_push(&lit, esc);
                break;
            }
            continue;
        }

        if (src[pos] == '\n')
            (*io_line)++;
        if (src[pos] != '\r')
            lb_push(&lit, src[pos]);
        pos++;
    }

    if (lit.len > 0 || part_count == 0) {
        if (part_count > 0)
            ts_emit_simple(ts, TOK_PLUS, *io_line, col);
        ts_emit_str(ts, TOK_STRING, lit.data ? lit.data : "", lit.len, *io_line, col);
    }
    free(lit.data);
    ts_emit_simple(ts, TOK_RPAREN, *io_line, col);
    return pos;
}

static int lex_internal(const char *src, size_t len, DSTokenStream *ts, int base_line, int expr_mode) {
    size_t pos = 0;
    if (!expr_mode && len >= 3 && (uint8_t)src[0] == 0xEF && (uint8_t)src[1] == 0xBB && (uint8_t)src[2] == 0xBF) {
        pos = 3;
    }

    int line = base_line > 0 ? base_line : 1;
    int line_start_pos = (int)pos;
    int at_bol = expr_mode ? 0 : 1;
    int bracket_depth = 0;
    int indent_stack[64];
    int indent_top = 0;
    indent_stack[0] = 0;
    int emitted_on_line = 0;

    while (pos < len && !ts->had_error) {
        /* Handle Pythonic line-start indentation when outside brackets */
        if (at_bol && bracket_depth == 0 && !expr_mode) {
            int indent = 0;
            while (pos < len) {
                if (src[pos] == ' ') {
                    indent++;
                    pos++;
                } else if (src[pos] == '\t') {
                    indent = (indent + 4) & ~3;
                    pos++;
                } else if (src[pos] == '\r') {
                    pos++;
                } else {
                    break;
                }
            }
            /* Skip blank lines and comment-only lines without changing indentation */
            if (pos >= len)
                break;
            if (src[pos] == '\n') {
                pos++;
                line++;
                line_start_pos = (int)pos;
                continue;
            }
            if (src[pos] == '#') {
                while (pos < len && src[pos] != '\n')
                    pos++;
                continue;
            }

            at_bol = 0;
            emitted_on_line = 0;
            int cur_ind = indent_stack[indent_top];
            if (indent > cur_ind) {
                if (indent_top + 1 >= 64) {
                    ts_error(ts, line, "слишком глубокая вложенность отступов");
                    return 0;
                }
                indent_stack[++indent_top] = indent;
                ts_emit_simple(ts, TOK_INDENT, line, indent + 1);
            } else if (indent < cur_ind) {
                while (indent_top > 0 && indent < indent_stack[indent_top]) {
                    indent_top--;
                    ts_emit_simple(ts, TOK_DEDENT, line, indent + 1);
                }
                if (indent != indent_stack[indent_top]) {
                    ts_error(ts, line, "неверный уровень отступа (ожидалось %d пробелов)", indent_stack[indent_top]);
                    return 0;
                }
            }
        }

        char c = src[pos];
        int col = (int)pos - line_start_pos + 1;

        /* Whitespace */
        if (c == ' ' || c == '\t' || c == '\r') {
            pos++;
            continue;
        }
        /* Explicit line continuation backslash */
        if (c == '\\' && pos + 1 < len && (src[pos + 1] == '\n' || (src[pos + 1] == '\r' && pos + 2 < len && src[pos + 2] == '\n'))) {
            pos++;
            if (src[pos] == '\r') pos++;
            pos++;
            line++;
            line_start_pos = (int)pos;
            continue;
        }
        /* Comment */
        if (c == '#') {
            while (pos < len && src[pos] != '\n')
                pos++;
            continue;
        }
        /* Newline */
        if (c == '\n') {
            pos++;
            if (!expr_mode && bracket_depth == 0) {
                if (emitted_on_line)
                    ts_emit_simple(ts, TOK_NEWLINE, line, col);
                at_bol = 1;
                emitted_on_line = 0;
            }
            line++;
            line_start_pos = (int)pos;
            continue;
        }
        /* Semicolon acts as Python statement separator */
        if (c == ';') {
            pos++;
            if (!expr_mode && bracket_depth == 0 && emitted_on_line) {
                ts_emit_simple(ts, TOK_NEWLINE, line, col);
                emitted_on_line = 0;
            }
            continue;
        }

        emitted_on_line = 1;

        /* f-strings and r-strings: f"...", f'...', r"...", r'...' */
        if ((c == 'f' || c == 'F' || c == 'r' || c == 'R') && pos + 1 < len &&
            (src[pos + 1] == '"' || src[pos + 1] == '\'')) {
            int is_f = (c == 'f' || c == 'F');
            char q = src[pos + 1];
            pos += 2;
            int triple = 0;
            if (pos + 1 < len && src[pos] == q && src[pos + 1] == q) {
                triple = 1;
                pos += 2;
            }
            if (is_f) {
                pos = lex_fstring(src, pos, len, ts, q, triple, &line, col);
                continue;
            } else {
                /* Raw string literal */
                LexBuf rb = {NULL, 0, 0};
                int closed = 0;
                while (pos < len) {
                    if (triple) {
                        if (pos + 2 < len && src[pos] == q && src[pos + 1] == q && src[pos + 2] == q) {
                            pos += 3;
                            closed = 1;
                            break;
                        }
                    } else if (src[pos] == q) {
                        pos++;
                        closed = 1;
                        break;
                    }
                    if (src[pos] == '\n') line++;
                    if (src[pos] != '\r') lb_push(&rb, src[pos]);
                    pos++;
                }
                if (!closed)
                    ts_error(ts, line, "незакрытая raw-строка");
                ts_emit_str(ts, TOK_STRING, rb.data ? rb.data : "", rb.len, line, col);
                free(rb.data);
                continue;
            }
        }

        /* Normal / Triple-quoted Strings */
        if (c == '"' || c == '\'') {
            char q = c;
            int sline = line;
            pos++;
            int triple = 0;
            if (pos + 1 < len && src[pos] == q && src[pos + 1] == q) {
                triple = 1;
                pos += 2;
            }
            LexBuf sb = {NULL, 0, 0};
            int closed = 0;
            while (pos < len) {
                if (triple) {
                    if (pos + 2 < len && src[pos] == q && src[pos + 1] == q && src[pos + 2] == q) {
                        pos += 3;
                        closed = 1;
                        break;
                    }
                } else {
                    if (src[pos] == q) {
                        pos++;
                        closed = 1;
                        break;
                    }
                    if (src[pos] == '\n')
                        break;
                }
                if (src[pos] == '\\' && pos + 1 < len) {
                    pos++;
                    char esc = src[pos++];
                    switch (esc) {
                    case 'n': lb_push(&sb, '\n'); break;
                    case 't': lb_push(&sb, '\t'); break;
                    case 'r': lb_push(&sb, '\r'); break;
                    case '\\': lb_push(&sb, '\\'); break;
                    case '\'': lb_push(&sb, '\''); break;
                    case '"': lb_push(&sb, '"'); break;
                    case '0': lb_push(&sb, '\0'); break;
                    case 'x':
                        if (pos + 1 < len && hex_val(src[pos]) >= 0 && hex_val(src[pos + 1]) >= 0) {
                            lb_push(&sb, (char)((hex_val(src[pos]) << 4) | hex_val(src[pos + 1])));
                            pos += 2;
                        } else {
                            lb_push(&sb, 'x');
                        }
                        break;
                    default:
                        lb_push(&sb, esc);
                        break;
                    }
                    continue;
                }
                if (src[pos] == '\n')
                    line++;
                if (src[pos] != '\r')
                    lb_push(&sb, src[pos]);
                pos++;
            }
            if (!closed)
                ts_error(ts, sline, "незакрытый строковый литерал");
            ts_emit_str(ts, TOK_STRING, sb.data ? sb.data : "", sb.len, sline, col);
            free(sb.data);
            continue;
        }

        /* Numbers: decimal, float, hex (0x), binary (0b), with '_' separators */
        if ((c >= '0' && c <= '9') || (c == '.' && pos + 1 < len && src[pos + 1] >= '0' && src[pos + 1] <= '9')) {
            double val = 0.0;
            if (c == '0' && pos + 1 < len && (src[pos + 1] == 'x' || src[pos + 1] == 'X')) {
                pos += 2;
                uint64_t hv = 0;
                int digits = 0;
                while (pos < len) {
                    if (src[pos] == '_') { pos++; continue; }
                    int d = hex_val(src[pos]);
                    if (d < 0) break;
                    hv = (hv << 4) | (uint64_t)d;
                    digits++;
                    pos++;
                }
                if (digits == 0)
                    ts_error(ts, line, "ожидались шестнадцатеричные цифры после 0x");
                val = (double)hv;
            } else if (c == '0' && pos + 1 < len && (src[pos + 1] == 'b' || src[pos + 1] == 'B')) {
                pos += 2;
                uint64_t bv = 0;
                int digits = 0;
                while (pos < len) {
                    if (src[pos] == '_') { pos++; continue; }
                    if (src[pos] != '0' && src[pos] != '1') break;
                    bv = (bv << 1) | (uint64_t)(src[pos] - '0');
                    digits++;
                    pos++;
                }
                if (digits == 0)
                    ts_error(ts, line, "ожидались двоичные цифры после 0b");
                val = (double)bv;
            } else {
                char nbuf[128];
                int ni = 0;
                int seen_dot = 0;
                int seen_exp = 0;
                while (pos < len && ni < 120) {
                    char nc = src[pos];
                    if (nc == '_') { pos++; continue; }
                    if (nc >= '0' && nc <= '9') {
                        nbuf[ni++] = nc;
                        pos++;
                    } else if (nc == '.' && !seen_dot && !seen_exp) {
                        seen_dot = 1;
                        nbuf[ni++] = '.';
                        pos++;
                    } else if ((nc == 'e' || nc == 'E') && !seen_exp) {
                        seen_exp = 1;
                        nbuf[ni++] = nc;
                        pos++;
                        if (pos < len && (src[pos] == '+' || src[pos] == '-'))
                            nbuf[ni++] = src[pos++];
                    } else {
                        break;
                    }
                }
                nbuf[ni] = '\0';
                val = strtod(nbuf, NULL);
            }
            DSToken t;
            memset(&t, 0, sizeof(t));
            t.kind = TOK_NUMBER;
            t.line = line;
            t.col = col;
            t.num_val = val;
            ts_emit(ts, t);
            continue;
        }

        /* Identifiers & Keywords (ASCII + UTF-8 Cyrillic) */
        if (is_ident_start((uint8_t)c)) {
            size_t start = pos;
            while (pos < len && is_ident_cont((uint8_t)src[pos]))
                pos++;
            size_t ilen = pos - start;
            DSTokenKind kw = lookup_keyword(src + start, ilen);
            /* Support Python 2-word operators: 'not in' */
            if (kw == TOK_IDENT) {
                ts_emit_str(ts, TOK_IDENT, src + start, ilen, line, col);
            } else {
                ts_emit_simple(ts, kw, line, col);
            }
            continue;
        }

        /* Operators & Delimiters */
        char c2 = (pos + 1 < len) ? src[pos + 1] : '\0';
        char c3 = (pos + 2 < len) ? src[pos + 2] : '\0';

        if (c == '(') { bracket_depth++; pos++; ts_emit_simple(ts, TOK_LPAREN, line, col); continue; }
        if (c == ')') { if (bracket_depth > 0) bracket_depth--; pos++; ts_emit_simple(ts, TOK_RPAREN, line, col); continue; }
        if (c == '[') { bracket_depth++; pos++; ts_emit_simple(ts, TOK_LBRACKET, line, col); continue; }
        if (c == ']') { if (bracket_depth > 0) bracket_depth--; pos++; ts_emit_simple(ts, TOK_RBRACKET, line, col); continue; }
        if (c == '{') { bracket_depth++; pos++; ts_emit_simple(ts, TOK_LBRACE, line, col); continue; }
        if (c == '}') { if (bracket_depth > 0) bracket_depth--; pos++; ts_emit_simple(ts, TOK_RBRACE, line, col); continue; }

        if (c == '/' && c2 == '/' && c3 == '=') { pos += 3; ts_emit_simple(ts, TOK_FLOORDIVEQ, line, col); continue; }
        if (c == '*' && c2 == '*') { pos += 2; ts_emit_simple(ts, TOK_STARSTAR, line, col); continue; }
        if (c == '/' && c2 == '/') { pos += 2; ts_emit_simple(ts, TOK_SLASHSLASH, line, col); continue; }
        if (c == '<' && c2 == '<') { pos += 2; ts_emit_simple(ts, TOK_LSHIFT, line, col); continue; }
        if (c == '>' && c2 == '>') { pos += 2; ts_emit_simple(ts, TOK_RSHIFT, line, col); continue; }
        if (c == '=' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_EQEQ, line, col); continue; }
        if (c == '!' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_BANGEQ, line, col); continue; }
        if (c == '<' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_LE, line, col); continue; }
        if (c == '>' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_GE, line, col); continue; }
        if (c == '+' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_PLUSEQ, line, col); continue; }
        if (c == '-' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_MINUSEQ, line, col); continue; }
        if (c == '*' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_STAREQ, line, col); continue; }
        if (c == '/' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_SLASHEQ, line, col); continue; }
        if (c == '%' && c2 == '=') { pos += 2; ts_emit_simple(ts, TOK_PERCENTEQ, line, col); continue; }
        if (c == '-' && c2 == '>') { pos += 2; ts_emit_simple(ts, TOK_ARROW, line, col); continue; }

        pos++;
        switch (c) {
        case '+': ts_emit_simple(ts, TOK_PLUS, line, col); break;
        case '-': ts_emit_simple(ts, TOK_MINUS, line, col); break;
        case '*': ts_emit_simple(ts, TOK_STAR, line, col); break;
        case '/': ts_emit_simple(ts, TOK_SLASH, line, col); break;
        case '%': ts_emit_simple(ts, TOK_PERCENT, line, col); break;
        case '&': ts_emit_simple(ts, TOK_AMP, line, col); break;
        case '|': ts_emit_simple(ts, TOK_PIPE, line, col); break;
        case '^': ts_emit_simple(ts, TOK_CARET, line, col); break;
        case '~': ts_emit_simple(ts, TOK_TILDE, line, col); break;
        case '=': ts_emit_simple(ts, TOK_EQ, line, col); break;
        case '<': ts_emit_simple(ts, TOK_LT, line, col); break;
        case '>': ts_emit_simple(ts, TOK_GT, line, col); break;
        case ',': ts_emit_simple(ts, TOK_COMMA, line, col); break;
        case ':': ts_emit_simple(ts, TOK_COLON, line, col); break;
        case '.': ts_emit_simple(ts, TOK_DOT, line, col); break;
        default:
            ts_error(ts, line, "неожиданный символ '%c' (0x%02x)", (c >= 32 && c < 127) ? c : '?', (uint8_t)c);
            return 0;
        }
    }

    if (!expr_mode && !ts->had_error) {
        if (bracket_depth > 0) {
            ts_error(ts, line, "незакрытая скобка '(' / '[' / '{' в конце файла");
            return 0;
        }
        if (emitted_on_line)
            ts_emit_simple(ts, TOK_NEWLINE, line, 1);
        while (indent_top > 0) {
            indent_top--;
            ts_emit_simple(ts, TOK_DEDENT, line, 1);
        }
        ts_emit_simple(ts, TOK_EOF, line, 1);
    }
    return !ts->had_error;
}

int ds_lex_source(const char *src, size_t len, DSTokenStream *out, int base_line) {
    memset(out, 0, sizeof(*out));
    return lex_internal(src ? src : "", src ? len : 0, out, base_line, 0);
}

void ds_token_stream_free(DSTokenStream *ts) {
    if (!ts)
        return;
    for (int i = 0; i < ts->owned_count; i++)
        free(ts->owned_bufs[i]);
    free(ts->owned_bufs);
    free(ts->tokens);
    memset(ts, 0, sizeof(*ts));
}

const char *ds_token_kind_name(DSTokenKind k) {
    switch (k) {
    case TOK_EOF: return "конец файла";
    case TOK_NEWLINE: return "перевод строки";
    case TOK_INDENT: return "отступ";
    case TOK_DEDENT: return "конец блока";
    case TOK_NUMBER: return "число";
    case TOK_STRING: return "строка";
    case TOK_IDENT: return "имя";
    case TOK_COLON: return "':'";
    case TOK_RPAREN: return "')'";
    case TOK_RBRACKET: return "']'";
    case TOK_RBRACE: return "'}'";
    default: return "токен";
    }
}
