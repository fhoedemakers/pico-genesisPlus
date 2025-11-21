# Pico-genesisPlus

A Sega Genesis/Mega Drive emulator for the Raspberry Pi Pico 2 (RP2350). Loads roms from SD-card, uses hdmi for display. Works best with [Adafruit Fruitjam](https://www.adafruit.com/product/6200)

Create a FAT32 (recommended) or exFAT formatted SD card and copy your NES roms and [optional metadata](#using-metadata) on to it. It is possible to organize your roms into different folders. Then insert the SD Card into the card slot. Needless to say you must own all the roms you put on the card.

Audio works, but quality is currently poor. When audio is enabled the emulator uses frame skipping to maintain performance. You can also enable or disable frame skip in the Settings menu. (Press SELECT in the menu to open the settings screen)

Games that use interlace mode like are not supported. For example "Sonic the Hedgehog 2" uses interlace mode for some levels. Those levels show a blank screen.

Also games developed with the popular [SGDK](https://github.com/Stephane-D/SGDK) mostly do not work. See [#11](https://github.com/fhoedemakers/pico-genesisPlus/issues/11)

Based on [Gwenesis](https://github.com/bzhxx/gwenesis) and [Pico-Megadrive for murmulator board](https://github.com/xrip/pico-megadrive)

Roms that are too big to load in flash or PSRAM are not listed.

> [!WARNING]
> **Only HSTX boards (e.g. Adafruit Fruit Jam) deliver proper 60 Hz output and universal monitor compatibility; non‑HSTX (PicoDVI) builds set the refresh rate to 77.1 Hz and may be rejected by some displays.**  
> The high refresh rate on non-HSTX boards is related to the high overclocking of the RP2350.
> This can't be lowered using PicoDVI. See [#4](https://github.com/fhoedemakers/pico-genesisPlus/issues/4)
> If you experience problems, try using a **different monitor or TV**.  


## Setup

[The emulator is based on Pico-InfonesPlus. Please refer to that repository for how to setup.](https://github.com/fhoedemakers/pico-infonesPlus)


## Supported controllers and in-game button mapping

- Dual Shock/Dual Sense and PSClassic. 
- Xbox style controllers (XInput)
- Vintage NES controller: **Note** No C-button
- ALiExpress SNES USB controller: **Note** To enable B-button you need to press Y on this controller every time you start a game or boot into the menu. 
- AliExpress NES USB controller: **Note** No C-button
- Genesis Mini 1 C button is also SELECT. (Not ideal)
- Genesis Mini 2 Mode button is SELECT
- [Retro-Bit 8 button Arcade Pad with USB](https://www.retro-bit.com/controllers/genesis/#usb). Mode button is SELECT
- Fruit Jam: SNES Classic/WII classic Pro controllers over I2C. Connect controller to [Adafruit Wii Nunchuck Breakout Adapter - Qwiic / STEMMA QT](https://www.adafruit.com/product/4836).
- USB Keyboard


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
- SELECT: Opens a setting menu. Here you can change settings like screen mode, scanlines, framerate display, menu colors and other board specific settings. Settings can also be changed in-game by pressing some button combinations as explained below. The settings menu can also be opened in-game.

When using an USB-Keyboard:
- Cursor keys: Up, Down, left, right
- Z: Back to parent folder
- X: Open Folder/flash and start a game
- S: Show [metadata](#using-metadata) and box art (when available).
- A: acts as the select button.

## Emulator (in game)
Gamepad buttons:
- SELECT + START, Xbox button: opens the settings menu. From there, you can:
  - Quit the game and return to the SD card menu
  - Adjust settings and resume your game.
- SELECT + UP/SELECT + DOWN: switches screen modes.
- SELECT + Button1/Button2: toggle rapid-fire.
- START + Button2: Toggle framerate display
- **Pimoroni Pico DV Demo Base only**: SELECT + LEFT: Switch audio output to the connected speakers on the line-out jack of the Pimoroni Pico DV Demo Base. The speaker setting will be remembered when the emulator is restarted.
- **Fruit Jam Only** 
  - SELECT + UP: Toggle scanlines. 
  - pushbutton 1 (on board): Mute audio of built-in speaker. Audio is still outputted to the audio jack.
  - pushbutton 2 (on board) or SELECT + RIGHT: Toggles the VU meter on or off. (NeoPixel LEDs light up in sync with the music rhythm)
- **Genesis Mini Controller**: When using a Genesis Mini controller with 3 buttons, press C for SELECT. 8 buttons Genesis controllers press MODE for SELECT
- **USB-keyboard**: When using an USB-Keyboard
  - Cursor keys: up, down, left, right
  - A: SELECT
  - S: START
  - Z: Button1
  - X: Button2
  - C: Button3

## Using metadata.

Download the metadata pack from the [releases page](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/GenesisPlusMetadata.zip) and extract its contents to the root of the SD card. It contains box art and game info for many games. The metadata is used in the menu to show box art and game info when a rom is selected. Press START to view the information. When the screensaver is started, random box art is shown.

<img width="1920" height="1080" alt="Screenshot 2025-11-07 06-00-18" src="https://github.com/user-attachments/assets/2d9a7663-1ea2-46b4-81d9-70c8f7478b5f" />

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
