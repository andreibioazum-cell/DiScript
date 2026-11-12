// Native ARM64 (AArch64) & x86_64 W^X machine-code JIT for numeric DiScript functions.
// Falls back cleanly to Direct-Threaded C closures if SELinux/OS denies PROT_EXEC.

#include "cpp/ds_cpp.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/* Windows has no mmap/mprotect. The JIT only needs page-aligned executable
 * memory, which VirtualAlloc/VirtualProtect provide directly; both calls map
 * one-to-one onto the W^X sequence below. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)
static void *jit_mmap(size_t size) {
    void *p = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return p ? p : MAP_FAILED;
}
static int jit_mprotect(void *addr, size_t size, int prot) {
    DWORD flag = (prot & PROT_EXEC) ? PAGE_EXECUTE_READ : PAGE_READONLY;
    DWORD old = 0;
    return VirtualProtect(addr, size, flag, &old) ? 0 : -1;
}
static void jit_munmap(void *addr, size_t size) { VirtualFree(addr, 0, MEM_RELEASE); }
#define mmap(addr, size, prot, flags, fd, off) jit_mmap(size)
#define mprotect(addr, size, prot) jit_mprotect((addr), (size), (prot))
#define munmap(addr, size) jit_munmap((addr), (size))
#define getpagesize() 4096
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

    /* Back-patches a 32-bit little-endian field already written. */
    void patch32(size_t at, uint32_t v) {
        if (at + 4 <= len) memcpy(buf + at, &v, 4);
    }
};

size_t page_align(size_t n) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    size_t page = si.dwPageSize ? static_cast<size_t>(si.dwPageSize) : 4096u;
#else
    long ps = sysconf(_SC_PAGESIZE);
    size_t page = (ps > 0) ? static_cast<size_t>(ps) : 4096u;
#endif
    return (n + page - 1u) & ~(page - 1u);
}

/* ---- Branch bookkeeping --------------------------------------------------
 * Jumps name a target instruction index, but machine offsets only exist after
 * emission. Every jump is encoded at a fixed width, so a forward jump is
 * written with a placeholder displacement and patched once every instruction
 * has an offset. That is what makes loops and if/else expressible. */
#define JIT_MAX_INSTS 8192
#define JIT_MAX_FIXUPS 512

struct JumpFixup {
    size_t patch_at; /* byte offset of the displacement field */
    int target;      /* target instruction index */
};

struct Emission {
    size_t off[JIT_MAX_INSTS + 1];
    JumpFixup fixups[JIT_MAX_FIXUPS];
    int nfixups = 0;
    int count = 0;

    bool add_fixup(size_t at, int target) {
        if (nfixups >= JIT_MAX_FIXUPS) return false;
        fixups[nfixups].patch_at = at;
        fixups[nfixups].target = target;
        nfixups++;
        return true;
    }
};

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

/* AArch64 lowering. Same conventions as before: d0-d7 and d16-d21 hold values,
 * x0 points at the slots array. */
bool emit_machine_code(ByteEmitter &e, const DSJitInst *insts, int count, Emission &em) {
    const uint32_t d_one = 30;
    const uint32_t d_zero = 31;
    const uint32_t d_scratch = 29;
    bool loaded_bool_consts = false;

    for (int i = 0; i < count; ++i) {
        const DSJitInst &in = insts[i];
        em.off[i] = e.len;
        if (in.dst >= 14 || in.a >= 14 || in.b >= 14) return false;
        uint32_t rd = arm_fpreg(in.dst);
        uint32_t ra = arm_fpreg(in.a);
        uint32_t rb = arm_fpreg(in.b);

        switch (in.op) {
        case DS_JOP_LOAD_ARG:
        case DS_JOP_LOAD_SLOT:
            if (in.imm < 0 || in.imm > 4095) return false;
            if (!e.u32(0xFD400000u | (static_cast<uint32_t>(in.imm) << 10) | rd)) return false;
            break;
        case DS_JOP_STORE_SLOT:
            if (in.imm < 0 || in.imm > 4095) return false;
            if (!e.u32(0xFD000000u | (static_cast<uint32_t>(in.imm) << 10) | ra)) return false;
            break;
        case DS_JOP_LOAD_CONST:
            if (!emit_arm64_const(e, rd, in.k)) return false;
            break;
        case DS_JOP_MOV_CONST_SLOT:
            if (in.imm < 0 || in.imm > 4095) return false;
            if (!emit_arm64_const(e, d_scratch, in.k)) return false;
            if (!e.u32(0xFD000000u | (static_cast<uint32_t>(in.imm) << 10) | d_scratch))
                return false;
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
        case DS_JOP_MOD:
            /* Python modulo: FREM gives C fmod, then a negative remainder is
             * shifted by the divisor so the result follows the divisor's sign. */
            if (!e.u32(0x1E608800u | (rb << 16) | (ra << 5) | rd)) return false;
            if (!e.u32(0x1E602800u | (rb << 16) | (rd << 5) | d_scratch)) return false;
            if (!e.u32(0x1E602000u | (d_zero << 16) | (rd << 5))) return false;
            if (!e.u32(0x1E600C00u | (rd << 16) | (4u << 12) | (d_scratch << 5) | rd)) return false;
            if (!e.u32(0x1E602000u | (d_zero << 16) | (rb << 5))) return false;
            if (!e.u32(0x1E600C00u | (rd << 16) | (12u << 12) | (d_scratch << 5) | rd)) return false;
            break;
        case DS_JOP_CMP_LT:
        case DS_JOP_CMP_LE:
        case DS_JOP_CMP_GT:
        case DS_JOP_CMP_GE:
        case DS_JOP_CMP_EQ:
        case DS_JOP_CMP_NE: {
            if (!loaded_bool_consts) {
                if (!emit_arm64_const(e, d_one, 1.0) || !emit_arm64_const(e, d_zero, 0.0))
                    return false;
                loaded_bool_consts = true;
            }
            if (!e.u32(0x1E602000u | (rb << 16) | (ra << 5))) return false;
            uint32_t cond = (in.op == DS_JOP_CMP_LT) ? 4u :
                            (in.op == DS_JOP_CMP_LE) ? 9u :
                            (in.op == DS_JOP_CMP_GT) ? 12u :
                            (in.op == DS_JOP_CMP_EQ) ? 0u :
                            (in.op == DS_JOP_CMP_NE) ? 1u : 10u;
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
            if (!e.u32(0x1E602000u | (d_zero << 16) | (rcond << 5))) return false;
            if (!e.u32(0x1E600C00u | (rb << 16) | (1u << 12) | (ra << 5) | rd)) return false;
            break;
        }
        case DS_JOP_JMP: {
            size_t at = e.len;
            if (!e.u32(0x14000000u)) return false;
            if (!em.add_fixup(at, in.imm)) return false;
            break;
        }
        case DS_JOP_JMP_IF_ZERO:
        case DS_JOP_JMP_IF_NOT_ZERO: {
            if (!loaded_bool_consts) {
                if (!emit_arm64_const(e, d_zero, 0.0)) return false;
                loaded_bool_consts = true;
            }
            if (!e.u32(0x1E602000u | (d_zero << 16) | (rd << 5))) return false;
            size_t at = e.len;
            uint32_t cond = (in.op == DS_JOP_JMP_IF_ZERO) ? 0u : 1u;
            if (!e.u32(0x54000000u | cond)) return false;
            if (!em.add_fixup(at, in.imm)) return false;
            break;
        }
        case DS_JOP_NOP:
            break;
        case DS_JOP_RET:
            if (ra != 0 && !e.u32(0x1E604000u | (ra << 5) | 0u)) return false;
            if (!e.u32(0xD65F03C0u)) return false;
            break;
        default:
            return false;
        }
    }
    em.off[count] = e.len;
    em.count = count;
    return e.u32(0xD65F03C0u);
}

void patch_branches(ByteEmitter &e, const Emission &em) {
    for (int i = 0; i < em.nfixups; ++i) {
        int t = em.fixups[i].target;
        if (t < 0 || t > em.count) continue;
        size_t at = em.fixups[i].patch_at;
        int64_t delta = static_cast<int64_t>(em.off[t]) - static_cast<int64_t>(at);
        int64_t words = delta / 4;
        uint32_t insn = 0;
        memcpy(&insn, e.buf + at, 4);
        if ((insn & 0xFC000000u) == 0x14000000u)
            insn = (insn & 0xFC000000u) | (static_cast<uint32_t>(words) & 0x03FFFFFFu);
        else
            insn = (insn & 0xFF00001Fu) | ((static_cast<uint32_t>(words) & 0x7FFFFu) << 5);
        e.patch32(at, insn);
    }
}

#elif defined(__x86_64__)

bool emit_sse_rr(ByteEmitter &e, uint8_t prefix, uint8_t op, uint8_t dst, uint8_t src) {
    if (prefix && !e.u8(prefix)) return false;
    uint8_t rex = 0x40u | ((dst >= 8) ? 0x04u : 0u) | ((src >= 8) ? 0x01u : 0u);
    if (rex != 0x40u && !e.u8(rex)) return false;
    return e.u8(0x0Fu) && e.u8(op) &&
           e.u8(static_cast<uint8_t>(0xC0u | ((dst & 7u) << 3) | (src & 7u)));
}

bool emit_x64_const(ByteEmitter &e, uint8_t dst, double val) {
    uint64_t bits = 0;
    memcpy(&bits, &val, 8);
    if (bits == 0) return emit_sse_rr(e, 0x66u, 0x57u, dst, dst); /* XORPD dst, dst */
    if (!e.u8(0x48u) || !e.u8(0xB8u) || !e.u64(bits)) return false; /* MOVABS rax, imm64 */
    uint8_t rex = static_cast<uint8_t>(0x48u | ((dst >= 8) ? 0x04u : 0u));
    return e.u8(0x66u) && e.u8(rex) && e.u8(0x0Fu) && e.u8(0x6Eu) &&
           e.u8(static_cast<uint8_t>(0xC0u | ((dst & 7u) << 3))); /* MOVQ xmm_dst, rax */
}


/* ROUNDSD xmm_dst, xmm_dst, imm8 : round toward zero (imm8 = 3).
 * REX needs R when dst >= 8; the source is the same register, so B = R. */
bool emit_roundsd_trunc(ByteEmitter &e, uint8_t dst) {
    if (!e.u8(0x66u)) return false;
    uint8_t rex = static_cast<uint8_t>(0x40u | ((dst >= 8) ? 0x05u : 0x00u));
    if (rex != 0x40u && !e.u8(rex)) return false;
    if (!e.u8(0x0Fu) || !e.u8(0x3Au) || !e.u8(0x0Bu)) return false;
    if (!e.u8(static_cast<uint8_t>(0xC0u | ((dst & 7u) << 3) | (dst & 7u)))) return false;
    return e.u8(0x03u);
}

/* movsd xmm_dst, [rdi + disp32] */
bool x64_load_slot(ByteEmitter &e, uint8_t dst, int32_t disp) {
    if (!e.u8(0xF2u)) return false;
    if (dst >= 8 && !e.u8(0x44u)) return false;
    if (!e.u8(0x0Fu) || !e.u8(0x10u)) return false;
    if (!e.u8(static_cast<uint8_t>(0x87u | ((dst & 7u) << 3)))) return false;
    return e.u32(static_cast<uint32_t>(disp));
}

/* movsd [rdi + disp32], xmm_src */
bool x64_store_slot(ByteEmitter &e, uint8_t src, int32_t disp) {
    if (!e.u8(0xF2u)) return false;
    if (src >= 8 && !e.u8(0x44u)) return false;
    if (!e.u8(0x0Fu) || !e.u8(0x11u)) return false;
    if (!e.u8(static_cast<uint8_t>(0x87u | ((src & 7u) << 3)))) return false;
    return e.u32(static_cast<uint32_t>(disp));
}

bool x64_emit_one(ByteEmitter &e, const DSJitInst &in, Emission &em) {
    const uint8_t xmm_tmp = 14;
    const uint8_t xmm_zero = 15;
    switch (in.op) {
    case DS_JOP_LOAD_ARG:
    case DS_JOP_LOAD_SLOT:
        if (in.imm < 0 || in.imm > 65535) return false;
        return x64_load_slot(e, in.dst, in.imm * 8);
    case DS_JOP_STORE_SLOT:
        if (in.imm < 0 || in.imm > 65535) return false;
        return x64_store_slot(e, in.a, in.imm * 8);
    case DS_JOP_LOAD_CONST:
        return emit_x64_const(e, in.dst, in.k);
    case DS_JOP_MOV_CONST_SLOT:
        if (in.imm < 0 || in.imm > 65535) return false;
        if (!emit_x64_const(e, xmm_tmp, in.k)) return false;
        return x64_store_slot(e, xmm_tmp, in.imm * 8);
    case DS_JOP_MOV:
        return in.dst == in.a || emit_sse_rr(e, 0xF2u, 0x10u, in.dst, in.a);
    case DS_JOP_ADD:
    case DS_JOP_SUB:
    case DS_JOP_MUL:
    case DS_JOP_DIV: {
        uint8_t sse_op = (in.op == DS_JOP_ADD) ? 0x58u :
                         (in.op == DS_JOP_SUB) ? 0x5Cu :
                         (in.op == DS_JOP_MUL) ? 0x59u : 0x5Eu;
        if (in.dst == in.b) {
            /* dst = a op dst; both operands are still needed, so go via tmp. */
            if (!emit_sse_rr(e, 0xF2u, 0x10u, xmm_tmp, in.a)) return false;
            if (!emit_sse_rr(e, 0xF2u, sse_op, xmm_tmp, in.b)) return false;
            return emit_sse_rr(e, 0xF2u, 0x10u, in.dst, xmm_tmp);
        }
        if (in.dst != in.a && !emit_sse_rr(e, 0xF2u, 0x10u, in.dst, in.a)) return false;
        return emit_sse_rr(e, 0xF2u, sse_op, in.dst, in.b);
    }
    case DS_JOP_NEG:
        if (!emit_x64_const(e, xmm_tmp, -0.0)) return false;
        if (in.dst != in.a && !emit_sse_rr(e, 0xF2u, 0x10u, in.dst, in.a)) return false;
        return emit_sse_rr(e, 0x66u, 0x57u, in.dst, xmm_tmp); /* XORPD flips the sign */
    case DS_JOP_CMP_LT:
    case DS_JOP_CMP_LE:
    case DS_JOP_CMP_GT:
    case DS_JOP_CMP_GE:
    case DS_JOP_CMP_EQ:
    case DS_JOP_CMP_NE: {
        uint8_t setcc = (in.op == DS_JOP_CMP_LT) ? 0x92u :
                        (in.op == DS_JOP_CMP_LE) ? 0x96u :
                        (in.op == DS_JOP_CMP_GT) ? 0x97u :
                        (in.op == DS_JOP_CMP_EQ) ? 0x94u :
                        (in.op == DS_JOP_CMP_NE) ? 0x95u : 0x93u;
        /* xor eax,eax; ucomisd a,b; setcc al; cvtsi2sd dst,eax */
        if (!e.u8(0x31u) || !e.u8(0xC0u)) return false;
        if (!emit_sse_rr(e, 0x66u, 0x2Eu, in.a, in.b)) return false;
        if (!e.u8(0x0Fu) || !e.u8(setcc) || !e.u8(0xC0u)) return false;
        if (!e.u8(0xF2u)) return false;
        if (in.dst >= 8 && !e.u8(0x44u)) return false;
        if (!e.u8(0x0Fu) || !e.u8(0x2Au)) return false;
        return e.u8(static_cast<uint8_t>(0xC0u | ((in.dst & 7u) << 3)));
    }
    case DS_JOP_MOD: {
        /* Python modulo: r = a - b*trunc(a/b), then += b when the non-zero
         * remainder's sign differs from the divisor's. All work happens in
         * xmm_tmp, and dst is written before any clobber, so dst may alias a. */
        if (!emit_sse_rr(e, 0x66u, 0x57u, xmm_zero, xmm_zero)) return false;
        if (!emit_sse_rr(e, 0x66u, 0x2Eu, in.b, xmm_zero)) return false;
        if (!e.u8(0x0Fu) || !e.u8(0x85u)) return false; /* JNE b != 0 */
        size_t jnz_at = e.len;
        if (!e.u32(0u)) return false;
        if (!emit_sse_rr(e, 0x66u, 0x57u, in.dst, in.dst)) return false; /* 0 div: result 0 */
        if (!e.u8(0xE9u)) return false;
        size_t jmp_at = e.len;
        if (!e.u32(0u)) return false;
        size_t body = e.len;
        e.patch32(jnz_at, static_cast<uint32_t>(static_cast<int32_t>(body - (jnz_at + 4))));
        if (!emit_sse_rr(e, 0xF2u, 0x10u, xmm_tmp, in.a)) return false;
        if (!emit_sse_rr(e, 0xF2u, 0x5Eu, xmm_tmp, in.b)) return false;
        if (!emit_roundsd_trunc(e, xmm_tmp)) return false;
        if (!emit_sse_rr(e, 0xF2u, 0x59u, xmm_tmp, in.b)) return false;
        if (in.dst != in.a && !emit_sse_rr(e, 0xF2u, 0x10u, in.dst, in.a)) return false;
        if (!emit_sse_rr(e, 0xF2u, 0x5Cu, in.dst, xmm_tmp)) return false;
        if (!emit_sse_rr(e, 0x66u, 0x2Eu, in.dst, xmm_zero)) return false;
        if (!e.u8(0x0Fu) || !e.u8(0x84u)) return false; /* JE done */
        size_t je_at = e.len;
        if (!e.u32(0u)) return false;
        if (!emit_sse_rr(e, 0xF2u, 0x10u, xmm_tmp, in.dst)) return false;
        if (!emit_sse_rr(e, 0x66u, 0x57u, xmm_tmp, in.b)) return false;     /* t ^= b */
        if (!emit_sse_rr(e, 0x66u, 0x2Eu, xmm_tmp, xmm_zero)) return false;
        if (!e.u8(0x0Fu) || !e.u8(0x83u)) return false;                     /* JAE done */
        size_t jae_at = e.len;
        if (!e.u32(0u)) return false;
        if (!emit_sse_rr(e, 0xF2u, 0x58u, in.dst, in.b)) return false;
        size_t after = e.len;
        e.patch32(je_at, static_cast<uint32_t>(static_cast<int32_t>(after - (je_at + 4))));
        e.patch32(jae_at, static_cast<uint32_t>(static_cast<int32_t>(after - (jae_at + 4))));
        e.patch32(jmp_at, static_cast<uint32_t>(static_cast<int32_t>(after - (jmp_at + 4))));
        return true;
    }
    case DS_JOP_SELECT: {
        uint8_t rcond = static_cast<uint8_t>(in.imm & 0xFF);
        if (rcond >= 14) return false;
        if (!emit_sse_rr(e, 0x66u, 0x57u, xmm_zero, xmm_zero)) return false;
        if (!emit_sse_rr(e, 0x66u, 0x2Eu, rcond, xmm_zero)) return false;
        if (!emit_sse_rr(e, 0xF2u, 0x10u, xmm_tmp, in.a)) return false;
        ByteEmitter sub; /* measure the false move so the offset is exact */
        if (!emit_sse_rr(sub, 0xF2u, 0x10u, xmm_tmp, in.b)) return false;
        uint8_t off = static_cast<uint8_t>(sub.len);
        if (!e.u8(0x7Au) || !e.u8(static_cast<uint8_t>(off + 2u))) return false; /* JP: NaN truthy */
        if (!e.u8(0x75u) || !e.u8(off)) return false;                            /* JNE over move */
        for (size_t k = 0; k < sub.len; ++k)
            if (!e.u8(sub.buf[k])) return false;
        return emit_sse_rr(e, 0xF2u, 0x10u, in.dst, xmm_tmp);
    }
    case DS_JOP_JMP: {
        if (!e.u8(0xE9u)) return false;
        size_t at = e.len;
        if (!e.u32(0u)) return false;
        return em.add_fixup(at, in.imm);
    }
    case DS_JOP_JMP_IF_ZERO:
    case DS_JOP_JMP_IF_NOT_ZERO: {
        if (in.dst >= 14) return false;
        /* ucomisd dst, zero; JP over the branch (NaN counts as true); Jcc rel32 */
        if (!emit_sse_rr(e, 0x66u, 0x57u, xmm_zero, xmm_zero)) return false;
        if (!emit_sse_rr(e, 0x66u, 0x2Eu, in.dst, xmm_zero)) return false;
        if (!e.u8(0x7Au) || !e.u8(0x08u)) return false; /* JP +8 skips jcc+rel32 */
        if (!e.u8(0x0Fu)) return false;
        if (!e.u8(in.op == DS_JOP_JMP_IF_NOT_ZERO ? 0x85u : 0x84u)) return false;
        size_t at = e.len;
        if (!e.u32(0u)) return false;
        return em.add_fixup(at, in.imm);
    }
    case DS_JOP_NOP:
        return true;
    case DS_JOP_RET:
        if (in.a != 0 && !emit_sse_rr(e, 0xF2u, 0x10u, 0, in.a)) return false;
        return e.u8(0xC3u);
    default:
        return false;
    }
}

bool emit_machine_code(ByteEmitter &e, const DSJitInst *insts, int count, Emission &em) {
    for (int i = 0; i < count; ++i) {
        em.off[i] = e.len;
        if (insts[i].dst >= 14 || insts[i].a >= 14 || insts[i].b >= 14) return false;
        if (!x64_emit_one(e, insts[i], em)) return false;
    }
    em.off[count] = e.len;
    em.count = count;
    return e.u8(0xC3u);
}

void patch_branches(ByteEmitter &e, const Emission &em) {
    for (int i = 0; i < em.nfixups; ++i) {
        int t = em.fixups[i].target;
        if (t < 0 || t > em.count) continue;
        size_t at = em.fixups[i].patch_at;
        /* rel32 is measured from the end of the displacement field. */
        int64_t delta = static_cast<int64_t>(em.off[t]) - static_cast<int64_t>(at + 4);
        e.patch32(at, static_cast<uint32_t>(static_cast<int32_t>(delta)));
    }
}

#else

bool emit_machine_code(ByteEmitter &, const DSJitInst *, int, Emission &) { return false; }
void patch_branches(ByteEmitter &, const Emission &) {}

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
    if (!insts || count <= 0 || count > JIT_MAX_INSTS || num_regs <= 0 || num_regs > 14 ||
        !ds_jit_supported())
        return nullptr;

    Emission em;
    ByteEmitter emitter;
    if (!emit_machine_code(emitter, insts, count, em) || emitter.len == 0)
        return nullptr;
    patch_branches(emitter, em);

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
