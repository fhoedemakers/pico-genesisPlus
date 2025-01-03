/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

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

static bool fps_enabled = false;
static uint32_t start_tick_us = 0;
static uint32_t fps = 0;
static char fpsString[3] = "00";
#define fpsfgcolor 0;     // black
#define fpsbgcolor 0xFFF; // white

#define MARGINTOP 0
#define MARGINBOTTOM 0

#define FPSSTART (((MARGINTOP + 7) / 8) * 8)
#define FPSEND ((FPSSTART) + 8)

bool reset = false;
bool reboot = false;

const uint16_t __not_in_flash_func(GenesisPalette)[512] = {
    0x000, 0x003, 0x005, 0x007, 0x009, 0x00A, 0x00C, 0x00F,
    0x030, 0x033, 0x035, 0x037, 0x039, 0x03A, 0x03C, 0x03F,
    0x050, 0x053, 0x055, 0x057, 0x059, 0x05A, 0x05C, 0x05F,
    0x070, 0x073, 0x075, 0x077, 0x079, 0x07A, 0x07C, 0x07F,
    0x090, 0x093, 0x095, 0x097, 0x099, 0x09A, 0x09C, 0x09F,
    0x0A0, 0x0A3, 0x0A5, 0x0A7, 0x0A9, 0x0AA, 0x0AC, 0x0AF,
    0x0C0, 0x0C3, 0x0C5, 0x0C7, 0x0C9, 0x0CA, 0x0CC, 0x0CF,
    0x0F0, 0x0F3, 0x0F5, 0x0F7, 0x0F9, 0x0FA, 0x0FC, 0x0FF,
    0x300, 0x303, 0x305, 0x307, 0x309, 0x30A, 0x30C, 0x30F,
    0x330, 0x333, 0x335, 0x337, 0x339, 0x33A, 0x33C, 0x33F,
    0x350, 0x353, 0x355, 0x357, 0x359, 0x35A, 0x35C, 0x35F,
    0x370, 0x373, 0x375, 0x377, 0x379, 0x37A, 0x37C, 0x37F,
    0x390, 0x393, 0x395, 0x397, 0x399, 0x39A, 0x39C, 0x39F,
    0x3A0, 0x3A3, 0x3A5, 0x3A7, 0x3A9, 0x3AA, 0x3AC, 0x3AF,
    0x3C0, 0x3C3, 0x3C5, 0x3C7, 0x3C9, 0x3CA, 0x3CC, 0x3CF,
    0x3F0, 0x3F3, 0x3F5, 0x3F7, 0x3F9, 0x3FA, 0x3FC, 0x3FF,
    0x500, 0x503, 0x505, 0x507, 0x509, 0x50A, 0x50C, 0x50F,
    0x530, 0x533, 0x535, 0x537, 0x539, 0x53A, 0x53C, 0x53F,
    0x550, 0x553, 0x555, 0x557, 0x559, 0x55A, 0x55C, 0x55F,
    0x570, 0x573, 0x575, 0x577, 0x579, 0x57A, 0x57C, 0x57F,
    0x590, 0x593, 0x595, 0x597, 0x599, 0x59A, 0x59C, 0x59F,
    0x5A0, 0x5A3, 0x5A5, 0x5A7, 0x5A9, 0x5AA, 0x5AC, 0x5AF,
    0x5C0, 0x5C3, 0x5C5, 0x5C7, 0x5C9, 0x5CA, 0x5CC, 0x5CF,
    0x5F0, 0x5F3, 0x5F5, 0x5F7, 0x5F9, 0x5FA, 0x5FC, 0x5FF,
    0x700, 0x703, 0x705, 0x707, 0x709, 0x70A, 0x70C, 0x70F,
    0x730, 0x733, 0x735, 0x737, 0x739, 0x73A, 0x73C, 0x73F,
    0x750, 0x753, 0x755, 0x757, 0x759, 0x75A, 0x75C, 0x75F,
    0x770, 0x773, 0x775, 0x777, 0x779, 0x77A, 0x77C, 0x77F,
    0x790, 0x793, 0x795, 0x797, 0x799, 0x79A, 0x79C, 0x79F,
    0x7A0, 0x7A3, 0x7A5, 0x7A7, 0x7A9, 0x7AA, 0x7AC, 0x7AF,
    0x7C0, 0x7C3, 0x7C5, 0x7C7, 0x7C9, 0x7CA, 0x7CC, 0x7CF,
    0x7F0, 0x7F3, 0x7F5, 0x7F7, 0x7F9, 0x7FA, 0x7FC, 0x7FF,
    0x900, 0x903, 0x905, 0x907, 0x909, 0x90A, 0x90C, 0x90F,
    0x930, 0x933, 0x935, 0x937, 0x939, 0x93A, 0x93C, 0x93F,
    0x950, 0x953, 0x955, 0x957, 0x959, 0x95A, 0x95C, 0x95F,
    0x970, 0x973, 0x975, 0x977, 0x979, 0x97A, 0x97C, 0x97F,
    0x990, 0x993, 0x995, 0x997, 0x999, 0x99A, 0x99C, 0x99F,
    0x9A0, 0x9A3, 0x9A5, 0x9A7, 0x9A9, 0x9AA, 0x9AC, 0x9AF,
    0x9C0, 0x9C3, 0x9C5, 0x9C7, 0x9C9, 0x9CA, 0x9CC, 0x9CF,
    0x9F0, 0x9F3, 0x9F5, 0x9F7, 0x9F9, 0x9FA, 0x9FC, 0x9FF,
    0xA00, 0xA03, 0xA05, 0xA07, 0xA09, 0xA0A, 0xA0C, 0xA0F,
    0xA30, 0xA33, 0xA35, 0xA37, 0xA39, 0xA3A, 0xA3C, 0xA3F,
    0xA50, 0xA53, 0xA55, 0xA57, 0xA59, 0xA5A, 0xA5C, 0xA5F,
    0xA70, 0xA73, 0xA75, 0xA77, 0xA79, 0xA7A, 0xA7C, 0xA7F,
    0xA90, 0xA93, 0xA95, 0xA97, 0xA99, 0xA9A, 0xA9C, 0xA9F,
    0xAA0, 0xAA3, 0xAA5, 0xAA7, 0xAA9, 0xAAA, 0xAAC, 0xAAF,
    0xAC0, 0xAC3, 0xAC5, 0xAC7, 0xAC9, 0xACA, 0xACC, 0xACF,
    0xAF0, 0xAF3, 0xAF5, 0xAF7, 0xAF9, 0xAFA, 0xAFC, 0xAFF,
    0xC00, 0xC03, 0xC05, 0xC07, 0xC09, 0xC0A, 0xC0C, 0xC0F,
    0xC30, 0xC33, 0xC35, 0xC37, 0xC39, 0xC3A, 0xC3C, 0xC3F,
    0xC50, 0xC53, 0xC55, 0xC57, 0xC59, 0xC5A, 0xC5C, 0xC5F,
    0xC70, 0xC73, 0xC75, 0xC77, 0xC79, 0xC7A, 0xC7C, 0xC7F,
    0xC90, 0xC93, 0xC95, 0xC97, 0xC99, 0xC9A, 0xC9C, 0xC9F,
    0xCA0, 0xCA3, 0xCA5, 0xCA7, 0xCA9, 0xCAA, 0xCAC, 0xCAF,
    0xCC0, 0xCC3, 0xCC5, 0xCC7, 0xCC9, 0xCCA, 0xCCC, 0xCCF,
    0xCF0, 0xCF3, 0xCF5, 0xCF7, 0xCF9, 0xCFA, 0xCFC, 0xCFF,
    0xF00, 0xF03, 0xF05, 0xF07, 0xF09, 0xF0A, 0xF0C, 0xF0F,
    0xF30, 0xF33, 0xF35, 0xF37, 0xF39, 0xF3A, 0xF3C, 0xF3F,
    0xF50, 0xF53, 0xF55, 0xF57, 0xF59, 0xF5A, 0xF5C, 0xF5F,
    0xF70, 0xF73, 0xF75, 0xF77, 0xF79, 0xF7A, 0xF7C, 0xF7F,
    0xF90, 0xF93, 0xF95, 0xF97, 0xF99, 0xF9A, 0xF9C, 0xF9F,
    0xFA0, 0xFA3, 0xFA5, 0xFA7, 0xFA9, 0xFAA, 0xFAC, 0xFAF,
    0xFC0, 0xFC3, 0xFC5, 0xFC7, 0xFC9, 0xFCA, 0xFCC, 0xFCF,
    0xFF0, 0xFF3, 0xFF5, 0xFF7, 0xFF9, 0xFFA, 0xFFC, 0xFFF};

namespace
{
    constexpr uint32_t CPUFreqKHz = 266000; // 252000;
}

int sampleIndex = 0;
void __not_in_flash_func(processaudio)(int offset)
{
    int samples = 4; // 735/192 = 3.828125 192*4=768 735/3=245

    // if (offset == (IS_GG ? 24 : 0))
    // {
    //     sampleIndex = 0;
    // }
    // else
    // {
    //     sampleIndex += samples;
    //     if (sampleIndex >= 735)
    //     {
    //         return;
    //     }
    // }
    // short *p1 = snd.buffer[0] + sampleIndex;
    // short *p2 = snd.buffer[1] + sampleIndex;
    // while (samples)
    // {
    //     auto &ring = dvi_->getAudioRingBuffer();
    //     auto n = std::min<int>(samples, ring.getWritableSize());
    //     if (!n)
    //     {
    //         return;
    //     }
    //     auto p = ring.getWritePointer();
    //     int ct = n;
    //     while (ct--)
    //     {
    //         int l = (*p1++ << 16) + *p2++;
    //         // works also : int l = (*p1++ + *p2++) / 2;
    //         int r = l;
    //         // int l = *wave1++;
    //         *p++ = {static_cast<short>(l), static_cast<short>(r)};
    //     }
    //     ring.advanceWritePointer(n);
    //     samples -= n;
    // }
}

extern "C" void __not_in_flash_func(render_line)(int line, const uint8_t *buffer)
{
    // DVI top margin has #MARGINTOP lines
    // DVI bottom margin has #MARGINBOTTOM lines
    // DVI usable screen estate: MARGINTOP .. (240 - #MARGINBOTTOM)
    // Genesis 320×224, 256×224, 320x240 (some PAL games), 256x240 (some PAL games),
    //         320×448, 256×448, 320x480 (some PAL games), 256x480 (some PAL games)
    // Emulator loops from scanline 0 to 261
    // Audio needs to be processed per scanline

    processaudio(line);
    // Adjust line number to center the emulator display
    line += MARGINTOP;
    // Only render lines that are visible on the screen, keeping into account top and bottom margins
    if (line < MARGINTOP || line >= 240 - MARGINBOTTOM)
        return;

    auto b = dvi_->getLineBuffer();
    uint16_t *sbuffer;
    if (buffer)
    {
        // uint16_t *sbuffer = b->data() + 32 + (IS_GG ? 48 : 0);
        // for (int i = screenCropX; i < BMP_WIDTH - screenCropX; i++)
        // {
        //     sbuffer[i - screenCropX] = palette444[(buffer[i + BMP_X_OFFSET]) & 31];
        // }
    }
    else
    {
        sbuffer = b->data() + 32;
        __builtin_memset(sbuffer, 0, 512);
    }
    // Display frame rate
    if (fps_enabled && line >= FPSSTART && line < FPSEND)
    {
        WORD *fpsBuffer = b->data() + 40;
        int rowInChar = line % 8;
        for (auto i = 0; i < 2; i++)
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
    dvi_->setLineBuffer(line, b);
}

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
    // nespad_read_finish(); // Sets global nespad_state var
    tuh_task();
    // Frame rate calculation
    if (fps_enabled)
    {
        // calculate fps and round to nearest value (instead of truncating/floor)
        uint32_t tick_us = Frens::time_us() - start_tick_us;
        fps = (1000000 - 1) / tick_us + 1;
        start_tick_us = Frens::time_us();
        fpsString[0] = '0' + (fps / 10);
        fpsString[1] = '0' + (fps % 10);
    }
    return count;
}

static DWORD prevButtons[2]{};
static DWORD prevButtonssystem[2]{};

static int rapidFireMask[2]{};
static int rapidFireCounter = 0;
void processinput(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem, bool ignorepushed)
{
    // pwdPad1 and pwdPad2 are only used in menu and are only set on first push
    *pdwPad1 = *pdwPad2 = *pdwSystem = 0;

    unsigned long pushed, pushedsystem;
    bool usbConnected = false;
    for (int i = 0; i < 2; i++)
    {

        auto &dst = (i == 0) ? *pdwPad1 : *pdwPad2;
        auto &gp = io::getCurrentGamePadState(i);
        if (i == 0)
        {
            usbConnected = gp.isConnected();
        }
        int v = 0;
        // int v = (gp.buttons & io::GamePadState::Button::LEFT ? LEFT : 0) |
        //         (gp.buttons & io::GamePadState::Button::RIGHT ? RIGHT : 0) |
        //         (gp.buttons & io::GamePadState::Button::UP ? UP : 0) |
        //         (gp.buttons & io::GamePadState::Button::DOWN ? DOWN : 0) |
        //         (gp.buttons & io::GamePadState::Button::A ? A : 0) |
        //         (gp.buttons & io::GamePadState::Button::B ? B : 0) |
        //         (gp.buttons & io::GamePadState::Button::SELECT ? SELECT : 0) |
        //         (gp.buttons & io::GamePadState::Button::START ? START : 0) |
        //         0;

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

        dst = rv;

        auto p1 = v;

        auto pushed = v & ~prevButtons[i];
        // if (p1 & INPUT_PAUSE)
        // {
        //     if (pushedsystem & INPUT_START)
        //     {
        //         reset = true;
        //         printf("Reset pressed\n");
        //     }
        // }
        // if (p1 & INPUT_START)
        // {
        //     // Toggle frame rate display
        //     if (pushed & INPUT_BUTTON1)
        //     {
        //         fps_enabled = !fps_enabled;
        //         printf("FPS: %s\n", fps_enabled ? "ON" : "OFF");
        //     }
        //     if (pushed & INPUT_UP)
        //     {
        //         Frens::screenMode(-1);
        //     }
        //     else if (pushed & INPUT_DOWN)
        //     {
        //         Frens::screenMode(+1);
        //     }
        // }
        prevButtons[i] = v;

        // return only on first push
        if (pushed)
        {
            dst = v;
        }
    }
}
void gwenesis_io_get_buttons()
{
}
void __not_in_flash_func(process)(void)
{
    DWORD pdwPad1, pdwPad2, pdwSystem; // have only meaning in menu
    while (reset == false)
    {
        processinput(&pdwPad1, &pdwPad2, &pdwSystem, false);
        // renderframe
        // TODO
        ProcessAfterFrameIsRendered();
    }
}

uint8_t maxkol = 0;
void __not_in_flash_func(processEmulatorScanLine)(uint8_t *current_line, uint16_t *buffer, int screenWidth)
{
    for (int kol = 0; kol < screenWidth; kol += 4)
    {
        buffer[kol] = GenesisPalette[current_line[kol]];
        buffer[kol + 1] = GenesisPalette[current_line[kol + 1]];
        buffer[kol + 2] = GenesisPalette[current_line[kol + 2]];
        buffer[kol + 3] = GenesisPalette[current_line[kol + 3]];
    }
    //  for (int kol = 0; kol < screenWidth; kol ++)
    // {
    //     if ( current_line[kol] > maxkol)
    //     {
    //         maxkol = current_line[kol];
    //         printf("maxkol %d\n", maxkol);
    //     }
    //     buffer[kol] = GenesisPalette[current_line[kol]];
    // }
}
/* Clocks and synchronization */
/* system clock is video clock */
int system_clock;
unsigned int lines_per_frame = LINES_PER_FRAME_NTSC; // 262; /* NTSC: 262, PAL: 313 */
int scan_line;
unsigned int frame_counter = 0;
unsigned int drawFrame = 1;
int z80_enable_mode = 2;
bool interlace = true;  // was true
int frame = 0;
int frame_cnt = 0;
int frame_timer_start = 0;
bool limit_fps = false;      // was true
bool frameskip = false;    // was true
int audio_enabled = 0;
bool sn76489_enabled = true;
uint8_t snd_accurate = 0;
extern unsigned char gwenesis_vdp_regs[0x20];
extern unsigned int gwenesis_vdp_status;
extern unsigned int screen_width, screen_height;
extern int hint_pending;

int sn76489_index; /* sn78649 audio buffer index */
int sn76489_clock;
void __time_critical_func(emulate)()
{

    // FH gwenesis_vdp_set_buffer((uint8_t *)SCREEN);
    uint8_t tmpbuffer[SCREENWIDTH];
    while (!reboot)
    {
        /* Eumulator loop */
        int hint_counter = gwenesis_vdp_regs[10];

        const bool is_pal = REG1_PAL;
        screen_width = REG12_MODE_H40 ? 320 : 256;
        screen_height = is_pal ? 240 : 224;
        lines_per_frame = is_pal ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
        // Printf values
#if 0
        printf("frame %d, is_pal %d, screen_width: %d, screen_height: %d, lines_per_frame: %d\n", frame, is_pal, screen_width, screen_height, lines_per_frame);
#endif        
        // graphics_set_buffer(buffer, screen_width, screen_height);
        // TODO: move to separate function graphics_set_dimensions ?
        // FH graphics_set_buffer((uint8_t*)SCREEN, screen_width, screen_height);
        // FH graphics_set_offset(screen_width != 320 ? 32 : 0, screen_height != 240 ? 8 : 0);
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
            // printf("%d\n", scan_line);

            uint8_t  *frameline_buffer = tmpbuffer;
            

            /* CPUs */
            m68k_run(system_clock + VDP_CYCLES_PER_LINE);
            if (z80_enable_mode == 2)
                z80_run(system_clock + VDP_CYCLES_PER_LINE);
            /* Video */
            // Interlace mode
            // if (drawFrame && !interlace || (frame % 2 == 0 && scan_line % 2) || scan_line % 2 == 0)
            // {
                if (scan_line < 240)
                {
                   frameline_buffer = Frens::framebufferCore0 + (scan_line * 320);
                }
                gwenesis_vdp_render_line(scan_line, frameline_buffer); /* render scan_line */
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
        }
        Frens::markFrameReadyForReendering();
        frame++;
        if (limit_fps)
        {
            frame_cnt++;
            if (frame_cnt == (is_pal ? 5 : 6))
            {
                while (time_us_64() - frame_timer_start < (is_pal ? 20000 * 5 : 16666 * 6))
                {
                    busy_wait_at_least_cycles(10);
                }; // 60 Hz
                frame_timer_start = time_us_64();
                frame_cnt = 0;
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
    }

    reboot = false;
}
/// @brief
/// Start emulator.
/// @return
int main()
{
    char selectedRom[FF_MAX_LFN];
    romName = selectedRom;
    ErrorMessage[0] = selectedRom[0] = 0;

    int fileSize = 0;
    bool isGameGear = false;

    // Set voltage and clock frequency
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(CPUFreqKHz, true);

    stdio_init_all();
    sleep_ms(500);
    printf("Starting Genesis System Emulator\n");
    printf("CPU freq: %d\n", clock_get_hz(clk_sys));
    printf("Starting Tinyusb subsystem\n");
    tusb_init();
    
    isFatalError = !Frens::initAll(selectedRom, CPUFreqKHz, MARGINTOP, MARGINBOTTOM, 256, true, true);
    bool showSplash = true;

    while (true)
    {
#if 1
        if (strlen(selectedRom) == 0 || reset == true)
        {
            menu("Pico-Genesis+", ErrorMessage, isFatalError, showSplash, ".md"); // never returns, but reboots upon selecting a game
        }
#endif
        scaleMode8_7_ = Frens::applyScreenMode(ScreenMode::MAX);
        dvi_->getBlankSettings().top = 0;
        dvi_->getBlankSettings().bottom = 0;
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
        Frens::SetFrameBufferProcessScanLineFunction(processEmulatorScanLine);

        // Todo: Initialize emulator
        printf("Starting game\n");
        init_emulator_mem();
        load_cartridge(ROM_FILE_ADDR); // 0x100d1000);  // 0x100e2000); // ROM_FILE_ADDR);
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
