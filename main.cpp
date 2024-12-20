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


namespace
{
    constexpr uint32_t CPUFreqKHz = 252000;
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
            // reset margin to give menu more screen space
            dvi_->getBlankSettings().top = 4 * 2;
            dvi_->getBlankSettings().bottom = 4 * 2;
            scaleMode8_7_ = Frens::applyScreenMode(ScreenMode::NOSCANLINE_8_7);
            menu("Pico-SMS+", ErrorMessage, isFatalError, showSplash, ".sms .gg"); // never returns, but reboots upon selecting a game
        }
        reset = false;
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
        
        // Todo: Initialize emulator
        printf("Starting game\n");
        init_emulator_mem();
        load_cartridge(ROM_FILE_ADDR);
        process();
        free_emulator_mem();
        // system_shutdown();
        selectedRom[0] = 0;
        showSplash = false;
    }

    return 0;
}
