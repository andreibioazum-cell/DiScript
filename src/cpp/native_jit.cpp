// Native ARM64 (AArch64) & x86_64 W^X machine-code JIT for numeric DiScript functions.
// Falls back cleanly to Direct-Threaded C closures if SELinux/OS denies PROT_EXEC.

#include "cpp/ds_cpp.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct DSJitFunc {
    void *mem;
    size_t map_bytes;
    size_t code_bytes;
};

namespace {

class ByteEmitter {
public:
    uint8_t *buf = nullptr;
    size_t len = 0;
    size_t cap = 0;

    ~ByteEmitter() { free(buf); }

    bool reserve(size_t extra) {
        if (len + extra <= cap) return true;
        size_t nc = cap ? cap * 2 : 512;
        while (nc < len + extra) nc *= 2;
        auto *nb = static_cast<uint8_t *>(realloc(buf, nc));
        if (!nb) return false;
        buf = nb;
        cap = nc;
        return true;
    }

    bool u8(uint8_t v) {
        if (!reserve(1)) return false;
        buf[len++] = v;
        return true;
    }

    bool u32(uint32_t v) {
        if (!reserve(4)) return false;
        memcpy(buf + len, &v, 4);
        len += 4;
        return true;
    }

    bool u64(uint64_t v) {
        if (!reserve(8)) return false;
        memcpy(buf + len, &v, 8);
        len += 8;
        return true;
    }
};

size_t page_align(size_t n) {
    long ps = sysconf(_SC_PAGESIZE);
    size_t page = (ps > 0) ? static_cast<size_t>(ps) : 4096u;
    return (n + page - 1u) & ~(page - 1u);
}

#if defined(__aarch64__)

inline uint32_t arm_fpreg(uint8_t vreg) {
    // Map 0..7 -> d0..d7, 8..13 -> d16..d21 (caller-saved in AAPCS64; d8..d15 untouched!)
    return (vreg < 8) ? static_cast<uint32_t>(vreg) : static_cast<uint32_t>(16 + (vreg - 8));
}

bool emit_arm64_const(ByteEmitter &e, uint32_t rd, double val) {
    uint64_t bits = 0;
    memcpy(&bits, &val, 8);
    // MOVZ X9, #imm0; MOVK X9, ...; FMOV Dd, X9
    if (!e.u32(0xD2800009u | (static_cast<uint32_t>(bits & 0xFFFFu) << 5))) return false;
    if ((bits >> 16) & 0xFFFFu)
        if (!e.u32(0xF2A00009u | (static_cast<uint32_t>((bits >> 16) & 0xFFFFu) << 5))) return false;
    if ((bits >> 32) & 0xFFFFu)
        if (!e.u32(0xF2C00009u | (static_cast<uint32_t>((bits >> 32) & 0xFFFFu) << 5))) return false;
    if ((bits >> 48) & 0xFFFFu)
        if (!e.u32(0xF2E00009u | (static_cast<uint32_t>((bits >> 48) & 0xFFFFu) << 5))) return false;
    return e.u32(0x9E670120u | (rd & 31u));
}

bool emit_machine_code(ByteEmitter &e, const DSJitInst *insts, int count) {
    const uint32_t d_one = 30;
    const uint32_t d_zero = 31;
    bool loaded_bool_consts = false;

    for (int i = 0; i < count; ++i) {
        const DSJitInst &in = insts[i];
        if (in.dst >= 14 || in.a >= 14 || in.b >= 14) return false;
        uint32_t rd = arm_fpreg(in.dst);
        uint32_t ra = arm_fpreg(in.a);
        uint32_t rb = arm_fpreg(in.b);

        switch (in.op) {
        case DS_JOP_LOAD_ARG:
            if (in.imm < 0 || in.imm > 512) return false;
            if (!e.u32(0xFD400000u | (static_cast<uint32_t>(in.imm) << 10) | (0u << 5) | rd))
                return false;
            break;
        case DS_JOP_LOAD_CONST:
            if (!emit_arm64_const(e, rd, in.k)) return false;
            break;
        case DS_JOP_MOV:
            if (rd != ra && !e.u32(0x1E604000u | (ra << 5) | rd)) return false;
            break;
        case DS_JOP_ADD:
            if (!e.u32(0x1E602800u | (rb << 16) | (ra << 5) | rd)) return false;
            break;
        case DS_JOP_SUB:
            if (!e.u32(0x1E603800u | (rb << 16) | (ra << 5) | rd)) return false;
            break;
        case DS_JOP_MUL:
            if (!e.u32(0x1E600800u | (rb << 16) | (ra << 5) | rd)) return false;
            break;
        case DS_JOP_DIV:
            if (!e.u32(0x1E601800u | (rb << 16) | (ra << 5) | rd)) return false;
            break;
        case DS_JOP_NEG:
            if (!e.u32(0x1E614000u | (ra << 5) | rd)) return false;
            break;
        case DS_JOP_CMP_LT:
        case DS_JOP_CMP_LE:
        case DS_JOP_CMP_GT:
        case DS_JOP_CMP_GE: {
            if (!loaded_bool_consts) {
                if (!emit_arm64_const(e, d_one, 1.0) || !emit_arm64_const(e, d_zero, 0.0))
                    return false;
                loaded_bool_consts = true;
            }
            if (!e.u32(0x1E602000u | (rb << 16) | (ra << 5))) return false; // FCMP ra, rb
            uint32_t cond = (in.op == DS_JOP_CMP_LT) ? 4u :   // MI (<)
                            (in.op == DS_JOP_CMP_LE) ? 9u :   // LS (<=)
                            (in.op == DS_JOP_CMP_GT) ? 12u :  // GT (>)
                                                       10u;   // GE (>=)
            if (!e.u32(0x1E600C00u | (d_zero << 16) | (cond << 12) | (d_one << 5) | rd))
                return false;
            break;
        }
        case DS_JOP_SELECT: {
            if (!loaded_bool_consts) {
                if (!emit_arm64_const(e, d_one, 1.0) || !emit_arm64_const(e, d_zero, 0.0))
                    return false;
                loaded_bool_consts = true;
            }
            uint32_t rcond = arm_fpreg(static_cast<uint8_t>(in.imm & 0xFF));
            if (!e.u32(0x1E602000u | (d_zero << 16) | (rcond << 5))) return false; // FCMP rcond, 0.0
            if (!e.u32(0x1E600C00u | (rb << 16) | (1u << 12) | (ra << 5) | rd))    // FCSEL rd, ra, rb, NE
                return false;
            break;
        }
        case DS_JOP_RET:
            if (ra != 0 && !e.u32(0x1E604000u | (ra << 5) | 0u)) return false;
            if (!e.u32(0xD65F03C0u)) return false;
            break;
        default:
            return false;
        }
    }
    return e.u32(0xD65F03C0u);
}

#elif defined(__x86_64__)

bool emit_sse_rr(ByteEmitter &e, uint8_t prefix, uint8_t op, uint8_t dst, uint8_t src) {
    if (prefix && !e.u8(prefix)) return false;
    uint8_t rex = 0x40u | ((dst >= 8) ? 0x04u : 0u) | ((src >= 8) ? 0x01u : 0u);
    if (rex != 0x40u && !e.u8(rex)) return false;
    return e.u8(0x0Fu) && e.u8(op) && e.u8(static_cast<uint8_t>(0xC0u | ((dst & 7u) << 3) | (src & 7u)));
}

bool emit_x64_const(ByteEmitter &e, uint8_t dst, double val) {
    uint64_t bits = 0;
    memcpy(&bits, &val, 8);
    if (bits == 0) {
        // XORPD dst, dst
        return emit_sse_rr(e, 0x66u, 0x57u, dst, dst);
    }
    // MOVABS rax, imm64; MOVQ xmm_dst, rax
    if (!e.u8(0x48u) || !e.u8(0xB8u) || !e.u64(bits)) return false;
    uint8_t rex = static_cast<uint8_t>(0x48u | ((dst >= 8) ? 0x04u : 0u));
    return e.u8(0x66u) && e.u8(rex) && e.u8(0x0Fu) && e.u8(0x6Eu) &&
           e.u8(static_cast<uint8_t>(0xC0u | ((dst & 7u) << 3)));
}

bool emit_machine_code(ByteEmitter &e, const DSJitInst *insts, int count) {
    const uint8_t xmm_tmp = 14;
    const uint8_t xmm_zero = 15;

    for (int i = 0; i < count; ++i) {
        const DSJitInst &in = insts[i];
        if (in.dst >= 14 || in.a >= 14 || in.b >= 14) return false;

        switch (in.op) {
        case DS_JOP_LOAD_ARG: {
            if (in.imm < 0 || in.imm > 4096) return false;
            int32_t disp = in.imm * 8;
            if (!e.u8(0xF2u)) return false;
            if (in.dst >= 8 && !e.u8(0x44u)) return false;
            if (!e.u8(0x0Fu) || !e.u8(0x10u)) return false;
            if (!e.u8(static_cast<uint8_t>(0x87u | ((in.dst & 7u) << 3)))) return false;
            if (!e.u32(static_cast<uint32_t>(disp))) return false;
            break;
        }
        case DS_JOP_LOAD_CONST:
            if (!emit_x64_const(e, in.dst, in.k)) return false;
            break;
        case DS_JOP_MOV:
            if (in.dst != in.a && !emit_sse_rr(e, 0xF2u, 0x10u, in.dst, in.a)) return false;
            break;
        case DS_JOP_ADD:
        case DS_JOP_SUB:
        case DS_JOP_MUL:
        case DS_JOP_DIV: {
            uint8_t sse_op = (in.op == DS_JOP_ADD) ? 0x58u :
                             (in.op == DS_JOP_SUB) ? 0x5Cu :
                             (in.op == DS_JOP_MUL) ? 0x59u : 0x5Eu;
            if (in.dst == in.a) {
                if (!emit_sse_rr(e, 0xF2u, sse_op, in.dst, in.b)) return false;
            } else if (in.dst == in.b) {
                if (!emit_sse_rr(e, 0xF2u, 0x10u, xmm_tmp, in.a)) return false;
                if (!emit_sse_rr(e, 0xF2u, sse_op, xmm_tmp, in.b)) return false;
                if (!emit_sse_rr(e, 0xF2u, 0x10u, in.dst, xmm_tmp)) return false;
            } else {
                if (!emit_sse_rr(e, 0xF2u, 0x10u, in.dst, in.a)) return false;
                if (!emit_sse_rr(e, 0xF2u, sse_op, in.dst, in.b)) return false;
            }
            break;
        }
        case DS_JOP_NEG:
            if (!emit_x64_const(e, xmm_tmp, -0.0)) return false;
            if (in.dst != in.a && !emit_sse_rr(e, 0xF2u, 0x10u, in.dst, in.a)) return false;
            if (!emit_sse_rr(e, 0x66u, 0x57u, in.dst, xmm_tmp)) return false; // XORPD sign bit
            break;
        case DS_JOP_CMP_LT:
        case DS_JOP_CMP_LE:
        case DS_JOP_CMP_GT:
        case DS_JOP_CMP_GE: {
            // ucomisd CF: a<b -> SETB 0x92, a<=b -> SETBE 0x96,
            //              a>b -> SETA 0x97, a>=b -> SETAE 0x93
            uint8_t setcc = 0x97u;
            switch (in.op) {
            case DS_JOP_CMP_LT: setcc = 0x92u; break;
            case DS_JOP_CMP_LE: setcc = 0x96u; break;
            case DS_JOP_CMP_GT: setcc = 0x97u; break;
            default:            setcc = 0x93u; break;
            }
            // xor eax, eax; ucomisd xmm_a, xmm_b; setcc al; cvtsi2sd xmm_dst, eax
            if (!e.u8(0x31u) || !e.u8(0xC0u)) return false;
            if (!emit_sse_rr(e, 0x66u, 0x2Eu, in.a, in.b)) return false;
            if (!e.u8(0x0Fu) || !e.u8(setcc) || !e.u8(0xC0u)) return false;
            if (!e.u8(0xF2u)) return false;
            if (in.dst >= 8 && !e.u8(0x44u)) return false;
            if (!e.u8(0x0Fu) || !e.u8(0x2Au) || !e.u8(static_cast<uint8_t>(0xC0u | ((in.dst & 7u) << 3))))
                return false;
            break;
        }
        case DS_JOP_SELECT: {
            uint8_t rcond = static_cast<uint8_t>(in.imm & 0xFF);
            if (rcond >= 14) return false;
            if (!emit_sse_rr(e, 0x66u, 0x57u, xmm_zero, xmm_zero)) return false;
            if (!emit_sse_rr(e, 0x66u, 0x2Eu, rcond, xmm_zero)) return false; // ucomisd rcond, 0.0
            // movsd xmm_tmp, in.a; jp +8 (NaN is truthy); jne +6; movsd xmm_tmp, in.b; movsd dst, xmm_tmp
            if (!emit_sse_rr(e, 0xF2u, 0x10u, xmm_tmp, in.a)) return false;
            size_t false_mov_len = (in.b >= 8) ? 5u : 4u; // F2 [41] 0F 10 modrm (xmm_tmp=14 >= 8 -> REX.R=0x44 or 0x45 -> always 5 bytes!)
            (void)false_mov_len;
            // Emit false move into a temp buffer to know exact jump offset
            ByteEmitter sub;
            if (!emit_sse_rr(sub, 0xF2u, 0x10u, xmm_tmp, in.b)) return false;
            uint8_t off = static_cast<uint8_t>(sub.len);
            if (!e.u8(0x7Au) || !e.u8(static_cast<uint8_t>(off + 2u))) return false; // JP over JNE + false_mov
            if (!e.u8(0x75u) || !e.u8(off)) return false;                            // JNE over false_mov
            for (size_t k = 0; k < sub.len; ++k) if (!e.u8(sub.buf[k])) return false;
            if (!emit_sse_rr(e, 0xF2u, 0x10u, in.dst, xmm_tmp)) return false;
            break;
        }
        case DS_JOP_RET:
            if (in.a != 0 && !emit_sse_rr(e, 0xF2u, 0x10u, 0, in.a)) return false;
            if (!e.u8(0xC3u)) return false;
            break;
        default:
            return false;
        }
    }
    return e.u8(0xC3u);
}

#else

bool emit_machine_code(ByteEmitter &, const DSJitInst *, int) {
    return false;
}

#endif

int g_jit_probe_state = -1;

}  // namespace

extern "C" int ds_jit_supported(void) {
    if (g_jit_probe_state != -1)
        return g_jit_probe_state;
#if defined(__aarch64__) || defined(__x86_64__)
    size_t sz = page_align(64);
    void *mem = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        g_jit_probe_state = 0;
        return 0;
    }
    if (mprotect(mem, sz, PROT_READ | PROT_EXEC) != 0) {
        munmap(mem, sz);
        g_jit_probe_state = 0;
        return 0;
    }
    munmap(mem, sz);
    g_jit_probe_state = 1;
    return 1;
#else
    g_jit_probe_state = 0;
    return 0;
#endif
}

extern "C" DSJitFunc *ds_jit_compile(const DSJitInst *insts, int count, int num_regs) {
    if (!insts || count <= 0 || num_regs <= 0 || num_regs > 14 || !ds_jit_supported())
        return nullptr;

    ByteEmitter emitter;
    if (!emit_machine_code(emitter, insts, count) || emitter.len == 0)
        return nullptr;

    size_t map_sz = page_align(emitter.len);
    void *mem = mmap(nullptr, map_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
        return nullptr;

    memcpy(mem, emitter.buf, emitter.len);
    if (mprotect(mem, map_sz, PROT_READ | PROT_EXEC) != 0) {
        munmap(mem, map_sz);
        g_jit_probe_state = 0;
        return nullptr;
    }
    __builtin___clear_cache(static_cast<char *>(mem), static_cast<char *>(mem) + emitter.len);

    auto *fn = static_cast<DSJitFunc *>(malloc(sizeof(DSJitFunc)));
    if (!fn) {
        munmap(mem, map_sz);
        return nullptr;
    }
    fn->mem = mem;
    fn->map_bytes = map_sz;
    fn->code_bytes = emitter.len;
    return fn;
}

extern "C" DSJitEntryFn ds_jit_entry(const DSJitFunc *fn) {
    return fn ? reinterpret_cast<DSJitEntryFn>(fn->mem) : nullptr;
}

extern "C" size_t ds_jit_code_size(const DSJitFunc *fn) {
    return fn ? fn->code_bytes : 0;
}

extern "C" void ds_jit_free(DSJitFunc *fn) {
    if (!fn) return;
    if (fn->mem && fn->map_bytes > 0)
        munmap(fn->mem, fn->map_bytes);
    free(fn);
}
