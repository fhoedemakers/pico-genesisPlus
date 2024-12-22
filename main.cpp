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
extern "C" {
#include "gwenesis/buffers.h"
#include "gwenesis/cpus/M68K/m68k.h"
#include "gwenesis/sound/z80inst.h"
#include "gwenesis/bus/gwenesis_bus.h"
#include "gwenesis/io/gwenesis_io.h"
#include "gwenesis/vdp/gwenesis_vdp.h"
#include "gwenesis/savestate/gwenesis_savestate.h"
#include  "gwenesis/sound/gwenesis_sn76489.h"
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

#define MARGINTOP 24
#define MARGINBOTTOM 4

#define FPSSTART (((MARGINTOP + 7) / 8) * 8)
#define FPSEND ((FPSSTART) + 8)

bool reset = false;

bool reboot = false;

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
        if ( i == 0 )
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
void gwenesis_io_get_buttons() {
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

/* Clocks and synchronization */
/* system clock is video clock */
int system_clock;
unsigned int lines_per_frame = LINES_PER_FRAME_NTSC; //262; /* NTSC: 262, PAL: 313 */
int scan_line;
unsigned int frame_counter = 0;
unsigned int drawFrame = 1;
int z80_enable_mode = 2;
bool interlace = true;
int frame = 0;
int frame_cnt = 0;
int frame_timer_start = 0;
bool limit_fps = true;
bool frameskip = true;
int audio_enabled = 0;
bool sn76489_enabled = true;
uint8_t snd_accurate = 0;
extern unsigned char gwenesis_vdp_regs[0x20];
extern unsigned int gwenesis_vdp_status;
extern unsigned int screen_width, screen_height;
extern int hint_pending;

int sn76489_index;                                                      /* sn78649 audio buffer index */
int sn76489_clock;  
void __time_critical_func(emulate)() {

   
    gwenesis_vdp_set_buffer((uint8_t *) SCREEN);
    while (!reboot) {
        /* Eumulator loop */
        int hint_counter = gwenesis_vdp_regs[10];

        const bool is_pal = REG1_PAL;
        screen_width = REG12_MODE_H40 ? 320 : 256;
        screen_height = is_pal ? 240 : 224;
        lines_per_frame = is_pal ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
        // Printf values
        printf("frame %d, is_pal %d, screen_width: %d, screen_height: %d, lines_per_frame: %d\n",frame, is_pal, screen_width, screen_height, lines_per_frame);
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

        while (scan_line < lines_per_frame) {
            /* CPUs */
            m68k_run(system_clock + VDP_CYCLES_PER_LINE);
            if (z80_enable_mode == 2)
                    z80_run(system_clock + VDP_CYCLES_PER_LINE);
            /* Video */
            // Interlace mode
            if (drawFrame && !interlace || (frame % 2 == 0 && scan_line % 2) || scan_line % 2 == 0) {
                gwenesis_vdp_render_line(scan_line); /* render scan_line */
            }

            // On these lines, the line counter interrupt is reloaded
            if (scan_line == 0 || scan_line > screen_height) {
                hint_counter = REG10_LINE_COUNTER;
            }

            // interrupt line counter
            if (--hint_counter < 0) {
                if (REG0_LINE_INTERRUPT != 0 && scan_line <= screen_height) {
                    hint_pending = 1;
                    if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
                        m68k_update_irq(4);
                }
                hint_counter = REG10_LINE_COUNTER;
            }

            scan_line++;

            // vblank begin at the end of last rendered line
            if (scan_line == screen_height) {
                if (REG1_VBLANK_INTERRUPT != 0) {
                    gwenesis_vdp_status |= STATUS_VIRQPENDING;
                    m68k_set_irq(6);
                }
                z80_irq_line(1);
            }

            if (!is_pal && scan_line == screen_height + 1) {
                z80_irq_line(0);
                // FRAMESKIP every 3rd frame
                drawFrame = frameskip && frame % 3 != 0;
                // if (frameskip && frame % 3 == 0) {
                //     drawFrame = 0;
                // } else {
                //     drawFrame = 1;
                // }
            }

            system_clock += VDP_CYCLES_PER_LINE;
        }

        frame++;
        if (limit_fps) {
            frame_cnt++;
            if (frame_cnt == (is_pal ? 5 : 6)) {
                while (time_us_64() - frame_timer_start < (is_pal ? 20000 * 5 : 16666 * 6)) {
                    busy_wait_at_least_cycles(10);
                }; // 60 Hz
                frame_timer_start = time_us_64();
                frame_cnt = 0;
            }
        }

        if (audio_enabled) {
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
        //gwenesis_sound_submit();

    }
   
    reboot = false;
}
/// @brief
/// Start emulator. Emulator does not run well in DEBUG mode, lots of red screen flicker. In order to keep it running fast enough, we need to run it in release mode or in
/// RelWithDebugInfo mode.
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
    printf("Starting Master System Emulator\n");
    printf("CPU freq: %d\n", clock_get_hz(clk_sys));
    printf("Starting Tinyusb subsystem\n");
    tusb_init();
    isFatalError =  !Frens::initAll(selectedRom, CPUFreqKHz, MARGINTOP, MARGINBOTTOM );
    bool showSplash = true;

    while (true)
    {
        if (strlen(selectedRom) == 0 || reset == true)
        {
            menu("Pico-SMS+", ErrorMessage, isFatalError, showSplash, ".md"); // never returns, but reboots upon selecting a game
        }
        reset = false;
    #if 1
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
        // Todo: Initialize emulator
        printf("Starting game\n");
        init_emulator_mem();
        load_cartridge(ROM_FILE_ADDR);
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
