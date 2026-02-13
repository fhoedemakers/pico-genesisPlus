#include <stdio.h>
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
/* Gwenesis Emulator */
extern "C"
{
#include "gwenesis/buffers.h"
#include "gwenesis/cpus/M68K/m68k.h"
#include "gwenesis/sound/z80inst.h"
#include "gwenesis/bus/gwenesis_bus.h"
#include "gwenesis/io/gwenesis_io.h"
#include "gwenesis/vdp/gwenesis_vdp.h"
#include "gwenesis/savestate/gwenesis_savestate.h"
#include "gwenesis/sound/gwenesis_sn76489.h"
#include "gwenesis/sound/ym2612.h"
}

bool isFatalError = false;
static FATFS fs;
char *romName;
bool showSettings = false;
static uint64_t start_tick_us = 0;
static uint64_t fps = 0;
static char fpsString[4] = "000";
#define fpsfgcolor 0     // black
#define fpsbgcolor 0xFFF // white

#define MARGINTOP 0
#define MARGINBOTTOM 0

#define FPSSTART (((MARGINTOP + 7) / 8) * 8)
#define FPSEND ((FPSSTART) + 8)

bool reset = false;
bool reboot = false;

extern unsigned short button_state[3];
// uint16_t __scratch_y("gen_palette1") palette444_1[64];
// uint16_t __scratch_y("gen_palette2") palette444_2[64];
uint16_t palette[64]; // = palette444_1;

/* Clocks and synchronization */
/* system clock is video clock */
int system_clock;
unsigned int lines_per_frame = LINES_PER_FRAME_NTSC; // 262; /* NTSC: 262, PAL: 313 */
int scan_line;
unsigned int frame_counter = 0;
unsigned int drawFrame = 1;
int z80_enable_mode = 2;
bool interlace = true; // was true
int frame = 0;
int frame_cnt = 0;
int frame_timer_start = 0;
bool limit_fps = true; // was true
 
static uint64_t next_frame_time = 0;  // Used to track next frame time for limiting FPS
int audio_enabled = 1; // Set to 1 to enable audio. Now disabled because its is not properly working.
// bool frameskip = audio_enabled; // was true
bool sn76489_enabled = true;
uint8_t snd_accurate = 1;

extern unsigned char gwenesis_vdp_regs[0x20];
extern unsigned int gwenesis_vdp_status;
extern unsigned int screen_width, screen_height;
extern int hint_pending;

int sn76489_index; /* sn78649 audio buffer index */
int sn76489_clock;
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
 */
#define EMULATOR_CLOCKFREQ_KHZ 378000 //  Overclock frequency in kHz when using HSTX
                                      // May cause artifacts on some screens, 336000 seems stable 
                                      // https://github.com/fhoedemakers/retroJam/issues/7
#define VOLTAGE VREG_VOLTAGE_1_60
#endif
// https://github.com/orgs/micropython/discussions/15722
static uint32_t CPUFreqKHz = EMULATOR_CLOCKFREQ_KHZ; // 340000; //266000;
// Visibility configuration for options menu (NES specific)
// 1 = show option line, 0 = hide.
// Order must match enum in menu_options.h
const int8_t g_settings_visibility_md[MOPT_COUNT] = {
    0,                               // Exit Game, or back to menu. Always visible when in-game.
    -1,                              // No save states/restore states for Genesis
    !HSTX,                           // Screen Mode (only when not HSTX)
    HSTX,                            // Scanlines toggle (only when HSTX)
    1,                               // FPS Overlay
    1,                               // Audio Enable
    1,                               // Frame Skip
    (EXT_AUDIO_IS_ENABLED), // External Audio
    1,                               // Font Color
    1,                               // Font Back Color
    ENABLE_VU_METER,                 // VU Meter
    (HW_CONFIG == 8),                // Fruit Jam Internal Speaker
    (HW_CONFIG == 8),                // Fruit Jam Volume Control
    0,                               // DMG Palette (Genesis emulator does not use GameBoy palettes)
    0,                               // Border Mode (Super Gameboy style borders not applicable for Genesis)
    0,                               // Rapid Fire on A (not applicable)
    0,                               // Rapid Fire on B (not applicable)
    1                                // Enter bootsel mode

};
const uint8_t g_available_screen_modes_md[] = {
    0, // SCANLINE_8_7,
    0, // NOSCANLINE_8_7
    1, // SCANLINE_1_1,
    1  // NOSCANLINE_1_1
};
#if 0
int sampleIndex = 0;
void __not_in_flash_func(processaudio)(int line)
{
    constexpr int samplesPerLine = ((GWENESIS_AUDIO_BUFFER_LENGTH_NTSC + SCREENHEIGHT - 1) / SCREENHEIGHT); // 735/192 = 3.828125 192*4=768 735/3=245
    if (line == 0)
    {
        sampleIndex = 0;
    }
    if (sampleIndex >= (GWENESIS_AUDIO_BUFFER_LENGTH_NTSC << 1))
    {
        return;
    }
    // rounded up sample rate per scanline
    int samples = samplesPerLine;
    // printf("line %d, SampleIndex: %d, Samples: %d\n", line,  sampleIndex    , samples);
    // short *p1 = gwenesis_sn76489_buffer + sampleIndex; // snd.buffer[0] + sampleIndex;
    // short *p2 = gwenesis_sn76489_buffer + sampleIndex + 1; // snd.buffer[1] + sampleIndex;

    //  = (gwenesis_sn76489_buffer[sampleIndex / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR]);
    while (samples)
    {
        auto &ring = dvi_->getAudioRingBuffer();
        auto n = std::min<int>(samples, ring.getWritableSize());
        // printf("\tSamples: %d, n: %d,\n", samples, n);
        if (!n)
        {
            // printf("Line %d, Audio buffer overrun\n", line);
            return;
        }
        auto p = ring.getWritePointer();
        int ct = n;
        // printf("\tSamples: %d, n: %d, ct: %d\n", samples, n, ct);
        while (ct--)
        {
            int l = gwenesis_sn76489_buffer[sampleIndex / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR];
            int r = gwenesis_sn76489_buffer[(sampleIndex + 1) / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR];
            // p1 += 2;
            // p2 += 2;
            *p++ = {static_cast<short>(l), static_cast<short>(r)};
            sampleIndex += 2;
            // printf("\tSamples: %d, n: %d, ct: %d\n", samples, n, ct);
        }
        ring.advanceWritePointer(n);
        samples -= n;
    }
}
#endif

int ProcessAfterFrameIsRendered()
{
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
        //FrensSettings::savesettings();
        // printf("VU Meter %s\n", settings.flags.enableVUMeter ? "enabled" : "disabled");
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
            reboot = true;
        }
        // audio_enabled may be changed from settings menu, Genesis specific
        audio_enabled = settings.flags.audioEnabled;
        // Reset next frame time for FPS limiter
        next_frame_time = 0;
    }
    return count;
}

static DWORD prevButtons[2]{};
static DWORD prevButtonssystem[2]{};

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
    //FrensSettings::savesettings();
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
void gwenesis_io_get_buttons()
{
    char timebuf[10];
    bool usbConnected = false;
    for (int i = 0; i < 2; i++)
    {
        // auto &dst = (i == 0) ? *pdwPad1 : *pdwPad2;
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

        // dst = rv;

        auto p1 = v;

        auto pushed = v & ~prevButtons[i];
        if (p1 & SELECT)
        {
            if (pushed & START)
            {
                // reboot = true;
                // printf("Reset pressed\n");
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
                //FrensSettings::savesettings();
            }
            // else if (pushed & RIGHT)
            // {
            //     settings.flags.audioEnabled = !settings.flags.audioEnabled;
            //     audio_enabled = settings.flags.audioEnabled;  // Needed to keep external audio_enabled variable in sync
            //     //audio_enabled = !audio_enabled;
            //     //frameskip = audio_enabled;
            //     printf("Audio %s, Frameskip %s\n", settings.flags.audioEnabled ? "enabled" : "disabled", settings.flags.frameSkip ? "enabled" : "disabled");
            //     FrensSettings::savesettings();
            // }
#if ENABLE_VU_METER
            else if (pushed & RIGHT)
            {
                settings.flags.enableVUMeter = !settings.flags.enableVUMeter;
                //FrensSettings::savesettings();
                // printf("VU Meter %s\n", settings.flags.enableVUMeter ? "enabled" : "disabled");
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
                //FrensSettings::savesettings();
            } else if (pushed & LEFT) {
#if HW_CONFIG == 8
               settings.fruitjamVolumeLevel = std::max(-63, settings.fruitjamVolumeLevel - 1);
               EXT_AUDIO_SETVOLUME(settings.fruitjamVolumeLevel);
#endif
            } else if (pushed & RIGHT) {
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
        // ((v & SELECT) ? 1 << PAD_C : 0);
        button_state[i] = ~button_state[i];
    }
}

const uint8_t __not_in_flash_func(paletteBrightness)[] = {0, 52, 87, 116, 144, 172, 206, 255};

extern "C" void genesis_set_palette(const uint8_t index, const uint32_t color)
{
    uint8_t b = paletteBrightness[(color >> 9) & 0x0F];
    uint8_t g = paletteBrightness[(color >> 5) & 0x0F];
    uint8_t r = paletteBrightness[(color >> 1) & 0x0F];
#if !HSTX
    // RGB444
    palette[index] = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
#else
    // RGB555
    palette[index] = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
#endif
}
#if 0
uint8_t maxkol = 0;
void __not_in_flash_func(processEmulatorScanLine)(int line, uint8_t *framebuffer, uint16_t *dvibuffer)
{
    // processaudio(line);
    if (line < 224)
    {
        auto current_line = &framebuffer[line * SCREENWIDTH];
        int srcIndex = 0;
        // screen_width is resolution from the emulator
        if (screen_width < SCREENWIDTH)
        {
            memset(dvibuffer, 0, 32 * 2);
        }
        for (int kol = (screen_width == SCREENWIDTH ? 0 : 32); kol < SCREENWIDTH; kol += 4)
        {
            if (kol < screen_width + 32)
            {
                dvibuffer[kol] = palette444[current_line[srcIndex] & 0x3f];
                dvibuffer[kol + 1] = palette444[current_line[srcIndex + 1] & 0x3f];
                dvibuffer[kol + 2] = palette444[current_line[srcIndex + 2] & 0x3f];
                dvibuffer[kol + 3] = palette444[current_line[srcIndex + 3] & 0x3f];
            }
            else
            {
                dvibuffer[kol] = 0;
                dvibuffer[kol + 1] = 0;
                dvibuffer[kol + 2] = 0;
                dvibuffer[kol + 3] = 0;
            }

            srcIndex += 4;
        }

        if (fps_enabled && line >= FPSSTART && line < FPSEND)
        {
            WORD *fpsBuffer = dvibuffer + 5;
            int rowInChar = line % 8;
            for (auto i = 0; i < 3; i++)
            {
                char firstFpsDigit = fpsString[i];
                char fontSlice = getcharslicefrom8x8font(firstFpsDigit, rowInChar);
                for (auto bit = 0; bit < 8; bit++)
                {
                    if (fontSlice & 1)
                    {
                        *fpsBuffer++ = fpsfgcolor;
                    }
                    else
                    {
                        *fpsBuffer++ = fpsbgcolor;
                    }
                    fontSlice >>= 1;
                }
            }
        }
    }
    else
    {
        // memset(dvibuffer, 0, SCREENWIDTH * 2);
    }
}
#endif

#if !HSTX
static void inline processaudioPerFrameDVI()
{
    // 3. Output audio buffer
    // Example: output to DVI ring buffer (stereo, duplicate mono)
    auto &ring = dvi_->getAudioRingBuffer();
    // GWENESIS_AUDIO_SAMPLING_DIVISOR  --> totalSamples
    //                        6               148
    //                        5               177
    //                        1               888
    int totalSamples = sn76489_index; // Number of samples generated this frame
    int written = 0;
    // sizeof snd_buf = 3522, idem as gwenesis_sn76489_buffer

    // static int16_t snd_buf[GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2];
    // for (int h = 0; h < GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2; h++) {
    //             snd_buf[h] = (gwenesis_sn76489_buffer[h / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR]);
    // }
    // printf("Audio samples to write: %d, %d target_clocks\n", totalSamples, target_clocks);
    while (written < (GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2))
    {
        int n = std::min<int>(GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2 - written, ring.getWritableSize());
        if (n == 0)
        {
            //  printf("Audio buffer full, wrote %d of %d samples\n", written, totalSamples);
            // Buffer full, can't write more
            break;
        }
        auto p = ring.getWritePointer();
        for (int i = 0; i < n; ++i)
        {
            int16_t sample = (gwenesis_sn76489_buffer[(written + i) / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR]) >> 2;
            *p++ = {sample, sample}; // Duplicate mono to stereo
        }
        ring.advanceWritePointer(n);
        written += n;
    }
}
#else
static void inline processaudioPerFramehstx()
{
    for (int i = 0; i < GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2; i += 2)
    {
        int16_t l = (gwenesis_sn76489_buffer[(i) / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR]);
        int16_t r = (gwenesis_sn76489_buffer[(i + 1) / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR]);
        l >>= 3;
        r >>= 3;
        hstx_push_audio_sample(l, r);
#if ENABLE_VU_METER
        if (settings.flags.enableVUMeter)
        {
            addSampleToVUMeter(l);
        }
#endif
    }
}
#endif
static void inline processaudioPerFrameI2S()
{
    for (int i = 0; i < GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2; i += 2)
    {
        int16_t l = (gwenesis_sn76489_buffer[(i) / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR]);
        int16_t r = (gwenesis_sn76489_buffer[(i + 1) / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR]);
        l >>= 3;
        r >>= 3;
        EXT_AUDIO_ENQUEUE_SAMPLE(l, r);
#if ENABLE_VU_METER
        if (settings.flags.enableVUMeter)
        {
            addSampleToVUMeter(l);
        }
#endif
    }
}
void inline output_audio_per_frame()
{
    // 1. Calculate total PSG clocks for this frame
    const bool is_pal = REG1_PAL;
    const int lines_per_frame = is_pal ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
    const int target_clocks = lines_per_frame * VDP_CYCLES_PER_LINE;

    // 2. Generate all audio samples for the frame
    gwenesis_SN76489_run(target_clocks);
#if EXT_AUDIO_IS_ENABLED
    if (settings.flags.useExtAudio == 1)
    {
        processaudioPerFrameI2S();
        return;
    }
#endif
#if !HSTX
    processaudioPerFrameDVI();
#else
    processaudioPerFramehstx();
#endif
}

void __not_in_flash_func(emulate)()
{

    // FH gwenesis_vdp_set_buffer((uint8_t *)SCREEN);
    uint8_t frameline_buffer[SCREENWIDTH];
    bool firstLoop = true;
    unsigned int old_screen_width = 0;
    unsigned int old_screen_height = 0;
    char tbuf[32];
   
    while (!reboot)
    {
        /* Eumulator loop */
        int hint_counter = gwenesis_vdp_regs[10];

        const bool is_pal = REG1_PAL;
        screen_width = REG12_MODE_H40 ? 320 : 256;
        screen_height = is_pal ? 240 : 224;
        lines_per_frame = is_pal ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
        // Printf values
        if (firstLoop || old_screen_height != screen_height || old_screen_width != screen_width)
        {
            // printf("Uptime %s, is_pal %d, screen_width: %d, screen_height: %d, lines_per_frame: %d\n", Frens::ms_to_d_hhmmss(Frens::time_ms(), tbuf, sizeof tbuf), is_pal, screen_width, screen_height, lines_per_frame);
            printf("Uptime %s, is_pal %d, screen_width: %d, screen_height: %d, lines_per_frame: %d, audio_enabled: %d, frameskip: %d\n", Frens::ms_to_d_hhmmss(Frens::time_ms(), tbuf, sizeof tbuf), is_pal, screen_width, screen_height, lines_per_frame, settings.flags.audioEnabled, settings.flags.frameSkip);
            firstLoop = false;
            old_screen_height = screen_height;
            old_screen_width = screen_width;
        }

        gwenesis_vdp_render_config();

        zclk = 0;
        /* Reset the difference clocks and audio index */
        system_clock = 0;
        sn76489_clock = 0;
        sn76489_index = 0;
        scan_line = 0;
        if (z80_enable_mode == 1)
            z80_run(lines_per_frame * VDP_CYCLES_PER_LINE);
        auto margin = SCREENHEIGHT - screen_height;
        if (margin > 0)
        {
            margin /= 2;
        }
        else
        {
            margin = 0;
        }
        while (scan_line < lines_per_frame)
        {
            // if (audio_enabled)
            // {
            //     processaudio(scan_line);
            // }
            // printf("%d\n", scan_line);

            uint8_t *tmpbuffer = frameline_buffer;

            /* CPUs */
            m68k_run(system_clock + VDP_CYCLES_PER_LINE);
            if (z80_enable_mode == 2)
                z80_run(system_clock + VDP_CYCLES_PER_LINE);
            /* Video */
            // Interlace mode
            // if (drawFrame && !interlace || (frame % 2 == 0 && scan_line % 2) || scan_line % 2 == 0)
            // {
            if (drawFrame)
            {
                // if (scan_line < 240)
                // {
                //     frameline_buffer = Frens::framebufferCore0 + (scan_line * 320);
                // }
                gwenesis_vdp_render_line(scan_line, frameline_buffer); /* render scan_line */
                if (scan_line < screen_height)
                {
                    auto currentLineBuf =
#if !HSTX
                        &Frens::framebuffer[(scan_line + margin) * 320];
#else
                        hstx_getlineFromFramebuffer(scan_line + margin);
#endif

#if 0
                    for (int kol = (screen_width == SCREENWIDTH ? 0 : 32); kol < SCREENWIDTH; kol += 4)
                    {
                        if (kol < screen_width + 32)
                        {
                            currentLineBuf[kol] = palette[tmpbuffer[0] & 0x3f];
                            currentLineBuf[kol + 1] = palette[tmpbuffer[1] & 0x3f];
                            currentLineBuf[kol + 2] = palette[tmpbuffer[2] & 0x3f];
                            currentLineBuf[kol + 3] = palette[tmpbuffer[3] & 0x3f];
                        }
                        else
                        {
                            currentLineBuf[kol] = 0;
                            currentLineBuf[kol + 1] = 0;
                            currentLineBuf[kol + 2] = 0;
                            currentLineBuf[kol + 3] = 0;
                        }
                        tmpbuffer += 4;
                    }
#else
                    // Optimized pixel transfer
                    int start = (screen_width == SCREENWIDTH) ? 0 : 32;
                    int visible = screen_width;                 // active pixels
                    int tail = SCREENWIDTH - (start + visible); // right border size

                    uint8_t *src = tmpbuffer;               // source indices
                    uint16_t *dst = currentLineBuf + start; // destination
                    const uint16_t *pal = palette;          // palette pointer

                    int groups = visible >> 2; // number of 4-pixel groups
                    while (groups--)
                    {
                        dst[0] = pal[src[0] & 0x3F];
                        dst[1] = pal[src[1] & 0x3F];
                        dst[2] = pal[src[2] & 0x3F];
                        dst[3] = pal[src[3] & 0x3F];
                        src += 4;
                        dst += 4;
                    }
                    if (tail > 0)
                    {
                        memset(dst, 0, tail * sizeof(uint16_t));
                    }
#endif
                    if (settings.flags.displayFrameRate && scan_line >= FPSSTART && scan_line < FPSEND)
                    {
                        WORD *fpsBuffer = currentLineBuf + 5;
                        int rowInChar = scan_line % 8;
                        for (auto i = 0; i < 3; i++)
                        {
                            char firstFpsDigit = fpsString[i];
                            char fontSlice = getcharslicefrom8x8font(firstFpsDigit, rowInChar);
                            for (auto bit = 0; bit < 8; bit++)
                            {
                                if (fontSlice & 1)
                                {
                                    *fpsBuffer++ = fpsfgcolor;
                                }
                                else
                                {
                                    *fpsBuffer++ = fpsbgcolor;
                                }
                                fontSlice >>= 1;
                            }
                        }
                    }
                }
            }
            // }

            // On these lines, the line counter interrupt is reloaded
            if (scan_line == 0 || scan_line > screen_height)
            {
                hint_counter = REG10_LINE_COUNTER;
            }

            // interrupt line counter
            if (--hint_counter < 0)
            {
                if (REG0_LINE_INTERRUPT != 0 && scan_line <= screen_height)
                {
                    hint_pending = 1;
                    if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
                        m68k_update_irq(4);
                }
                hint_counter = REG10_LINE_COUNTER;
            }

            scan_line++;

            // vblank begin at the end of last rendered line
            if (scan_line == screen_height)
            {
                if (REG1_VBLANK_INTERRUPT != 0)
                {
                    gwenesis_vdp_status |= STATUS_VIRQPENDING;
                    m68k_set_irq(6);
                }
                z80_irq_line(1);
            }

            if (!is_pal && scan_line == screen_height + 1)
            {
                z80_irq_line(0);
                // FRAMESKIP every 3rd frame
                drawFrame = !settings.flags.frameSkip || (frame % 3 != 0);
            }

            system_clock += VDP_CYCLES_PER_LINE;
        }

        ProcessAfterFrameIsRendered();

        frame++;

#if 0
        // Original FPS limiter code.
        if (limit_fps)
        {
            // Improved FPS limiter: fixed timestep, no drift
            const uint64_t frame_period = is_pal ? 20000 : 16666; // microseconds per frame
            if (next_frame_time == 0)
                next_frame_time = time_us_64();
            while ((int64_t)(next_frame_time + frame_period - time_us_64()) > 0)
            {
                busy_wait_at_least_cycles(10);
            }
            next_frame_time += frame_period;
        }
#else
        // this should be more accurate, especially in DVI mode
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
                do {
                    next_frame_time += frame_period;
                } while (now >= next_frame_time);
            }
        }
#endif
        if (settings.flags.audioEnabled)
        {
            //  gwenesis_SN76489_run(REG1_PAL ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC * VDP_CYCLES_PER_LINE);
            output_audio_per_frame();
        }
        // ym2612_run(262 * VDP_CYCLES_PER_LINE);
        /*
        gwenesis_SN76489_run(262 * VDP_CYCLES_PER_LINE);
        ym2612_run(262 * VDP_CYCLES_PER_LINE);
        static int16_t snd_buf[GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2];
        for (int h = 0; h < ym2612_index * 2 * GWENESIS_AUDIO_SAMPLING_DIVISOR; h++) {
            snd_buf[h] = (gwenesis_sn76489_buffer[h / 2 / GWENESIS_AUDIO_SAMPLING_DIVISOR]) << 3;
        }
        i2s_dma_write(&i2s_config, snd_buf);*/
        // reset m68k cycles to the begin of next frame cycle
        m68k.cycles -= system_clock;

        /* copy audio samples for DMA */
        // gwenesis_sound_submit();

        // calculate framerate
        if (settings.flags.displayFrameRate)
        {
            uint64_t tick_us = Frens::time_us() - start_tick_us;
            if (tick_us > 1000000)
            {
                fps = frame_counter;
                start_tick_us = Frens::time_us();
                frame_counter = 0;
            }
            fpsString[0] = '0' + (fps / 100) % 10;
            fpsString[1] = '0' + (fps / 10) % 10;
            fpsString[2] = '0' + (fps % 10);
            frame_counter++;
        }
    }

    reboot = false;
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

    int fileSize = 0;
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
    while (true)
    {
#if 1
        if (strlen(selectedRom) == 0 || reset == true)
        {
            menu("Pico-Genesis+", ErrorMessage, isFatalError, showSplash, ".md .bin", selectedRom); 
        }
#endif
#if !HSTX
        if (settings.screenMode != ScreenMode::SCANLINE_1_1 && settings.screenMode != ScreenMode::NOSCANLINE_1_1)
        {
            settings.screenMode = ScreenMode::SCANLINE_1_1;
            FrensSettings::savesettings();
        }
        scaleMode8_7_ = Frens::applyScreenMode(settings.screenMode);
#endif

        reset = false;

        abSwapped = 0; // don't swap A and B buttons
        audio_enabled = settings.flags.audioEnabled;
        next_frame_time = 0;  // Reset next frame time for FPS limiter
        EXT_AUDIO_MUTE_INTERNAL_SPEAKER(settings.flags.fruitJamEnableInternalSpeaker == 0);
        memset(palette, 0, sizeof(palette));
        printf("Starting game\n");
        init_emulator_mem();
        load_cartridge(ROM_FILE_ADDR); // ROM_FILE_ADDR); // 0x100de000); // 0x100d1000);  // 0x100e2000); // ROM_FILE_ADDR);
        power_on();
        reset_emulation();
        emulate();
        free_emulator_mem();
        // system_shutdown();
        selectedRom[0] = 0;
        showSplash = false;
    }

    return 0;
}
