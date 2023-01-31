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
  zx_cls( PAPER_WHITE|INK_BLACK );
  zx_border(0);

  uint8_t border_colour = INK_BLACK;
  while(1)
  {
    zx_border(border_colour);

    if( border_colour++ == INK_WHITE )
      border_colour = INK_BLACK;
  }

}
