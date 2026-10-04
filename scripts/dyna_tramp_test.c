/* dyna_tramp_test.c — executes the REAL register_native trampoline bytes
 * (amxmodx/amxx_dyna_codegen.h) on the CI runner's native architecture.
 *
 * This is the regression test for the aarch64 codegen fix: the sequence used to
 * encode `ldr x17, [pc, #4]` instead of `[pc, #8]`, loading x17 from the
 * `br x17` word itself -> every plugin-provided native call (Zombie Plague's
 * RegisterHamBots via cs_ham_bots_api.amxx) jumped to an unmapped address
 * and killed the game inside amx_Callback.
 *
 * Build:  cc -O0 -I amxmodx-FWGS/amxmodx scripts/dyna_tramp_test.c -o dyna_tramp_test
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "amxx_dyna_codegen.h"

/* Minimal stand-ins matching the shapes the codegen only passes through. */
typedef struct { int dummy; } AMX;
typedef int32_t cell;

static int g_last_id = -1;

static int dyna_callback(int id, AMX *amx, cell *params)
{
	(void)amx;
	g_last_id = id;
	return id * 10 + (int)params[1];
}

int main(void)
{
	const size_t page = (size_t)sysconf(_SC_PAGESIZE);
	unsigned char *buf = mmap(NULL, page,
				  PROT_READ | PROT_WRITE | PROT_EXEC,
				  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED) {
		perror("mmap RWX");
		/* W^X enforced: fall back to memfd-style RW->RX */
		buf = mmap(NULL, page, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (buf == MAP_FAILED) {
			perror("mmap RW");
			return 2;
		}
	}

	if (amxx_dyna_codesize() <= 0) {
		fprintf(stderr, "no codegen for this arch — test not applicable, SKIPPED\n");
		return 77;
	}

	memset(buf, 0, 64);
	amxx_dyna_make(buf, 7, (void *)dyna_callback);
	mprotect(buf, page, PROT_READ | PROT_EXEC);

	int (*fn)(AMX *, cell *) = (int (*)(AMX *, cell *))buf;
	AMX amx = { 0 };
	cell params[2] = { 4, 3 }; /* params[0]=bytes, params[1]=first arg */

	int ret = fn(&amx, params);

	/* expect 7*10 + 3 = 73, dispatched id 7 */
	if (ret == 73 && g_last_id == 7) {
		printf("DYNA TRAMPOLINE OK (ret=%d, id=%d)\n", ret, g_last_id);
		return 0;
	}

	fprintf(stderr, "DYNA TRAMPOLINE FAILED (ret=%d, id=%d, expected ret=73 id=7)\n",
		ret, g_last_id);
	return 1;
}
