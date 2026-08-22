/*
port/gwsram.c — see gwsram.h.

Header detection mirrors Genesis Plus GX's sram_init(), including its fixups
for the many carts that declare a nonsensical range.
*/
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "gwenesis_port.h"
#include "gwsram.h"

/* Cartridge save RAM can only live in the $200000-$3FFFFF half of the cart
   window; anything else in the header is a broken image, not a mapping. */
#define SRAM_WINDOW_LOW 0x200000u
#define SRAM_WINDOW_HIGH 0x3FFFFFu

uint8_t *gwsram_data;
uint32_t gwsram_start;
uint32_t gwsram_span;
uint32_t gwsram_bytes;
uint32_t gwsram_live;
int gwsram_packed;
int gwsram_odd;
int gwsram_bankable;
int gwsram_battery;
int gwsram_protect;
int gwsram_dirty;

/* The loader byte-swaps every 16-bit word of the image for the core's
   little-endian fetches, so the byte at file offset N lives at [N ^ 1] —
   the same idiom isValidGenesisRom() uses in main.cpp. */
static uint8_t hdr8(const unsigned char *rom, uint32_t off)
{
    return rom[off ^ 1];
}

static uint32_t hdr32(const unsigned char *rom, uint32_t off)
{
    return ((uint32_t)hdr8(rom, off) << 24) | ((uint32_t)hdr8(rom, off + 1) << 16) |
           ((uint32_t)hdr8(rom, off + 2) << 8) | (uint32_t)hdr8(rom, off + 3);
}

void gwsram_detect(const unsigned char *rom, size_t romSize)
{
    uint32_t type, start, end;
    int type_byte_wide, range_byte_wide;

    /* The firmware never reboots between games, so every global has to be put
       back rather than assumed zero. */
    gwsram_data = NULL;
    gwsram_start = 0;
    gwsram_span = 0;
    gwsram_bytes = 0;
    gwsram_live = 0;
    gwsram_packed = 0;
    gwsram_odd = 0;
    gwsram_bankable = 0;
    gwsram_battery = 0;
    gwsram_protect = 0;
    gwsram_dirty = 0;

    if (!rom || romSize < 0x200)
        return;

    /* "RA" at $1B0 is the only marker. Genesis Plus GX also enables a blanket
       64 KB for every ROM below 2 MB that lacks it; that trade is wrong here,
       where the 64 KB would come out of the same heap the menu needs. */
    if (hdr8(rom, 0x1B0) != 'R' || hdr8(rom, 0x1B1) != 'A')
        return;

    type = hdr8(rom, 0x1B2);
    start = hdr32(rom, 0x1B4);
    end = hdr32(rom, 0x1B8);

    if (start < SRAM_WINDOW_LOW || start > SRAM_WINDOW_HIGH) {
        printf("SRAM: header claims save RAM at %06x-%06x, outside the cart window - ignored\n",
               (unsigned)start, (unsigned)end);
        return;
    }
    /* Same fixups Genesis Plus GX applies to bad headers. */
    if (end < start || (end - start) >= 0x10000u)
        end = start + 0xFFFFu;
    if (end > SRAM_WINDOW_HIGH)
        end = SRAM_WINDOW_HIGH;

    /* Chip width. Bits 4-3 of the type byte say odd-only (%11), even-only
       (%10) or word-wide (%00), but enough headers get it wrong that it is
       cross-checked against the parity of the declared range: a byte-wide chip
       covers one parity only, so its start and end have the same one. When the
       two disagree, fall back to the word-wide layout, which is never wrong,
       only twice as large. */
    type_byte_wide = (type & 0x18u) != 0;
    range_byte_wide = ((start ^ end) & 1u) == 0;

    gwsram_packed = type_byte_wide && range_byte_wide;
    gwsram_odd = gwsram_packed ? (int)(start & 1u) : 0;

    gwsram_start = start & ~1u;
    gwsram_span = (end | 1u) - gwsram_start + 1u;

    /* The save window is a single 64 KB page — that is what the flat .srm
       layout addresses — so a header whose range would run past the end of it
       gets truncated rather than wrapped. */
    if ((gwsram_start & 0xFFFFu) + gwsram_span > 0x10000u)
        gwsram_span = 0x10000u - (gwsram_start & 0xFFFFu);

    /* A two-byte range is a serial EEPROM sharing the same header field, not
       an SRAM chip. Emulating those needs an I2C device model and a per-game
       table; leaving it unmapped keeps the pre-existing behaviour. */
    if (gwsram_span <= 2) {
        printf("SRAM: %06x-%06x is a serial EEPROM, not supported - no saves for this game\n",
               (unsigned)start, (unsigned)end);
        gwsram_start = 0;
        gwsram_span = 0;
        gwsram_packed = 0;
        gwsram_odd = 0;
        return;
    }

    gwsram_bytes = gwsram_packed ? (gwsram_span >> 1) : gwsram_span;
    gwsram_battery = (type & 0x40u) != 0;

    /* When the ROM itself reaches into the save window the two overlap, and
       the cart uses $A130F1 to switch between them (Phantasy Star IV is 3 MB).
       ROM wins until the game asks for save RAM. */
    gwsram_bankable = romSize > gwsram_start;

    printf("SRAM: %06x-%06x, %s, %s, %u bytes%s\n",
           (unsigned)gwsram_start, (unsigned)(gwsram_start + gwsram_span - 1),
           gwsram_packed ? (gwsram_odd ? "odd bytes" : "even bytes") : "word wide",
           gwsram_battery ? "battery backed" : "no battery",
           (unsigned)gwsram_bytes,
           gwsram_bankable ? ", banked over ROM via $A130F1" : "");
}

/* ------------------------------------------------------------------ */
/* Bus accessors. Reached only after gwsram_hit(), i.e. only for the    */
/* carts that have save RAM and only inside their mapped range.         */
/* ------------------------------------------------------------------ */

unsigned int GW_SRAM_FUNC(gwsram_read8)(unsigned int address)
{
    unsigned int off = address - gwsram_start;

    if (gwsram_packed) {
        if ((int)(off & 1u) != gwsram_odd)
            return 0xFF; /* the half the chip does not drive */
        off >>= 1;
    }
    return gwsram_data[off];
}

void GW_SRAM_FUNC(gwsram_write8)(unsigned int address, unsigned int value)
{
    unsigned int off;

    if (gwsram_protect)
        return;
    off = address - gwsram_start;
    if (gwsram_packed) {
        if ((int)(off & 1u) != gwsram_odd)
            return;
        off >>= 1;
    }
    gwsram_data[off] = (uint8_t)value;
    gwsram_dirty = 1;
}

/* Word accesses are even aligned on the 68000; masking guarantees the second
   byte stays inside the range, since both start and span are even. */
unsigned int GW_SRAM_FUNC(gwsram_read16)(unsigned int address)
{
    unsigned int a = address & ~1u;

    return (gwsram_read8(a) << 8) | gwsram_read8(a + 1);
}

void GW_SRAM_FUNC(gwsram_write16)(unsigned int address, unsigned int value)
{
    unsigned int a = address & ~1u;

    gwsram_write8(a, (value >> 8) & 0xFF);
    gwsram_write8(a + 1, value & 0xFF);
}

/* Clamp a caller's window to what is actually mapped. */
static uint32_t clamp_window(uint32_t offset, uint32_t n)
{
    if (!gwsram_data || offset >= gwsram_span)
        return 0;
    if (n > gwsram_span - offset)
        n = gwsram_span - offset;
    return n;
}

void gwsram_export(uint32_t offset, uint8_t *flat, uint32_t n)
{
    uint32_t i;

    n = clamp_window(offset, n);
    for (i = 0; i < n; i++) {
        uint32_t at = offset + i;
        if (gwsram_packed) {
            if ((int)(at & 1u) != gwsram_odd) {
                flat[i] = 0xFF; /* the chip drives nothing here */
                continue;
            }
            at >>= 1;
        }
        flat[i] = gwsram_data[at];
    }
}

void gwsram_import(uint32_t offset, const uint8_t *flat, uint32_t n)
{
    uint32_t i;

    n = clamp_window(offset, n);
    for (i = 0; i < n; i++) {
        uint32_t at = offset + i;
        if (gwsram_packed) {
            if ((int)(at & 1u) != gwsram_odd)
                continue;
            at >>= 1;
        }
        gwsram_data[at] = flat[i];
    }
}

void GW_SRAM_FUNC(gwsram_time_write)(unsigned int address, unsigned int value)
{
    /* Only the save RAM control register. $A130F3-$FF are the SSF2 bank
       registers, which this port does not implement. */
    if ((address & 0xFFu) != 0xF1u)
        return;

    /* Only carts whose ROM overlaps the save window are wired to this
       register; Genesis Plus GX does not even install a handler for the
       others. Honouring it regardless would let a stray write protect or
       unmap save RAM the cart has no way of switching. */
    if (!gwsram_span || !gwsram_data || !gwsram_bankable)
        return;

    gwsram_protect = (value & 2u) != 0;
    gwsram_live = (value & 1u) ? gwsram_span : 0;
}
