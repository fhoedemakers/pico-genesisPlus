/*
port/buffers.c — see buffers.h.

Also defines the frame-loop globals the core expects the host to own
(scan_line, frame_counter, system_clock).
*/
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gwenesis_bus.h"
#include "gwenesis_vdp.h"
#include "ym2612.h"
#include "buffers.h"

/* Core-visible memory (extern pointers declared in the vendored core). */
unsigned char *M68K_RAM = NULL; /* 64 KB — 68000 work RAM */
unsigned char *ZRAM = NULL;     /*  8 KB — Z80 RAM */
unsigned char *VRAM = NULL;     /* 64 KB — VDP RAM */

/* Audio sample buffers, one per chip, mono int16 at ~53.267 kHz.
   PAL frames need up to 1061 samples (see GWENESIS_AUDIO_BUFFER_LENGTH_PAL). */
int16_t *gwenesis_ym2612_buffer = NULL;
int16_t *gwenesis_sn76489_buffer = NULL;

/* Chip catch-up bookkeeping (the core declares these extern). */
int ym2612_index;
int ym2612_clock;
int sn76489_index;
int sn76489_clock;

/* Frame-loop globals owned by the host (core references them extern). */
int scan_line;
int frame_counter;
int system_clock;

#if defined(GWENESIS_LUTS_IN_RAM) && GWENESIS_LUTS_IN_RAM != 0
/* RAM working copies of the YM2612 LUTs (~46 KB). The synthesis reads
   them on every sample; leaving them in XIP flash makes core1 (offload)
   or core0 (sync) contend with the 68000 dispatch tables and the PSRAM
   ROM on the shared QMI port — measured as frame drops in DAC-heavy
   games. Heap-allocated per game like everything else here. */
static void *ym_tl_ram, *ym_sin_ram, *ym_lfo_ram;
#endif

bool init_emulator_mem(void)
{
    free_emulator_mem();

    M68K_RAM = malloc(MAX_RAM_SIZE);
    ZRAM = malloc(MAX_Z80_RAM_SIZE);
    VRAM = malloc(VRAM_MAX_SIZE);
    gwenesis_ym2612_buffer = malloc(GWENESIS_AUDIO_BUFFER_LENGTH_PAL * sizeof(int16_t));
    gwenesis_sn76489_buffer = malloc(GWENESIS_AUDIO_BUFFER_LENGTH_PAL * sizeof(int16_t));

    if (!M68K_RAM || !ZRAM || !VRAM || !gwenesis_ym2612_buffer || !gwenesis_sn76489_buffer) {
        free_emulator_mem();
        return false;
    }

#if defined(GWENESIS_LUTS_IN_RAM) && GWENESIS_LUTS_IN_RAM != 0
    ym_tl_ram = malloc(YM2612_TL_TAB_BYTES);
    ym_sin_ram = malloc(YM2612_SIN_TAB_BYTES);
    ym_lfo_ram = malloc(YM2612_LFO_PM_TABLE_BYTES);
    if (!ym_tl_ram || !ym_sin_ram || !ym_lfo_ram) {
        free_emulator_mem();
        return false;
    }
    ym2612_luts_init_ram(ym_tl_ram, ym_sin_ram, ym_lfo_ram);
#endif

    memset(M68K_RAM, 0, MAX_RAM_SIZE);
    memset(ZRAM, 0, MAX_Z80_RAM_SIZE);
    memset(VRAM, 0, VRAM_MAX_SIZE);
    memset(gwenesis_ym2612_buffer, 0, GWENESIS_AUDIO_BUFFER_LENGTH_PAL * sizeof(int16_t));
    memset(gwenesis_sn76489_buffer, 0, GWENESIS_AUDIO_BUFFER_LENGTH_PAL * sizeof(int16_t));
    return true;
}

void free_emulator_mem(void)
{
    free(M68K_RAM);
    free(ZRAM);
    free(VRAM);
    free(gwenesis_ym2612_buffer);
    free(gwenesis_sn76489_buffer);
    M68K_RAM = NULL;
    ZRAM = NULL;
    VRAM = NULL;
    gwenesis_ym2612_buffer = NULL;
    gwenesis_sn76489_buffer = NULL;
#if defined(GWENESIS_LUTS_IN_RAM) && GWENESIS_LUTS_IN_RAM != 0
    free(ym_tl_ram);
    free(ym_sin_ram);
    free(ym_lfo_ram);
    ym_tl_ram = ym_sin_ram = ym_lfo_ram = NULL;
#endif
}
