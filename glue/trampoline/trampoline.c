/* ARM backends for the libffcall-compatible
 * trampoline API. See trampoline.h for the contract.
 *
 * The thunk hamsandwich needs is the simplest possible trampoline:
 *
 *     *variable = data;      // one 8-byte (aarch64) / 4-byte (arm32) store
 *     goto address;          // tail-jump, all call arguments pass through
 *
 * Because the jump is a tail call, EVERY part of the calling convention
 * survives untouched: argument registers (integer and FP/SIMD), stack
 * arguments, by-value struct layouts and the return value slot. That is
 * why no signature knowledge is required here — unlike argument-shifting
 * thunks, this works for every hamsandwich hook signature on both ABIs.
 *
 * Register budget (nothing else is clobbered):
 *   - aarch64: x16/x17, the two intra-procedure-call scratch registers of
 *     AAPCS64. They may be clobbered at any call boundary, including a
 *     tail call.
 *   - arm32:   r12 (IP), the intra-procedure-call scratch register, plus
 *     r4 which is saved/restored around the store.
 *
 * Executable-memory allocation (W^X):
 *   1. anonymous RWX mmap — works on devices/ROMs without enforcement;
 *   2. memfd_create + write + mmap PROT_READ|PROT_EXEC — the Android 10+
 *      (targetSdk >= 29) compatible path used by JIT runtimes: the mapping
 *      itself is never writable.
 * If both fail, alloc_trampoline returns NULL and hamsandwich skips the
 * hook instead of crashing.
 */

#include "trampoline.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#if defined(__aarch64__) || defined(__arm__)

#ifndef __NR_memfd_create
#  if defined(__aarch64__)
#    define __NR_memfd_create 279
#  elif defined(__arm__)
#    define __NR_memfd_create 385
#  endif
#endif

/* Map one page of executable memory holding `code`. The returned pointer
 * is the page base so free_trampoline() can munmap it directly. */
static void *tramp_alloc_exec(const unsigned char *code, size_t len)
{
        long psz = sysconf(_SC_PAGESIZE);

        if (psz <= 0)
                psz = 4096;

        /* Path 1: anonymous RWX (legacy devices, emulators, targetSdk < 29) */
        void *p = mmap(NULL, psz, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (p != MAP_FAILED)
        {
                memcpy(p, code, len);
                __builtin___clear_cache((char *)p, (char *)p + len);
                return p;
        }

        /* Path 2: memfd-backed mapping (Android 10+ W^X enforcement).
         * Map RW first, memcpy + __clear_cache while the page is still
         * WRITABLE, then mprotect to RX. The previous code cleared the cache
         * AFTER mapping RX-only -- IC IVAU requires store permission on aarch64
         * and faults (SIGSEGV) on read-only pages on several devices. */
#ifdef __NR_memfd_create
        {
                int fd = (int)syscall(__NR_memfd_create, "hamtramp", 0u);

                if (fd >= 0)
                {
                        void *q = MAP_FAILED;

                        if (ftruncate(fd, psz) == 0)
                                q = mmap(NULL, psz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

                        if (q != MAP_FAILED)
                        {
                                memcpy(q, code, len);
                                __builtin___clear_cache((char *)q, (char *)q + len);

                                if (mprotect(q, psz, PROT_READ | PROT_EXEC) == 0)
                                {
                                        close(fd);
                                        return q;
                                }

                                munmap(q, psz);
                        }

                        close(fd);
                }
        }
#endif

        return NULL;
}

#if defined(__aarch64__)
/* aarch64 thunk (14 instructions, 56 bytes):
 *
 *   movz  x16, #data[15:0]              ; x16 = data (64-bit build)
 *   movk  x16, #data[31:16], lsl #16
 *   movk  x16, #data[47:32], lsl #32
 *   movk  x16, #data[63:48], lsl #48
 *   movz  x17, #variable[15:0]          ; x17 = variable
 *   movk  x17, ...
 *   movk  x17, ...
 *   movk  x17, ...
 *   str   x16, [x17]                    ; *variable = data
 *   movz  x16, #address[15:0]           ; x16 = address
 *   movk  x16, ...
 *   movk  x16, ...
 *   movk  x16, ...
 *   br    x16                           ; tail jump
 */
__TR_function alloc_trampoline (__TR_function address, void* variable, void* data)
{
        unsigned char buf[64];
        uint32_t *w = (uint32_t *)buf;
        uintptr_t d = (uintptr_t)data;
        uintptr_t v = (uintptr_t)variable;
        uintptr_t a = (uintptr_t)address;
        int i;

        /* movz/movk Xn, #imm16, LSL #hw*16 */
#define A64_MOVZ(Rd, imm16, hw) (0xD2800000u | ((hw) << 21) | ((imm16) << 5) | (Rd))
#define A64_MOVK(Rd, imm16, hw) (0xF2800000u | ((hw) << 21) | ((imm16) << 5) | (Rd))

        for (i = 0; i < 4; i++)
                w[i] = (i == 0) ? A64_MOVZ(16, (d >> (16 * i)) & 0xFFFF, i)
                        : A64_MOVK(16, (d >> (16 * i)) & 0xFFFF, i);

        for (i = 0; i < 4; i++)
                w[4 + i] = (i == 0) ? A64_MOVZ(17, (v >> (16 * i)) & 0xFFFF, i)
                        : A64_MOVK(17, (v >> (16 * i)) & 0xFFFF, i);

        w[8] = 0xF9000000u | (17u << 5) | 16u;          /* str x16, [x17] */

        for (i = 0; i < 4; i++)
                w[9 + i] = (i == 0) ? A64_MOVZ(16, (a >> (16 * i)) & 0xFFFF, i)
                        : A64_MOVK(16, (a >> (16 * i)) & 0xFFFF, i);

        w[13] = 0xD61F0000u | (16u << 5);               /* br x16 */

#undef A64_MOVZ
#undef A64_MOVK

        return (__TR_function)tramp_alloc_exec(buf, 14 * 4);
}
#elif defined(__arm__)
/* arm32 (A32 state) thunk (7 instructions + 3 literal words, 40 bytes):
 *
 *   push  {r4}
 *   ldr   r4,  [pc, #16]    ; r4  = data      (literal @0x1C)
 *   ldr   r12, [pc, #16]    ; r12 = variable  (literal @0x20)
 *   str   r4,  [r12]        ; *variable = data
 *   pop   {r4}
 *   ldr   r12, [pc, #8]     ; r12 = address   (literal @0x24)
 *   bx    r12               ; tail jump (Thumb interworking via bit0)
 *   .word data
 *   .word variable
 *   .word address
 *
 * ldr literal reads PC as (insn + 8); all three offsets stay positive
 * with this layout. The address literal keeps bit0 as given by the
 * caller so `bx` performs the ARM/Thumb state switch correctly.
 */
__TR_function alloc_trampoline (__TR_function address, void* variable, void* data)
{
        unsigned char buf[64];
        uint32_t *w = (uint32_t *)buf;

        w[0] = 0xE92D0010u;                             /* push {r4}      */
        w[1] = 0xE59F4000u | 0x10u;                     /* ldr r4,[pc,#16] */
        w[2] = 0xE59FC000u | 0x10u;                     /* ldr r12,[pc,#16] */
        w[3] = 0xE58C4000u;                             /* str r4,[r12]    */
        w[4] = 0xE8BD0010u;                             /* pop {r4}        */
        w[5] = 0xE59FC000u | 0x08u;                     /* ldr r12,[pc,#8] */
        w[6] = 0xE12FFF1Cu;                             /* bx r12          */
        w[7] = (uint32_t)(uintptr_t)data;               /* @0x1C */
        w[8] = (uint32_t)(uintptr_t)variable;           /* @0x20 */
        w[9] = (uint32_t)(uintptr_t)address;            /* @0x24 */

        return (__TR_function)tramp_alloc_exec(buf, 10 * 4);
}
#endif

void free_trampoline (__TR_function tramp)
{
        long psz = sysconf(_SC_PAGESIZE);

        if (psz <= 0)
                psz = 4096;

        if (tramp)
                munmap((void *)tramp, psz);
}

int is_trampoline (void* tramp)
{
        (void)tramp;
        return 0;
}

#else /* !aarch64 && !arm */

#if defined(__x86_64__)
/* real x86_64 (SysV) backend -- 42-byte thunk.
 * The previous fallback returned `address` itself as the "trampoline",
 * which made hamsandwich write Hook_Void_Void straight into the vtable
 * while the `hook` global was never populated (NULL deref / hang on the
 * very first hook fire -- the "ZP + fake client connects -> hang" bug).
 *
 *   movabs r10, data        49 BA imm64
 *   movabs r11, variable    49 BB imm64
 *   mov    [r11], r10       4D 89 13
 *   movabs r11, address     49 BB imm64
 *   jmp    r11              41 FF E3
 *
 * r10/r11 are the two intra-procedure-call scratch registers of SysV
 * AMD64 -- free to clobber, every real argument register passes through.
 */
__TR_function alloc_trampoline (__TR_function address, void* variable, void* data)
{
        unsigned char buf[64];
        unsigned char *w = buf;
        uintptr_t d = (uintptr_t)data;
        uintptr_t v = (uintptr_t)variable;
        uintptr_t a = (uintptr_t)address;
        long psz = sysconf(_SC_PAGESIZE);

        if (psz <= 0)
                psz = 4096;

        *w++ = 0x49; *w++ = 0xBA;                 /* movabs r10, data   */
        memcpy(w, &d, 8); w += 8;
        *w++ = 0x49; *w++ = 0xBB;                 /* movabs r11, var    */
        memcpy(w, &v, 8); w += 8;
        *w++ = 0x4D; *w++ = 0x89; *w++ = 0x13;    /* mov [r11], r10     */
        *w++ = 0x49; *w++ = 0xBB;                 /* movabs r11, addr   */
        memcpy(w, &a, 8); w += 8;
        *w++ = 0x41; *w++ = 0xFF; *w++ = 0xE3;    /* jmp r11            */

        void *p = mmap(NULL, psz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (p == MAP_FAILED)
                return NULL;

        memcpy(p, buf, (size_t)(w - buf));
        __builtin___clear_cache((char *)p, (char *)p + (w - buf));

        if (mprotect(p, psz, PROT_READ | PROT_EXEC) != 0)
        {
                munmap(p, psz);
                return NULL;
        }

        return (__TR_function)p;
}

void free_trampoline (__TR_function tramp)
{
        long psz = sysconf(_SC_PAGESIZE);

        if (psz <= 0)
                psz = 4096;

        if (tramp)
                munmap((void *)tramp, psz);
}

int is_trampoline (void* tramp)
{
        (void)tramp;
        return 0;
}

#else /* neither ARM nor x86_64 */

__TR_function alloc_trampoline (__TR_function address, void* variable, void* data)
{
        (void)variable;
        (void)data;
        /* return NULL (NOT `address`!) so hamsandwich logs the failure
         * and leaves the vtable alone instead of silently installing a
         * callback that dereferences an unset `hook` pointer. */
        fprintf(stderr, "trampoline: no backend for this architecture\n");
        return NULL;
}

void free_trampoline (__TR_function tramp)
{
        (void)tramp;
}

int is_trampoline (void* tramp)
{
        (void)tramp;
        return 0;
}

#endif /* __x86_64__ */

#endif
