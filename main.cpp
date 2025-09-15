#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/divider.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
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

static bool fps_enabled = true;
static uint64_t start_tick_us = 0;
static uint64_t fps = 0;
static char fpsString[4] = "000";
#define fpsfgcolor 0;     // black
#define fpsbgcolor 0xFFF; // white

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
bool limit_fps = false;  // was true
bool frameskip = false; // was true
int audio_enabled = 0;  // Set to 1 to enable audio. Now disabled because its is not properly working.
bool sn76489_enabled = true;
uint8_t snd_accurate = 0;

extern unsigned char gwenesis_vdp_regs[0x20];
extern unsigned int gwenesis_vdp_status;
extern unsigned int screen_width, screen_height;
extern int hint_pending;

int sn76489_index; /* sn78649 audio buffer index */
int sn76489_clock;

#define AUDIOBUFFERSIZE 1024
// Overclock tests:
// 252000 OK
// 266000 OK
// 280000 OK
// 294000 OK
// 308000 OK
// 322000 Panic System clock of %u kHz cannot be exactly achieved
// 324000 OK
// 325000 Panic System clock of %u kHz cannot be exactly achieved
// 326000 Panic System clock of %u kHz cannot be exactly achieved
// 328000 Not supported signal on samsung tv
// 330000 Not supported signal on samsung tv
// 340000 Not supported signal on samsung tv
#define EMULATOR_CLOCKFREQ_KHZ  252000 // 324000 Overclock frequency in kHz when using Emulator
// https://github.com/orgs/micropython/discussions/15722
static uint32_t CPUFreqKHz = EMULATOR_CLOCKFREQ_KHZ; // 340000; //266000;

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
    auto count = dvi_->getFrameCounter();
    auto onOff = hw_divider_s32_quotient_inlined(count, 60) & 1;
    Frens::blinkLed(onOff);
#if NES_PIN_CLK != -1
    nespad_read_finish(); // Sets global nespad_state var
#endif
    tuh_task();

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
    if (settings.screenMode == ScreenMode::SCANLINE_1_1)
    {
        settings.screenMode = ScreenMode::NOSCANLINE_1_1;
    }
    else
    {
        settings.screenMode = ScreenMode::SCANLINE_1_1;
    }
    Frens::savesettings();
    Frens::applyScreenMode(settings.screenMode);
}
void gwenesis_io_get_buttons()
{
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
                (gp.buttons & io::GamePadState::Button::X ? C : 0) |
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
                v |= wiipad_read();
            }
        }
        else // if no USB controller is connected, wiipad acts as controller 1
        {
            if (i == 0)
            {
                v |= wiipad_read();
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
                reboot = true;
                printf("Reset pressed\n");
            }
        }
        if (p1 & START)
        {
            // Toggle frame rate display
            if (pushed & A)
            {
                fps_enabled = !fps_enabled;
                printf("FPS: %s\n", fps_enabled ? "ON" : "OFF");
            }
            if (pushed & UP)
            {
                toggleScreenMode();
            }
            // else if (pushed & DOWN)
            // {
            //     toggleScreenMode();
            // }
        }
        sizeof(unsigned short);
        prevButtons[i] = v;
        button_state[i] = ((v & LEFT) ? 1 << PAD_LEFT : 0) |
                          ((v & RIGHT) ? 1 << PAD_RIGHT : 0) |
                          ((v & UP) ? 1 << PAD_UP : 0) |
                          ((v & DOWN) ? 1 << PAD_DOWN : 0) |
                          ((v & START) ? 1 << PAD_S : 0) |
                          ((v & A) ? 1 << PAD_A : 0) |
                          ((v & B) ? 1 << PAD_B : 0) |
                          ((v & C) ? 1 << PAD_C : 0) |
                          ((v & SELECT) ? 1 << PAD_C : 0);
        button_state[i] = ~button_state[i];
    }
}

const uint8_t __not_in_flash_func(paletteBrightness)[] = {0, 52, 87, 116, 144, 172, 206, 255};

extern "C" void genesis_set_palette(const uint8_t index, const uint32_t color)
{
#if !HSTX
    // RGB444
    uint8_t b = paletteBrightness[(color >> 9) & 0x0F];
    uint8_t g = paletteBrightness[(color >> 5) & 0x0F];
    uint8_t r = paletteBrightness[(color >> 1) & 0x0F];
    palette[index] = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
#else
    // RGB565
    uint8_t b = (color >> 8) & 0xF8;
    uint8_t g = (color >> 5) & 0xFC;
    uint8_t r = (color >> 3) & 0xF8;
    palette[index] = (r << 8) | (g << 3) | (b >> 3);
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
void __not_in_flash_func(emulate)()
{

    // FH gwenesis_vdp_set_buffer((uint8_t *)SCREEN);
    uint8_t frameline_buffer[SCREENWIDTH];
    bool firstLoop = true;
    unsigned int old_screen_width = 0;
    unsigned int old_screen_height = 0;
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
            printf("is_pal %d, screen_width: %d, screen_height: %d, lines_per_frame: %d\n", is_pal, screen_width, screen_height, lines_per_frame);
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
            // if (drawFrame) {
            // if (scan_line < 240)
            // {
            //     frameline_buffer = Frens::framebufferCore0 + (scan_line * 320);
            // }
            gwenesis_vdp_render_line(scan_line, frameline_buffer); /* render scan_line */
            // }
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
            // if (line_buffer)
            // {
            //     dvi_->setLineBuffer(scan_line, current_dvi_buffer);
            // }
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
                // drawFrame = frameskip && frame % 3 != 0;
                drawFrame = 1;
                // if (frameskip && frame % 3 == 0) {
                //     drawFrame = 0;
                // } else {
                //     drawFrame = 1;
                // }
            }

            system_clock += VDP_CYCLES_PER_LINE;
            if (scan_line < screen_height)
            {
                auto currentLineBuf = &Frens::framebuffer[(scan_line + 8) * 320];

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
                if (fps_enabled && scan_line >= FPSSTART && scan_line < FPSEND)
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

        ProcessAfterFrameIsRendered();

        frame++;
        if (limit_fps)
        {
            frame_cnt++;
            if (frame_cnt == (is_pal ? 5 : 6))
            {
                // int cnt = 0;
                while (time_us_64() - frame_timer_start < (is_pal ? 20000 * 5 : 16666 * 6))
                {
                    busy_wait_at_least_cycles(10);
                    // cnt++;
                }; // 60 Hz
                frame_timer_start = time_us_64();
                frame_cnt = 0;
                // if (cnt > 1)
                // {
                //     printf("cnt: %d\n", cnt);
                // }
            }
        }

        if (audio_enabled)
        {
            gwenesis_SN76489_run(REG1_PAL ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC * VDP_CYCLES_PER_LINE);
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
        if (fps_enabled)
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

    // Set voltage and clock frequency
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(CPUFreqKHz, true);

    stdio_init_all();
    printf("==========================================================================================\n");
    printf("Pico-Genesis+ %s\n", SWVERSION);
    printf("Build date: %s\n", __DATE__);
    printf("Build time: %s\n", __TIME__);
    printf("CPU freq: %d kHz\n", clock_get_hz(clk_sys) / 1000);
    printf("Stack size: %d bytes\n", PICO_STACK_SIZE);
    printf("==========================================================================================\n");
    printf("Starting up...\n");

    isFatalError = !Frens::initAll(selectedRom, CPUFreqKHz, MARGINTOP, MARGINBOTTOM, AUDIOBUFFERSIZE, true, true);
#if !HSTX
    scaleMode8_7_ = Frens::applyScreenMode(settings.screenMode);
#endif
    bool showSplash = true;

    while (true)
    {
#if 1
        if (strlen(selectedRom) == 0 || reset == true)
        {
            menu("Pico-Genesis+", ErrorMessage, isFatalError, showSplash, ".md, .bin", selectedRom, "MD"); // never returns, but reboots upon selecting a game
        }
#endif
        if (settings.screenMode != ScreenMode::SCANLINE_1_1 && settings.screenMode != ScreenMode::NOSCANLINE_1_1)
        {
            settings.screenMode = ScreenMode::SCANLINE_1_1;
            Frens::savesettings();
        }
        scaleMode8_7_ = Frens::applyScreenMode(settings.screenMode);
        // dvi_->getBlankSettings().top = 8 * 2;
        // dvi_->getBlankSettings().bottom = 8 * 2;
        reset = false;
#if 0
        FRESULT fr;
        FIL file;
        fr = f_open(&file, selectedRom, FA_READ);
        if (fr != FR_OK)
        {
            snprintf(ErrorMessage, 40, "Cannot open rom %d", fr);
            printf("%s\n", ErrorMessage);
            selectedRom[0] = 0;
            continue;
        }
        fileSize = f_size(&file);
        f_close(&file);

        printf("Now playing: %s (%d bytes)\n", selectedRom, fileSize);
#endif
        abSwapped = 0; // don't swap A and B buttons

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
