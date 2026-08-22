# gwenesis core — port notes

This directory is a vendored copy of upstream
[bzhxx/gwenesis](https://github.com/bzhxx/gwenesis) @ `168e466` ("Major
update"), lightly patched for the RP2350 port. The goal is that every
difference from upstream is mechanical, documented here, and re-appliable
to a future upstream update. The sound architecture around the core
(catch-up timestamps, core1 offload, resampling) lives outside the core in
`port/` — the core itself keeps upstream's `GWENESIS_AUDIO_ACCURATE=1`
semantics untouched.

Files NOT copied from upstream: `cpus/Z80/Debug.c`, `cpus/Z80/ConDebug.c`
(debugger, needs AY8910/Console headers that don't exist), `cpus/Z80/Z80.o`
(stray object file).

New files (not upstream): `gwenesis_port.h`, `sound/luts/*.h` (generated
by `hosttest/lutgen`), `CMakeLists.txt`, this file.

## Build contract

The port always defines: `GWENESIS_PICO=1`, `TABLES_FULL=1`,
`GNW_TARGET_MARIO=0`, `GNW_TARGET_ZELDA=0`,
`GWENESIS_PIXEL_FMT=555|444|565`, and on the Linux harness additionally
`GWENESIS_HOST=1`. `GWENESIS_Z80_DIVISOR` (default 14) overrides the Z80
clock divisor. `#pragma GCC optimize("Ofast")` sites (guarded by the GNW
defines, so inactive) are replaced by per-file `-O2` in CMake.

Pico builds additionally define `GWENESIS_LUTS_IN_RAM=1`: XIP flash
bandwidth is the scarce resource (the M68K dispatch tables, the PSRAM ROM
and — in offload mode — core1's synthesis all share the QMI port and a
16 KB XIP cache), so tables read on per-sample/per-instruction hot paths
must not live in flash. The three big YM2612 LUTs get per-game heap
copies (`ym2612_luts_init_ram`, called from `port/buffers.c`); small hot
const tables are placed in RAM at all times via `GW_SRAM_DATA`
(`__not_in_flash`). Leaving these in flash measurably drops core0 below
60 fps in DAC-heavy games (Sonic, Xeno Crisis).

## Patches, by file

### `gwenesis_port.h` (new)
Single adaptation point: SRAM/flash placement macros (`GW_SRAM_FUNC`,
`GW_IN_FLASH` — no-ops on the host), the CRAM→pixel conversion macro set
per display format (RGB565 host / RGB555 HSTX / RGB444 PicoDVI) including
the shadow/highlight mask and the H32 4→5 upscale channel spread macros,
and the sound-seam prototypes (`gwsnd_ym_write` / `gwsnd_ym_read` /
`gwsnd_psg_write`) implemented in `port/gwsnd_core0.c`.

### `bus/gwenesis_bus.c`
- `ROM_DATA` static 8 MB array → `const unsigned char *` pointing into
  PSRAM/XIP flash; new `rom_addr_mask` (pow2ceil(size)−1) bounds/mirrors
  every fetch. `M68K_RAM`/`ZRAM` → extern pointers allocated per game by
  `port/buffers.c` (freed on menu return — non-PSRAM boards need the SRAM
  heap for the menu).
- New `load_cartridge(const unsigned char *, size_t)` variant: no memcpy,
  no ROM_SWAP (pico_shared pre-byte-swaps), sets pointer+mask, resets the
  TMSS latch (survives relaunch-without-reboot on PSRAM boards).
- `power_on()`: `memset(&m68k, 0, sizeof m68k)` before `m68k_init()` —
  stale context crashed relaunches (fix carried over from the old port).
- The 6 sound-chip call sites route through the `gwsnd_*` seam with their
  exact `m68k_cycles_master()` timestamps.
- `NONE` enum renamed `NONE_` (collides with pico_shared `SaveStateTypes`).
- `GW_SRAM_FUNC` on `m68k_read/write_memory_8/16/32`.
- Cartridge save RAM, all of whose logic lives in `port/gwsram.c`; the core
  only routes to it:
  - `gwenesis_bus_map_address()`: the `range < 0x80` arm returns `SRAM_ADDR`
    when `gwsram_hit(address)`, else `ROM_ADDR` as before. The test is one
    subtract and one compare, and can never match on a cart without save
    RAM, so ordinary ROM reads pay a failed compare.
  - `gwenesis_bus_map_io_address()`: `$A13xxx` (the /TIME region, which
    carries the save RAM control register at `$A130F1`) now returns
    `TIME_CTRL`. Upstream's `address & 0x1000` test routed it to the Z80
    control registers, where it matched neither BUSREQ nor RESET and did
    nothing beyond a `z80_sync()`; reads got `z80_read_ctrl()`'s `0xFF`
    default, which the `TIME_CTRL` read case returns unchanged.
  - `SRAM_ADDR` / `TIME_CTRL` cases in `gwenesis_bus_read_memory_8/16` and
    `gwenesis_bus_write_memory_8/16`.

### `bus/gwenesis_bus.h`
- `GWENESIS_AUDIO_BUFFER_LENGTH_PAL` 1056 → **1072**: a PAL frame
  generates up to 313·3420/1009 = 1060.9 samples; upstream's end-of-frame
  top-up overflowed both audio buffers by ~5 samples.
- `Z80_FREQ_DIVISOR` overridable via `GWENESIS_Z80_DIVISOR` (upstream 14 ≈
  7% fast Z80; 15 is hardware-exact MCLK/15 — affects XGM1 PCM pitch, keep
  14 for upstream parity until A/B-tested on hardware).

### `cpus/M68K/m68k.h`
- `GWENESIS_PICO` branch: `ROM_DATA` as const pointer + masked
  `FETCH8/16/32ROM` macros.
- `cpu_memory_map memory_map[256]` (5 KB) compiled out of
  `m68ki_cpu_core` — every reader is inside `#if 0` in m68kcpu.h.

### `cpus/M68K/m68kcpu.h`
- `m68ki_read_8/16/32` short-circuit everything below `$800000` straight to
  `FETCH*ROM`, so a 68000 data read never reaches `m68k_read_memory_*` and
  therefore never reaches the bus mapper. Cartridge save RAM lives in that
  window, so these three now test `gwsram_hit()` first and go to
  `gwsram_read8/16` (the 32-bit one delegates to `m68k_read_memory_32`,
  which maps each word separately and so handles a read straddling the
  ROM/save-RAM boundary).

  Without this the writes land — `m68ki_write_*` already routes everything
  below `$FF0000` through `m68k_write_memory_*` — but every read-back
  returns the ROM mirror, so a game stores its save and then cannot see it.
  `hosttest`'s `GEN_SRAM_SELFTEST` reads through `m68ki_read_8` (via the
  `gwenesis_host_cpu_read8` hook in m68kcpu.c) precisely because checking
  only the bus entry point passes while every real game fails.

  This is the one part of save RAM that can cost frames: it is inlined into
  every opcode handler that reads memory. `gwsram_hit()` is wrapped in
  `__builtin_expect(..., 0)` so the save-RAM path stays off the straight-line
  fall-through, and the whole test compiles away under
  `-DGENESIS_CART_SRAM=0` for an on-hardware A/B (see the README).

  Still bypassed, deliberately: `m68k_read_immediate_*` and
  `m68k_read_pcrelative_*`. Both are instruction-stream fetches, and PC
  relative displacement is +/-32 KB, so neither can reach save RAM from
  code running in ROM.

### `cpus/M68K/m68kcpu.c`
- `GW_SRAM_FUNC` on `m68k_run` (dispatch loop only; the opcode handlers
  and the 320 KB `TABLES_FULL` const tables stay in flash).
- Host-only `gwenesis_host_cpu_read8()` test hook exposing `m68ki_read_8`
  (see the m68kcpu.h note above). Compiled out of the firmware.

### `cpus/Z80/Z80.c`
- **Bug fix**: the `GENESIS` opcode-fetch path declared
  `extern byte *Z80_RAM[]` (an 8-entry page table) and indexed
  `Z80_RAM[A>>13]`, but z80inst.c defines `Z80_RAM` as a single base
  pointer — any opcode fetch at A ≥ 0x2000 dereferenced garbage (Xeno
  Crisis executes its driver from the 2000-3FFF RAM mirror; caught by
  ASan in the host harness). `OpZ80` now fetches RAM/mirror below 0x4000
  and falls back to full `RdZ80` decode above.

### `sound/z80inst.c`
- **Bug fix**: `z80_start()` did not clear `Z80_BANK` (or `current_timeslice`).
  The firmware starts a new game without rebooting, so the previous game's
  bank left the Z80's 32 KB window into 68000 space pointing at the wrong
  address — a sound driver reading its samples through that window (XGM)
  got garbage, and Xeno Crisis launched as a second game showed a black
  screen. Upstream masks this with an `assert(0)` in
  `gwenesis_bus_map_z80_address` that is compiled out on the Pico.
- **Bug fix** (carried over from the old port): the Z80 RESET register
  handler pulsed the Z80 on *every* non-zero write; SGDK writes this
  register repeatedly (`Z80_getAndRequestBus` polling), restarting the
  driver forever — SGDK games hung at boot. Reset is now edge-triggered
  (pulse only on asserted→deasserted).
- The 3 chip call sites route through the `gwsnd_*` seam, preserving the
  sub-timeslice timestamps `zclk + current_timeslice − ICount·divisor`
  (this is what makes DAC PCM and XGM sample-accurate).
- `GW_SRAM_FUNC` on `RdZ80`/`WrZ80`/`z80_run`.

### `sound/ym2612.c`
- The three runtime-built LUTs (`tl_tab` 26 KB, `sin_tab` 4 KB,
  `lfo_pm_table` 16 KB, all built with libm) become generated const
  arrays in flash (`sound/luts/*.h`, emitted and checked by
  `hosttest/lutgen`); `init_tables()` keeps only the integer DETUNE
  table. `GWENESIS_HOST_LUTGEN=1` restores the original builders for the
  generator itself.
- Under `GWENESIS_LUTS_IN_RAM` the flash arrays become masters
  (`*_flash`) with per-game RAM working copies wired up by
  `ym2612_luts_init_ram()` (see the build-contract section for why).
- Small per-sample const tables (`sl_table`, `eg_inc`,
  `eg_rate_select/shift`, `lfo_samples_per_step`) placed in RAM via
  `GW_SRAM_DATA`.
- `GW_SRAM_FUNC` on `YM2612Update`/`ym2612_run`/`YM2612Write`/`YM2612Read`.
- No synthesis-logic changes.

### `sound/ym2612.h`, `sound/gwenesis_sn76489.h`
- Audio buffer externs become pointers under `GWENESIS_PICO` (allocated
  per game by `port/buffers.c`).

### Audio buffer bounds (both chips)
- `ym2612_run()` / `gwenesis_SN76489_run()` clamp the per-frame sample
  index to `GWENESIS_AUDIO_BUFFER_MAX`. Both derive the index from a
  caller-supplied master-clock timestamp, so a bad timestamp wrote past
  the audio buffers and corrupted the heap — a failure that only surfaced
  much later inside malloc/free. `gwenesis_audio_report_clamp()` (port)
  prints the offending index/timestamp once per chip.

### `sound/gwenesis_sn76489.c`
- **Bug fix**: `WhiteNoiseFeedback` was never initialized, so the noise
  LFSR shifted in zeros and the white-noise channel (drums, waves,
  explosions) went silent. Set to `0x0009` (bits 0+3, the SMS2/Genesis
  tap pattern) in `gwenesis_SN76489_Reset()`.
- `GW_SRAM_FUNC` on Update/run/Write; `GW_SRAM_DATA` on
  `PSGVolumeValues`.

### `cpus/Z80/Tables.h`
- `GW_SRAM_DATA` on the per-instruction tables (`Cycles*`, `ZSTable`,
  `PZSTable`, ~1.8 KB). `DAATable` (4 KB) stays in flash — the DAA
  opcode is rare.

### `io/gwenesis_io.c`
- **Bug fix**: new `gwenesis_io_reset()` (called from `reset_emulation()`).
  `io_reg[]` and the pad shadow were static initialisers only and never
  re-initialised, so a second game inherited the previous game's port
  direction / TH-select masks and mis-read the pads at boot (Space
  Invaders '91 rendered corrupt when launched after another game). The
  region/version byte is preserved because `set_region()` sets it per ROM.

### `vdp/gwenesis_vdp_mem.c`
- `VRAM` → extern pointer (port-allocated).
- `gwenesis_vdp_reset()` also clears `fifo[]` and `hvcounter_latch`, which
  upstream missed (same inter-game staleness class as the two above).
- The 4 inline CRAM→RGB565 conversions → `GWENESIS_CRAM_TO_PIXEL()`
  (format selected by `GWENESIS_PIXEL_FMT`).
- `GW_SRAM_FUNC` on `gwenesis_vdp_vram_write`, `gwenesis_vdp_hcounter`,
  `gwenesis_vdp_read/write_memory_16`.

### `vdp/gwenesis_vdp_gfx.c`
- The 16-bit renderer (upstream's Game & Watch branch) is selected under
  `GWENESIS_PICO`; the 24-bit desktop branch compiles out.
- Pixel-format parametrization: highlight mask and `SPACE`/`CONV` (H32
  4→5 upscale) come from `gwenesis_port.h`. The 565 `SPACE` red mask is
  widened from upstream's `0xe800` to the full field so shadowed pixels
  keep their low red bit.
- `buffer_line_H32`/`scaled_buffer_line` made `static` (1.1 KB of stack
  per H32 scanline vs the 3 KB core0 stack).
- `GW_SRAM_FUNC` on `gwenesis_vdp_render_line`, `draw_line_b`,
  `draw_line_aw`, `draw_sprites`, `draw_sprites_over_planes`,
  `blit_4to5_line`.

## Known limitations (unchanged from upstream)

- Interlace mode unimplemented (`gwenesis_vdp_render_line` returns —
  Sonic 2 two-player is blank).
- No SSF2 mapper: the bank registers at `$A130F3-$A130FF` are ignored, so
  ROMs larger than 4 MB are unsupported.
- No serial EEPROM (Wonder Boy in Monster World, NBA Jam, Micro Machines
  2, Mega Man: The Wily Wars). Those carts declare a two-byte range in the
  same header field save RAM uses; `gwsram_detect()` recognises them and
  leaves the window unmapped, so they behave as they did before save RAM
  existed. Supporting them needs an I2C device model and a per-game table.
- VDP DMA reads cartridge space through `FETCH16ROM()`
  (`gwenesis_vdp_dma_m68k`), bypassing the bus, so a DMA sourced from save
  RAM would transfer ROM. No known game does this — save RAM is byte-wide
  and slow, which is exactly what DMA is not for.
- VDP DMA is instantaneous; FIFO not emulated.
- YM2612 busy flag (status bit 7) not emulated; stereo panning compiled
  out (mono mix).

## Verification

`hosttest/` runs this exact core on Linux under AddressSanitizer, dumps
PPM frames and per-chip WAVs, regenerates and diffs the LUT headers
(`build.sh check`), and `GEN_VERIFY_SHADOW=1` runs the core0 timer shadow
(`port/gwsnd_shadow.c`) in lockstep with the real chip — proven bit-exact
over ~950,000 status reads across Sonic 1/3, Xeno Crisis, Streets of Rage
2, Gunstar Heroes, Thunder Force IV and Columns.

`GEN_FIRST_ROM=<rom> gen_host <rom2> …` runs one game, tears it down in
the firmware's exact order, then launches a second — the condition that
exposed the inter-game state bugs above. A second game's output must be
byte-identical to launching it alone; the fixes were confirmed by
reproducing the corruption with them disabled and then verifying a
4x4 warm-up/second-game matrix.
