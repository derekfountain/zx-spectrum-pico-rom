/*
 * ZX Pico ROM Firmware, a Raspberry Pi Pico based ZX Spectrum ROM emulator
 * Copyright (C) 2024 Derek Fountain
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * cmake -DCMAKE_BUILD_TYPE=Debug
 * make -j10
 * sudo openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -c "program ./zx_pico_rom_fw.elf verify reset exit"
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "hardware/timer.h"


/* 1 instruction on the 133MHz microprocessor is 7.5ns */
/* 1 instruction on the 140MHz microprocessor is 7.1ns */
/* 1 instruction on the 150MHz microprocessor is 6.6ns */
/* 1 instruction on the 200MHz microprocessor is 5.0ns */

/* With the debounce counter code in place, 190MHz is needed */
#define OVERCLOCK 200000

#include "roms.h"

const uint8_t LED_PIN = PICO_DEFAULT_LED_PIN;

/*
 * These pin values are the GPxx ones in green background on the pinout diagram.
 * See schematic for how the signals are fed into the Pico's GPIOs
 */
const uint8_t A0_GP          = 11;
const uint8_t A1_GP          = 12;
const uint8_t A2_GP          = 13;
const uint8_t A3_GP          = 14;
const uint8_t A4_GP          = 20;
const uint8_t A5_GP          = 21;
const uint8_t A6_GP          = 22;
const uint8_t A7_GP          = 26;
const uint8_t A8_GP          = 19;
const uint8_t A9_GP          = 18;
const uint8_t A10_GP         = 17;
const uint8_t A11_GP         = 16;
const uint8_t A12_GP         = 10;
const uint8_t A13_GP         = 9;

/*
 * Given the GPIOs with an address bus value on them, this packs the
 * 14 address bits down into the least significant 14 bits
 */
inline uint16_t pack_address_gpios( uint32_t gpios )
{
  /*     Bits 0,1,2,3,4,5       Bits 6,7,8,9,10,11,12     Bit 13                   */
  return ((gpios>>9) & 0x03F) | ((gpios>>10) & 0x1FC0) | ((gpios & 0x4000000) >> 13);
}

/*
 * Given value i, this calculates the pattern of the GPIOs if
 * value i were to appear on the Z80 address bus.
 */
uint32_t create_gpios_for_address( uint32_t i )
{
  uint32_t gpio_pattern = 
    ( ((i & 0x0001) >>  0) <<  A0_GP ) |
    ( ((i & 0x0002) >>  1) <<  A1_GP ) |
    ( ((i & 0x0004) >>  2) <<  A2_GP ) |
    ( ((i & 0x0008) >>  3) <<  A3_GP ) |
    ( ((i & 0x0010) >>  4) <<  A4_GP ) |
    ( ((i & 0x0020) >>  5) <<  A5_GP ) |
    ( ((i & 0x0040) >>  6) <<  A6_GP ) |
    ( ((i & 0x0080) >>  7) <<  A7_GP ) |
    ( ((i & 0x0100) >>  8) <<  A8_GP ) |
    ( ((i & 0x0200) >>  9) <<  A9_GP ) |
    ( ((i & 0x0400) >> 10) << A10_GP ) |
    ( ((i & 0x0800) >> 11) << A11_GP ) |
    ( ((i & 0x1000) >> 12) << A12_GP ) |
    ( ((i & 0x2000) >> 13) << A13_GP );

  return gpio_pattern;
}


const uint8_t  D0_GP          = 0;
const uint8_t  D1_GP          = 1;
const uint8_t  D2_GP          = 2;
const uint8_t  D3_GP          = 4; 
const uint8_t  D4_GP          = 6;
const uint8_t  D5_GP          = 3;
const uint8_t  D6_GP          = 5;
const uint8_t  D7_GP          = 7;

const uint32_t  D0_BIT_MASK  = ((uint32_t)1 <<  D0_GP);
const uint32_t  D1_BIT_MASK  = ((uint32_t)1 <<  D1_GP);
const uint32_t  D2_BIT_MASK  = ((uint32_t)1 <<  D2_GP);
const uint32_t  D3_BIT_MASK  = ((uint32_t)1 <<  D3_GP);
const uint32_t  D4_BIT_MASK  = ((uint32_t)1 <<  D4_GP);
const uint32_t  D5_BIT_MASK  = ((uint32_t)1 <<  D5_GP);
const uint32_t  D6_BIT_MASK  = ((uint32_t)1 <<  D6_GP);
const uint32_t  D7_BIT_MASK  = ((uint32_t)1 <<  D7_GP);

const uint8_t  ROM_ACCESS_GP            = 8;
const uint32_t ROM_ACCESS_BIT_MASK      = ((uint32_t)1 << ROM_ACCESS_GP);

/* This pin triggers a transistor which shorts the Z80's /RESET to ground */
const uint8_t  PICO_RESET_Z80_GP        = 28;

/* This pin sits low; it's switched to high. That's the user input */
const uint8_t  PICO_USER_INPUT_GP       = 27;
const uint32_t PICO_USER_INPUT_BIT_MASK = ((uint32_t)1 << PICO_USER_INPUT_GP);

/* NMI output to Spectrum */
const uint8_t NMI_GP         = 15;

const uint32_t DBUS_MASK     = ((uint32_t)1 << D0_GP) |
                               ((uint32_t)1 << D1_GP) |
                               ((uint32_t)1 << D2_GP) |
                               ((uint32_t)1 << D3_GP) |
                               ((uint32_t)1 << D4_GP) |
                               ((uint32_t)1 << D5_GP) |
                               ((uint32_t)1 << D6_GP) |
                               ((uint32_t)1 << D7_GP);

/*
 * The 14 address bus bits arrive on the GPIOs in a weird pattern which is
 * defined by the edge connector layout and the board design. Shifting all
 * 14 bits into place works, but it's slow, it needs all 14 masks and shifts
 * done each byte read from ROM. The Pico needs a significant overclock to
 * manage that.
 *
 * This is an optimisation. Take the 14 address bus bits, which are scattered
 * in the 32bit GPIO value, and shift them down into the lowest 14 bits of
 * a 16 bit word. They're not in order A0 to A13, they're in some weird order.
 * So this is a conversion table. The index into this is the weird 14 bit
 * value, the value at that offset into this table is the actual address bus
 * value the weird value represents.
 *
 * e.g. you might read the GPIOs, shuffle the 14 bits down and end with,
 * say, 0x032A. Look up entry 0x032A in this table and find, say, 0x0001.
 * That means when 0x32A appears on the mixed up address bus, the actual
 * address bus value the Z80 has passed in is 0x0001. It wants that byte
 * from the ROM.
 *
 * This table is filled in at the start; a lookup is done here for each
 * ROM byte read.
 */
uint16_t address_indirection_table[ 16384 ];


/*
 * The bits of the bytes in the ROM need shuffling around to match the
 * ordering of the D0-D7 bits on the output GPIOs. See the schematic.
 * Do this now so the pre-converted bytes can be put straight onto
 * the GPIOs at runtime.
 */
void preconvert_rom( uint8_t *image_ptr, uint32_t length )
{
  uint16_t conv_index;
  for( conv_index=0; conv_index < length; conv_index++ )
  {
    uint8_t rom_byte = *(image_ptr+conv_index);
    *(image_ptr+conv_index) =  (rom_byte & 0x87)       |        /* bxxx xbbb */
                              ((rom_byte & 0x08) << 1) |        /* xxxb xxxx */
                              ((rom_byte & 0x10) << 2) |        /* xbxx xxxx */
                              ((rom_byte & 0x20) >> 2) |        /* xxxx bxxx */
                              ((rom_byte & 0x40) >> 1);         /* xxbx xxxx */
  }
}


/*
 * Populate the address bus indirection table.
 */
void create_indirection_table( void )
{
  uint32_t i;

  /*
   * Loop over all 16384 address the Z80 might ask for. For each one calculate
   * the pattern of the GPIOs when that value is on the address bus. Then pack
   * that pattern down into the lowest 14 bits. That's the value which is
   * found for each read byte, so the value at that offset into the table is
   * the original value which will match what the Z80's after.
   */
  for( i=0; i<16384; i++ )
  {
    uint32_t raw_bit_pattern = create_gpios_for_address( i );

    uint32_t packed_bit_pattern = pack_address_gpios( raw_bit_pattern );

    address_indirection_table[packed_bit_pattern] = i;
  }

  return;
}


/* Just the one ROM image for this version */
const uint8_t *rom_image_ptr = __ROMs_48_original_rom;


/*
 * This is called by an alarm function. It lets the Z80 run by pulling the
 * Pico's controlling GPIO low
 */
int64_t start_z80_alarm_func( alarm_id_t id, void *user_data )
{
  gpio_put( PICO_RESET_Z80_GP, 0 );
  return 0;
}


int main()
{
  bi_decl(bi_program_description("ZX Spectrum Pico ROM board binary."));

#ifdef OVERCLOCK
  set_sys_clock_khz( OVERCLOCK, 1 );
#endif

  /*
   * Set up Pico's Z80 reset pin, hold this at 0 to let Z80 run.
   * Set and hold 1 here to hold Spectrum in reset at startup until we're good
   * to provide its ROM
   */
  gpio_init( PICO_RESET_Z80_GP );  gpio_set_dir( PICO_RESET_Z80_GP, GPIO_OUT );
  gpio_put( PICO_RESET_Z80_GP, 1 );

  /* All interrupts off except the timers */
  irq_set_mask_enabled( 0xFFFFFFFF, 0 );
  irq_set_mask_enabled( 0x0000000F, 1 );

  /* Create address indirection table, this is the address bus optimisation  */
  create_indirection_table();

  /* Switch the bits in the ROM bytes around, this is the data bus optimisation */
  preconvert_rom( __ROMs_48_original_rom, 16384 );

  /* Pull the buses to zeroes */
  gpio_init( A0_GP  ); gpio_set_dir( A0_GP,  GPIO_IN );  gpio_pull_down( A0_GP  );
  gpio_init( A1_GP  ); gpio_set_dir( A1_GP,  GPIO_IN );  gpio_pull_down( A1_GP  );
  gpio_init( A2_GP  ); gpio_set_dir( A2_GP,  GPIO_IN );  gpio_pull_down( A2_GP  );
  gpio_init( A3_GP  ); gpio_set_dir( A3_GP,  GPIO_IN );  gpio_pull_down( A3_GP  );
  gpio_init( A4_GP  ); gpio_set_dir( A4_GP,  GPIO_IN );  gpio_pull_down( A4_GP  );
  gpio_init( A5_GP  ); gpio_set_dir( A5_GP,  GPIO_IN );  gpio_pull_down( A5_GP  );
  gpio_init( A6_GP  ); gpio_set_dir( A6_GP,  GPIO_IN );  gpio_pull_down( A6_GP  );
  gpio_init( A7_GP  ); gpio_set_dir( A7_GP,  GPIO_IN );  gpio_pull_down( A7_GP  );
  gpio_init( A8_GP  ); gpio_set_dir( A8_GP,  GPIO_IN );  gpio_pull_down( A8_GP  );
  gpio_init( A9_GP  ); gpio_set_dir( A9_GP,  GPIO_IN );  gpio_pull_down( A9_GP  );
  gpio_init( A10_GP ); gpio_set_dir( A10_GP, GPIO_IN );  gpio_pull_down( A10_GP );
  gpio_init( A11_GP ); gpio_set_dir( A11_GP, GPIO_IN );  gpio_pull_down( A11_GP );
  gpio_init( A12_GP ); gpio_set_dir( A12_GP, GPIO_IN );  gpio_pull_down( A12_GP );
  gpio_init( A13_GP ); gpio_set_dir( A13_GP, GPIO_IN );  gpio_pull_down( A13_GP );

  gpio_init( D0_GP  ); gpio_set_dir( D0_GP,  GPIO_OUT ); gpio_put( D0_GP, 0 );
  gpio_init( D1_GP  ); gpio_set_dir( D1_GP,  GPIO_OUT ); gpio_put( D1_GP, 0 );
  gpio_init( D2_GP  ); gpio_set_dir( D2_GP,  GPIO_OUT ); gpio_put( D2_GP, 0 );
  gpio_init( D3_GP  ); gpio_set_dir( D3_GP,  GPIO_OUT ); gpio_put( D3_GP, 0 );
  gpio_init( D4_GP  ); gpio_set_dir( D4_GP,  GPIO_OUT ); gpio_put( D4_GP, 0 );
  gpio_init( D5_GP  ); gpio_set_dir( D5_GP,  GPIO_OUT ); gpio_put( D5_GP, 0 );
  gpio_init( D6_GP  ); gpio_set_dir( D6_GP,  GPIO_OUT ); gpio_put( D6_GP, 0 );
  gpio_init( D7_GP  ); gpio_set_dir( D7_GP,  GPIO_OUT ); gpio_put( D7_GP, 0 );

  /* Input from logic hardware, indicates the ROM is being accessed by the Z80 */
  gpio_init( ROM_ACCESS_GP ); gpio_set_dir( ROM_ACCESS_GP, GPIO_IN );
  gpio_pull_down( ROM_ACCESS_GP );

  /* Set up Pico's user input pin, pull to zero, switch will send it to 1 */
  gpio_init( PICO_USER_INPUT_GP ); gpio_set_dir( PICO_USER_INPUT_GP, GPIO_IN );
  gpio_pull_down( PICO_USER_INPUT_GP );

  /* Set NMI output inactive */
  gpio_init( NMI_GP ); gpio_set_dir( NMI_GP, GPIO_OUT );
  gpio_put( NMI_GP, 1 );

  /* Blip LED to show we're running */
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  int signal;
  for( signal=0; signal<2; signal++ )
  {
    gpio_put(LED_PIN, 1);
    busy_wait_us_32(250000);
    gpio_put(LED_PIN, 0);
    busy_wait_us_32(250000);
  }
  gpio_put(LED_PIN, 0);


  uint64_t debounce_timestamp_us = 0;


  /*
   * Ready to go, give it a few milliseconds for this Pico code to get into
   * its main loop, then let the Z80 start
   */
  add_alarm_in_ms( 5, start_z80_alarm_func, NULL, 0 );

  uint8_t  user_button_pressed = 0;
  uint32_t debounce_counter = 0;
  uint8_t  nmi_asserted = 0;
  while(1)
  {
    register uint32_t gpios_state;

    /*
     * Spin while the hardware is saying at least one of A14, A15 and MREQ is 1.
     * ROM_ACCESS is active low - if it's 1 then the ROM is not being accessed.
     * Also break out when the user button is pressed.
     */
    while( ( (gpios_state=gpio_get_all()) & ROM_ACCESS_BIT_MASK )
	   &&
	   ( (gpios_state & PICO_USER_INPUT_BIT_MASK) == 0 ) );

    if( !(gpios_state & ROM_ACCESS_BIT_MASK) )
    {
      register uint16_t raw_bit_pattern = pack_address_gpios( gpios_state );

      register uint16_t rom_address = address_indirection_table[raw_bit_pattern];

      register uint8_t rom_value = *(rom_image_ptr+rom_address);

      /* The level shifter is enabled via hardware, so just set the GPIOs */
      gpio_put_masked( DBUS_MASK, rom_value );

      /*
       * Spin until the Z80 releases MREQ indicating the read is complete.
       * ROM_ACCESS is active low - if it's 0 then the ROM is still being accessed.
       */
      while( (gpio_get_all() & ROM_ACCESS_BIT_MASK) == 0 );

      /*
       * Just leave the value there. The level shifter gets turned off by hardware
       * which means the value will disappear from the Z80's view when the Z80's
       * read is complete. At which point the GPIO's state doesn't matter.
       */
    }

    /*
     * The above section deals with a ROM read if their was one. It will have
     * waited until the Z80 is finished (MREQ going inactive), then we drop to
     * here. There's a small bit of time when other stuff can be done, but the
     * Z80 is possibly already preparing the next ROM read, so whatever happens
     * below this point needs to be quick.
     * Note that the Pico example get_time_us() function, which I was using for
     * the switch debounce, takes 1.5uS, which is too slow.
     */

    /*
     * If NMI was asserted, turn it off. NMI causes the Z80 to jump to 0x0066
     * which is in the ROM, so it's guaranteed that when NMI is asserted the
     * Pico will come back round this loop next iteration. Turning NMI off here
     * means it was asserted for one Z80 memory read instruction.
     */
    if( nmi_asserted )
    {
      gpio_put(NMI_GP, 1);
      nmi_asserted = 0;

      gpio_put(LED_PIN, 0);
    }

    /* If the user button is pressed, fire the NMI */
    if( gpios_state & PICO_USER_INPUT_BIT_MASK )
    {
      if( !user_button_pressed )
      {
	/*
	 * Debounce with a counter, the switch is a bit noisy. Wait for this
	 * many iterations of the loop to complete with the button down before
	 * deciding the button is really down.
	 * Value was empirically found.
	 */
	if( ++debounce_counter == 1000 )
	{
	  gpio_put(LED_PIN, 1);
	
	  user_button_pressed = 1;

	  gpio_put(NMI_GP, 0);
	  nmi_asserted = 1;
	}
      }
    }
    else if( (gpios_state & PICO_USER_INPUT_BIT_MASK) == 0 )
    {	
      if( user_button_pressed )
      {
	/* Was pressed, now isn't, it's been released */
	user_button_pressed = 0;
	debounce_counter = 0;
      }
      else
      {
	/* Button is not pressed (or has bounced "off"), reset the counter */
	debounce_counter = 0;
      }
    }

  } /* Infinite loop */

}



#if 0
/* Blip the result pin, shows on scope */
gpio_put( TEST_OUTPUT_GP, 1 ); busy_wait_us_32(5);
gpio_put( TEST_OUTPUT_GP, 0 ); busy_wait_us_32(5);

gpio_put( TEST_OUTPUT_GP, 1 ); busy_wait_us_32(1000);
gpio_put( TEST_OUTPUT_GP, 0 ); busy_wait_us_32(1000);
__asm volatile ("nop");
#endif

