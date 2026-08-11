# Pico-genesisPlus

A Sega Genesis/Mega Drive emulator for the Raspberry Pi Pico 2 (RP2350). Loads roms from SD-card, uses hdmi for display. Works best with [Adafruit Fruitjam](https://www.adafruit.com/product/6200)

Create a FAT32 (recommended) or exFAT formatted SD card and copy your Genesis/Mega Drive roms and [optional metadata](#using-metadata) on to it. It is possible to organize your roms into different folders. Then insert the SD Card into the card slot. Needless to say you must own all the roms you put on the card.

Games that use interlace mode like are not supported. For example "Sonic the Hedgehog 2" uses interlace mode for some levels. Those levels show a blank screen.

Games cannot save their progress: cartridge save memory is not emulated, so games like "Sonic the Hedgehog 3" and "Phantasy Star IV" play fine but cannot store a save.

Based on [Gwenesis](https://github.com/bzhxx/gwenesis).

Roms that are too big to load in flash or PSRAM are not listed. Files that are not Mega Drive roms are refused with a message, instead of starting the emulator on whatever the file happens to contain.

> [!WARNING]
> **Only HSTX boards (e.g. Adafruit Fruit Jam) deliver proper 60 Hz output and universal monitor compatibility; non‑HSTX (PicoDVI) builds set the refresh rate to 77.1 Hz and may be rejected by some displays.**  
> The high refresh rate on non-HSTX boards is related to the high overclocking of the RP2350.
> This can't be lowered using PicoDVI. See [#4](https://github.com/fhoedemakers/pico-genesisPlus/issues/4)
> If you experience problems, try using a **different monitor or TV**.  

## SD card setup 

1. Prepare an SD card formatted as FAT32 (preferred) or exFAT
2. Transfer Genesis/Megadrive ROM files to the card, preferably in /roms/MD (subdirectory organization is supported).
3. Optionally include [metadata files](#using-metadata) for game information
4. Insert the SD card into the device
5. Use the menu to browse, select, and play games. Your settings are automatically persisted to the SD card.

## Setup

[The emulator is based on Pico-InfonesPlus. Please refer to that repository for how to setup.](https://github.com/fhoedemakers/pico-infonesPlus)


## Supported controllers and in-game button mapping

- Dual Shock/Dual Sense and PSClassic. 
- Xbox style controllers (XInput)
- Vintage NES controller: **Note** No C-button of its own, so SELECT acts as C while a game runs (this goes for any controller on the NES/SNES GPIO port). SELECT + START still opens the settings menu.
- ALiExpress SNES USB controller: **Note** To enable B-button you need to press Y on this controller every time you start a game or boot into the menu. 
- AliExpress NES USB controller: **Note** No C-button of its own, so SELECT acts as C while a game runs. SELECT + START still opens the settings menu.
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

Controllers without a third button get one while a game is running: SELECT doubles as Button3 (the Genesis C button). SELECT keeps all its other in-game jobs, and C is not sent while START is held, so SELECT + START still opens the settings menu.

This applies to every controller on the NES/SNES GPIO port — those are read as 8 buttons (A, B, Select, Start and the d-pad), so a SNES pad plugged in there has no way to reach C either — and to the AliExpress NES USB controller. USB SNES controllers are unaffected: they have a real X button.

## Menu 
Gamepad buttons:
- UP/DOWN: Next/previous item in the menu.
- LEFT/RIGHT: next/previous page.
- Button2: Open folder/flash and start game.
- Button1: Back to parent folder.
- START: Show [metadata](#using-metadata) and box art (when available)
- Button3: Show the list of [recently played games](#recently-played-games).
- SELECT: Opens a setting menu. Here you can change settings like screen mode, scanlines, framerate display, menu colors and other board specific settings. Settings can also be changed in-game by pressing some button combinations as explained below. The settings menu can also be opened in-game.

When using an USB-Keyboard:
- Cursor keys: Up, Down, left, right
- Z: Back to parent folder
- X: Open Folder/flash and start a game
- S: Show [metadata](#using-metadata) and box art (when available).
- C: Show the list of [recently played games](#recently-played-games).
- A: acts as the select button.

## Recently played games

The menu remembers the last 20 games you started, most recent first. Press Button3 in the rom browser to open the list, or pick **Recently played** in the settings menu (SELECT). The settings menu route also works on controllers without a third button, such as a NES pad on the GPIO port.

In the list:
- UP/DOWN: Move through the games.
- Button2: Start the highlighted game.
- SELECT: Remove it from the list.
- START: Show [metadata](#using-metadata) and box art (when available).
- Button1: Back to the rom browser.

Starting a game from the rom browser adds it to the list, or moves it back to the top if it is already there. Picking a game that is no longer on the SD card reports it and offers SELECT to drop it. The list lives in `/recent_MD.txt` in the root of the card and is plain text, so it can be edited or deleted from a PC.

The list is only available from the rom browser, not while a game is running.

On boards without PSRAM, roms are copied into flash before they start. The game whose rom is already in flash is marked `[READY]`: starting it skips the copy and begins in about a second instead of the usual several. Any other game is copied to flash as before. This also applies to starting a game the normal way from the rom browser.

## Emulator (in game)
Gamepad buttons:
- SELECT + START, Xbox button: opens the settings menu. From there, you can:
  - Quit the game and return to the SD card menu
  - Adjust settings and resume your game.
- **Controllers on the NES/SNES GPIO port, and the AliExpress NES USB controller**: SELECT on its own acts as the C button, since these have no third button available. All the SELECT + ... combinations below keep working, and holding START suppresses C so the settings menu can still be opened with SELECT + START.
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

### Emulator core and PC test harness

The emulator core in `gwenesis/` is a copy of the upstream
[Gwenesis](https://github.com/bzhxx/gwenesis) sources with a small set of port
changes. Every one of those changes is documented in
[gwenesis/PORTING.md](gwenesis/PORTING.md), so the core can be refreshed from
upstream later without losing them. The Pico-specific glue (sound engine, memory
management, frame loop) lives in `port/`.

`hosttest/` builds the same emulator core as a normal Linux program, which makes it
possible to investigate emulation bugs without hardware. It renders frames to PPM
files and writes the audio to WAV files, and runs under AddressSanitizer.

````bash
./hosttest/build.sh                                   # build hosttest/gen_host
./hosttest/gen_host <rom.md> 600 60 hosttest/out      # 600 frames, dump every 60th
python3 hosttest/ppm2png.py 'hosttest/out/*.ppm'      # PPM -> PNG
````

This writes `mixed.wav` (the final 44.1 kHz output) plus `ym.wav` and `psg.wav`
(the FM and PSG chips separately, at their native rate), which is useful when
tracking down a sound problem in one specific chip.

To reproduce bugs that only appear when a game is started after another one, run a
warm-up game first; the second game's output must be identical to starting it on
its own:

````bash
GEN_FIRST_ROM=roms/sonic.md ./hosttest/gen_host roms/other.md 400 200 hosttest/out
````

Test roms placed in `hosttest/roms/` are ignored by git.
