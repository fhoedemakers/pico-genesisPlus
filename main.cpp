#include <stdio.h>
#include <algorithm>
#include "pico/stdlib.h"
#include "hardware/divider.h"
#include "hardware/watchdog.h"
#include "util/work_meter.h"
#include "ff.h"
#include "tusb.h"
#include "gamepad.h"
#include "menu.h"
#include "nespad.h"
#include "wiipad.h"
#include "FrensHelpers.h"
#include "settings.h"
#include "FrensFonts.h"
#include "vumeter.h"
#include "menu_settings.h"

/* Gwenesis emulator core (vendored upstream + port patches, see
   gwenesis/PORTING.md) and the port layer around it. */
extern "C"
{
#include "gwenesis_bus.h"
#include "gwenesis_vdp.h"
#include "gwenesis_io.h"
#include "gwenesis_savestate.h"
#include "m68k.h"
#include "z80inst.h"
#include "ym2612.h"
#include "gwenesis_sn76489.h"
#include "gwsnd.h"
#include "buffers.h"
}

bool isFatalError = false;
char *romName;
bool showSettings = false;
static uint64_t start_tick_us = 0;
static uint64_t fps = 0;
static int fpsFrameCount = 0;
static char fpsString[4] = "000";
#if HSTX
#define fpsfgcolor 0      // black (RGB555)
#define fpsbgcolor 0x7FFF // white (RGB555)
#else
#define fpsfgcolor 0     // black (RGB444)
#define fpsbgcolor 0xFFF // white (RGB444)
#endif

#define MARGINTOP 0
#define MARGINBOTTOM 0

#define FPSSTART (((MARGINTOP + 7) / 8) * 8)
#define FPSEND ((FPSSTART) + 8)

static bool reset = false;
static bool resetGame = false;

extern "C" unsigned char button_state[3];

static unsigned int drawFrame = 1;
static int frame = 0;
static bool limit_fps = true;
static uint64_t next_frame_time = 0; // next deadline for the FPS limiter
int audio_enabled = 1;
bool toggleDebugFPS = false;
#if WII_PIN_SDA >= 0 and WII_PIN_SCL >= 0
// Cached Wii pad state updated once per frame in ProcessAfterFrameIsRendered()
static uint16_t wiipad_raw_cached = 0;
#endif
#define AUDIOBUFFERSIZE (1024 * 4)
/* Core clock experiment log (values in kHz):
 * Stable (video + USB OK):
 *   252000 : Baseline; allows exact 60.00 Hz PicoDVI timing.
 *   266000 : Stable, slight refresh deviation.
 *   280000 : Stable.
 *   294000 : Stable.
 *   308000 : Stable.
 *   324000 : Stable; chosen for PicoDVI build (≈77.2 Hz observed refresh).
 * Unstable / rejected:
 *   322000 : PLL cannot lock exactly (SDK panic: “System clock ... cannot be exactly achieved”).
 *   325000 : Same PLL precision failure.
 *   326000 : Same PLL precision failure.
 *   350000 : Same PLL precision failure.
 * Not supported by specific display (Samsung TV):
 *   328000, 330000, 340000 : TMDS mode not accepted (PicoDVI); HSTX path OK at 340000.
 * Untested / partial:
 *   360000 : Listed; no result logged.
 * HSTX high overclocks:
 *   378000 : Works with HSTX (stable video); supports exact 126 MHz pixel clock for 60.00 Hz.
 * Notes:
 * - “Not supported signal” indicates monitor rejected generated video timing.
 * - “Panic” entries are from clock config failing to derive an exact integer divider chain.
 * - Selected EMULATOR_CLOCKFREQ_KHZ below depends on PicoDVI vs HSTX build.
 */
#if !HSTX // Using PicoDVI
/* PicoDVI timing note:
 * For a true 60.00 Hz output the RP2350 system clock must be exactly 252 MHz.
 * Any deviation changes the derived TMDS / pixel clock and shifts the display refresh rate.
 *
 * We overclock to 324 MHz for acceptable emulator performance; at this frequency
 * the observed monitor refresh becomes ~77.2 Hz instead of 60 Hz. Most displays
 * tolerate this higher rate, but some may reject the signal.
 *
 * The current PicoDVI implementation cannot produce an exact 60 Hz mode at arbitrary
 * higher core clocks because we lack a suitable integer divisor chain for the pixel clock.
 * See: https://github.com/Wren6991/PicoDVI/issues/56
 */
#define EMULATOR_CLOCKFREQ_KHZ 324000 // Overclock frequency in kHz when using Emulator
#define VOLTAGE VREG_VOLTAGE_1_30
#else
/* HSTX overclock notes:
 * Tested core clocks:
 *   340000 kHz: Video OK, TinyUSB unstable (PIO USB still works)
 *   378000 kHz: Stable; allows exact 126 MHz HSTX pixel clock for 60.00 Hz output
 *
 * At both tested values the HSTX (serial video) clock can be tuned so display refresh stays 60 Hz.
 * Debug quirk: At 378 MHz a hard fault (signal trap) may occur when starting a debug session.
 * Mitigation: Enter BOOTSEL mode before attaching the debugger at 378 MHz.
 *
 * Voltage: 1.50 V, matching the FlashParams overclock limits
 * (pico_shared/FlashParams.cpp pairs 378000 kHz with VREG_VOLTAGE_1_50).
 */
#define EMULATOR_CLOCKFREQ_KHZ 378000 //  Overclock frequency in kHz when using HSTX
                                      // May cause artifacts on some screens, 336000 seems stable
                                      // https://github.com/fhoedemakers/retroJam/issues/7
#define VOLTAGE VREG_VOLTAGE_1_50
#endif
static uint32_t CPUFreqKHz = EMULATOR_CLOCKFREQ_KHZ;

// Visibility configuration for options menu
// 1 = show option line, 0 = hide.
// Order must match enum in menu_settings.h
const int8_t g_settings_visibility_md[MOPT_COUNT] = {
    0,                               // Exit Game, or back to menu. Always visible when in-game.
    0,                               // Reset Game
    BOOTLOADER_BUILD,                // Return to emuLoader picker (only when built for the loader)
    0,                               // Save / Restore State
    1,                               // Screen Mode
    0,                               // Scanlines toggle (superseded by Screen Mode)
    HSTX,                            // Scanline Type (HSTX only)
    1,                               // FPS Overlay
    0,                               // Audio Enable
    0,                               // Frame Skip
    HSTX && ENABLEDVI,               // Display Mode (HDMI or DVI, only when HSTX is enabled, because non-HSTX builds always use HDMI)
    (EXT_AUDIO_IS_ENABLED),          // External Audio
    1,                               // Font Color
    1,                               // Font Back Color
    ENABLE_VU_METER,                 // VU Meter
    (HW_CONFIG == 8),                // Fruit Jam Volume Control
    0,                               // DMG Palette
    0,                               // Border Mode
    0,                               // Rapid Fire on A
    0,                               // Rapid Fire on B
    0,                               // Auto Insert Disk A
    0,                               // Auto Swap FDS
    0,                               // FDS Disk Swap
    0,                               // Overclock (CPU high clock toggle)
    0,                               // YM Audio (SMS only)
    1,                               // Enter bootsel mode
    1,                               // Controller Test
};
const uint8_t g_available_screen_modes_md[] = {
    0, // SCANLINE_8_7,
    0, // NOSCANLINE_8_7
    1, // SCANLINE_1_1,
    1  // NOSCANLINE_1_1
};

int ProcessAfterFrameIsRendered()
{
    Frens::pollHeadPhoneJack();
#if NES_PIN_CLK != -1
    nespad_read_start();
#endif
    auto count =
#if !HSTX
        dvi_->getFrameCounter();
#else
        hstx_getframecounter();
#endif
    auto onOff = hw_divider_s32_quotient_inlined(count, 60) & 1;
    Frens::blinkLed(onOff);
#if NES_PIN_CLK != -1
    nespad_read_finish();
#endif
    tuh_task();
#if WII_PIN_SDA >= 0 and WII_PIN_SCL >= 0
    // Poll Wii pad once per frame (function called once per rendered frame)
    wiipad_raw_cached = wiipad_read();
#endif
#if ENABLE_VU_METER
    if (isVUMeterToggleButtonPressed())
    {
        settings.flags.enableVUMeter = !settings.flags.enableVUMeter;
        turnOffAllLeds();
    }
#endif
    if (showSettings)
    {
        showSettings = false;
        FrensSettings::savesettings();
        abSwapped = 1;
        int rval = showSettingsMenu(true);
        abSwapped = 0;
        if (rval == 3)
        {
            reset = true;
        }
        if (rval == 5)
        {
            reset = true;
            resetGame = true;
        }
        audio_enabled = settings.flags.audioEnabled;
        // Reset next frame time for FPS limiter
        next_frame_time = 0;
    }
    return count;
}

static DWORD prevButtons[2]{};

static int rapidFireMask[2]{};
static int rapidFireCounter = 0;

static constexpr int LEFT = 1 << 6;
static constexpr int RIGHT = 1 << 7;
static constexpr int UP = 1 << 4;
static constexpr int DOWN = 1 << 5;
static constexpr int SELECT = 1 << 2;
static constexpr int START = 1 << 3;
static constexpr int A = 1 << 0;
static constexpr int B = 1 << 1;
static constexpr int C = 1 << 8;

void toggleScreenMode()
{
#if !HSTX
    if (settings.screenMode == ScreenMode::SCANLINE_1_1)
    {
        settings.screenMode = ScreenMode::NOSCANLINE_1_1;
    }
    else
    {
        settings.screenMode = ScreenMode::SCANLINE_1_1;
    }
    Frens::applyScreenMode(settings.screenMode);
#else
    Frens::toggleScanLines();
#endif
}

static inline int mapWiipadButtons(uint16_t buttonData)
{
    int mapped = 0;
    // swap A and B buttons
    if (buttonData & A)
    {
        mapped |= B;
    }
    if (buttonData & B)
    {
        mapped |= A;
    }
    if (buttonData & SELECT)
    {
        mapped |= SELECT;
    }
    if (buttonData & START)
    {
        mapped |= START;
    }
    if (buttonData & UP)
    {
        mapped |= UP;
    }
    if (buttonData & DOWN)
    {
        mapped |= DOWN;
    }
    if (buttonData & LEFT)
    {
        mapped |= LEFT;
    }
    if (buttonData & RIGHT)
    {
        mapped |= RIGHT;
    }
    if (buttonData & C)
    {
        mapped |= C;
    }
    return mapped;
}

/* Core callback: refresh button_state[] (active low, S A C B R L D U). */
extern "C" void gwenesis_io_get_buttons()
{
    char timebuf[10];
    bool usbConnected = false;
    for (int i = 0; i < 2; i++)
    {
        auto &gp = io::getCurrentGamePadState(i);
        if (i == 0)
        {
            usbConnected = gp.isConnected();
        }
        int v = (gp.buttons & io::GamePadState::Button::LEFT ? LEFT : 0) |
                (gp.buttons & io::GamePadState::Button::RIGHT ? RIGHT : 0) |
                (gp.buttons & io::GamePadState::Button::UP ? UP : 0) |
                (gp.buttons & io::GamePadState::Button::DOWN ? DOWN : 0) |
                (gp.buttons & io::GamePadState::Button::A ? A : 0) |
                (gp.buttons & io::GamePadState::Button::B ? B : 0) |
                (gp.buttons & io::GamePadState::Button::X ? C : 0) | // X button maps to C button on non-genesis controllers
                (gp.buttons & io::GamePadState::Button::C ? C : 0) |
                (gp.buttons & io::GamePadState::Button::SELECT ? SELECT : 0) |
                (gp.buttons & io::GamePadState::Button::START ? START : 0) |
                0;

#if NES_PIN_CLK != -1
        // When USB controller is connected both NES ports act as controller 2
        if (usbConnected)
        {
            if (i == 1)
            {
                v = v | nespad_states[1] | nespad_states[0];
            }
        }
        else
        {
            v |= nespad_states[i];
        }
#endif
// When USB controller is connected  wiipad acts as controller 2
#if WII_PIN_SDA >= 0 and WII_PIN_SCL >= 0
        if (usbConnected)
        {
            if (i == 1)
            {
                v |= mapWiipadButtons(wiipad_raw_cached);
            }
        }
        else
        {
            if (i == 0)
            {
                v |= mapWiipadButtons(wiipad_raw_cached);
            }
        }
#endif

        int rv = v;
        if (rapidFireCounter & 2)
        {
            // 15 fire/sec
            rv &= ~rapidFireMask[i];
        }

        auto p1 = v;

        auto pushed = v & ~prevButtons[i];
        if (p1 & SELECT)
        {
            if (pushed & START)
            {
                showSettings = true;
            }
            else if (pushed & UP)
            {
                toggleScreenMode();
            }
            else if (pushed & DOWN)
            {
                toggleDebugFPS = !toggleDebugFPS;
                Frens::ms_to_d_hhmmss(Frens::time_ms(), timebuf, sizeof timebuf);
                printf("Uptime %s, Debug FPS %s\n", timebuf, toggleDebugFPS ? "ON" : "OFF");
            }
            else if (pushed & LEFT)
            {
                // Toggle audio output, ignore if HSTX is enabled, because HSTX must use external audio
#if EXT_AUDIO_IS_ENABLED && !HSTX
                settings.flags.useExtAudio = !settings.flags.useExtAudio;
                if (settings.flags.useExtAudio)
                {
                    printf("Using I2S Audio\n");
                }
                else
                {
                    printf("Using DVIAudio\n");
                }
#else
                settings.flags.useExtAudio = 0;
#endif
            }
#if ENABLE_VU_METER
            else if (pushed & RIGHT)
            {
                settings.flags.enableVUMeter = !settings.flags.enableVUMeter;
                turnOffAllLeds();
            }
#endif
        }
        if (p1 & START)
        {
            // Toggle frame rate display
            if (pushed & A)
            {
                settings.flags.displayFrameRate = !settings.flags.displayFrameRate;
                printf("FPS: %s\n", settings.flags.displayFrameRate ? "ON" : "OFF");
            }
            else if (pushed & LEFT)
            {
#if HW_CONFIG == 8
                settings.fruitjamVolumeLevel = std::max(-63, settings.fruitjamVolumeLevel - 1);
                EXT_AUDIO_SETVOLUME(settings.fruitjamVolumeLevel);
#endif
            }
            else if (pushed & RIGHT)
            {
#if HW_CONFIG == 8
                settings.fruitjamVolumeLevel = std::min(23, settings.fruitjamVolumeLevel + 1);
                EXT_AUDIO_SETVOLUME(settings.fruitjamVolumeLevel);
#endif
            }
        }
        prevButtons[i] = v;
        button_state[i] = ((v & LEFT) ? 1 << PAD_LEFT : 0) |
                          ((v & RIGHT) ? 1 << PAD_RIGHT : 0) |
                          ((v & UP) ? 1 << PAD_UP : 0) |
                          ((v & DOWN) ? 1 << PAD_DOWN : 0) |
                          ((v & START) ? 1 << PAD_S : 0) |
                          ((v & A) ? 1 << PAD_A : 0) |
                          ((v & B) ? 1 << PAD_B : 0) |
                          ((v & C) ? 1 << PAD_C : 0);
        button_state[i] = ~button_state[i];
    }
}

/* ------------------------------------------------------------------ */
/* Audio sinks: the gwsnd resampler delivers 44.1 kHz stereo samples   */
/* through one of these, chosen per frame.                             */
/* ------------------------------------------------------------------ */

static bool i2sActive = false;

static void audioOutI2S(int16_t l, int16_t r)
{
    EXT_AUDIO_ENQUEUE_SAMPLE(l, r);
#if ENABLE_VU_METER
    if (settings.flags.enableVUMeter)
    {
        addSampleToVUMeter(l);
    }
#endif
}

#if HSTX
/* HSTX builds run the resampler on core1 (offload). HDMI audio may be
   pushed from core1 (the DI ring's consumer is the core1 DMA IRQ), but
   I2S and the VU meter must stay on core0 — those samples go through the
   gwsnd bridge, drained once per frame in the emulate loop. */
static void __not_in_flash_func(audioOutHstx)(int16_t l, int16_t r)
{
    hstx_push_audio_sample(l, r);
#if ENABLE_VU_METER
    if (settings.flags.enableVUMeter)
    {
        gwsnd_bridge_push(l, r); /* VU fed on core0 via the bridge */
    }
#endif
}

static void bridgeVUOnly(int16_t l, int16_t r)
{
    (void)r;
#if ENABLE_VU_METER
    if (settings.flags.enableVUMeter)
    {
        addSampleToVUMeter(l);
    }
#else
    (void)l;
#endif
}
#else
static void audioOutDVI(int16_t l, int16_t r)
{
    auto &ring = dvi_->getAudioRingBuffer();
    if (ring.getWritableSize() < 1)
    {
        return;
    }
    auto p = ring.getWritePointer();
    *p = {l, r};
    ring.advanceWritePointer(1);
}
#endif

/* Sink backlog in permille of its target (1000 = on target), used by the
   resampler's drift trim. In offload mode this runs on CORE1 — it only
   reads volatile counters. */
static int sinkFillPermille()
{
#if EXT_AUDIO_IS_ENABLED
    if (i2sActive)
    {
        return audio_i2s_get_fill_permille() * 2; /* target: ring half full */
    }
#endif
#if HSTX
    return (int)(hstx_di_queue_get_level() * 1000 / 64); /* target: 64 of 256 packets */
#else
    auto &ring = dvi_->getAudioRingBuffer();
    int fill = AUDIOBUFFERSIZE - (int)ring.getWritableSize();
    return fill * 1000 / (AUDIOBUFFERSIZE / 2); /* target: ring half full */
#endif
}

static void selectAudioSink()
{
    if (!audio_enabled)
    {
        i2sActive = false;
        gwsnd_set_output(nullptr);
        return;
    }
#if EXT_AUDIO_IS_ENABLED
    if (settings.flags.useExtAudio == 1 || Frens::isHeadPhoneJackConnected())
    {
        i2sActive = true;
#if HSTX
        /* resampler runs on core1: hand samples to core0 via the bridge */
        gwsnd_set_output(gwsnd_bridge_push);
        gwsnd_set_bridge_sink(audioOutI2S);
#else
        gwsnd_set_output(audioOutI2S);
#endif
        return;
    }
#endif
    i2sActive = false;
#if HSTX
    gwsnd_set_output(audioOutHstx);
    gwsnd_set_bridge_sink(bridgeVUOnly);
#else
    gwsnd_set_output(audioOutDVI);
#endif
}

/* ------------------------------------------------------------------ */
/* Frame loop: shared verbatim with the host harness.                  */
/* ------------------------------------------------------------------ */
extern "C"
{
#include "frame_loop.inc"
}

static inline uint16_t *framebufferLine(int line)
{
#if HSTX
    return hstx_getlineFromFramebuffer(line);
#else
    return &Frens::framebuffer[line * SCREENWIDTH];
#endif
}

/* FPS overlay: drawn straight into the framebuffer after the frame. */
static void drawFpsOverlay()
{
    for (int line = FPSSTART; line < FPSEND; line++)
    {
        uint16_t *fpsBuffer = framebufferLine(line) + 5;
        int rowInChar = line % 8;
        for (auto i = 0; i < 3; i++)
        {
            char fontSlice = getcharslicefrom8x8font(fpsString[i], rowInChar);
            for (auto bit = 0; bit < 8; bit++)
            {
                *fpsBuffer++ = (fontSlice & 1) ? fpsfgcolor : fpsbgcolor;
                fontSlice >>= 1;
            }
        }
    }
}

void __not_in_flash_func(emulate)()
{
    bool firstLoop = true;
    int old_screen_width = 0;
    int old_screen_height = 0;
    char tbuf[32];

    /* Perf diagnostics, printed once per second while the SELECT+DOWN
       debug toggle is on: core work time per frame (emulation vs whole
       pre-pacing loop), sound FIFO high-water/drops, DI queue level. */
    uint32_t dbgEmuSum = 0, dbgEmuMax = 0, dbgTotSum = 0, dbgTotMax = 0, dbgFrames = 0;
    uint64_t dbgLastPrint = time_us_64();

    while (!reset)
    {
        uint64_t t_frame0 = time_us_64();
        int is_pal = gwenesis_frame_get_config();
        gwsnd_set_pal(is_pal);
        if (firstLoop || old_screen_height != screen_height || old_screen_width != screen_width)
        {
            printf("Uptime %s, is_pal %d, screen_width: %d, screen_height: %d, audio_enabled: %d, frameskip: %d\n",
                   Frens::ms_to_d_hhmmss(Frens::time_ms(), tbuf, sizeof tbuf), is_pal, screen_width, screen_height,
                   settings.flags.audioEnabled, settings.flags.frameSkip);
            firstLoop = false;
            old_screen_height = screen_height;
            old_screen_width = screen_width;
        }

        /* Vertical centering: the framebuffer is 320x240, NTSC games are
           224 lines. The core renders directly into the framebuffer. */
        int margin = (SCREENHEIGHT - screen_height) / 2;
        if (margin < 0)
        {
            margin = 0;
        }
        gwenesis_vdp_set_buffer(framebufferLine(margin));

        /* Frameskip: render every frame unless enabled (then skip 1 of 3). */
        drawFrame = !settings.flags.frameSkip || (frame % 3 != 0);

        selectAudioSink();

        uint64_t t_emu0 = time_us_64();
        gwenesis_frame_run(drawFrame);
        uint32_t emu_us = (uint32_t)(time_us_64() - t_emu0);

        if (drawFrame && settings.flags.displayFrameRate)
        {
            drawFpsOverlay();
        }

#if HSTX
        /* Final sweep of the core1->core0 sample bridge (I2S / VU); the
           bulk is drained incrementally per scanline by gwsnd_line_tick. */
        gwsnd_bridge_drain();
#endif

        ProcessAfterFrameIsRendered();
        frame++;
        rapidFireCounter++;

        /* Heap-corruption watch: report the first frame in which any
           emulator buffer's guard word is clobbered, then stop checking
           so the log stays readable. */
        static bool guardReported = false;
        if (!guardReported && check_emulator_mem("in-game") > 0)
        {
            printf("  (frame %d, scan_line %d, screen %dx%d)\n", frame, scan_line,
                   screen_width, screen_height);
            guardReported = true;
        }

        uint32_t tot_us = (uint32_t)(time_us_64() - t_frame0);
        dbgEmuSum += emu_us;
        dbgTotSum += tot_us;
        if (emu_us > dbgEmuMax)
            dbgEmuMax = emu_us;
        if (tot_us > dbgTotMax)
            dbgTotMax = tot_us;
        dbgFrames++;
        if (toggleDebugFPS && (time_us_64() - dbgLastPrint) >= 1000000)
        {
            printf("perf: frames=%u emu avg=%u max=%u us, loop avg=%u max=%u us",
                   dbgFrames, dbgEmuSum / dbgFrames, dbgEmuMax,
                   dbgTotSum / dbgFrames, dbgTotMax);
#if GWSND_OFFLOAD
            printf(", fifo_hw=%u drops=%u", gwsnd_stats_fifo_highwater(),
                   gwsnd_stats_drops());
#endif
#if HSTX
            printf(", di=%u resync=%d", (unsigned)hstx_di_queue_get_level(),
                   get_video_output_resync_count());
#endif
            printf("\n");
            dbgEmuSum = dbgEmuMax = dbgTotSum = dbgTotMax = dbgFrames = 0;
            dbgLastPrint = time_us_64();
        }
        else if (!toggleDebugFPS && dbgFrames >= 600)
        {
            /* keep the accumulators fresh so enabling the toggle shows
               recent numbers, not an average since game start */
            dbgEmuSum = dbgEmuMax = dbgTotSum = dbgTotMax = dbgFrames = 0;
            dbgLastPrint = time_us_64();
        }

        if (limit_fps)
        {
            const uint64_t frame_period = is_pal ? 20000 : 16667; // 50Hz or 60Hz
            const uint64_t now = time_us_64();

            // Initialize first deadline
            if (next_frame_time == 0)
                next_frame_time = now + frame_period;

            // Wait if ahead of schedule
            if (now < next_frame_time)
            {
                uint64_t remaining = next_frame_time - now;
                if (remaining > 150)
                    sleep_us(remaining - 150);
                while (time_us_64() < next_frame_time)
                {
                    tight_loop_contents();
                }
                // Advance by exactly one frame period
                next_frame_time += frame_period;
            }
            else
            {
                // Late: advance until the deadline is in the future (catch up without drifting)
                do
                {
                    next_frame_time += frame_period;
                } while (now >= next_frame_time);
            }
        }

        // calculate framerate
        if (settings.flags.displayFrameRate)
        {
            fpsFrameCount++;
            uint64_t tick_us = Frens::time_us() - start_tick_us;
            if (tick_us > 1000000)
            {
                fps = fpsFrameCount;
                start_tick_us = Frens::time_us();
                fpsFrameCount = 0;
            }
            fpsString[0] = '0' + (fps / 100) % 10;
            fpsString[1] = '0' + (fps / 10) % 10;
            fpsString[2] = '0' + (fps % 10);
        }
    }
}

/* Size of the currently selected ROM file (the core needs it to build its
   address mask; pico_shared only exports the data pointer). */
static size_t getSelectedRomSize(const char *path)
{
    FILINFO fno;
    if (f_stat(path, &fno) == FR_OK)
    {
        return (size_t)fno.fsize;
    }
    printf("f_stat(%s) failed, assuming 4MB rom\n", path);
    return 4 * 1024 * 1024;
}

/// @brief
/// Start emulator.
/// @return
int main()
{
#if !defined(PICO_RP2350)
#error "This code is for RP2350 only"
#endif
    char selectedRom[FF_MAX_LFN];
    romName = selectedRom;
    ErrorMessage[0] = selectedRom[0] = 0;

    Frens::setClocksAndStartStdio(CPUFreqKHz, VOLTAGE);

    printf("==========================================================================================\n");
    printf("Pico-Genesis+ %s\n", SWVERSION);
    printf("Build date: %s\n", __DATE__);
    printf("Build time: %s\n", __TIME__);
    printf("CPU freq: %d kHz\n", clock_get_hz(clk_sys) / 1000);
#if HSTX
    printf("HSTX freq: %d kHz\n", clock_get_hz(clk_hstx) / 1000);
#endif
    printf("Stack size: %d bytes\n", PICO_STACK_SIZE);
    printf("==========================================================================================\n");
    printf("Starting up...\n");
    FrensSettings::initSettings(FrensSettings::emulators::GENESIS);
    isFatalError = !Frens::initAll(selectedRom, CPUFreqKHz, MARGINTOP, MARGINBOTTOM, AUDIOBUFFERSIZE, true, true);
#if !HSTX
    scaleMode8_7_ = Frens::applyScreenMode(settings.screenMode);
#endif
    bool showSplash = true;
    g_settings_visibility = g_settings_visibility_md;
    g_available_screen_modes = g_available_screen_modes_md;
    gwsnd_set_fill_query(sinkFillPermille);
    while (true)
    {
        if (strlen(selectedRom) == 0 || reset == true)
        {
            menu("Pico-Genesis+", ErrorMessage, isFatalError, showSplash, ".md .bin", selectedRom);
        }
#if !HSTX
        if (settings.screenMode != ScreenMode::SCANLINE_1_1 && settings.screenMode != ScreenMode::NOSCANLINE_1_1)
        {
            settings.screenMode = ScreenMode::SCANLINE_1_1;
            FrensSettings::savesettings();
        }
        scaleMode8_7_ = Frens::applyScreenMode(settings.screenMode);
#endif

        audio_enabled = settings.flags.audioEnabled;
        size_t romSize = getSelectedRomSize(selectedRom);

        do
        {
            abSwapped = 0; // don't swap A and B buttons
            reset = resetGame = false;
            next_frame_time = 0; // Reset next frame time for FPS limiter
            printf("Starting game (%d KB rom) rom@%p\n", (int)(romSize / 1024),
                   (void *)ROM_FILE_ADDR);
            if (!init_emulator_mem())
            {
                snprintf(ErrorMessage, 40, "Out of memory starting game");
                printf("%s\n", ErrorMessage);
                reset = true;
                break;
            }
            load_cartridge((const unsigned char *)ROM_FILE_ADDR, romSize);
            power_on();
            reset_emulation();
            Frens::dumpHeapStats("game start"); /* peak usage, both heaps */
            gwsnd_init(0 /* pal detected per frame */, HSTX);
            emulate();
            gwsnd_shutdown();
            free_emulator_mem();
        } while (resetGame);

        /* Release the ROM before returning to the menu. Holding it while
           the menu allocates (RomLister, artwork) leaves those blocks
           sitting above it in PSRAM, so freeing it later — inside
           loadRomInPsRam, immediately before allocating the next one —
           can leave no contiguous room for a larger ROM and f_malloc
           panics. Freeing here keeps the arena defragmented across games.
           Only valid with PSRAM: without it ROM_FILE_ADDR points into XIP
           flash, which must never be passed to free(). */
        if (Frens::isPsramEnabled() && ROM_FILE_ADDR)
        {
            Frens::f_free((void *)ROM_FILE_ADDR);
            ROM_FILE_ADDR = 0;
        }
        selectedRom[0] = 0;
        showSplash = false;
    }

    return 0;
}
