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
> **Only HSTX based boards like Adafruit Fruit Jam work on every monitor!**
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

- Dual Shock/Dual Sense and PSClassic. **Note** Dual Sense seems not to work on Adafruit Fruit Jam.
- Xbox style controllers (XInput)
- Vintage NES controller: **Note** No C-button
- ALiExpress SNES USB controller: **Note** To enable B-button you need to press Y on this controller every time you start a game or boot into the menu. 
- AliExpress NES USB controller: **Note** No C-button
- Genesis Mini 1 C button is also SELECT. (Not ideal)
- Genesis Mini 2 Mode button is SELECT
- [Retro-Bit 8 button Arcade Pad with USB](https://www.retro-bit.com/controllers/genesis/#usb). Mode button is SELECT
- Fruit Jam: SNES Classic/WII classic Pro controllers over I2C. Connect controller to [Adafruit Wii Nunchuck Breakout Adapter - Qwiic / STEMMA QT](https://www.adafruit.com/product/4836). 


|     | (S)NES | Genesis | XInput | Dual Shock/Sense | 
| --- | ------ | ------- | ------ | ---------------- |
| Button1 | B  |    A    |   A    |    X             |
| Button2 | A  |    B    |   B    |   Circle         |
| Button3 | X (SNES only)  |    C    |   Y    |   Triangle       |
| Select  | select | Mode (C on 3 button controller) | Select | Select     |

## Menu 
Gamepad buttons:
- UP/DOWN: Next/previous item in the menu.
- LEFT/RIGHT: next/previous page.
- Button2: Open folder/flash and start game.
- Button1: Back to parent folder.
- START: Show [metadata](#using-metadata) and box art (when available)

The colors in the menu can be changed and saved:
  - SELECT + UP/DOWN changes the foreground color.
  - SELECT + LEFT/RIGHT changes the background color.
  - SELECT + Button1 saves the colors. On RP2040, screen will flicker when saved.
  - SELECT + Button2 resets the colors to default. (Black on white)

When using an USB-Keyboard:
- Cursor keys: Up, Down, left, right
- Z: Back to parent folder
- X: Open Folder/flash and start a game
- S: Show [metadata](#using-metadata) and box art (when available).
- A: acts as the select button.

## Emulator (in game)
Gamepad buttons:
- SELECT + START, Xbox button: Resets back to the SD Card menu. Game saves are saved to the SD card.
- SELECT + UP/SELECT + DOWN: switches screen modes.
- SELECT + Button1/Button2: toggle rapid-fire.
- START + Button2: Toggle framerate display
- **Pimoroni Pico DV Demo Base only**: SELECT + LEFT: Switch audio output to the connected speakers on the line-out jack of the Pimoroni Pico DV Demo Base. The speaker setting will be remembered when the emulator is restarted.
- **Fruit Jam Only** 
  - pushbutton 1 (on board): Mute audio of built-in speaker. Audio is still outputted to the audio jack.
  - SELECT + UP: Toggle scanlines. 
  - pushbutton 2 (on board) or SELECT + RIGHT: Toggles the VU meter on or off. (NeoPixel LEDs light up in sync with the music rhythm)
- **Genesis Mini Controller**: When using a Genesis Mini controller with 3 buttons, press C for SELECT. 8 buttons Genesis controllers press MODE for SELECT
- **USB-keyboard**: When using an USB-Keyboard
  - Cursor keys: up, down, left, right
  - A: SELECT
  - S: START
  - Z: Button1
  - X: Button2

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
