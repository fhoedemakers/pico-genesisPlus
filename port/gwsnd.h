/*
port/gwsnd.h — the port's sound engine around the gwenesis sound chips.

The vendored core routes every YM2612/SN76489 register access through the
seam declared in gwenesis/gwenesis_port.h (gwsnd_ym_write / gwsnd_ym_read /
gwsnd_psg_write), preserving the exact master-clock timestamp of the access.
That timestamped catch-up is what makes DAC PCM (the "SEGA" voice) and
SGDK/XGM-driver audio sound right — never coarsen it.

Two modes, chosen at gwsnd_init():

  sync mode (PicoDVI builds and the host harness)
      Seam calls go straight to the chips (upstream GWENESIS_AUDIO_ACCURATE
      behavior). gwsnd_frame_end() tops the chips up to the frame's clock,
      mixes both mono buffers, resamples ~53.267 kHz -> 44.1 kHz and pushes
      stereo samples to the output callback — all on the calling core.

  offload mode (HSTX builds)
      Seam writes are timestamped events pushed to an SPSC FIFO drained by
      core1 (registered as the pico_hdmi background task); a small core0
      shadow of the YM2612 timers answers status reads locally. Synthesis,
      mixing and resampling all run on core1.
*/
#ifndef GWENESIS_PORT_GWSND_H
#define GWENESIS_PORT_GWSND_H

#include <stdint.h>
#include "gwenesis_port.h" /* seam prototypes the core calls */

#ifdef __cplusplus
extern "C" {
#endif

/* Where resampled stereo output goes (one call per 44.1 kHz frame).
   Set before gwsnd_init(). */
void gwsnd_set_output(void (*fn)(int16_t left, int16_t right));

/* offload != 0 requests the core1 engine (HSTX builds only). */
void gwsnd_init(int is_pal, int offload);
void gwsnd_shutdown(void);

/* End-of-frame: top-up both chips to system_clock, emit audio, reset the
   per-frame chip clocks. Call after the scanline loop, before pacing. */
void gwsnd_frame_end(int system_clock);

/* Per-scanline progress tick, called from the frame loop after both CPUs
   finished the line (system_clock = the line's end). Sync mode: no-op.
   Offload mode: publishes a clock watermark so core1 synthesizes
   continuously through the frame instead of in one end-of-frame burst
   (a 735-sample burst overshoots the HDMI DI queue's drop watermark and
   audibly chops the audio), and drains the core1->core0 sample bridge
   incrementally for the same reason on the I2S path. */
void gwsnd_line_tick(int system_clock);

/* Output attenuation: samples are (ym + psg) >> shift, saturated. */
void gwsnd_set_volume_shift(int shift);

/* Sink feedback for drift trim: current fill of the output queue in
   permille of its target (1000 = exactly on target). The resampler nudges
   its step up/down within +/-0.5% to keep the queue level stable. */
void gwsnd_set_fill_permille(int permille);

/* Register the query that reads the sink's fill level (permille of
   target, 1000 = on target). In offload mode it is called FROM CORE1 at
   every frame end, so it must only read volatile counters. NULL disables
   drift trim. */
void gwsnd_set_fill_query(int (*fn)(void));

/* Report the PAL/NTSC video mode each frame (from REG1_PAL). Retunes the
   resampler ratio when it changes — in offload mode the switch is applied
   on core1 at the next frame boundary. */
void gwsnd_set_pal(int is_pal);

/* --- offload mode (HSTX, GWSND_OFFLOAD=1) --------------------------- */
/* Samples that must be delivered on core0 (I2S DAC, VU meter) travel
   core1 -> core0 through a small SPSC bridge: point the output callback
   at gwsnd_bridge_push, register the core0 consumer with
   gwsnd_set_bridge_sink, and the bridge drains incrementally from
   gwsnd_line_tick (plus a final sweep via gwsnd_bridge_drain per frame). */
void gwsnd_bridge_push(int16_t l, int16_t r);
void gwsnd_set_bridge_sink(void (*fn)(int16_t l, int16_t r));
void gwsnd_bridge_drain(void);

/* Perf diagnostics (offload mode): event-FIFO high-water mark since the
   last call (self-resetting) and total dropped events. */
unsigned int gwsnd_stats_fifo_highwater(void);
unsigned int gwsnd_stats_drops(void);
/* Samples the core1->core0 bridge discarded because core0 stopped
   draining; read-and-reset. Nonzero means the I2S/VU path is losing
   audio upstream of the I2S ring. */
unsigned int gwsnd_stats_bridge_drops(void);

/* --- core0 shadow of the YM2612 timers/status (gwsnd_shadow.c) ------ */
void gwsnd_shadow_reset(void);
void gwsnd_shadow_frame_reset(void);
void gwsnd_shadow_run(int target_mclk);
void gwsnd_shadow_write(unsigned int a, unsigned int v, int target_mclk);
unsigned int gwsnd_shadow_status(void);

/* Debug/analysis tap: called once per frame (before mixing) with the raw
   chip-rate buffers. Used by the host harness to write per-chip WAVs.
   NULL (default) disables it. */
void gwsnd_set_frame_tap(void (*fn)(const int16_t *ym, const int16_t *psg, int samples));

/* --- resampler internals shared between the sync and offload engines --- */
void gwsnd_resample_reset(int is_pal);
/* Mix ym+psg over sample indices [from, to) and stream into the
   resampler, emitting stereo frames through the output callback. */
void gwsnd_resample_mix_feed(int from, int to);

#ifdef __cplusplus
}
#endif

#endif
