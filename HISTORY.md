# History of changes.

# v0.10 Release notes

## Features

- Settings are saved to /settings_md.dat instead of /settings.dat. This allows to have separate settings files for different emulators (e.g. pico-infonesPlus and pico-peanutGB etc.).
- Added a settings menu.
  - Main menu: press SELECT to open; adjust options without using in-game button combos.
  - In-game: press SELECT+START to open; from here you can also quit from the game.
- Switched to Fatfs R0.16.

## Fixes
- Improved FPS limiter: now uses a fixed timestep approach and initializes timing at the start of emulation, ensuring stable frame rate from the first frame and correct behavior when emulator is overloaded.
- Fruit Jam: Check VU-Meter toggle only once per frame in stead of multiple times per frame.
- Fruit Jam: In game SELECT + RIGHT to toggle VU-Meter now works properly. 
- Show correct buttonlabels in menus.
- removed wrappers for f_chdir en f_cwd, fixed in Fatfs R0.16. (there was a long standing issue with f_chdir and f_cwd not working with exFAT formatted SD cards.)

# v0.9 release notes

## Features
- Fruit Jam: overclock speed set to 378Mhz (was 340 Mhz)
- When (S)NES classic controller is connected, a W is shown next to the flash/PSRAM size

## Fixes

- Fixed not properly working SNES Classic/WII Pro I2C controller. (NES Classic I2C always worked fine). 
- Fruit Jam: Initialisation of (S)NES classic/WII Pro I2C controller will now take place after DAC is initialized. [#4](https://github.com/fhoedemakers/pico-genesisPlus/issues/10). You need to plug-in the controller **after** the intro screen is shown. In case the controller is connected and the DAC fails to initialize, an error screen is shown.

# v0.8 release notes

- Game pad fixes for Genesis style USB controllers. See README.

# v0.7 release notes

- Added support for [Retro-Bit Genesis/Megadrive 8 button Arcade Pad with USB](https://www.retro-bit.com/controllers/genesis/#usb).
- Settings:
  - Version number added. When the version in settings.dat does not match, settings will be reset to defaults.

# v0.6 release notes

## Features

- Audio works now [#3](https://github.com/fhoedemakers/pico-genesisPlus/issues/3), although quality is not good. You can toggle audio on/off with SELECT + RIGHT on the controller. This increases the emulation speed a bit.
- Metadata support added. Download the [metadata]((https://github.com/fhoedemakers/pico-genesisPlus/releases/latest/download/GenesisPlusMetadata.zip)) from the releases below and extract the zip file to the root of your SD card. This adds game titles and box art to the menu. The screensaver also uses box art images.
- Support for [Adafruit Fruit Jam](https://www.adafruit.com/product/6200). This version keeps the monitor at a 60 Hz refresh rate and should work on every monitor. The Fruit Jam has a built-in speaker and a NeoPixel LED strip that can be used as a VU meter.
- [Pimoroni Pico Plus 2](https://shop.pimoroni.com/products/pimoroni-pico-plus-2?variant=42092668289107) support added.
- When PSRAM is available, roms are loaded from SD card to PSRAM instead of flash. This speads the loading time up a lot.
- Uses Pico SDK 2.2.0

## Fixes

- When SD card is prepared on MacOs, the additional metadata files created by MacOs caused several issues. This is fixed now. (Hopefully, because I cannot test this myself) 
- several small fixes and improvements.

# v0.5 release notes (this is a re-release)

- Releases now built with SDK 2.1.1
- Support added for Adafruit Metro RP2350 board. See README for more info. No RISCV support yet.
- Switched to SD card driver pico_fatfs from https://github.com/elehobica/pico_fatfs. This is required for the Adafruit Metro RP2350. Thanks to [elehobica](https://github.com/elehobica/pico_fatfs) for helping making it work for the Pimoroni Pico DV Demo board.
- Besides FAT32, SD cards can now also be formatted as exFAT.
- Nes controller PIO code updated by [@ManCloud](https://github.com/ManCloud). This fixes the NES controller issues on the Waveshare RP2040 - PiZero board. [#8](https://github.com/fhoedemakers/pico_shared/issues/8)
- Board configs are moved to pico_shared.

## Fixes
- Fixed Pico 2 W: Led blinking causes screen flicker and ioctl timeouts [#2](https://github.com/fhoedemakers/pico_shared/issues/2). Solved with in SDK 2.1.1
- WII classic controller: i2c bus instance (i2c0 / i2c1) not hardcoded anymore but configurable via CMakeLists.txt. 

## Features
- Releases now built with SDK 2.1.1
- Support added for Adafruit Metro RP2350 board. See README for more info. No RISCV support yet.
- Switched to SD card driver pico_fatfs https://github.com/elehobica/pico_fatfs. This is required for the Adafruit Metro RP2350. The Pimoroni Pico DV does not work with this updated version and still needs the old version. (see [https://github.com/elehobica/pico_fatfs/issues/7#issuecomment-2817953143](https://github.com/elehobica/pico_fatfs/issues/7#issuecomment-2817953143) ) Therefore, the old version is still included in the repository. (pico_shared/drivers/pio_fatfs) 
    This is configured in CMakeLists.txt file by setting USE_OLD_SDDRIVER to 1.
- Besides FAT32, SD cards can now also be formatted as exFAT.
- Nes controller PIO code updated by [@ManCloud](https://github.com/ManCloud). This fixes the NES controller issues on the Waveshare RP2040 - PiZero board. [#8](https://github.com/fhoedemakers/pico_shared/issues/8)

## Fixes
- Fixed Pico 2 W: Led blinking causes screen flicker and ioctl timeouts [#2](https://github.com/fhoedemakers/pico_shared/issues/2). Solved with in SDK 2.1.1
- WII classic controller: i2c bus instance (i2c0 / i2c1) not hardcoded anymore but configurable via CMakeLists.txt. 