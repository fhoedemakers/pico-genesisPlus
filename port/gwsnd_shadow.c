/*
port/gwsnd_shadow.c — core0 shadow of the YM2612 timers and status register.

In offload mode the real chip state lives on core1, but the 68000/Z80 read
the YM2612 status register constantly — SGDK's XGM drivers spin on the
timer A overflow bit to pace PCM at ~13 kHz. Those reads must be answered
immediately and exactly, so core0 keeps this tiny replica of the only
chip state a read can observe: mode/timer registers and the status flags.

Exactness argument: ym2612_run() maps master-clock targets to sample
indices as index = floor(target / 1009), which is path-independent, and
timer A is decremented once per generated sample (INTERNAL_TIMER_A in
ym2612.c). The shadow replays the same per-sample recurrence at the same
event timestamps as the real chip, so its status bits are bit-exact.
Timer B is batched (INTERNAL_TIMER_B(step)); its counter can drift from
the core1 replica when batch boundaries differ (reads are not forwarded
to core1), but timer B state has no audio side effect inside the chip —
only these shadow flags are ever observed by the game.

The host harness proves all of this: GEN_VERIFY_SHADOW=1 runs the shadow
against the real chip on every read and asserts equality.
*/
#include <string.h>
#include "gwenesis_bus.h"
#include "gwsnd.h"

typedef struct {
    unsigned int address; /* 9-bit latched register address */
    unsigned int mode;    /* reg 0x27 */
    int TA;               /* timer A period register (10 bit) */
    int TAL;              /* 1024 - TA */
    int TAC;              /* timer A counter */
    int TB;               /* timer B period register (8 bit) */
    int TBL;              /* (256 - TB) << 4 */
    int TBC;              /* timer B counter */
    unsigned int status;  /* bits 0/1 = timer A/B overflow */
    int clock;            /* master-clock position, like ym2612_clock */
    int index;            /* sample position, like ym2612_index */
} gwsnd_shadow_t;

static gwsnd_shadow_t sh;

void gwsnd_shadow_reset(void)
{
    memset(&sh, 0, sizeof(sh));
    /* Mirror YM2612ResetChip(): TA=0 -> TAL=1024, TB=0 -> TBL=4096,
       set_timers(0x30) -> mode=0x30, status cleared. */
    sh.TAL = 1024;
    sh.TBL = 256 << 4;
    sh.mode = 0x30;
}

void gwsnd_shadow_frame_reset(void)
{
    sh.clock = 0;
    sh.index = 0;
}

void GW_SRAM_FUNC(gwsnd_shadow_run)(int target)
{
    if (sh.clock >= target)
        return;
    int prev = sh.index;
    sh.index += (target - sh.clock) / AUDIO_FREQ_DIVISOR;
    if (sh.index <= prev) {
        sh.index = prev;
        return;
    }
    int n = sh.index - prev;
    sh.clock = sh.index * AUDIO_FREQ_DIVISOR;

    /* Timer A: one tick per sample (mirrors INTERNAL_TIMER_A). */
    if (sh.mode & 0x01) {
        int tac = sh.TAC;
        for (int i = 0; i < n; i++) {
            if (--tac <= 0) {
                if (sh.mode & 0x04)
                    sh.status |= 0x01;
                tac = sh.TAL;
            }
        }
        sh.TAC = tac;
    }

    /* Timer B: one batched step (mirrors INTERNAL_TIMER_B(length)). */
    if (sh.mode & 0x02) {
        sh.TBC -= n;
        if (sh.TBC <= 0) {
            if (sh.mode & 0x08)
                sh.status |= 0x02;
            if (sh.TBL)
                sh.TBC += sh.TBL;
            else
                sh.TBC = sh.TBL;
        }
    }
}

void GW_SRAM_FUNC(gwsnd_shadow_write)(unsigned int a, unsigned int v, int target)
{
    gwsnd_shadow_run(target); /* mirrors the ym2612_run() in YM2612Write */

    v &= 0xff;
    switch (a) {
    case 0: /* address port 0 */
        sh.address = v;
        return;
    case 2: /* address port 1 */
        sh.address = v | 0x100;
        return;
    default: /* data port */
        break;
    }

    /* Only the port-0 mode registers 0x24-0x27 affect timers/status. */
    switch (sh.address) {
    case 0x24:
        sh.TA = (sh.TA & 0x03) | (((int)v) << 2);
        sh.TAL = 1024 - sh.TA;
        break;
    case 0x25:
        sh.TA = (sh.TA & 0x3fc) | (v & 3);
        sh.TAL = 1024 - sh.TA;
        break;
    case 0x26:
        sh.TB = v;
        sh.TBL = (256 - (int)v) << 4;
        break;
    case 0x27:
        /* mirrors set_timers(): reload on 0->1 load edge, clear flags */
        if ((v & 1) && !(sh.mode & 1))
            sh.TAC = sh.TAL;
        if ((v & 2) && !(sh.mode & 2))
            sh.TBC = sh.TBL;
        sh.status &= (~v >> 4);
        sh.mode = v;
        break;
    default:
        break;
    }
}

unsigned int GW_SRAM_FUNC(gwsnd_shadow_status)(void)
{
    return sh.status & 0xff;
}
