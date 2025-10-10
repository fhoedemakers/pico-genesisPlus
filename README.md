# Pico-genesisPlus

A Sega Genesis/Mega Drive emulator for the Raspberry Pi Pico 2 (RP2350). Loads roms from SD-card, uses hdmi for display. Works best with [Adafruit Fruitjam](https://www.adafruit.com/product/6200)

Create a FAT32 or exFAT formatted SD card and copy your NES roms and optional metadata on to it. It is possible to organize your roms into different folders. Then insert the SD Card into the card slot. Needless to say you must own all the roms you put on the card.

Sound works but its quality is not good. You can disable sound with SELECT + RIGHT on the controller.

Games that use interlace mode like are not supported. For example "Sonic the Hedgehog 2" uses interlace mode for some levels. Those levels show a blank screen.

Based on [Gwenesis](https://github.com/bzhxx/gwenesis) and [Pico-Megadrive for murmulator board](https://github.com/xrip/pico-megadrive)

Roms that are to big to load in flash or PSRAM are not listed.

> [!WARNING]  
> **Overclock Notice**  
>  
> **Only HSTX based boards like Adafruit Fruit Jam can run the display at 60 Hz.**
> Boards with no HSTX use the PicoDVI driver, which due to the high overclock, sets the monitor refresh rate to **77.1 Hz**.
> Some monitors may **not support this refresh rate**, which can cause display or unsupported signal issues.  
> This can't be lowered using PicoDVI. See [#4](https://github.com/fhoedemakers/pico-genesisPlus/issues/4)
>  
> If you experience problems, try using a **different monitor or TV**.  
>  
> **Note:** This limitation does **not** apply to **HSTX-based boards** (e.g., *Adafruit Fruit Jam*), where the monitor refresh rate can be set to **60 Hz**.

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
  - Button 2 (on board): Toggles the VU meter on or off. (NeoPixel LEDs light up in sync with the music rhythm)

In menu:
- D-Pad: Move selection.
- B: Select folder or flash and start rom.
- A: Go back.
- Start: shows metadata (if available) for selected rom.
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

Building for the Adafruit [Fruit Jam](https://www.adafruit.com/product/6200)

````bash
git clone https://github.com/fhoedemakers/pico-genesisPlus.git
cd pico-genesisPlus
git submodule update --init
./bld.sh -c8
````
