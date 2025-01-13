# Pico Genesis Plus

A Sega Genesis/Mega Drive emulator for the Raspberry Pi Pico 2 (0RP2350). Loads roms from SD-card, uses hdmi for display.

Sound currently nog working. Help wanted.

Based on [Gwenesis](https://github.com/bzhxx/gwenesis)

Due to the flash size of the Raspberry Pi pico 2, 4MB roms cannot be loaded. Also not every game is guaranteed to work.

## Setup

[The emulator is based on Pico-InfonesPlus. Please refer to that repository for how to setup.](https://github.com/fhoedemakers/pico-infonesPlus)

## Building from source

Raspberry Pi Pico 2 arm-s is the only supported config.  Builds for Risc-v currently do not work.

Building for breadboard and PCB configurations. 

````bash
git clone
cd pico-genesisPlus
git submodule update --init
./bld.sh -c2 -2
````

Building for the Pimoroni [Pico DV Demo Base](https://shop.pimoroni.com/products/pimoroni-pico-dv-demo-base?variant=39494203998291)

````bash
git clone
cd pico-genesisPlus
git submodule update --init
./bld.sh -c1 -2
````
