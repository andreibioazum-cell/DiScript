#ifndef DISCRIPT_IDE_HIGHLIGHT_H
#define DISCRIPT_IDE_HIGHLIGHT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Token classes the editor paints differently. IDE_TK_NONE means "plain code"
 * and is never emitted; runs between tokens inherit the default colour. */
typedef enum {
    IDE_TK_DEFAULT = 0,
    IDE_TK_KEYWORD,
    IDE_TK_BUILTIN,
    IDE_TK_NUMBER,
    IDE_TK_STRING,
    IDE_TK_COMMENT,
    IDE_TK_FUNCTION, /* name right after `def` */
    IDE_TK_CLASS,    /* name right after `class` */
    IDE_TK_OPERATOR,
    IDE_TK_TYPE
} IDETokenKind;

typedef struct IDEToken {
    size_t start;   /* byte offset into the buffer */
    size_t end;     /* exclusive */
    unsigned char kind;
} IDEToken;

/* Tokenises `text` (the whole document) into `out`, returning the number of
 * tokens written. Tokens are clipped to `max_tokens`; pass 0 to query the
 * count needed for the whole text. The tokenizer is stateless across calls, so
 * multi-line constructs (triple-quoted strings, block comments) are handled by
 * scanning from the start of the document every time. */
int ide_highlight(const char *text, size_t len, IDEToken *out, int max_tokens);

#ifdef __cplusplus
}
#endif

#endif /* DISCRIPT_IDE_HIGHLIGHT_H */
