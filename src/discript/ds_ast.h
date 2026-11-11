#ifndef DS_AST_H
#define DS_AST_H

#include "discript/ds_internal.h"

/* ============================================================================
 * DIRECT-THREADED AST: every node carries its own specialized evaluation
 * function pointer. There is NO bytecode format and NO interpreter dispatch
 * loop — the compiler resolves each node to a C closure and calls it directly.
 * ============================================================================ */

typedef enum {
    EX_NUM = 0,
    EX_STR,
    EX_NAME,
    EX_LIST,
    EX_DICT,
    EX_BINOP,
    EX_UNOP,
    EX_CALL,
    EX_DOT,
    EX_INDEX,
    EX_SLICE,
    EX_TERNARY,
    EX_LAMBDA,
    EX_FMT
} DSExprKind;

typedef enum {
    OP_ADD = 0,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_FLOORDIV,
    OP_MOD,
    OP_POW,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_AND,
    OP_OR,
    OP_BITAND,
    OP_BITOR,
    OP_BITXOR,
    OP_SHL,
    OP_SHR,
    OP_IN,
    OP_NOTIN,
    OP_IS,
    OP_ISNOT
} DSBinOpKind;

typedef enum {
    NAME_GLOBAL = 0,
    NAME_LOCAL = 1,
    NAME_UPVAL = 2,
    NAME_CLASSATTR = 3
} DSNameScope;

struct DSExprNode {
    uint8_t kind;
    uint8_t op;
    uint8_t scope;
    uint8_t aux;
    int line;
    DSExprEvalFn eval; /* specialized evaluator assigned by ds_compiler.c */
    double num;
    DSString *str;
    int index;
    struct DSExprNode *a;
    struct DSExprNode *b;
    struct DSExprNode *c;
    struct DSExprNode **items; /* list/dict values, call args */
    struct DSExprNode **keys;  /* dict keys */
    int count;
    DSFuncProto *proto; /* lambda */
    DSInlineCache ic;   /* per-site inline cache for dot/index dispatch */
};

typedef enum {
    ST_EXPR = 0,
    ST_ASSIGN,
    ST_AUGASSIGN,
    ST_IF,
    ST_WHILE,
    ST_FOR,
    ST_BREAK,
    ST_CONTINUE,
    ST_RETURN,
    ST_PASS,
    ST_DEF,
    ST_CLASS,
    ST_IMPORT,
    ST_DEL,
    ST_ASSERT,
    ST_GLOBAL
} DSStmtKind;

struct DSStmtNode {
    uint8_t kind;
    uint8_t op;    /* augassign operator */
    uint8_t flags; /* 1 = print value in REPL, 2 = for-loop tuple unpack */
    uint8_t pad;
    int line;
    DSStmtExecFn exec;
    DSExprNode *expr;
    DSExprNode *cond;
    DSStmtNode *body;
    DSStmtNode *orelse;
    DSStmtNode *next;
    DSExprNode **targets;
    int ntargets;
    DSExprNode **conds;
    DSStmtNode **bodies;
    int nbranches;
    int nconds;
    DSExprNode **items; /* import names */
    int count;
    DSFuncProto *proto; /* def / class -> __init__ etc. */
    DSString *name;     /* def/class/import name */
    uint8_t target_scope; /* NAME_GLOBAL / NAME_LOCAL / NAME_UPVAL for def/class/import */
    int target_index;
};

/* ---- Compilation Arena --------------------------------------------------- */
typedef struct DSCompiledChunk {
    struct DSCompiledChunk *next;
    size_t used;
    size_t cap;
    uint8_t *data; /* 64-byte aligned payload */
} DSCompiledChunk;

DSCompiledArena *ds_arena_create(void);
void *ds_arena_alloc(DSCompiledArena *arena, size_t size, size_t align);
void ds_arena_retain(DSCompiledArena *arena);
void ds_arena_release(DSCompiledArena *arena);

/* ---- Parser (ds_parser.c) ----------------------------------------------- */
typedef struct DSParser DSParser;

DSStmtNode *ds_parse_program(DSVM *vm, DSCompiledArena *arena, const char *filename, const char *src, size_t len,
                             int is_repl, char *err, size_t err_cap, int *err_line);
DSStmtNode *ds_parse_expression_only(DSVM *vm, DSCompiledArena *arena, const char *filename, const char *src,
                                     size_t len, char *err, size_t err_cap, int *err_line);

/* ---- Compiler passes (ds_compiler.c) ----------------------------------- */
void ds_compile_block(DSVM *vm, DSModule *mod, DSStmtNode *block, DSCompiledArena *arena, int repl_top);

#endif /* DS_AST_H */
