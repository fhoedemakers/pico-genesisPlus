# CHANGELOG

> Support for the pico-bootLoader bootloader, HDMI audio on HSTX boards, a reworked settings menu with a controller test screen, and improved USB controller support.

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

# v0.13 Release notes

The notes below cover all changes since **v0.11**. The items that were first published in v0.12 are repeated here, so that this list is complete for users upgrading directly from v0.11.

## pico-bootLoader

- This emulator can now be used with the new [pico-bootLoader](https://github.com/fhoedemakers/pico-bootLoader). The bootloader lets you keep several emulators, and a *Doom* port, on one board and choose which one to start from an on-screen menu at power-on. Switching between them no longer requires a computer.
- The bootloader and an SD card archive containing this emulator are available on the [pico-bootLoader releases page](https://github.com/fhoedemakers/pico-bootLoader/releases). Installation is described in the readme of that project.
- When the emulator is started from the bootloader, the settings menu contains an extra item, **Return to emulator selection**, to return to the boot menu.
- The binaries listed at the end of this page are standalone versions and are installed via BOOTSEL as before. To build a bootloader version yourself, add `-DBUILD_FOR_BOOTLOADER=ON` to the cmake command line, or use `./bld.sh -2 -c <HW_CONFIG> -b`.

## Video and HDMI

- For the boards that use HSTX instead of PicoDVI, HDMI audio is supported via the HSTX video driver. Thanks to [@fliperama86](https://github.com/fliperama86) for the [pico_hdmi](https://github.com/fliperama86/pico_hdmi) driver that made this possible and for helping out.
  - Adafruit Fruit Jam.
  - Murmulator M2.
- Other RP2350 configurations that use HSTX (GPIO 12 - 19) instead of PicoDVI:
  - [Breadboard](https://github.com/fhoedemakers/pico-infonesPlus?tab=readme-ov-file#raspberry-pi-pico-or-pico-2-setup-with-adafruit-hardware-and-breadboard)
  - [PCB](https://github.com/fhoedemakers/pico-infonesPlus?tab=readme-ov-file#pcb-with-raspberry-pi-pico-or-pico-2)
  - [Adafruit Metro RP2350](https://github.com/fhoedemakers/pico-infonesPlus?tab=readme-ov-file#adafruit-metro-rp2350)

  All the other boards still use PicoDVI. To enable audio over HDMI, make sure external audio is disabled in the settings menu.
- HDMI audio on HSTX boards is more reliable: audio dropouts are resolved and more TVs and AV receivers are accepted.
- Fixed dots and dotted lines that could appear in the picture on some HDMI monitors.
- New **Display Mode** setting on HSTX boards, to choose between HDMI and DVI output. DVI has slightly lower latency but carries no audio.

## Settings menu

- New layout, with a SAVE / CANCEL / DEFAULT row and a scrollable list of options.
- The scanlines on/off option has been replaced by **Screen Mode**, which offers 1:1 with and without scanlines.
- New **Scanline Type** option on HSTX boards: *Simple* or *LCD*.
- New **Controller Test** screen. It shows a gamepad on screen that follows the controller you last pressed a button on, and lists the connected controllers. This makes it possible to check wiring and button mappings without starting a game. Hold SELECT+START for 2 seconds to leave the screen.
- Added an option to enter BOOTSEL mode for flashing firmware.
- Added an option to return to the boot menu when the emulator was started from the bootloader.
- The game list now starts in the `/roms/MD` folder instead of the root of the SD card. When that folder does not exist, the root folder is used. Placing your ROMs in `/roms/MD` is the recommended layout.
- When leaving a subfolder, that folder stays selected in the list instead of returning to the top.
- Settings are saved correctly when a game is reset, and settings changed with in-game button combinations are saved when returning to the menu.
- The software version is shown on the splash screen.
- Note: the format of the settings file has changed. Existing settings in `/settings_md.dat` are reset to their default values the first time this version starts.

## Controllers

- Retro-bit Mega Drive Arcade pad: the X, Y, Z, L and R buttons now work.
- DualShock 4 / DualSense: the L2 and R2 triggers now act as L and R.
- PlayStation Classic controller: the Square button now works.
- Wii Classic controller: the L and R shoulder buttons now work, as do ZL and ZR.
- SNES controllers can now be used on the controller port of the PCB and breadboard setups. NES controllers keep working as before and are recognised automatically.
- USB keyboard: added V, Q and W.
- Fruit Jam: fixed the sound chip failing to start when an SNES Classic Mini controller is connected at power-on. That controller can now also be used from the moment the menu appears.

## Games

- Games developed with [SGDK](https://github.com/Stephane-D/SGDK), such as *Xeno Crisis*, now start and are playable. Sound effects in these games are still missing. [#11](https://github.com/fhoedemakers/pico-genesisPlus/issues/11)
- Starting a second game without switching the board off in between no longer leaves data of the previous game behind.

## Other

- More stable SD card access.
- Several stability fixes in the menu and the video output.

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
