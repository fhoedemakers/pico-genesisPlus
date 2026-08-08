/*
port/frame_loop.inc — one emulated frame, shared VERBATIM between the pico
build (main.cpp) and the host harness (hosttest/host_main.c) so the two can
never drift apart. Modeled on the reference loop in
game-and-watch-retro-go/Core/Src/porting/gwenesis/main_gwenesis.c.

Include AFTER the core headers (gwenesis_bus.h, gwenesis_vdp.h, m68k.h,
z80inst.h, ym2612.h, gwenesis_sn76489.h) and port/gwsnd.h.
The including translation unit must be C, or wrap this in extern "C".

Contract:
  - gwenesis_frame_get_config() refreshes screen geometry from the VDP regs;
    call it, place the output buffer (320-wide rows, first visible line at
    the pointer given), then run gwenesis_frame_run().
  - gwenesis_frame_run() executes all scanlines with per-line 68000/Z80
    interleave, renders when draw_frame != 0, raises H/V interrupts, then
    tops up the sound chips via gwsnd_frame_end() and rebases the clocks.
*/

/* Core globals not exported through headers. */
extern unsigned char gwenesis_vdp_regs[];
extern unsigned short gwenesis_vdp_status;
extern int screen_width;
extern int screen_height;
extern int hint_pending;
extern int scan_line;      /* port-owned (port/buffers.c) */
extern int frame_counter;  /* port-owned (port/buffers.c) */
extern int system_clock;   /* port-owned (port/buffers.c) */

static int gwenesis_frame_lines_per_frame = LINES_PER_FRAME_NTSC;
static int gwenesis_frame_is_pal = 0;

/* Refresh geometry from the VDP registers; returns nonzero when PAL. */
static int gwenesis_frame_get_config(void)
{
    gwenesis_frame_is_pal = REG1_PAL ? 1 : 0;
    screen_height = REG1_PAL ? 240 : 224;
    screen_width = REG12_MODE_H40 ? 320 : 256;
    gwenesis_frame_lines_per_frame =
        REG1_PAL ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
    return gwenesis_frame_is_pal;
}

static void gwenesis_frame_run(int draw_frame)
{
    int hint_counter = gwenesis_vdp_regs[10];

    gwenesis_vdp_render_config();

    /* Reset the per-frame clocks (chip clocks are reset by
       gwsnd_frame_end at the end of the previous frame). */
    system_clock = 0;
    zclk = 0;
    scan_line = 0;

    while (scan_line < gwenesis_frame_lines_per_frame) {

        /* CPUs, interleaved per scanline; the sound chips catch up on
           every register access inside these (GWENESIS_AUDIO_ACCURATE). */
        m68k_run(system_clock + VDP_CYCLES_PER_LINE);
        z80_run(system_clock + VDP_CYCLES_PER_LINE);

        /* Both CPUs are done with this line: let the sound engine make
           progress up to its end (no-op in sync mode). */
        gwsnd_line_tick(system_clock + VDP_CYCLES_PER_LINE);

        /* Video */
        if (draw_frame)
            gwenesis_vdp_render_line(scan_line);

        /* On these lines, the H-int line counter is reloaded */
        if ((scan_line == 0) || (scan_line > screen_height))
            hint_counter = REG10_LINE_COUNTER;

        /* interrupt line counter */
        if (--hint_counter < 0) {
            if ((REG0_LINE_INTERRUPT != 0) && (scan_line <= screen_height)) {
                hint_pending = 1;
                if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
                    m68k_update_irq(4);
            }
            hint_counter = REG10_LINE_COUNTER;
        }

        scan_line++;

        /* vblank begins at the end of the last rendered line */
        if (scan_line == screen_height) {
            if (REG1_VBLANK_INTERRUPT != 0) {
                gwenesis_vdp_status |= STATUS_VIRQPENDING;
                m68k_set_irq(6);
            }
            z80_irq_line(1);
        }
        if (scan_line == (screen_height + 1))
            z80_irq_line(0);

        system_clock += VDP_CYCLES_PER_LINE;
    }

    /* Top up both sound chips to the frame boundary, emit the frame's
       audio, and rebase the chip clocks. */
    gwsnd_frame_end(system_clock);

    /* Rebase the 68000 cycle counter to the start of the next frame. */
    m68k.cycles -= system_clock;

    frame_counter++;
}
