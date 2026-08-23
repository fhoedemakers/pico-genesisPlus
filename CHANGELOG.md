# CHANGELOG

> Rebuilt emulator core with a new sound engine: audio quality is greatly enhanced, and FM music, PSG noise, DAC samples (the "SEGAAA!" voice) and SGDK game audio all work now. Performance on HSTX boards is greatly improved. 256-wide games fill the screen, and starting games one after another is stable.

# General Info


[Binaries for each configuration and PCB design are at the end of this page](#downloads___).

Only RP2350 (pico 2 based boards) supported. Works best with [Adafruit Fruit Jam](https://www.adafruit.com/product/6200)


[See the readme for how to install and wire up your board](https://github.com/fhoedemakers/pico-genesisPlus/blob/main/README.md#getting-started)


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
>
> Games also **run slower** on PicoDVI boards. See [Performance](#performance) below.

# v0.14 Release notes

The emulator core has been rebuilt from scratch from the upstream
[Gwenesis](https://github.com/bzhxx/gwenesis) sources, with a new sound engine.
Sound is the headline change: it should now be close to what the hardware does,
including the games that had no sound effects at all before.

## Saved games

- **Games with a battery-backed cartridge memory can now save, to the SD card.**
  *Sonic the Hedgehog 3*, *Sonic & Knuckles*, the *Phantasy Star* and *Shining
  Force* games, *Story of Thor*, the NHL series and many others were previously
  unable to keep any progress. Nothing to switch on; the emulator recognises the
  cartridges that have save memory and gives them a file of their own.
- The save is written when you quit the game, when you reset it, and when you open
  the settings menu with SELECT + START — so leaving through **Enter bootsel mode**
  or **Return to emulator selection menu**, which restart the board immediately,
  keeps it too. Nothing is written while you play, so a board switched off mid-game
  loses whatever was saved since the menu was last opened.
- The files are in `/SAVES` on the card, named after the rom with a `.srm`
  extension, in the same format Genesis Plus GX and Kega use — so a save can be
  carried to a PC emulator and back.
- Memory for a game's save is only claimed once the game actually uses it, so the
  many homebrew games that declare a save memory they never touch cost nothing.
  Games that declare a large one — common with SGDK — use PSRAM when the board has
  it; on a board without PSRAM they play normally but cannot save.
- Cartridges with a serial EEPROM rather than a RAM chip (*Wonder Boy in Monster
  World*, *NBA Jam*, *Micro Machines 2*, *Mega Man: The Wily Wars*) still cannot
  save. They need a different chip emulated, which is not in this release.

## PAL games

- **Games for European (PAL) consoles now run at 50 Hz.** The region was read from
  the rom header but never reached the frame timing, so PAL games ran at 60 Hz --
  too fast, with the music and sound pitched up to match. A rom marked as Europe
  now runs at the correct speed. Roms marked for more than one region (`JUE`) keep
  running at 60 Hz, as they would on an American console.

## Sound

- **Rewritten sound engine.** Audio is generated at the YM2612's own sample rate
  (~53 kHz) with cycle-accurate register timing and resampled to 44.1 kHz, instead
  of the low-rate, end-of-frame approach used before. FM music, PSG and DAC samples
  are all affected.
- **The "SEGAAA!" voice and other DAC samples now play correctly.**
- **The PSG noise channel works again.** It was completely silent before, so drums,
  explosions, waves and similar effects were missing from every game.
- **Games developed with [SGDK](https://github.com/Stephane-D/SGDK), such as
  *Xeno Crisis*, now have their sound effects and music.** This closes
  [#11](https://github.com/fhoedemakers/pico-genesisPlus/issues/11).
- Audio no longer drops out in busy scenes: samples are paced to the HDMI/I2S
  output continuously through the frame rather than in one burst per frame.
- On HSTX boards the entire sound synthesis runs on the second processor core,
  next to the video output, leaving the first core free for the emulation itself.

## Video

- **256-pixel-wide (H32) games now fill the screen.** Games such as *Columns* were
  previously shown with black borders on the left and right; they are now scaled to
  the full width, as on real hardware.
- Fixed picture corruption in games that switch between 224 and 240 line modes
  while a frame is being drawn.

## Stability

- Fixed hard faults and out-of-memory errors when leaving a game and starting
  another one. Games can now be started and exited repeatedly.
- Fixed *Space Invaders '91* showing a corrupted screen when started after another
  game had been played.
- Fixed *Xeno Crisis* showing a black screen when started as the second game after
  power-on.
- Fixed memory corruption when loading a file with an odd number of bytes: the
  byte-swap step wrote one byte past the end of the buffer. Real cartridge images
  are always an even number of bytes, so this only happened with a file that was
  not a rom.

<a name="performance"></a>
## Performance

- Full speed (60 fps) on HSTX boards such as the Adafruit Fruit Jam, including in
  games with heavy sound activity.
- **Games run slower on boards without HSTX**, which use the PicoDVI driver for the
  picture. Making that picture takes so much of the board's attention that the
  emulator does not get enough left over, so the action, the music and the sound all
  drag a little -- how much depends on the game. Everything else works normally, and
  the games are still playable. The boards this applies to:
  Pimoroni Pico DV Demo Base (`-c1`), Waveshare RP2350-Zero / PicoNES Mini (`-c6`),
  Waveshare RP2350-USB-A / PicoNES Micro (`-c9`), Spotpear HDMI board (`-c10`) and
  Murmulator M1 (`-c12`). This is not something that can be tuned away: the board is
  already clocked as high as it will go. For full speed use an HSTX board -- the
  Fruit Jam, a Pico 2 or Pimoroni Pico Plus 2 with an Adafruit DVI breakout (also on
  the PicoNES PCB), the Metro RP2350 or the Murmulator M2.

## Controllers

- **NES controllers can now press C.** Controllers on the NES/SNES GPIO port and the
  AliExpress NES USB controller have no third button, which left the Genesis C
  button out of reach. SELECT now doubles as C while a game runs. Every SELECT + ...
  combination keeps working, and C is held back while START is down, so
  SELECT + START still opens the settings menu. USB SNES controllers are unaffected —
  they have a real X button — and nothing changes in the menu.
- **The Genesis C button opens the recently played list in the menu.** C reports
  itself differently from the X button other pads use, so it did not reach the rom
  browser.

## Menu

- **Recently played games.** The menu now remembers the last 20 games you started,
  most recent first. Open the list with Button3 in the rom browser, or with
  **Recently played** in the settings menu (SELECT) — the settings route also works
  on controllers without a third button, such as a NES pad on the GPIO port. Games
  can be started from the list or removed from it with SELECT. The list is stored as
  plain text in `/recent_MD.txt` on the SD card, so it can be edited or deleted from
  a PC.
- **Boards without PSRAM no longer copy the rom to flash when it is already there.**
  Restarting the game you just played, or picking it again from the recently played
  list where it is marked `[READY]`, now takes about a second instead of the several
  seconds of blank screen the flash write used to cost. The emulator verifies the
  image in flash before trusting it, and copies the rom again whenever anything
  differs — including when the file on the card has changed since it was written.
- **Files that are not Mega Drive roms are refused.** The rom list filters on file
  name only, and both `.md` and `.bin` match plenty of files that are not games — a
  markdown README shows up in the list. Picking one used to run the 68000 on random
  data; it now reports the problem and returns to the menu.

## Hardware

- **The PicoNES PCB now takes a Pimoroni Pico Plus 2.** Design **v2.6**
  (`pico_nesPCB_v2.6.zip`, attached to this release) added through-holes, so the
  Pico can be mounted on male headers instead of soldered flat — which is what a
  Pimoroni Pico Plus 2 needs, its SP/CE connector prevents it from lying against
  the PCB.
  That gets you 8 MB of PSRAM on the PCB: games start the moment you select them,
  with none of the flash copying a plain Pico 2 has to do, and larger roms fit.
  No separate binary is needed — `picogenesisPlus_AdafruitDVISD_pico2_arm.uf2`
  reads the flash size and detects PSRAM at boot.
- **The readme has a new Custom PCBs section** covering all three designs — the
  PicoNES, the PicoNES Mini (Waveshare RP2350-Zero) and the PicoNES Micro
  (Waveshare RP2350-USB-A) — with mounting, parts, which binary to flash and the
  3D-printed cases. The old copy of the PCB files has been removed from the
  repository; the current designs live in `pico_shared/PCB` and are attached to
  every release.

## Known limitations

- Games that use interlace mode are still not supported; for example the two-player
  levels of *Sonic the Hedgehog 2* show a blank screen.
- Games cannot save their progress: cartridge save memory is not emulated, so
  titles such as *Sonic 3* and *Phantasy Star IV* play but cannot store a save.
- Non-HSTX (PicoDVI) boards still run the display at 77.1 Hz, see
  [#4](https://github.com/fhoedemakers/pico-genesisPlus/issues/4).
- Games do not run at full speed on non-HSTX (PicoDVI) boards, see
  [Performance](#performance).

## For developers

- Every difference between this emulator's copy of the Gwenesis core and upstream is
  documented in `gwenesis/PORTING.md`, so the core can be updated from upstream again.
- A PC test harness has been added in `hosttest/`. It builds the same emulator core
  for Linux and dumps video frames and per-chip audio to files, which makes it
  possible to find and fix emulation bugs without hardware.

# previous changes

See [HISTORY.md](https://github.com/fhoedemakers/pico-genesisPlus/blob/main/HISTORY.md)

<a name="downloads___"></a>
## Downloads by configuration

Binaries for each configuration are listed below. Binaries for Pico(2) also work for Pico(2)-w. No blinking led however on the -w boards.
There are no risc-v binaries available.


### Standalone boards

>[!NOTE]
> There is no binary for the WaveShare RP2350-PiZero board. See [#7](https://github.com/fhoedemakers/pico-genesisPlus/issues/7)


| Board | Binary | Readme | |
|:--|:--|:--|:--|
| Adafruit Metro RP2350 | [picogenesisPlus_AdafruitMetroRP2350_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitMetroRP2350_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#adafruit-metro-rp2350) | |
| Adafruit Fruit Jam | [picogenesisPlus_AdafruitFruitJam_arm_piousb.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitFruitJam_arm_piousb.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#adafruit-fruit-jam)| |
| Waveshare RP2350-PiZero | Unavailable [#7](https://github.com/fhoedemakers/pico-genesisPlus/issues/7) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#waveshare-rp2040rp2350-pizero-development-board)| [3-D Printed case](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#3d-printed-case-for-rp2040rp2350-pizero) |

### Breadboard

| Board | Binary | Readme |
|:--|:--|:--|
| Pico 2 | [picogenesisPlus_AdafruitDVISD_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitDVISD_pico2_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#raspberry-pi-pico-or-pico-2-setup-with-adafruit-hardware-and-breadboard) |
| Pico 2 W | [picogenesisPlus_AdafruitDVISD_pico2_w_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitDVISD_pico2_w_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#raspberry-pi-pico-or-pico-2-setup-with-adafruit-hardware-and-breadboard) |
| Pimoroni Pico Plus 2 | [picogenesisPlus_AdafruitDVISD_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitDVISD_pico2_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#raspberry-pi-pico-or-pico-2-setup-with-adafruit-hardware-and-breadboard) |


### PicoNES PCB

Designed by John Edgar Park. See the [Custom PCBs section of the readme](https://github.com/fhoedemakers/pico-genesisPlus/blob/main/README.md#picones-pcb).

| Board | Binary | Readme |
|:--|:--|:--|
| Pico 2 | [picogenesisPlus_AdafruitDVISD_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitDVISD_pico2_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#pcb-with-raspberry-pi-pico-or-pico-2-and-pimoroni-pico-plus-2) |
| Pico 2 W | [picogenesisPlus_AdafruitDVISD_pico2_w_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitDVISD_pico2_w_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#pcb-with-raspberry-pi-pico-or-pico-2-and-pimoroni-pico-plus-2) |
| Pimoroni Pico Plus 2 (PCB v2.6 and male headers) | [picogenesisPlus_AdafruitDVISD_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitDVISD_pico2_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#pcb-with-raspberry-pi-pico-or-pico-2-and-pimoroni-pico-plus-2) |

PCB [pico_nesPCB_v2.6.zip](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/pico_nesPCB_v2.6.zip)

Design v2.6 has through-holes, so the Pico can be mounted on male headers. That is the only way to use a Pimoroni Pico Plus 2, which brings 8 MB of PSRAM: games then start immediately instead of after the flash copy.

3D-printed case designs for PCB, by Gavin Knight ([DynaMight1124](https://github.com/DynaMight1124)):

[https://www.thingiverse.com/thing:6689537](https://www.thingiverse.com/thing:6689537). 
For the latest two player PCB 2.0, you need:

- Top_v2.0_with_Bootsel_Button.stl. This allows for software upgrades without removing the cover. (*)
- Base_v2.0.stl
- Power_Switch.stl.
(*) in case you don't want to access the bootsel button on the Pico, you can choose Top_v2.0.stl

> [!IMPORTANT]
> If the Pico is mounted on male headers, download the **latest** top cover. Headers raise the Pico, and only the newest cover leaves room for the USB cable.

### PicoNES Mini PCB (Waveshare RP2350-Zero, PCB required)

Designed by Gavin Knight ([DynaMight1124](https://github.com/DynaMight1124)). See the [Custom PCBs section of the readme](https://github.com/fhoedemakers/pico-genesisPlus/blob/main/README.md#picones-mini-pcb).

| Board | Binary | Readme |
|:--|:--|:--|
| Waveshare RP2350-Zero | [picogenesisPlus_WaveShareRP2350ZeroWithPCB_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_WaveShareRP2350ZeroWithPCB_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#pcb-with-waveshare-rp2040rp2350-zero) |

PCB: [Gerber_PicoNES_Mini_PCB_v2.0.zip](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/Gerber_PicoNES_Mini_PCB_v2.0.zip)

3D-printed case design, also by Gavin Knight:
[https://www.thingiverse.com/thing:7041536](https://www.thingiverse.com/thing:7041536)

### PicoNES Micro PCB (Waveshare RP2350-USB-A)

Designed by Gavin Knight ([DynaMight1124](https://github.com/DynaMight1124)). See the [Custom PCBs section of the readme](https://github.com/fhoedemakers/pico-genesisPlus/blob/main/README.md#picones-micro-pcb).

[Binary](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_WaveShare2350USBA_arm_piousb.uf2)

PCB: [Gerber_PicoNES_Micro_v1.2.zip](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/Gerber_PicoNES_Micro_v1.2.zip)

[Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#pcb-with-waveshare-rp2350-usb-a)

[Build guide](https://www.instructables.com/PicoNES-RaspberryPi-Pico-Based-NES-Emulator/)

### Pimoroni Pico DV

| Board | Binary | Readme |
|:--|:--| :--|
| Pico 2/Pico 2 w | [picogenesisPlus_PimoroniDVI_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_PimoroniDVI_pico2_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#raspberry-pi-pico-or-pico-2-setup-for-pimoroni-pico-dv-demo-base) |
| Pimoroni Pico Plus 2 | [picogenesisPlus_PimoroniDVI_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_PimoroniDVI_pico2_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#raspberry-pi-pico-or-pico-2-setup-for-pimoroni-pico-dv-demo-base) |

> [!NOTE]
> On Pico W and Pico2 W, the CYW43 driver (used only for blinking the onboard LED) causes a DMA conflict with I2S audio on the Pimoroni Pico DV Demo Base, leading to emulator lock-ups. For now, no Pico W or Pico2 W binaries are provided; please use the Pico or Pico2 binaries instead.

### SpotPear HDMI (Untested)

This board has no setup section in the readme. Flash the binary below and wire the board according to its own [documentation](https://spotpear.com/index/product/detail/id/1207.html).

| Board | Binary |
|:--|:--|
| Pico 2/Pico 2 w | [picogenesisPlus_SpotpearHDMI_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_SpotpearHDMI_pico2_arm.uf2) |

### Murmulator M1 (Untested)

For more info about the Murmulator see this website: https://murmulator.ru/ and [#150](https://github.com/fhoedemakers/pico-infonesPlus/issues/150)

| Board | Binary |
|:--|:--|
| Pico 2/Pico 2 w | [picogenesisPlus_MurmulatorM1_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_MurmulatorM1_pico2_arm.uf2) |

### Murmulator M2 (untested)

For more info about the Murmulator see this website: https://murmulator.ru/ and [#150](https://github.com/fhoedemakers/pico-infonesPlus/issues/150)

| Board | Binary |
|:--|:--|
| Pico/Pico w | [picogenesisPlus_MurmulatorM2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_MurmulatorM2_arm.uf2) |

### Other downloads

- Metadata: [GenesisPlusMetadata.zip](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/GenesisPlusMetadata.zip)


Extract the zip file to the root folder of the SD card. Select a game in the menu and press START to show more information and box art. Works for most official released games. Screensaver shows floating random cover art.
