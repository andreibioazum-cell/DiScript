#ifndef DS_LEXER_H
#define DS_LEXER_H

#include "discript/ds_internal.h"

typedef enum DSTokenKind {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,

    /* Literals & Identifiers */
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENT,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NONE,

    /* Keywords */
    TOK_DEF,
    TOK_RETURN,
    TOK_IF,
    TOK_ELIF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_IN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_PASS,
    TOK_CLASS,
    TOK_IMPORT,
    TOK_FROM,
    TOK_AS,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_GLOBAL,
    TOK_LAMBDA,
    TOK_DEL,
    TOK_ASSERT,
    TOK_TRY,
    TOK_EXCEPT,

    /* F-string format intrinsic emitted by lexer */
    TOK_FSTR_FMT,

    /* Operators & Punctuation */
    TOK_PLUS,        /* + */
    TOK_MINUS,       /* - */
    TOK_STAR,        /* * */
    TOK_STARSTAR,    /* ** */
    TOK_SLASH,       /* / */
    TOK_SLASHSLASH,  /* // */
    TOK_PERCENT,     /* % */
    TOK_AMP,         /* & */
    TOK_PIPE,        /* | */
    TOK_CARET,       /* ^ */
    TOK_TILDE,       /* ~ */
    TOK_LSHIFT,      /* << */
    TOK_RSHIFT,      /* >> */

    TOK_EQ,          /* = */
    TOK_PLUSEQ,      /* += */
    TOK_MINUSEQ,     /* -= */
    TOK_STAREQ,      /* *= */
    TOK_SLASHEQ,     /* /= */
    TOK_FLOORDIVEQ,  /* //= */
    TOK_PERCENTEQ,   /* %= */

    TOK_EQEQ,        /* == */
    TOK_BANGEQ,      /* != */
    TOK_LT,          /* < */
    TOK_LE,          /* <= */
    TOK_GT,          /* > */
    TOK_GE,          /* >= */

    TOK_LPAREN,      /* ( */
    TOK_RPAREN,      /* ) */
    TOK_LBRACKET,    /* [ */
    TOK_RBRACKET,    /* ] */
    TOK_LBRACE,      /* { */
    TOK_RBRACE,      /* } */
    TOK_COMMA,       /* , */
    TOK_COLON,       /* : */
    TOK_DOT,         /* . */
    TOK_ARROW        /* -> (optional return type annotation) */
} DSTokenKind;

typedef struct DSToken {
    DSTokenKind kind;
    int line;
    int col;
    double num_val;
    const char *str_ptr; /* Allocated inside compile scratch pool for strings/idents */
    uint32_t str_len;
} DSToken;

typedef struct DSTokenStream {
    DSToken *tokens;
    int count;
    int cap;
    char **owned_bufs;
    int owned_count;
    int owned_cap;
    int had_error;
    int error_line;
    char error_msg[256];
} DSTokenStream;

int ds_lex_source(const char *src, size_t len, DSTokenStream *out, int base_line);
void ds_token_stream_free(DSTokenStream *ts);
const char *ds_token_kind_name(DSTokenKind k);

#endif /* DS_LEXER_H */
