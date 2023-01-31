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

void print_str( uint8_t x, uint8_t y, uint8_t *str );
void print_char( uint8_t *screen_addr, uint8_t c );

/* main() has to come first, with no CRT the first function goes in at 0x0000 */
void main(void)
{
  zx_cls( PAPER_WHITE|INK_BLACK );
  zx_border(0);

  print_str( 0,  1, " Raspberry Pi Pico ROM Emulator " );
  print_str( 0,  2, "              v1.1              " );

  print_str( 0, 11, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" );
  print_str( 0, 13, "          is loading...         " );

  uint8_t border_colour = INK_BLACK;
  while(1)
  {
    zx_border(border_colour);

    if( border_colour++ == INK_WHITE )
      border_colour = INK_BLACK;
  }

}

void print_str( uint8_t x, uint8_t y, uint8_t *str )
{
  uint8_t i;

  for( i=0; i<strlen(str); i++ )
  {
    uint8_t *addr = zx_cxy2saddr(x+i, y);
    print_char( addr, *(str+i) );
  }
}

void print_char( uint8_t *screen_addr, uint8_t c )
{
  extern uint8_t font[];

  uint8_t i;
  for(i=0;i<8;i++)
  {
    *screen_addr = font[((c-' ')*8)+i];
    screen_addr += 256;
  }
}

