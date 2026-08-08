# CHANGELOG

> Rebuilt emulator core with a new sound engine: FM music, PSG noise, DAC samples (the "SEGAAA!" voice) and SGDK game audio all work now. 256-wide games fill the screen, and starting games one after another is stable.

# General Info


[Binaries for each configuration and PCB design are at the end of this page](#downloads___).

Only RP2350 (pico 2 based boards) supported. Works best with [Adafruit Fruit Jam](https://www.adafruit.com/product/6200)


[See setup section in the readme of the pico-infonesPlus repo on how to install and wire up](https://github.com/fhoedemakers/pico-infonesPlus#pico-setup)


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

# v0.14 Release notes

The emulator core has been rebuilt from scratch from the upstream
[Gwenesis](https://github.com/bzhxx/gwenesis) sources, with a new sound engine.
Sound is the headline change: it should now be close to what the hardware does,
including the games that had no sound effects at all before.

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

## Performance

- Full speed (60 fps) on HSTX boards such as the Adafruit Fruit Jam, including in
  games with heavy sound activity.

## Known limitations

- Games that use interlace mode are still not supported; for example the two-player
  levels of *Sonic the Hedgehog 2* show a blank screen.
- Games cannot save their progress: cartridge save memory is not emulated, so
  titles such as *Sonic 3* and *Phantasy Star IV* play but cannot store a save.
- Non-HSTX (PicoDVI) boards still run the display at 77.1 Hz, see
  [#4](https://github.com/fhoedemakers/pico-genesisPlus/issues/4).

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


### PCB Pico2

| Board | Binary | Readme |
|:--|:--|:--|
| Pico 2 | [picogenesisPlus_AdafruitDVISD_pico2_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitDVISD_pico2_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#pcb-with-raspberry-pi-pico-or-pico-2) |
| Pico 2 W | [picogenesisPlus_AdafruitDVISD_pico2_w_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_AdafruitDVISD_pico2_w_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#pcb-with-raspberry-pi-pico-or-pico-2) |

PCB [pico_nesPCB_v2.1.zip](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/pico_nesPCB_v2.1.zip)

3D-printed case designs for PCB:

[https://www.thingiverse.com/thing:6689537](https://www.thingiverse.com/thing:6689537). 
For the latest two player PCB 2.0, you need:

- Top_v2.0_with_Bootsel_Button.stl. This allows for software upgrades without removing the cover. (*)
- Base_v2.0.stl
- Power_Switch.stl.
(*) in case you don't want to access the bootsel button on the Pico, you can choose Top_v2.0.stl

### PCB WS2350-Zero (PCB required)

| Board | Binary | Readme |
|:--|:--|:--|
| Waveshare RP2350-Zero | [picogenesisPlus_WaveShareRP2350ZeroWithPCB_arm.uf2](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/picogenesisPlus_WaveShareRP2350ZeroWithPCB_arm.uf2) | [Readme](https://github.com/fhoedemakers/pico-infonesPlus/blob/main/README.md#pcb-with-waveshare-rp2040rp2350-zero) |

PCB: [Gerber_PicoNES_Mini_PCB_v2.0.zip](https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/Gerber_PicoNES_Mini_PCB_v2.0.zip)

3D-printed case designs for PCB WS2350-Zero:
[https://www.thingiverse.com/thing:7041536](https://www.thingiverse.com/thing:7041536)

### PCB Waveshare RP2350-USBA with PCB
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
