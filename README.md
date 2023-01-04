# zx-spectrum-pico-rom

## A Pico-based switchable ROM interface for the ZX Spectrum

There are lots of ROM interfaces for the ZX Spectrum. Sinclair's
Interface 2 is the obvious one from the 1980s, but there were
clones of that, and there are quite a few variations available
today. It's quite an easy sort of thing to build.

I thought I'd have a go at it, but with a slight variation: I
wanted to see if I could make one using a Raspberry Pi Pico
microcontroller. In theory it's relatively simple: tie the Spectrum's
address bus to 16 of the Picos GPIOs and have the Pico sit
spinning looking for addresses in the range 0x0000 to 0x3FFF. When
one arrives, look up the value for that address in a ROM image and
apply those 8 bits to 8 more Pico GPIO lines configured as outputs.
Piece of cake.

Only, not really. The Spectrum is +5V, the Pico is 3V3, so every bus
or signal pin needs level shifting, down on the way into the Pico,
up on the way out. GPIO count is also pretty tight. 24 needed for
the two buses, /MREQ and /RD are required to ensure the interface
only responds to memory reads from the ROM. But it should fit.

There's also the question of speed. Pg8 of the Z80 manual says
that the timing for M1, the instruction fetch, is most time critical.
The data is sampled off the bus one and a half clocks after MREQ
goes low. On the Spectrum's 3.5MHz Z80 that's 4.28571428571e-07
seconds. That's about 430ns. Pico should manage a table lookup and
setting 8 GPIO outputs in that time without too much trouble.

The data bus lines need to be set to high impedence when the
interface isn't outputing a value. So either set them as inputs
on the Pico, or turn off the outputs of the level shifter which
returns them to the Z80.

Also /ROMCS, the Spectrum's line which forces the Spectrum's internal
ROM offline. I presume I can just pull that high? But if I want to
toggle the device's ROM on and off (i.e. have the Spectrum's normal
ROM accessible when the device isn't being used) then that line
needs to be on a Pico GPIO output.

What about changing ROM image? Some jump switches, maybe? Or maybe
I can talk to the device and have the Pico do things dynamically?
After a ROM switch the Z80 will need resetting.

The Pico boots slower than the Spectrum. The ROM needs to be there
when the Z80 starts up, which it won't be. So I need to hold the
Z80 offline until the Pico is up and running.

So all possible, just about. I've created a board and sent it off
for fabrication. Let's see if it works. Details can follow.

4th Jan 2023
