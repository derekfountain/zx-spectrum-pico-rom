/*
 * Switcher ROM image, loads at address 0 as a normal ROM which the 
 * Pico can deliver.
 */

#include <stdint.h>
#include <arch/zx.h>
#include <string.h>
#include <z80.h>
#include <intrinsic.h>

/* Don't use statics, the RAM model isn't set up for that */

void main(void)
{
  uint8_t *screen_addr = (uint8_t*)0x5800;
  uint8_t blink = 0;

  while(1)
  {
    if( blink)
      *screen_addr = (PAPER_WHITE|INK_BLACK);
    else
      *screen_addr = (PAPER_GREEN|INK_WHITE);
    
    blink = !blink;
  }
}
