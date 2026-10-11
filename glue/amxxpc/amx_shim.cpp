// the amxxpc driver only uses amx_Align16/32 (header byte-swap) from
// amx.cpp. The rest of amx.cpp does not compile on 64-bit targets with
// 32-bit cells (pointer-in-cell casts). Little-endian swap = no-op.
#include "amx.h"
extern "C" {
uint16_t * AMXAPI amx_Align16(uint16_t *v) { (void)v; return v; }
uint32_t * AMXAPI amx_Align32(uint32_t *v) { (void)v; return v; }
}
