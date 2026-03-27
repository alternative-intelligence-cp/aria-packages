// aria_jit_shim.c — Handle-based C wrapper for the Aria assembler runtime
//
// Maps Assembler* and WildXGuard to int64 handles so Aria code can
// use plain extern func: declarations (no extern block needed).

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ── Forward declarations for runtime assembler (linked into ariac) ──

typedef enum {
    REG_RAX = 0, REG_RCX, REG_RDX, REG_RBX,
    REG_RSP, REG_RBP, REG_RSI, REG_RDI,
    REG_R8, REG_R9, REG_R10, REG_R11,
    REG_R12, REG_R13, REG_R14, REG_R15
} AsmRegister;

typedef enum { WX_WRITABLE, WX_EXECUTABLE, WX_SEALED, WX_FREED } WildXState;
typedef struct { void* ptr; size_t size; WildXState state; int sealed; } WildXGuard;
typedef struct Assembler Assembler;

extern Assembler* aria_asm_create(void);
extern void       aria_asm_destroy(Assembler* ctx);
extern void       aria_asm_mov_r64_imm64(Assembler* ctx, AsmRegister dst, int64_t val);
extern void       aria_asm_mov_r64_r64(Assembler* ctx, AsmRegister dst, AsmRegister src);
extern void       aria_asm_add_r64_r64(Assembler* ctx, AsmRegister dst, AsmRegister src);
extern void       aria_asm_sub_r64_r64(Assembler* ctx, AsmRegister dst, AsmRegister src);
extern void       aria_asm_ret(Assembler* ctx);
extern void       aria_asm_jmp(Assembler* ctx, int label_id);
extern int        aria_asm_new_label(Assembler* ctx);
extern void       aria_asm_bind_label(Assembler* ctx, int label_id);
extern WildXGuard aria_asm_finalize(Assembler* ctx);
extern int64_t    aria_asm_execute(WildXGuard* guard);
extern int64_t    aria_asm_execute_i64(WildXGuard* guard, int64_t a);
extern int64_t    aria_asm_execute_i64_i64(WildXGuard* guard, int64_t a, int64_t b);
extern void       aria_free_exec(WildXGuard* guard);

// ── Handle table for WildXGuard (heap-allocated copies) ─────────────

#define MAX_GUARDS 256
static WildXGuard guard_table[MAX_GUARDS];
static int        guard_used[MAX_GUARDS];

static int64_t guard_alloc(WildXGuard g) {
    for (int i = 0; i < MAX_GUARDS; i++) {
        if (!guard_used[i]) {
            guard_used[i] = 1;
            guard_table[i] = g;
            return (int64_t)i;
        }
    }
    return -1;
}

static WildXGuard* guard_get(int64_t handle) {
    if (handle < 0 || handle >= MAX_GUARDS || !guard_used[handle]) return NULL;
    return &guard_table[handle];
}

static void guard_release(int64_t handle) {
    if (handle >= 0 && handle < MAX_GUARDS) guard_used[handle] = 0;
}

// ── Shim functions (all use int64 handles) ──────────────────────────

// Create assembler context, return handle (Assembler* cast to int64)
int64_t aria_jit_shim_create(void) {
    return (int64_t)(uintptr_t)aria_asm_create();
}

// Destroy assembler context
void aria_jit_shim_destroy(int64_t ctx_handle) {
    aria_asm_destroy((Assembler*)(uintptr_t)ctx_handle);
}

// MOV reg, imm64
void aria_jit_shim_mov_reg_imm(int64_t ctx_h, int32_t dst, int64_t val) {
    aria_asm_mov_r64_imm64((Assembler*)(uintptr_t)ctx_h, (AsmRegister)dst, val);
}

// MOV reg, reg
void aria_jit_shim_mov_reg_reg(int64_t ctx_h, int32_t dst, int32_t src) {
    aria_asm_mov_r64_r64((Assembler*)(uintptr_t)ctx_h, (AsmRegister)dst, (AsmRegister)src);
}

// ADD reg, reg
void aria_jit_shim_add_reg_reg(int64_t ctx_h, int32_t dst, int32_t src) {
    aria_asm_add_r64_r64((Assembler*)(uintptr_t)ctx_h, (AsmRegister)dst, (AsmRegister)src);
}

// SUB reg, reg
void aria_jit_shim_sub_reg_reg(int64_t ctx_h, int32_t dst, int32_t src) {
    aria_asm_sub_r64_r64((Assembler*)(uintptr_t)ctx_h, (AsmRegister)dst, (AsmRegister)src);
}

// RET
void aria_jit_shim_ret(int64_t ctx_h) {
    aria_asm_ret((Assembler*)(uintptr_t)ctx_h);
}

// JMP label
void aria_jit_shim_jmp(int64_t ctx_h, int32_t label_id) {
    aria_asm_jmp((Assembler*)(uintptr_t)ctx_h, label_id);
}

// NEW_LABEL -> label_id
int32_t aria_jit_shim_new_label(int64_t ctx_h) {
    return (int32_t)aria_asm_new_label((Assembler*)(uintptr_t)ctx_h);
}

// BIND_LABEL
void aria_jit_shim_bind_label(int64_t ctx_h, int32_t label_id) {
    aria_asm_bind_label((Assembler*)(uintptr_t)ctx_h, label_id);
}

// Finalize → WildXGuard handle
int64_t aria_jit_shim_finalize(int64_t ctx_h) {
    WildXGuard g = aria_asm_finalize((Assembler*)(uintptr_t)ctx_h);
    return guard_alloc(g);
}

// Execute (no args) → int64 result
int64_t aria_jit_shim_execute(int64_t guard_h) {
    WildXGuard* g = guard_get(guard_h);
    if (!g) return -1;
    return aria_asm_execute(g);
}

// Execute with 1 int64 arg
int64_t aria_jit_shim_execute_i64(int64_t guard_h, int64_t a) {
    WildXGuard* g = guard_get(guard_h);
    if (!g) return -1;
    return aria_asm_execute_i64(g, a);
}

// Execute with 2 int64 args
int64_t aria_jit_shim_execute_i64_i64(int64_t guard_h, int64_t a, int64_t b) {
    WildXGuard* g = guard_get(guard_h);
    if (!g) return -1;
    return aria_asm_execute_i64_i64(g, a, b);
}

// Free executable memory + release handle
void aria_jit_shim_free(int64_t guard_h) {
    WildXGuard* g = guard_get(guard_h);
    if (g) {
        aria_free_exec(g);
        guard_release(guard_h);
    }
}
