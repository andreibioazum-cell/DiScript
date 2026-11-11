/* Recursive-descent parser: Pythonic indentation blocks -> Direct-Threaded AST */

#include "discript/ds_ast.h"
#include "discript/ds_lexer.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct DSParser {
    DSVM *vm;
    DSCompiledArena *arena;
    DSTokenStream ts;
    int pos;
    const char *filename;
    char *err;
    size_t err_cap;
    int *err_line;
    int failed;
    int is_repl;
};

static DSExprNode *parse_expr(DSParser *p);
static DSExprNode *parse_testlist(DSParser *p);
static DSStmtNode *parse_block_after_colon(DSParser *p);
static DSFuncProto *parse_lambda(DSParser *p, int line);

static void *p_alloc(DSParser *p, size_t n) { return ds_arena_alloc(p->arena, n, 16); }

static DSExprNode *ex_new(DSParser *p, DSExprKind kind, int line) {
    DSExprNode *e = (DSExprNode *)p_alloc(p, sizeof(DSExprNode));
    memset(e, 0, sizeof(*e));
    e->kind = (uint8_t)kind;
    e->line = line;
    return e;
}

static DSStmtNode *st_new(DSParser *p, DSStmtKind kind, int line) {
    DSStmtNode *s = (DSStmtNode *)p_alloc(p, sizeof(DSStmtNode));
    memset(s, 0, sizeof(*s));
    s->kind = (uint8_t)kind;
    s->line = line;
    return s;
}

static DSToken *tok(DSParser *p) {
    if (p->ts.count == 0)
        return NULL;
    if (p->pos >= p->ts.count)
        return &p->ts.tokens[p->ts.count - 1];
    return &p->ts.tokens[p->pos];
}

static DSTokenKind tk(DSParser *p) {
    DSToken *t = tok(p);
    return t ? t->kind : TOK_EOF;
}

static int tok_line(DSParser *p) {
    DSToken *t = tok(p);
    return t ? t->line : 0;
}

static DSToken *peek(DSParser *p, int n) {
    int i = p->pos + n;
    if (i >= p->ts.count)
        i = p->ts.count > 0 ? p->ts.count - 1 : 0;
    return &p->ts.tokens[i];
}

static void advance(DSParser *p) {
    if (p->pos < p->ts.count - 1)
        p->pos++;
}

static int check(DSParser *p, DSTokenKind k) { return tk(p) == k; }

static int match(DSParser *p, DSTokenKind k) {
    if (check(p, k)) {
        advance(p);
        return 1;
    }
    return 0;
}

static void perr(DSParser *p, int line, const char *fmt, ...) {
    if (p->failed)
        return;
    p->failed = 1;
    if (p->err && p->err_cap > 0) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(p->err, p->err_cap, fmt, ap);
        va_end(ap);
    }
    if (p->err_line)
        *p->err_line = line;
}

static int expect(DSParser *p, DSTokenKind k, const char *what) {
    if (match(p, k))
        return 1;
    perr(p, tok_line(p), "ожидалось %s, но найдено %s (строка %d)", what, ds_token_kind_name(tk(p)), tok_line(p));
    return 0;
}

static void skip_newlines(DSParser *p) {
    while (check(p, TOK_NEWLINE))
        advance(p);
}

static void push_item(DSParser *p, DSExprNode ***arr, int *count, DSExprNode *item) {
    int n = *count;
    DSExprNode **na = (DSExprNode **)p_alloc(p, sizeof(DSExprNode *) * (size_t)(n + 1));
    if (n > 0 && *arr)
        memcpy(na, *arr, sizeof(DSExprNode *) * (size_t)n);
    na[n] = item;
    *arr = na;
    *count = n + 1;
}

static void push_stmt(DSParser *p, DSStmtNode ***arr, int *count, DSStmtNode *item) {
    int n = *count;
    DSStmtNode **na = (DSStmtNode **)p_alloc(p, sizeof(DSStmtNode *) * (size_t)(n + 1));
    if (n > 0 && *arr)
        memcpy(na, *arr, sizeof(DSStmtNode *) * (size_t)n);
    na[n] = item;
    *arr = na;
    *count = n + 1;
}

static DSString *intern_str(DSParser *p, const char *s, size_t n) {
    DSString *str = ds_string_new(p->vm, s ? s : "", n);
    if (str)
        str->pinned = 1; /* compile-time literals stay alive while the arena lives */
    return str;
}

static DSString *tok_str(DSParser *p) {
    DSToken *t = tok(p);
    if (!t)
        return intern_str(p, "", 0);
    return intern_str(p, t->str_ptr, t->str_len);
}

/* ---- Expressions -------------------------------------------------------- */

static DSExprNode *parse_atom(DSParser *p) {
    DSToken *t = tok(p);
    if (!t || p->failed)
        return NULL;
    int line = t->line;
    DSTokenKind k = t->kind;

    switch (k) {
    case TOK_NUMBER: {
        advance(p);
        DSExprNode *e = ex_new(p, EX_NUM, line);
        e->num = t->num_val;
        return e;
    }
    case TOK_STRING: {
        DSString *s = tok_str(p);
        advance(p);
        DSExprNode *e = ex_new(p, EX_STR, line);
        e->str = s;
        return e;
    }
    case TOK_TRUE:
    case TOK_FALSE: {
        advance(p);
        DSExprNode *e = ex_new(p, EX_NUM, line);
        e->num = (k == TOK_TRUE) ? 1.0 : 0.0;
        e->aux = 1; /* boolean literal */
        return e;
    }
    case TOK_NONE: {
        advance(p);
        DSExprNode *e = ex_new(p, EX_NUM, line);
        e->aux = 2; /* None literal */
        return e;
    }
    case TOK_IDENT: {
        DSString *name = tok_str(p);
        advance(p);
        DSExprNode *e = ex_new(p, EX_NAME, line);
        e->str = name;
        e->scope = NAME_GLOBAL;
        return e;
    }
    case TOK_FSTR_FMT: {
        DSString *fmt = tok_str(p);
        advance(p);
        if (!expect(p, TOK_LPAREN, "'('"))
            return NULL;
        DSExprNode *inner = parse_expr(p);
        if (!expect(p, TOK_RPAREN, "')'"))
            return NULL;
        DSExprNode *e = ex_new(p, EX_FMT, line);
        e->a = inner;
        e->b = ex_new(p, EX_STR, line);
        e->b->str = fmt;
        return e;
    }
    case TOK_LPAREN: {
        advance(p);
        if (match(p, TOK_RPAREN)) {
            DSExprNode *e = ex_new(p, EX_LIST, line);
            return e;
        }
        DSExprNode *inner = parse_testlist(p);
        expect(p, TOK_RPAREN, "')'");
        return inner;
    }
    case TOK_LBRACKET: {
        advance(p);
        DSExprNode *e = ex_new(p, EX_LIST, line);
        while (!check(p, TOK_RBRACKET) && !p->failed) {
            push_item(p, &e->items, &e->count, parse_expr(p));
            if (!match(p, TOK_COMMA))
                break;
            skip_newlines(p);
        }
        expect(p, TOK_RBRACKET, "']'");
        return e;
    }
    case TOK_LBRACE: {
        advance(p);
        DSExprNode *e = ex_new(p, EX_DICT, line);
        while (!check(p, TOK_RBRACE) && !p->failed) {
            skip_newlines(p);
            if (check(p, TOK_RBRACE))
                break;
            DSExprNode *first = parse_expr(p);
            if (match(p, TOK_COLON)) {
                DSExprNode *v = parse_expr(p);
                int base = e->count;
                int kc = base, vc = base;
                push_item(p, &e->keys, &kc, first);
                push_item(p, &e->items, &vc, v);
                e->count = base + 1;
            } else {
                perr(p, line, "ожидалось ':' в словаре");
                return e;
            }
            if (!match(p, TOK_COMMA))
                break;
        }
        expect(p, TOK_RBRACE, "'}'");
        return e;
    }
    case TOK_LAMBDA: {
        advance(p);
        DSFuncProto *proto = parse_lambda(p, line);
        if (!proto)
            return NULL;
        DSExprNode *e = ex_new(p, EX_LAMBDA, line);
        e->proto = proto;
        return e;
    }
    default:
        perr(p, line, "неожиданный токен %s в выражении (строка %d)", ds_token_kind_name(k), line);
        return NULL;
    }
}

static DSExprNode *parse_call_args(DSParser *p, DSExprNode *fn, int line) {
    DSExprNode *call = ex_new(p, EX_CALL, line);
    call->a = fn;
    while (!check(p, TOK_RPAREN) && !p->failed) {
        skip_newlines(p);
        if (check(p, TOK_RPAREN))
            break;
        if (check(p, TOK_STAR) || check(p, TOK_STARSTAR)) {
            perr(p, line, "распаковка аргументов (* / **) пока не поддерживается");
            return NULL;
        }
        if (check(p, TOK_IDENT) && peek(p, 1)->kind == TOK_EQ) {
            perr(p, line, "именованные аргументы пока не поддерживаются");
            return NULL;
        }
        push_item(p, &call->items, &call->count, parse_expr(p));
        if (!match(p, TOK_COMMA))
            break;
    }
    expect(p, TOK_RPAREN, "')'");
    return call;
}

static DSExprNode *parse_trailers(DSParser *p, DSExprNode *e) {
    for (;;) {
        if (p->failed || !e)
            return e;
        if (check(p, TOK_LPAREN)) {
            int line = tok_line(p);
            advance(p);
            e = parse_call_args(p, e, line);
            continue;
        }
        if (check(p, TOK_DOT)) {
            int line = tok_line(p);
            advance(p);
            if (!check(p, TOK_IDENT)) {
                perr(p, line, "ожидалось имя атрибута после '.'");
                return e;
            }
            DSExprNode *dot = ex_new(p, EX_DOT, line);
            dot->a = e;
            dot->str = tok_str(p);
            advance(p);
            e = dot;
            continue;
        }
        if (check(p, TOK_LBRACKET)) {
            int line = tok_line(p);
            advance(p);
            if (check(p, TOK_COLON)) {
                DSExprNode *slice = ex_new(p, EX_SLICE, line);
                slice->a = e;
                slice->b = NULL;
                advance(p);
                if (!check(p, TOK_COLON) && !check(p, TOK_RBRACKET))
                    slice->c = parse_expr(p);
                while (match(p, TOK_COLON)) {
                    if (!check(p, TOK_RBRACKET))
                        parse_expr(p);
                }
                expect(p, TOK_RBRACKET, "']'");
                e = slice;
                continue;
            }
            DSExprNode *idx = parse_testlist(p);
            if (match(p, TOK_COLON)) {
                DSExprNode *slice = ex_new(p, EX_SLICE, line);
                slice->a = e;
                slice->b = idx;
                if (!check(p, TOK_COLON) && !check(p, TOK_RBRACKET))
                    slice->c = parse_expr(p);
                while (match(p, TOK_COLON)) {
                    if (!check(p, TOK_RBRACKET))
                        parse_expr(p);
                }
                expect(p, TOK_RBRACKET, "']'");
                e = slice;
                continue;
            }
            expect(p, TOK_RBRACKET, "']'");
            DSExprNode *ix = ex_new(p, EX_INDEX, line);
            ix->a = e;
            ix->b = idx;
            e = ix;
            continue;
        }
        break;
    }
    return e;
}

static DSExprNode *parse_unary(DSParser *p) {
    int line = tok_line(p);
    if (check(p, TOK_MINUS)) {
        advance(p);
        DSExprNode *e = ex_new(p, EX_UNOP, line);
        e->op = OP_SUB;
        e->a = parse_unary(p);
        return e;
    }
    if (check(p, TOK_PLUS)) {
        advance(p);
        return parse_unary(p);
    }
    if (check(p, TOK_NOT)) {
        advance(p);
        DSExprNode *e = ex_new(p, EX_UNOP, line);
        e->op = OP_ISNOT; /* logical not (compiler-specialized) */
        e->a = parse_unary(p);
        return e;
    }
    if (check(p, TOK_TILDE)) {
        advance(p);
        DSExprNode *e = ex_new(p, EX_UNOP, line);
        e->op = OP_BITXOR;
        e->a = parse_unary(p);
        return e;
    }
    return parse_trailers(p, parse_atom(p));
}

static int binop_prec(DSTokenKind k, int *out_op) {
    switch (k) {
    case TOK_OR: *out_op = OP_OR; return 1;
    case TOK_AND: *out_op = OP_AND; return 2;
    case TOK_EQEQ: *out_op = OP_EQ; return 3;
    case TOK_BANGEQ: *out_op = OP_NE; return 3;
    case TOK_LT: *out_op = OP_LT; return 3;
    case TOK_LE: *out_op = OP_LE; return 3;
    case TOK_GT: *out_op = OP_GT; return 3;
    case TOK_GE: *out_op = OP_GE; return 3;
    case TOK_IN: *out_op = OP_IN; return 3;
    case TOK_PIPE: *out_op = OP_BITOR; return 4;
    case TOK_CARET: *out_op = OP_BITXOR; return 5;
    case TOK_AMP: *out_op = OP_BITAND; return 6;
    case TOK_LSHIFT: *out_op = OP_SHL; return 7;
    case TOK_RSHIFT: *out_op = OP_SHR; return 7;
    case TOK_PLUS: *out_op = OP_ADD; return 8;
    case TOK_MINUS: *out_op = OP_SUB; return 8;
    case TOK_STAR: *out_op = OP_MUL; return 9;
    case TOK_SLASH: *out_op = OP_DIV; return 9;
    case TOK_SLASHSLASH: *out_op = OP_FLOORDIV; return 9;
    case TOK_PERCENT: *out_op = OP_MOD; return 9;
    case TOK_STARSTAR: *out_op = OP_POW; return 11;
    default: return 0;
    }
}

static DSExprNode *parse_bin(DSParser *p, int min_prec) {
    DSExprNode *left = parse_unary(p);
    for (;;) {
        if (p->failed)
            return left;
        int op = 0;
        int prec = binop_prec(tk(p), &op);
        if (prec == 0 || prec < min_prec)
            break;
        int line = tok_line(p);
        advance(p);
        int next_min = (op == OP_POW) ? prec : prec + 1;
        DSExprNode *right = parse_bin(p, next_min);
        DSExprNode *e = ex_new(p, EX_BINOP, line);
        e->op = (uint8_t)op;
        e->a = left;
        e->b = right;
        left = e;
    }
    return left;
}

static DSExprNode *parse_ternary(DSParser *p) {
    DSExprNode *value = parse_bin(p, 1);
    if (check(p, TOK_IF)) {
        int line = tok_line(p);
        advance(p);
        DSExprNode *test = parse_bin(p, 1);
        DSExprNode *e = ex_new(p, EX_TERNARY, line);
        e->a = value;
        e->b = test;
        if (match(p, TOK_ELSE))
            e->c = parse_ternary(p);
        return e;
    }
    return value;
}

static DSExprNode *parse_expr(DSParser *p) { return parse_ternary(p); }

/* Comma-separated top-level expression list -> list node (tuples become lists) */
static DSExprNode *parse_testlist(DSParser *p) {
    int line = tok_line(p);
    DSExprNode *first = parse_expr(p);
    if (!check(p, TOK_COMMA) || p->failed)
        return first;
    DSExprNode *lst = ex_new(p, EX_LIST, line);
    push_item(p, &lst->items, &lst->count, first);
    while (match(p, TOK_COMMA)) {
        if (check(p, TOK_RPAREN) || check(p, TOK_RBRACKET) || check(p, TOK_RBRACE) || check(p, TOK_NEWLINE) ||
            check(p, TOK_EOF) || check(p, TOK_EQ) || check(p, TOK_COLON))
            break;
        push_item(p, &lst->items, &lst->count, parse_expr(p));
    }
    return lst;
}

/* ---- Statements --------------------------------------------------------- */

static int is_stmt_end(DSParser *p) {
    return check(p, TOK_NEWLINE) || check(p, TOK_EOF) || check(p, TOK_DEDENT);
}

static void parse_params(DSParser *p, DSFuncProto *fn) {
    int arity = 0;
    int min_arity = 0;
    int varargs = 0;
    int seen_default = 0;
    int cap = 0;
    if (!expect(p, TOK_LPAREN, "'('"))
        return;
    while (!check(p, TOK_RPAREN) && !p->failed) {
        skip_newlines(p);
        if (check(p, TOK_RPAREN))
            break;
        if (match(p, TOK_STAR)) {
            varargs = 1;
            if (check(p, TOK_IDENT))
                advance(p);
            if (match(p, TOK_COLON))
                parse_expr(p);
            if (match(p, TOK_COMMA))
                continue;
            break;
        }
        if (!check(p, TOK_IDENT)) {
            perr(p, tok_line(p), "ожидалось имя параметра");
            return;
        }
        DSString *pname = tok_str(p);
        arity++;
        advance(p);
        if (arity > cap) {
            int ncap = cap < 4 ? 4 : cap * 2;
            DSString **np = (DSString **)realloc(fn->params, (size_t)ncap * sizeof(DSString *));
            DSExprNode **nd = (DSExprNode **)realloc(fn->default_exprs, (size_t)ncap * sizeof(DSExprNode *));
            if (np && nd) {
                for (int k = cap; k < ncap; k++) {
                    np[k] = NULL;
                    nd[k] = NULL;
                }
                fn->params = np;
                fn->default_exprs = nd;
                cap = ncap;
            }
        }
        fn->params[arity - 1] = pname;
        if (match(p, TOK_COLON))
            parse_expr(p); /* type annotation ignored at runtime */
        if (match(p, TOK_EQ)) {
            seen_default = 1;
            fn->default_exprs[arity - 1] = parse_expr(p);
            fn->num_defaults = arity;
        }
        if (!seen_default)
            min_arity = arity;
        if (!match(p, TOK_COMMA))
            break;
    }
    expect(p, TOK_RPAREN, "')'");
    if (match(p, TOK_ARROW))
        parse_expr(p); /* return annotation ignored */
    fn->arity = arity;
    fn->min_arity = min_arity;
    fn->has_varargs = varargs;
}

static DSStmtNode *append_chain(DSStmtNode *head, DSStmtNode *tail, DSStmtNode *s) {
    (void)tail;
    if (!head)
        return s;
    DSStmtNode *it = head;
    while (it->next)
        it = it->next;
    it->next = s;
    return head;
}

static DSStmtNode *parse_simple_stmt(DSParser *p) {
    int line = tok_line(p);
    DSTokenKind k = tk(p);

    if (k == TOK_PASS) {
        advance(p);
        return st_new(p, ST_PASS, line);
    }
    if (k == TOK_BREAK) {
        advance(p);
        return st_new(p, ST_BREAK, line);
    }
    if (k == TOK_CONTINUE) {
        advance(p);
        return st_new(p, ST_CONTINUE, line);
    }
    if (k == TOK_RETURN) {
        advance(p);
        DSStmtNode *s = st_new(p, ST_RETURN, line);
        if (!is_stmt_end(p))
            s->expr = parse_testlist(p);
        return s;
    }
    if (k == TOK_IMPORT) {
        advance(p);
        DSStmtNode *node = st_new(p, ST_IMPORT, line);
        for (;;) {
            if (!check(p, TOK_IDENT)) {
                perr(p, line, "ожидалось имя модуля");
                return node;
            }
            DSExprNode *nm = ex_new(p, EX_NAME, tok_line(p));
            nm->str = tok_str(p);
            advance(p);
            while (check(p, TOK_DOT)) {
                advance(p);
                if (!check(p, TOK_IDENT))
                    break;
                DSString *seg = tok_str(p);
                nm->str = ds_string_concat(p->vm, nm->str, intern_str(p, ".", 1));
                nm->str = ds_string_concat(p->vm, nm->str, seg);
                nm->str->pinned = 1;
                advance(p);
            }
            if (match(p, TOK_AS)) {
                if (!check(p, TOK_IDENT)) {
                    perr(p, line, "ожидалось имя после 'as'");
                    return node;
                }
                nm->b = ex_new(p, EX_NAME, tok_line(p));
                nm->b->str = tok_str(p);
                advance(p);
            }
            push_item(p, &node->items, &node->count, nm);
            if (!match(p, TOK_COMMA))
                break;
        }
        return node;
    }
    if (k == TOK_FROM) {
        advance(p);
        if (!check(p, TOK_IDENT)) {
            perr(p, line, "ожидалось имя модуля после 'from'");
            return NULL;
        }
        DSString *mod_name = tok_str(p);
        advance(p);
        while (check(p, TOK_DOT)) {
            advance(p);
            if (check(p, TOK_IDENT))
                advance(p);
        }
        if (!expect(p, TOK_IMPORT, "'import'"))
            return NULL;
        DSStmtNode *node = st_new(p, ST_IMPORT, line);
        node->name = mod_name;
        node->flags = 1; /* from-import */
        if (match(p, TOK_STAR)) {
            node->count = 0;
            return node;
        }
        for (;;) {
            if (!check(p, TOK_IDENT)) {
                perr(p, line, "ожидалось имя для импорта");
                return node;
            }
            DSExprNode *nm = ex_new(p, EX_NAME, tok_line(p));
            nm->str = tok_str(p);
            advance(p);
            if (match(p, TOK_AS)) {
                if (!check(p, TOK_IDENT)) {
                    perr(p, line, "ожидалось имя после 'as'");
                    return node;
                }
                nm->b = ex_new(p, EX_NAME, tok_line(p));
                nm->b->str = tok_str(p);
                advance(p);
            }
            push_item(p, &node->items, &node->count, nm);
            if (!match(p, TOK_COMMA))
                break;
        }
        return node;
    }
    if (k == TOK_GLOBAL) {
        advance(p);
        DSStmtNode *s = st_new(p, ST_GLOBAL, line);
        for (;;) {
            if (!check(p, TOK_IDENT)) {
                perr(p, line, "ожидалось имя после 'global'");
                return s;
            }
            DSExprNode *nm = ex_new(p, EX_NAME, tok_line(p));
            nm->str = tok_str(p);
            advance(p);
            push_item(p, &s->items, &s->count, nm);
            if (!match(p, TOK_COMMA))
                break;
        }
        return s;
    }
    if (k == TOK_DEL) {
        advance(p);
        DSStmtNode *s = st_new(p, ST_DEL, line);
        s->expr = parse_testlist(p);
        return s;
    }
    if (k == TOK_ASSERT) {
        advance(p);
        DSStmtNode *s = st_new(p, ST_ASSERT, line);
        s->expr = parse_testlist(p);
        return s;
    }

    /* Expression / assignment statement */
    DSExprNode *target = parse_testlist(p);
    if (p->failed)
        return NULL;

    if (check(p, TOK_EQ)) {
        DSStmtNode *s = st_new(p, ST_ASSIGN, line);
        DSExprNode *value = target;
        while (match(p, TOK_EQ)) {
            push_item(p, &s->targets, &s->ntargets, value);
            value = parse_testlist(p);
        }
        s->expr = value;
        return s;
    }

    int aug = -1;
    switch (tk(p)) {
    case TOK_PLUSEQ: aug = OP_ADD; break;
    case TOK_MINUSEQ: aug = OP_SUB; break;
    case TOK_STAREQ: aug = OP_MUL; break;
    case TOK_SLASHEQ: aug = OP_DIV; break;
    case TOK_FLOORDIVEQ: aug = OP_FLOORDIV; break;
    case TOK_PERCENTEQ: aug = OP_MOD; break;
    default: break;
    }
    if (aug >= 0) {
        advance(p);
        DSStmtNode *s = st_new(p, ST_AUGASSIGN, line);
        s->op = (uint8_t)aug;
        push_item(p, &s->targets, &s->ntargets, target);
        s->expr = parse_testlist(p);
        return s;
    }

    DSStmtNode *s = st_new(p, ST_EXPR, line);
    s->expr = target;
    return s;
}

static DSFuncProto *parse_def(DSParser *p, int line, DSString *name) {
    DSFuncProto *fn = ds_proto_new(p->vm, name ? name : intern_str(p, "<аноним>", 9), NULL, NULL, p->arena);
    if (!fn)
        return NULL;
    fn->def_line = line;
    parse_params(p, fn);
    expect(p, TOK_COLON, "':'");
    fn->body = parse_block_after_colon(p);
    return fn;
}

static DSStmtNode *parse_if(DSParser *p) {
    int line = tok_line(p);
    advance(p);
    DSStmtNode *node = st_new(p, ST_IF, line);
    push_item(p, &node->conds, &node->nconds, parse_testlist(p));
    expect(p, TOK_COLON, "':'");
    push_stmt(p, &node->bodies, &node->nbranches, parse_block_after_colon(p));

    while (check(p, TOK_ELIF)) {
        advance(p);
        push_item(p, &node->conds, &node->nconds, parse_testlist(p));
        expect(p, TOK_COLON, "':'");
        push_stmt(p, &node->bodies, &node->nbranches, parse_block_after_colon(p));
    }
    if (check(p, TOK_ELSE)) {
        advance(p);
        expect(p, TOK_COLON, "':'");
        node->orelse = parse_block_after_colon(p);
    }
    return node;
}

static DSStmtNode *parse_while(DSParser *p) {
    int line = tok_line(p);
    advance(p);
    DSStmtNode *node = st_new(p, ST_WHILE, line);
    node->cond = parse_testlist(p);
    expect(p, TOK_COLON, "':'");
    node->body = parse_block_after_colon(p);
    if (check(p, TOK_ELSE)) {
        advance(p);
        expect(p, TOK_COLON, "':'");
        node->orelse = parse_block_after_colon(p);
    }
    return node;
}

static DSStmtNode *parse_for(DSParser *p) {
    int line = tok_line(p);
    advance(p);
    DSStmtNode *node = st_new(p, ST_FOR, line);
    /* Targets are parsed without binary operators so that 'in' terminates them. */
    {
        int tline = tok_line(p);
        DSExprNode *first = parse_unary(p);
        if (check(p, TOK_COMMA)) {
            DSExprNode *lst = ex_new(p, EX_LIST, tline);
            push_item(p, &lst->items, &lst->count, first);
            while (match(p, TOK_COMMA)) {
                if (check(p, TOK_IN) || check(p, TOK_IDENT) == 0)
                    break;
                push_item(p, &lst->items, &lst->count, parse_unary(p));
            }
            first = lst;
        }
        push_item(p, &node->targets, &node->ntargets, first);
    }
    if (!expect(p, TOK_IN, "'in'"))
        return node;
    node->expr = parse_testlist(p);
    expect(p, TOK_COLON, "':'");
    node->body = parse_block_after_colon(p);
    if (check(p, TOK_ELSE)) {
        advance(p);
        expect(p, TOK_COLON, "':'");
        node->orelse = parse_block_after_colon(p);
    }
    return node;
}

static DSStmtNode *parse_class(DSParser *p) {
    int line = tok_line(p);
    advance(p);
    if (!check(p, TOK_IDENT)) {
        perr(p, line, "ожидалось имя класса");
        return NULL;
    }
    DSStmtNode *node = st_new(p, ST_CLASS, line);
    node->name = tok_str(p);
    advance(p);
    if (match(p, TOK_LPAREN)) {
        while (!check(p, TOK_RPAREN) && !p->failed) {
            skip_newlines(p);
            if (check(p, TOK_RPAREN))
                break;
            DSExprNode *base = parse_expr(p);
            if (!node->expr)
                node->expr = base; /* single inheritance */
            if (!match(p, TOK_COMMA))
                break;
        }
        expect(p, TOK_RPAREN, "')'");
    }
    expect(p, TOK_COLON, "':'");
    node->body = parse_block_after_colon(p);
    return node;
}

static DSStmtNode *parse_stmt(DSParser *p) {
    switch (tk(p)) {
    case TOK_IF:
        return parse_if(p);
    case TOK_WHILE:
        return parse_while(p);
    case TOK_FOR:
        return parse_for(p);
    case TOK_DEF: {
        int line = tok_line(p);
        advance(p);
        if (!check(p, TOK_IDENT)) {
            perr(p, line, "ожидалось имя функции после 'def'");
            return NULL;
        }
        DSStmtNode *s = st_new(p, ST_DEF, line);
        s->name = tok_str(p);
        advance(p);
        s->proto = parse_def(p, line, s->name);
        return s;
    }
    case TOK_CLASS:
        return parse_class(p);
    default:
        return parse_simple_stmt(p);
    }
}

/* Block after ':' — inline suite or INDENT ... DEDENT */
static DSStmtNode *parse_block_after_colon(DSParser *p) {
    if (!check(p, TOK_NEWLINE) && !check(p, TOK_INDENT) && !check(p, TOK_EOF)) {
        DSStmtNode *s = parse_simple_stmt(p);
        return s;
    }
    if (!expect(p, TOK_NEWLINE, "перевод строки после ':'"))
        return NULL;
    skip_newlines(p);
    if (!expect(p, TOK_INDENT, "увеличенный отступ блока"))
        return NULL;
    skip_newlines(p);

    DSStmtNode *head = NULL;
    while (!check(p, TOK_DEDENT) && !check(p, TOK_EOF) && !p->failed) {
        skip_newlines(p);
        if (check(p, TOK_DEDENT) || check(p, TOK_EOF))
            break;
        DSStmtNode *s;
        if (tk(p) == TOK_IF || tk(p) == TOK_WHILE || tk(p) == TOK_FOR || tk(p) == TOK_DEF || tk(p) == TOK_CLASS)
            s = parse_stmt(p);
        else
            s = parse_simple_stmt(p);
        if (!s)
            break;
        head = append_chain(head, NULL, s);
        if (check(p, TOK_NEWLINE))
            advance(p);
    }
    match(p, TOK_DEDENT);
    return head;
}

static DSStmtNode *parse_program_inner(DSParser *p) {
    DSStmtNode *head = NULL;
    skip_newlines(p);
    while (!check(p, TOK_EOF) && !p->failed) {
        DSStmtNode *s;
        if (tk(p) == TOK_IF || tk(p) == TOK_WHILE || tk(p) == TOK_FOR || tk(p) == TOK_DEF || tk(p) == TOK_CLASS)
            s = parse_stmt(p);
        else
            s = parse_simple_stmt(p);
        if (!s)
            break;
        head = append_chain(head, NULL, s);
        if (check(p, TOK_NEWLINE))
            advance(p);
        skip_newlines(p);
    }
    return head;
}

DSStmtNode *ds_parse_program(DSVM *vm, DSCompiledArena *arena, const char *filename, const char *src, size_t len,
                             int is_repl, char *err, size_t err_cap, int *err_line) {
    DSParser p;
    memset(&p, 0, sizeof(p));
    p.vm = vm;
    p.arena = arena;
    p.filename = filename;
    p.err = err;
    p.err_cap = err_cap;
    p.err_line = err_line;
    p.is_repl = is_repl;

    if (!ds_lex_source(src, len, &p.ts, 1)) {
        if (err && err_cap)
            snprintf(err, err_cap, "%s", p.ts.error_msg);
        if (err_line)
            *err_line = p.ts.error_line;
        ds_token_stream_free(&p.ts);
        return NULL;
    }
    DSStmtNode *head = parse_program_inner(&p);
    int had_err = p.failed || p.ts.had_error;
    ds_token_stream_free(&p.ts);
    if (had_err)
        return NULL;
    return head;
}

DSStmtNode *ds_parse_expression_only(DSVM *vm, DSCompiledArena *arena, const char *filename, const char *src,
                                     size_t len, char *err, size_t err_cap, int *err_line) {
    DSParser p;
    memset(&p, 0, sizeof(p));
    p.vm = vm;
    p.arena = arena;
    p.filename = filename;
    p.err = err;
    p.err_cap = err_cap;
    p.err_line = err_line;
    if (!ds_lex_source(src, len, &p.ts, 1)) {
        if (err && err_cap)
            snprintf(err, err_cap, "%s", p.ts.error_msg);
        if (err_line)
            *err_line = p.ts.error_line;
        ds_token_stream_free(&p.ts);
        return NULL;
    }
    DSExprNode *e = parse_testlist(&p);
    int had_err = p.failed || p.ts.had_error;
    ds_token_stream_free(&p.ts);
    if (had_err || !e)
        return NULL;
    DSStmtNode *s = (DSStmtNode *)ds_arena_alloc(arena, sizeof(DSStmtNode), 16);
    memset(s, 0, sizeof(*s));
    s->kind = ST_EXPR;
    s->line = e->line;
    s->expr = e;
    return s;
}

/* ---- Lambda: needs parse_expr & arena, defined last -------------------- */
static DSFuncProto *parse_lambda(DSParser *p, int line) {
    DSFuncProto *fn = ds_proto_new(p->vm, intern_str(p, "<lambda>", 8), NULL, NULL, p->arena);
    if (!fn)
        return NULL;
    fn->def_line = line;
    int arity = 0;
    int cap = 0;
    while (check(p, TOK_IDENT)) {
        DSString *pname = tok_str(p);
        advance(p);
        if (arity == cap) {
            int ncap = cap < 4 ? 4 : cap * 2;
            DSString **np = (DSString **)realloc(fn->params, (size_t)ncap * sizeof(DSString *));
            if (np)
                fn->params = np;
            cap = ncap;
        }
        fn->params[arity++] = pname;
        if (match(p, TOK_COMMA))
            continue;
        break;
    }
    fn->arity = arity;
    fn->min_arity = arity;
    if (!expect(p, TOK_COLON, "':'"))
        return fn;
    DSStmtNode *ret = st_new(p, ST_RETURN, line);
    ret->expr = parse_expr(p);
    fn->body = ret;
    return fn;
}
