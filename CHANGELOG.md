# CHANGELOG

# General Info

Binaries for each configuration and PCB design are at the end of this page. 4MB roms will not load because of the Pico 2 flash size. The menu will not show roms too large for the Pico 2.

Only RP2350 supported.


[See setup section in the readme of the pico-infonesPlus repo on how to install and wire up](https://github.com/fhoedemakers/pico-infonesPlus#pico-setup)

3D-printed case design for PCB: [https://www.thingiverse.com/thing:6689537](https://www.thingiverse.com/thing:6689537). 
For the latest two player PCB 2.0, you need:

- Top_v2.0_with_Bootsel_Button.stl. This allows for software upgrades without removing the cover. (*)
- Base_v2.0.stl
- Power_Switch.stl.

(*) in case you don't want to access the bootsel button on the Pico, you can choose Top_v2.0.stl

3D-printed case design for Waveshare RP2040-PiZero: [https://www.thingiverse.com/thing:6758682](https://www.thingiverse.com/thing:6758682)

# v0.3 release notes

## Features

- Disabled framerate display by default. Can be toggled with START + A.

## Fixes

- lowered overclock speed from 340000 Khz to 324000 Khz. This should fix the issue with the screen not displaying on some televisions. (Did not notice this, because i was using a capture card for testing. which does not have this issue)
 Be aware that there are still displays that cannot display the image.

Sound still not working. Help wanted for this. Not all is tested.
