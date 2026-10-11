/* amxx_dyna_codegen.h — machine-code trampolines for plugin-registered
 * natives (register_native) on builds without USE_LIBFFCALL.
 *
 * The generated stub has the call signature of an AMX native,
 *     int fn(AMX *amx, cell *params)
 * and tail-dispatches to
 *     amxx_DynaCallback(id, amx, params)
 * so the AMXX core can look the dynamic native up by id.
 *
 * This header is intentionally dependency-free (only <stdint.h>) so that
 * scripts/dyna_tramp_test.c — executed for real on the arm64 CI runner —
 * compiles the EXACT same instruction sequence libmm_amxmodx.so ships.
 * Keep amxx_DynaCodesize()/amxx_DynaMake() in natives.cpp in sync with
 * amxx_dyna_codesize()/amxx_dyna_make() here.
 *
 * History: the original aarch64 sequence encoded `ldr x17, [pc, #4]`
 * (0x58000031) instead of `ldr x17, [pc, #8]` (0x58000051), so x17 was
 * loaded from the `br x17` word itself plus the low half of the gate
 * pointer — x17 became (gate_low32 << 32) | 0xD61F0220 and the branch
 * jumped to an unmapped address. Every plugin-provided native call
 * (e.g. Zombie Plague's RegisterHamBots via cs_ham_bots_api.amxx) then
 * died with SIGSEGV inside amx_Callback. Verified by the CI test.
 */
#ifndef AMXX_DYNA_CODEGEN_H
#define AMXX_DYNA_CODEGEN_H

#include <stdint.h>

static int amxx_dyna_codesize(void)
{
#if defined(__x86_64__)
	/* mov rdx,rsi + mov rsi,rdi + mov edi,id + movabs rax + jmp rax */
	return 32;
#elif defined(__aarch64__)
	/* 5 instructions (20 bytes) + 8-byte literal */
	return 28;
#elif defined(__arm__)
	/* 5 instructions (20 bytes) + 4-byte literal + 4-byte pad */
	return 28;
#elif defined(__i386__)
	/* mov edx,esp-offsets; not used in practice — see natives.cpp */
	return 0;
#else
	return 0;
#endif
}

static void amxx_dyna_make(void *buffer, int id, void *gate)
{
#if defined(__aarch64__)
	uint32_t *code = (uint32_t *)buffer;
	code[0] = 0xAA0103E2;                       /* mov x2, x1   (params) */
	code[1] = 0xAA0003E1;                       /* mov x1, x0   (amx)    */
	code[2] = 0xD2800000 | (((uint32_t)id & 0xFFFF) << 5); /* movz x0, #id */
	code[3] = 0x58000051;                       /* ldr x17, [pc, #8] — imm19=2, literal at code[5] */
	code[4] = 0xD61F0220;                       /* br x17 */
	*(uintptr_t *)(void *)&code[5] = (uintptr_t)gate; /* .quad gate */
#elif defined(__x86_64__)
	/* SysV: stub receives (amx=rdi, params=rsi); target needs (id=edi, amx=rsi, params=rdx) */
	uint8_t *c = (uint8_t *)buffer;
	int o = 0;
	c[o++] = 0x48; c[o++] = 0x89; c[o++] = 0xF2;            /* mov rdx, rsi  (params) */
	c[o++] = 0x48; c[o++] = 0x89; c[o++] = 0xFE;            /* mov rsi, rdi  (amx)    */
	c[o++] = 0xBF;                                          /* mov edi, id            */
	*(uint32_t *)(void *)(c + o) = (uint32_t)id; o += 4;
	c[o++] = 0x48; c[o++] = 0xB8;                           /* movabs rax, gate       */
	*(uintptr_t *)(void *)(c + o) = (uintptr_t)gate; o += 8;
	c[o++] = 0xFF; c[o++] = 0xE0;                           /* jmp rax                */
#elif defined(__arm__)
	uint32_t *code = (uint32_t *)buffer;
	code[0] = 0xE1A03000;                       /* mov r3, r0   (amx)    */
	code[1] = 0xE1A02001;                       /* mov r2, r1   (params) */
	code[2] = 0xE3000000 | ((((uint32_t)id >> 12) & 0xF) << 16) | ((uint32_t)id & 0xFFF); /* movw r0, #id */
	code[3] = 0xE59F1000;                       /* ldr r1, [pc] — ARM PC bias +8 -> literal at code[5] */
	code[4] = 0xE12FFF11;                       /* bx r1 */
	*(uint32_t *)(void *)&code[5] = (uint32_t)(uintptr_t)gate; /* .word gate */
#else
	(void)buffer; (void)id; (void)gate;
#endif
}

#endif /* AMXX_DYNA_CODEGEN_H */
