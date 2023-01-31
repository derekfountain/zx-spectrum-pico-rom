# Crude makefile to create the switcher ROM image. Build the Z80 code with the
# minimum of extras, then pad it out to 16K. The padding is for testing under
# Fuse which expects ROM images to be 16K.
#

sw.rom : switcher.c
	zcc +z80 -vn -startup=31 -clib=sdcc_iy switcher.c -o sw -create-app -s -m --list --c-code-in-asm -pragma-include:zpragma.inc
	dd if=/dev/zero of=pad0 bs=16384 count=1
	cat sw.rom pad0 > padded_rom
	dd if=padded_rom of=test_switcher.rom bs=16384 count=1

clean:
	rm -f *~ test_switcher.rom sw sw.rom padded_rom *.bin *.lis *.sym *.map pad0
