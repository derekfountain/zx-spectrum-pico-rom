# zx-spectrum-lower-border-nmi

## A Pico-based NMI at the lower border point for the ZX Spectrum

I previously did some work looking at the use of the Z80's Non Maskable Interrupt with the ZX Spectrum. That work is [here](https://github.com/derekfountain/zx-spectrum-pico-rom/tree/main/firmware_nmi).

This sub-project builds on that idea, using RP2040 PIOs to generate a non maskable interrupt at a precisely defined point in the frame generation.

### Background

As most Spectrum aficionados know, the best time to redraw the screen and update sprites is when the border is being drawn. That way the screen won't flicker. Only, the Spectrum's hardware only has a single timer, which is the 50Hz signal the ULA emits when the video frame is about to be generated - i.e. when the raster beam is at the top of the screen. What might be useful is a different interrupt which fires just as the ULA finishes drawing the screen data, and just as it's about to start drawing the lower border. Synching a program to that signal would give quite a bit more time to update the screen.

The RP2040 on the Pico has the PIOs, which are very good at very precise signal handling. This project uses the PIOs to generate a NMI signal exactly as the ULA is about to start drawing the lower border.

### The Video Frame

A frame of the Spectrum's video signal looks like this:

![alt text](../images/frame_trace_original.png "Frame trace")

There are 3 clearly defined areas of this signal, as annotated here:

![alt text](../images/frame_trace_annotated.png "Frame trace annotated")

The goal of this project is to generate a NMI signal at the exact point the bottom border is about to be drawn, as shown by the purple line here:

![alt text](../images/frame_trace_nmi.png "Frame trace NMI")

### The Result

The result is that it works. Look carefully at this photo of a Spectrum screen and you'll notice a thin magenta line being drawn just underneath the lowest line of text:

![alt text](../images/magenta_line.jpg "Magenta line")

That line is being drawn in the lower border by a NMI handler routine which just changes the border colour to magenta, then changes it back to white. The thin line thus created in the border is permanent, it just sits there.

This isn't a particularly useful example but it shows the NMI is being generated at exactly the right time, and the Z80 is running a simple handler for it.

## How it works



[Derek Fountain](https://www.derekfountain.org/zxspectrum.php), February 2025