/*
port/buffers.h — dynamic SRAM allocation of the emulator's working memory.

All big emulator buffers are heap-allocated at game start and freed when
returning to the menu: on boards without PSRAM the pico_shared menu
(RomLister, screen buffer, artwork) allocates from the same SRAM heap, so
keeping ~150 KB statically reserved would OOM the menu.
*/
#ifndef GWENESIS_PORT_BUFFERS_H
#define GWENESIS_PORT_BUFFERS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate M68K_RAM / ZRAM / VRAM / audio buffers with plain malloc (SRAM —
   never Frens::f_malloc, which would land hot data in slow PSRAM).
   Returns false if any allocation fails (caller reports and returns to the
   menu instead of panicking). Safe to call again after failure. */
bool init_emulator_mem(void);

/* Free everything init_emulator_mem() allocated. Safe to call twice.
   In HSTX offload mode, detach the core1 sound engine FIRST. */
void free_emulator_mem(void);

/* Verify the guard word past the end of every tracked buffer; reports and
   returns the number that were overrun (0 = clean). Cheap enough to call
   once per frame while chasing heap corruption. */
int check_emulator_mem(const char *when);

#ifdef __cplusplus
}
#endif

#endif
