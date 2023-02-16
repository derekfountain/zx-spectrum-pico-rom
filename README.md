# zx-spectrum-pico-rom

## A Pico-based switchable ROM interface for the ZX Spectrum

This is a ROM interface board for the ZX Spectrum. There are lots
of these around, all based on ROM and EEPROM chips. My design is based
on a Raspberry Pi Pico microcontroller programmed to emulate a ROM chip.

The main advantage of doing it this way is that the Pico has enough
memory for several ROM images, and enough flash for as many ROMs as
anyone would likely ever want. A button allows the loaded ROMs to be
cycled.

It looks like this:

[![IMAGE ALT TEXT](http://img.youtube.com/vi/0Ot5mol2f5A/0.jpg)](http://www.youtube.com/watch?v=0Ot5mol2f5A "ZX ROM Interface")

https://youtube.com/shorts/0Ot5mol2f5A?feature=share

The device is permanently enabled; the original ROM chip in the
machine is disabled while the interface is attached. The Pico software
contains the ROM images it's compiled with, the first of which would
typically be the original ZX ROM from 1982. I've added a diagnostics
ROM and the Gosh Wonderful ROM since they are free to redistribute.
Games ROMs are not included in the source release.

Distributed as free software and hardware design under the GPL. The ROM
images have their own licences, see the ROMs/ directory.

Derek Fountain
February 2023