/*
gwenesis_port.h — pico-genesisPlus port configuration for the vendored gwenesis core.

This is the single point where the (otherwise lightly patched) upstream gwenesis
core is adapted to:
  - the RP2350 target (flash/SRAM placement attributes),
  - the Linux host harness (no placement, plain const),
  - the display pixel format of the active video driver
    (RGB555 for HSTX, RGB444 for PicoDVI, RGB565 on the host),
  - the port's sound seam (gwsnd_*), which either calls the sound chips
    directly (sync mode: PicoDVI / host) or forwards timestamped register
    writes to core1 (offload mode: HSTX).

Every use of this header inside the core is documented in PORTING.md.
*/
#ifndef GWENESIS_PORT_H
#define GWENESIS_PORT_H

#ifndef GWENESIS_PICO
#define GWENESIS_PICO 0
#endif

/* ------------------------------------------------------------------ */
/* Code/data placement                                                 */
/* ------------------------------------------------------------------ */
#if defined(GWENESIS_HOST) && GWENESIS_HOST != 0
/* Linux host harness: no flash, everything is normal code/data. */
#define GW_SRAM_FUNC(f) f
#define GW_IN_FLASH
#define GW_SRAM_DATA
#else
/* m68k.h #defines uint, which breaks pico/types.h's "typedef unsigned int
   uint" when this header is included after it — shelve the macro around
   the SDK include. */
#ifdef uint
#pragma push_macro("uint")
#undef uint
#define GW_PORT_RESTORE_UINT 1
#endif
#include "pico.h"
#ifdef GW_PORT_RESTORE_UINT
#pragma pop_macro("uint")
#undef GW_PORT_RESTORE_UINT
#endif
/* Hot functions copied to SRAM; large const LUTs pinned in flash. */
#define GW_SRAM_FUNC(f) __not_in_flash_func(f)
#define GW_IN_FLASH __in_flash()
/* Small const tables read on the per-sample/per-instruction hot paths:
   keep them in SRAM. Flash (XIP) traffic is the scarce resource — the
   68000 dispatch tables, the ROM (PSRAM shares the QMI port) and, in
   offload mode, core1's synthesis all compete for it, and the 16 KB XIP
   cache is shared between cores. */
#define GW_SRAM_DATA __not_in_flash("gwdata")
#endif

/* ------------------------------------------------------------------ */
/* Display pixel format                                                */
/*                                                                     */
/* Genesis CRAM color is 9-bit: ----BBB-GGG-RRR- (3 bits per channel,  */
/* each channel's value sits in the TOP bits of its target field, so   */
/* a plain >>1 halves all channels in every format below — the shadow  */
/* path relies on that).                                               */
/* ------------------------------------------------------------------ */
#ifndef GWENESIS_PIXEL_FMT
#define GWENESIS_PIXEL_FMT 565
#endif

#if GWENESIS_PIXEL_FMT == 565
/* RGB565: R[15:11] G[10:5] B[4:0] — upstream/host format */
#define GWENESIS_CRAM_TO_PIXEL(v)                                              \
  ((((v) & 0xe00) >> 7) | (((v) & 0x0e0) << 3) | (((v) & 0x00e) << 12))
#define GWENESIS_PIX_HIGHLIGHT_MASK 0x8410 /* MSB of each channel */
/* H32 4->5 upscale helpers: spread channels apart so 4 pixels can be
   summed without cross-channel carries, then pack back. */
#define GWENESIS_PIX_SPACE(c)                                                  \
  ((((c) & 0xf800) << 10) | (((c) & 0x07e0) << 5) | ((c) & 0x001f))
#define GWENESIS_PIX_CONV(b)                                                   \
  ((((b) & 0x3e00000) >> 10) | (((b) & 0xfc00) >> 5) | ((b) & 0x001f))

#elif GWENESIS_PIXEL_FMT == 555
/* RGB555: R[14:10] G[9:5] B[4:0] — HSTX (pico_hdmi) framebuffer format */
#define GWENESIS_CRAM_TO_PIXEL(v)                                              \
  ((((v) & 0xe00) >> 7) | (((v) & 0x0e0) << 2) | (((v) & 0x00e) << 11))
#define GWENESIS_PIX_HIGHLIGHT_MASK 0x4210
#define GWENESIS_PIX_SPACE(c)                                                  \
  ((((c) & 0x7c00) << 10) | (((c) & 0x03e0) << 5) | ((c) & 0x001f))
#define GWENESIS_PIX_CONV(b)                                                   \
  ((((b) & 0x1f00000) >> 10) | (((b) & 0x7c00) >> 5) | ((b) & 0x001f))

#elif GWENESIS_PIXEL_FMT == 444
/* RGB444: R[11:8] G[7:4] B[3:0] — PicoDVI framebuffer format */
#define GWENESIS_CRAM_TO_PIXEL(v)                                              \
  ((((v) & 0xe00) >> 8) | ((v) & 0x0e0) | (((v) & 0x00e) << 8))
#define GWENESIS_PIX_HIGHLIGHT_MASK 0x0888
#define GWENESIS_PIX_SPACE(c)                                                  \
  ((((c) & 0x0f00) << 8) | (((c) & 0x00f0) << 4) | ((c) & 0x000f))
#define GWENESIS_PIX_CONV(b)                                                   \
  ((((b) & 0xf0000) >> 8) | (((b) & 0x0f00) >> 4) | ((b) & 0x000f))

#else
#error "GWENESIS_PIXEL_FMT must be 565, 555 or 444"
#endif

/* ------------------------------------------------------------------ */
/* Sound seam (implemented in port/gwsnd_core0.c)                      */
/*                                                                     */
/* The core never calls YM2612Write/YM2612Read/gwenesis_SN76489_Write  */
/* directly; it goes through these, preserving the exact master-clock  */
/* timestamp of the access. In sync mode they forward straight to the  */
/* chips (upstream catch-up behavior); in offload mode (HSTX) writes   */
/* are enqueued for core1 and reads are answered from a core0 shadow   */
/* of the YM2612 timers/status.                                        */
/* ------------------------------------------------------------------ */
#ifdef __cplusplus
extern "C" {
#endif

void gwsnd_ym_write(unsigned int addr, unsigned int value, int target_mclk);
unsigned int gwsnd_ym_read(int target_mclk);
void gwsnd_psg_write(unsigned int value, int target_mclk);

#ifdef __cplusplus
}
#endif

#endif /* GWENESIS_PORT_H */
