/*
port/gwsram.h — cartridge save RAM (battery-backed SRAM).

The vendored core has no notion of cart save RAM; all of the logic lives here
and the bus only gains two switch cases (see PORTING.md). Detection follows
Genesis Plus GX's sram_init(): the header at $1B0 says whether the cart has
save RAM and over which address range.

Storage is *packed*. Real carts wire a byte-wide SRAM to either the odd or the
even half of the 68000 data bus, so only half of the declared range is backed
by silicon; Genesis Plus GX keeps a flat 64 KB array and leaves the other half
at 0xFF, which is a luxury 520 KB of on-chip SRAM does not allow. Indexing
(address - start) >> 1 costs Sonic 3 512 bytes instead of 1 KB, and a cart that
declares the whole $200000-$20FFFF window 32 KB instead of 64 KB. Accesses to
the half the chip does not drive read back 0xFF, which is exactly what the
Genesis Plus GX filler yields, so nothing is lost. The .srm files written by
main.cpp re-interleave into the flat layout, so they stay interchangeable with
PC emulators.

Lifecycle per game:
    gwsram_detect(rom, romSize)   parse the header (no allocation)
    init_emulator_mem()           allocate gwsram_bytes, set gwsram_live
    ... game runs ...
    free_emulator_mem()           release
*/
#ifndef GWSRAM_H
#define GWSRAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t *gwsram_data;    /* NULL until init_emulator_mem() allocates */
extern uint32_t gwsram_start;   /* first mapped address, word aligned */
extern uint32_t gwsram_span;    /* address range covered, 0 = no save RAM */
extern uint32_t gwsram_bytes;   /* allocation size: span >> 1 packed, else span */
extern uint32_t gwsram_live;    /* == gwsram_span while mapped, else 0 */
extern int gwsram_packed;       /* byte-wide chip: only every other address backed */
extern int gwsram_odd;          /* which parity the chip drives (packed only) */
extern int gwsram_bankable;     /* ROM reaches into the window: $A130F1 controls it */
extern int gwsram_battery;      /* header says the RAM is battery backed */
extern int gwsram_protect;      /* $A130F1 bit 1: writes ignored */
extern int gwsram_dirty;        /* the game wrote to it since the last save */
extern int gwsram_oom;          /* save RAM is declared but could not be had */
extern int gwsram_in_psram;     /* buffer lives in PSRAM, not the SRAM heap */

/* Parse the cart header. Safe to call with any image; resets all state first,
   so nothing survives from the previously played game. */
void gwsram_detect(const unsigned char *rom, size_t romSize);

/* Obtain the buffer, allocating on first use. Returns 0 if the cart has no
   save RAM or the memory could not be had.

   The allocation is deferred rather than done at game start because SGDK puts
   a save-RAM declaration in the header of every game it builds, used or not,
   and its default asks for the whole $200000-$20FFFF window -- 32 KB packed,
   against roughly 10 KB of SRAM heap left once the emulator's fixed buffers
   are up. Allocating that eagerly would cost every SGDK title its saves (and,
   before the guard, panicked the board), when the overwhelming majority never
   touch save RAM at all. Nothing is allocated until a game actually writes,
   or until a .srm turns up for it on the card. */
int gwsram_ensure_buffer(void);

/* Release the buffer; called from free_emulator_mem(). */
void gwsram_release(void);

/* Supplied by the port (main.cpp on the firmware, host_main.c on the harness):
   a non-panicking PSRAM allocator, or NULL when the board has no PSRAM. Used
   only when the SRAM heap cannot take the buffer. */
void *gwsram_port_psram_alloc(size_t size);
void gwsram_port_psram_free(void *p);

/* $A130F1 (the /TIME region): bit 0 maps save RAM in over the ROM, bit 1 write
   protects it. Only meaningful for carts whose ROM reaches past gwsram_start. */
void gwsram_time_write(unsigned int address, unsigned int value);

/* Conversion between the packed buffer and the flat .srm layout, in which a
   byte at `offset` within the mapped range occupies one file byte whether the
   cart backs it or not. Both work on an arbitrary window of the range, so a
   caller can stream a file through a small chunk buffer rather than build a
   64 KB image. `offset` and `n` are clamped to the range.

   gwsram_export fills `flat` with 0xFF wherever the chip drives nothing;
   gwsram_import ignores those positions. */
void gwsram_export(uint32_t offset, uint8_t *flat, uint32_t n);
void gwsram_import(uint32_t offset, const uint8_t *flat, uint32_t n);

/* Perf A/B switch. Building with -DGENESIS_CART_SRAM=0 removes the save-RAM
   test from the 68000 read paths and the bus mapper completely — the compiler
   folds `if (0)` away — so the emulator can be measured with and without it on
   the same board and the same scene. Saves do not work in such a build; it
   exists only to attribute a frame-rate or audio-underrun change. */
#ifndef GENESIS_CART_SRAM
#define GENESIS_CART_SRAM 1
#endif

/* The only part of this on a hot path. It is inlined into the six
   m68k_read/write_memory_* entry points AND into m68ki_read_8/16/32, which the
   68000 opcode handlers expand in turn — so this is the most-duplicated code
   in the emulator and every instruction in it is multiplied by thousands of
   call sites.

   gwsram_live is 0 whenever the cart has no save RAM, the allocation failed,
   or the game banked it out, so an ordinary ROM access pays one failed
   compare. __builtin_expect keeps the save-RAM path off the straight-line
   fall-through, which matters because the opcode handlers run from XIP flash.

   The accessors below stay out of line on purpose. Inlining them cost ~1.7 KB
   of SRAM text, most of it in the 32-bit variants, which expand the 16-bit
   path twice — and save RAM is touched only when a game loads or stores its
   progress, so a call there is free in practice. */
#if GENESIS_CART_SRAM
static inline int gwsram_hit(unsigned int address)
{
    return __builtin_expect((address - gwsram_start) < gwsram_live, 0);
}
#else
#define gwsram_hit(address) 0
#endif

unsigned int gwsram_read8(unsigned int address);
unsigned int gwsram_read16(unsigned int address);
void gwsram_write8(unsigned int address, unsigned int value);
void gwsram_write16(unsigned int address, unsigned int value);

#ifdef __cplusplus
}
#endif

#endif /* GWSRAM_H */
