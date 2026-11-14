// Fast zero-exception C++17 fuzzy autocomplete engine for DiScript Android IDE.

#include "cpp/ds_cpp.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace {

struct StaticSym {
    const char *scope; // "" for global, or e.g. "love", "love.graphics", "math"
    const char *name;
    const char *tail;  // e.g. "(" or ""
    uint8_t kind;
};

constexpr StaticSym kBuiltins[] = {
    // Keywords
    {"", "def", " ", 0}, {"", "class", " ", 0}, {"", "return", " ", 0},
    {"", "if", " ", 0}, {"", "elif", " ", 0}, {"", "else:", "", 0},
    {"", "for", " ", 0}, {"", "while", " ", 0}, {"", "in", " ", 0},
    {"", "import", " ", 0}, {"", "from", " ", 0}, {"", "break", "", 0},
    {"", "continue", "", 0}, {"", "try:", "", 0}, {"", "except", " ", 0},
    {"", "True", "", 0}, {"", "False", "", 0}, {"", "None", "", 0},
    {"", "lambda", " ", 0}, {"", "global", " ", 0}, {"", "assert", " ", 0},
    // Global built-ins & modules
    {"", "print", "(", 1}, {"", "range", "(", 1}, {"", "len", "(", 1},
    {"", "int", "(", 1}, {"", "float", "(", 1}, {"", "str", "(", 1},
    {"", "abs", "(", 1}, {"", "min", "(", 1}, {"", "max", "(", 1},
    {"", "sum", "(", 1}, {"", "round", "(", 1}, {"", "sorted", "(", 1},
    {"", "enumerate", "(", 1}, {"", "isinstance", "(", 1},
    {"", "love", ".", 2}, {"", "math", ".", 2}, {"", "random", ".", 2},
    {"", "physics", ".", 2}, {"", "vec", ".", 2}, {"", "tween", ".", 2},
    {"", "json", ".", 2}, {"", "gc", ".", 2}, {"", "color", ".", 2},
    {"", "time", ".", 2}, {"", "re", ".", 2}, {"", "fs", ".", 2},
    // LÖVE2D top-level callbacks
    {"", "update", "(dt):", 1}, {"", "draw", "():", 1}, {"", "load", "():", 1},
    {"", "touchpressed", "(id, x, y):", 1}, {"", "touchmoved", "(id, x, y, dx, dy):", 1},
    {"", "keypressed", "(key):", 1},
    // love.*
    {"love", "graphics", ".", 2}, {"love", "timer", ".", 2},
    {"love", "touch", ".", 2}, {"love", "mouse", ".", 2},
    {"love", "keyboard", ".", 2}, {"love", "audio", ".", 2},
    {"love", "math", ".", 2}, {"love", "image", ".", 2},
    {"love", "filesystem", ".", 2}, {"love", "window", ".", 2},
    // love.graphics.*
    {"love.graphics", "setColor", "(", 1},
    {"love.graphics", "setBackgroundColor", "(", 1},
    {"love.graphics", "clear", "(", 1},
    {"love.graphics", "rectangle", "(\"fill\", ", 1},
    {"love.graphics", "circle", "(\"fill\", ", 1},
    {"love.graphics", "ellipse", "(\"fill\", ", 1},
    {"love.graphics", "line", "(", 1},
    {"love.graphics", "polygon", "(\"fill\", ", 1},
    {"love.graphics", "arc", "(\"fill\", ", 1},
    {"love.graphics", "print", "(", 1},
    {"love.graphics", "printf", "(", 1},
    {"love.graphics", "draw", "(", 1},
    {"love.graphics", "newImage", "(", 1},
    {"love.graphics", "push", "()", 1},
    {"love.graphics", "pop", "()", 1},
    {"love.graphics", "translate", "(", 1},
    {"love.graphics", "rotate", "(", 1},
    {"love.graphics", "scale", "(", 1},
    {"love.graphics", "origin", "()", 1},
    {"love.graphics", "setLineWidth", "(", 1},
    {"love.graphics", "setScissor", "(", 1},
    {"love.graphics", "getWidth", "()", 1},
    {"love.graphics", "getHeight", "()", 1},
    {"love.graphics", "getDimensions", "()", 1},
    {"love.graphics", "gradientRect", "(", 1},
    {"love.graphics", "rainbowText", "(", 1},
    {"love.graphics", "shake", "(", 1},
    // love.timer / touch / mouse / keyboard / audio / math / image
    {"love.timer", "getFPS", "()", 1}, {"love.timer", "getDelta", "()", 1}, {"love.timer", "getTime", "()", 1},
    {"love.touch", "getTouches", "()", 1}, {"love.touch", "getPosition", "(", 1},
    {"love.mouse", "getPosition", "()", 1}, {"love.mouse", "getX", "()", 1}, {"love.mouse", "getY", "()", 1}, {"love.mouse", "isDown", "(1)", 1},
    {"love.keyboard", "isDown", "(\"", 1},
    {"love.audio", "sfx", "(\"", 1}, {"love.audio", "tone", "(", 1}, {"love.audio", "stop", "()", 1},
    {"love.math", "noise", "(", 1}, {"love.math", "random", "(", 1}, {"love.math", "hsvToRgb", "(", 1},
    {"love.image", "newImageData", "(", 1},
    // math.*
    {"math", "sin", "(", 1}, {"math", "cos", "(", 1}, {"math", "tan", "(", 1},
    {"math", "atan2", "(", 1}, {"math", "sqrt", "(", 1}, {"math", "hypot", "(", 1},
    {"math", "floor", "(", 1}, {"math", "ceil", "(", 1}, {"math", "clamp", "(", 1},
    {"math", "lerp", "(", 1}, {"math", "pi", "", 1}, {"math", "tau", "", 1},
    // random.*
    {"random", "uniform", "(", 1}, {"random", "randint", "(", 1},
    {"random", "choice", "(", 1}, {"random", "shuffle", "(", 1},
    // physics.*
    {"physics", "world", "(", 1},
    // gc.*
    {"gc", "stats", "()", 1}, {"gc", "count", "()", 1}, {"gc", "collect", "()", 1},
    {"gc", "step", "(", 1}, {"gc", "mode", "(\"", 1},
    // tween.* / vec.* / json.* / color.*
    {"tween", "out_elastic", "(", 1}, {"tween", "out_bounce", "(", 1}, {"tween", "in_out_cubic", "(", 1},
    {"vec", "v2", "(", 1}, {"vec", "add", "(", 1}, {"vec", "norm", "(", 1}, {"vec", "dist", "(", 1},
    {"json", "dumps", "(", 1}, {"json", "loads", "(", 1},
    {"color", "hsv", "(", 1}, {"color", "rgb", "(", 1}, {"color", "lerp", "(", 1},
};

inline bool is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

int fuzzy_score(const char *cand, const char *prefix, size_t plen) {
    if (plen == 0) return 10;
    size_t clen = strlen(cand);
    if (clen < plen) return -1;
    // Exact case-sensitive prefix match
    if (strncmp(cand, prefix, plen) == 0) {
        return (clen == plen) ? -1 : (200 - static_cast<int>(clen));
    }
    // Case-insensitive prefix match
    bool ci_ok = true;
    for (size_t i = 0; i < plen; ++i) {
        if (tolower(static_cast<unsigned char>(cand[i])) != tolower(static_cast<unsigned char>(prefix[i]))) {
            ci_ok = false;
            break;
        }
    }
    if (ci_ok) return 140 - static_cast<int>(clen);

    // Ordered subsequence match
    size_t pi = 0;
    for (size_t ci = 0; ci < clen && pi < plen; ++ci) {
        if (tolower(static_cast<unsigned char>(cand[ci])) == tolower(static_cast<unsigned char>(prefix[pi]))) {
            ++pi;
        }
    }
    if (pi == plen) return 60 - static_cast<int>(clen);
    return -1;
}

struct Ranked {
    DSCompletionItem item;
    int score;
};

void insert_ranked(Ranked *list, int &count, int cap, const char *label, const char *suffix, uint8_t kind, int score) {
    if (score < 0) return;
    for (int i = 0; i < count; ++i) {
        if (strcmp(list[i].item.label, label) == 0) return;
    }
    Ranked r{};
    snprintf(r.item.label, sizeof(r.item.label), "%s", label);
    snprintf(r.item.insert_suffix, sizeof(r.item.insert_suffix), "%s", suffix);
    r.item.kind = kind;
    r.score = score;

    int pos = count;
    while (pos > 0 && list[pos - 1].score < score) --pos;
    if (pos >= cap) return;
    if (count < cap) ++count;
    for (int j = count - 1; j > pos; --j) list[j] = list[j - 1];
    list[pos] = r;
}

}  // namespace

extern "C" int ds_editor_complete(const char *src, size_t src_len, size_t cursor,
                                  DSCompletionItem *out_items, int max_items) {
    if (!src || !out_items || max_items <= 0) return 0;
    if (cursor > src_len) cursor = src_len;

    // Extract current identifier prefix before cursor
    size_t word_start = cursor;
    while (word_start > 0 && is_ident_char(src[word_start - 1])) {
        --word_start;
    }
    size_t plen = cursor - word_start;
    if (plen >= 40) return 0;

    char prefix[44];
    memcpy(prefix, src + word_start, plen);
    prefix[plen] = '\0';

    // Check dotted scope before word_start (e.g. "love.graphics.")
    char scope[64] = {0};
    if (word_start > 0 && src[word_start - 1] == '.') {
        size_t s_end = word_start - 1;
        size_t s_start = s_end;
        while (s_start > 0 && (is_ident_char(src[s_start - 1]) || src[s_start - 1] == '.')) {
            --s_start;
        }
        size_t slen = s_end - s_start;
        if (slen > 0 && slen < sizeof(scope)) {
            memcpy(scope, src + s_start, slen);
            scope[slen] = '\0';
        }
    } else if (plen == 0) {
        // Don't spam completions on blank space unless typing an identifier or after '.'
        return 0;
    }

    Ranked ranked[16];
    int count = 0;
    int cap = max_items > 16 ? 16 : max_items;

    for (const auto &sym : kBuiltins) {
        if (strcmp(sym.scope, scope) != 0) continue;
        int sc = fuzzy_score(sym.name, prefix, plen);
        if (sc >= 0) {
            char suffix[48];
            size_t nlen = strlen(sym.name);
            const char *rem = (strncmp(sym.name, prefix, plen) == 0 && nlen >= plen) ? (sym.name + plen) : sym.name;
            snprintf(suffix, sizeof(suffix), "%s%s", rem, sym.tail);
            insert_ranked(ranked, count, cap, sym.name, suffix, sym.kind, sc);
        }
    }

    // If top-level scope, also scan user identifiers in source buffer!
    if (scope[0] == '\0' && plen >= 1) {
        size_t i = 0;
        while (i < src_len) {
            if (src[i] == '#') {
                while (i < src_len && src[i] != '\n') ++i;
                continue;
            }
            if (src[i] == '"' || src[i] == '\'') {
                char q = src[i++];
                while (i < src_len && src[i] != q && src[i] != '\n') {
                    if (src[i] == '\\' && i + 1 < src_len) ++i;
                    ++i;
                }
                if (i < src_len) ++i;
                continue;
            }
            if (is_ident_char(src[i]) && !(src[i] >= '0' && src[i] <= '9')) {
                size_t id_start = i;
                while (i < src_len && is_ident_char(src[i])) ++i;
                // Skip the token currently being typed right at cursor!
                if (id_start == word_start) continue;
                size_t id_len = i - id_start;
                if (id_len >= 2 && id_len < 42) {
                    char tok[44];
                    memcpy(tok, src + id_start, id_len);
                    tok[id_len] = '\0';
                    int sc = fuzzy_score(tok, prefix, plen);
                    if (sc >= 0) {
                        bool is_fn = (i < src_len && src[i] == '(');
                        char suffix[48];
                        const char *rem = (strncmp(tok, prefix, plen) == 0) ? (tok + plen) : tok;
                        snprintf(suffix, sizeof(suffix), "%s%s", rem, is_fn ? "(" : "");
                        insert_ranked(ranked, count, cap, tok, suffix, 3, sc + 5);
                    }
                }
            } else {
                ++i;
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        out_items[i] = ranked[i].item;
    }
    return count;
}
