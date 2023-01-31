// https://github.com/z88dk/z88dk/wiki/NewLib--Platform--Embedded

// Start at address 0
//
#pragma output CRT_ORG_CODE = 0

// No preamble, nothing is needed
//
#pragma output CRT_INCLUDE_PREAMBLE = 0

// Uncompressed ROM model, so it can be tweaked dynamically
//
#pragma output CRT_MODEL = 0

// Stack at top of 16K RAM
//
#pragma output REGISTER_SP = 0x7fff

// No heap
//
#pragma output CLIB_MALLOC_HEAP_SIZE = 0
#pragma output CLIB_STDIO_HEAP_SIZE = 0

// No IM2 vector table
//
#pragma output CRT_ORG_VECTOR_TABLE = 0

// No RST code
//
#pragma output CRT_ENABLE_RST = 0x00

// DI at start, leave it like that
//
#pragma output CRT_ENABLE_EIDI = 0x01

// No interrupts
//
#pragma output CRT_INTERRUPT_MODE = -1