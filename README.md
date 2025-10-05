# Pico-genesisPlus

A Sega Genesis/Mega Drive emulator for the Raspberry Pi Pico 2 (RP2350). Loads roms from SD-card, uses hdmi for display.

Sound works but is quality is not good. You can disable sound with SELECT + RIGHT on the controller.

Based on [Gwenesis](https://github.com/bzhxx/gwenesis) and [Pico-Megadrive for murmulator board](https://github.com/xrip/pico-megadrive)

The Pico 2 cannot load 4 MB ROMs without PSRAM, and oversized ROMs will not appear in the menu. Game compatibility is not guaranteed. With PSRAM, 4 MB ROMs or higher are supported.

> [!WARNING]  
> Because of the high overclock, the emulator sets the monitor refresh rate to 77.1 Hz. Some monitors may not support this refresh rate. If you experience problems, try a different monitor or TV.
> This does not occur on HSTX based boards like Adafruit Fruit Jam, the monitor refresh rate can be set to 60 Hz.

## Setup

[The emulator is based on Pico-InfonesPlus. Please refer to that repository for how to setup.](https://github.com/fhoedemakers/pico-infonesPlus)


## Supported controllers and in-game button mapping

- Dual Shock/Dual Sense and PSClassic: Cross: A, Circle: B, Triangle: C
- Xbox style controllers (XInput): A: A, B: B, Y: C
- Vintage NES controller: B: A, A: B, Select: C
- ALiExpress SNES USB controller: B: A, A: B, X/Select: C (For this to work, you need to press Y on this controller every time you start a game or boot into the menu) 
- AliExpres NES USB controller: B: B, A: A, Select: C
- Genesis Mini 1 and 2 

To go back to the menu press Select + Start. Genesis mini controller C + Start.
Start + A toggles the framerate display.
Start + Dpad up toggles scanlines.
Select + Right toggles sound on/off. This increases the emulation speed a bit.
** Pimoroni Pico DV Demo base only **:  Select + Left: Toggles between I2S audio and DVI audio. 
**Fruit Jam Only** 
  - Button 1 (on board): Mute audio of built-in speaker. Audio is still outputted to the audio jack.
  - SELECT + UP: Toggle scanlines. 
  - Button 2 (on board) or SELECT + RIGHT: Toggles the VU meter on or off. (NeoPixel LEDs light up in sync with the music rhythm)

In menu:
- D-Pad: Move selection.
- B: Select folder or flash and start rom.
- A: Go back.
- Start: load previous flashed rom.
- Select + dpad up/down change foreground color.
- Select + dpad left/right background color.
- Select + B: Saves background and foreground color to config file.
- Select + A resets background and foreground color to default.

When using Genesis Mini controller, the C button is used for select.


## Building from source

Raspberry Pi Pico 2 arm-s is the only supported config.  Builds for Risc-v currently do not work.

Building for breadboard and PCB configurations. 

````bash
git clone https://github.com/fhoedemakers/pico-genesisPlus.git
cd pico-genesisPlus
git submodule update --init
./bld.sh -c2 -2
````

Building for the Pimoroni [Pico DV Demo Base](https://shop.pimoroni.com/products/pimoroni-pico-dv-demo-base?variant=39494203998291)

````bash
git clone https://github.com/fhoedemakers/pico-genesisPlus.git
cd pico-genesisPlus
git submodule update --init
./bld.sh -c1 -2
````
