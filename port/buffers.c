/*
port/buffers.c — see buffers.h.

Also defines the frame-loop globals the core expects the host to own
(scan_line, frame_counter, system_clock).
*/
#include <stdio.h>
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

/* Called by the sound chips when a caller-supplied timestamp would have
   pushed the per-frame sample index past the end of the audio buffers.
   Reports once per chip per game: the index and timestamp identify how
   far out of range the driving clock went. */
void gwenesis_audio_report_clamp(const char *chip, int index, int target)
{
    static int reported_ym, reported_psg;
    int *seen = (chip[0] == 'y') ? &reported_ym : &reported_psg;
    if (*seen)
        return;
    *seen = 1;
    printf("AUDIO CLAMP: %s index=%d (max %d) target=%d frame=%d line=%d\n",
           chip, index, GWENESIS_AUDIO_BUFFER_MAX, target, frame_counter, scan_line);
}

/* Every buffer is allocated with a trailing guard word. An emulator-side
   overrun would otherwise land on malloc's chunk header and only surface
   much later as a corrupt free list (a hard fault deep inside
   malloc/free/mallinfo), which is impossible to attribute. Checking the
   guards names the culprit at the moment it is detected. */
#define GUARD_MAGIC 0xA5C3F00Du
#define GUARD_BYTES 4

typedef struct {
    void *ptr;
    size_t size;
    const char *name;
} tracked_buf_t;

static tracked_buf_t tracked[10];
static int tracked_count;

static void *alloc_or_report(size_t size, const char *what)
{
    void *p = malloc(size + GUARD_BYTES);
    if (!p) {
        printf("init_emulator_mem: SRAM alloc of %u bytes for %s FAILED\n",
               (unsigned)size, what);
        return NULL;
    }
    *(volatile uint32_t *)((uint8_t *)p + size) = GUARD_MAGIC;
    if (tracked_count < (int)(sizeof(tracked) / sizeof(tracked[0]))) {
        tracked[tracked_count].ptr = p;
        tracked[tracked_count].size = size;
        tracked[tracked_count].name = what;
        tracked_count++;
    }
    return p;
}

/* Returns the number of buffers whose guard word was clobbered. */
int check_emulator_mem(const char *when)
{
    int bad = 0;
    for (int i = 0; i < tracked_count; i++) {
        if (!tracked[i].ptr)
            continue;
        uint32_t g = *(volatile uint32_t *)((uint8_t *)tracked[i].ptr + tracked[i].size);
        if (g != GUARD_MAGIC) {
            printf("HEAP GUARD CLOBBERED (%s): %s overran, guard=%08x\n",
                   when, tracked[i].name, (unsigned)g);
            bad++;
        }
    }
    return bad;
}

bool init_emulator_mem(void)
{
    free_emulator_mem();

    M68K_RAM = alloc_or_report(MAX_RAM_SIZE, "M68K_RAM");
    ZRAM = alloc_or_report(MAX_Z80_RAM_SIZE, "ZRAM");
    VRAM = alloc_or_report(VRAM_MAX_SIZE, "VRAM");
    gwenesis_ym2612_buffer =
        alloc_or_report(GWENESIS_AUDIO_BUFFER_LENGTH_PAL * sizeof(int16_t), "ym buffer");
    gwenesis_sn76489_buffer =
        alloc_or_report(GWENESIS_AUDIO_BUFFER_LENGTH_PAL * sizeof(int16_t), "psg buffer");

    if (!M68K_RAM || !ZRAM || !VRAM || !gwenesis_ym2612_buffer || !gwenesis_sn76489_buffer) {
        free_emulator_mem();
        return false;
    }

#if defined(GWENESIS_LUTS_IN_RAM) && GWENESIS_LUTS_IN_RAM != 0
    ym_tl_ram = alloc_or_report(YM2612_TL_TAB_BYTES, "ym tl_tab");
    ym_sin_ram = alloc_or_report(YM2612_SIN_TAB_BYTES, "ym sin_tab");
    ym_lfo_ram = alloc_or_report(YM2612_LFO_PM_TABLE_BYTES, "ym lfo_pm_table");
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
    if (tracked_count)
        check_emulator_mem("free");
    tracked_count = 0;
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
