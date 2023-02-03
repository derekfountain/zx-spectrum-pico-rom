#define ZX_IF1_VERSION 0

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

#define OVERCLOCK 150000
//#define OVERCLOCK 200000

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

#if !ZX_IF1_VERSION

/* This pin sits low; it's switched to high. That's the user input */
const uint8_t  PICO_USER_INPUT_GP       = 27;
const uint32_t PICO_USER_INPUT_BIT_MASK = ((uint32_t)1 << PICO_USER_INPUT_GP);

#endif

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

void preconvert_rom_image( uint8_t rom_index )
{
  preconvert_rom( cycle_roms[rom_index].rom_data, cycle_roms[rom_index].rom_size ); 
}

/*
 * Loop over all the ROM images in the header file and convert their bit
 * patterns to match the order of bits of the data bus. It's quicker to
 * preconvert these at the start than to fiddle the bits each read cycle.
 */
void preconvert_roms( void )
{
  uint8_t rom_index;
  for( rom_index = 0; rom_index < num_cycle_roms; rom_index++ )
  {
    preconvert_rom_image( rom_index );
  }  
}


/* From the timer_lowlevel.c example */
uint64_t get_time_us( void )
{
  uint32_t lo = timer_hw->timelr;
  uint32_t hi = timer_hw->timehr;
  return ((uint64_t)hi << 32u) | lo;
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


#if !ZX_IF1_VERSION

/* Default to a copy of the ZX ROM (or whatever is in cycle roms slot 0). */
uint8_t current_rom_index = 0;
uint8_t *rom_image_ptr = cycle_roms[ 0 ].rom_data;

#else

uint8_t *rom_image_ptr = __ROMs_48_original_rom;

#endif


/*
 * This is called by an alarm function at startup. It just resets
 * the Z80 by pulling the Pico's GPIO low
 */
int64_t initial_reset_alarm_func( alarm_id_t id, void *user_data )
{
  gpio_put( PICO_RESET_Z80_GP, 0 );
  return 0;
}


#if !ZX_IF1_VERSION

/*
 * Switcher function. When the user clicks the button to move to the next ROM
 * the utility switcher ROM is loaded which presents a banner saying which
 * ROM is about to appear. That stays for a moment, then this function runs.
 * This switches that switcher ROM out and loads the next ROM in the cycle.
 */
int64_t switcher_alarm_func( alarm_id_t id, void *user_data )
{
  gpio_put(LED_PIN, 1);
	
  if( ++current_rom_index == num_cycle_roms ) current_rom_index=0;
  rom_image_ptr = cycle_roms[ current_rom_index ].rom_data;

  gpio_put( PICO_RESET_Z80_GP, 1 );
  busy_wait_us_32(5000);
  gpio_put( PICO_RESET_Z80_GP, 0 );

  gpio_put(LED_PIN, 0);

  return 0;
}

#endif


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

  /* Buffer to run the preconverted switcher ROM from */
  uint8_t sw_rom_converted[ sw_rom_len ];

  /* Switch the bits in the ROM bytes around, this is the data bus optimisation */
  preconvert_roms();

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

#if !ZX_IF1_VERSION

  /* Set up Pico's user input pin, pull to zero, switch will send it to 1 */
  gpio_init( PICO_USER_INPUT_GP ); gpio_set_dir( PICO_USER_INPUT_GP, GPIO_IN );
  gpio_pull_down( PICO_USER_INPUT_GP );

#endif

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


#if !ZX_IF1_VERSION

  uint64_t debounce_timestamp_us = 0;

#endif


  /*
   * Ready to go, give it a few milliseconds for this Pico code to get into
   * its main loop, then let the Z80 start
   */
  add_alarm_in_ms( 50, initial_reset_alarm_func, NULL, 0 );


  while(1)
  {
    register uint32_t gpios_state;

#if !ZX_IF1_VERSION

    /*
     * Spin while the hardware is saying at least one of A14, A15 and MREQ is 1.
     * ROM_ACCESS is active low - if it's 1 then the ROM is not being accessed.
     * Also break out when the user button is pressed.
     */
    while( ( (gpios_state=gpio_get_all()) & ROM_ACCESS_BIT_MASK )
	   &&
	   ( (gpios_state & PICO_USER_INPUT_BIT_MASK) == 0 ) );

#else

    /*
     * Spin while the hardware is saying at least one of A14, A15 and MREQ is 1.
     * ROM_ACCESS is active low - if it's 1 then the ROM is not being accessed.
     */
    while( (gpios_state=gpio_get_all()) & ROM_ACCESS_BIT_MASK );

#endif


#if !ZX_IF1_VERSION

    /* If the user button is pressed, change ROM then reset */
    if( gpios_state & PICO_USER_INPUT_BIT_MASK )
    {
      /* Debounce pause, the switch is a bit noisy */
      if( (get_time_us() - debounce_timestamp_us) < 50000 )
      {
	debounce_timestamp_us = get_time_us();
      }
      else
      {
	/*
	 * Switch ROMs. The Pico's LED is lit to show that something is happening,
	 * the switcher ROM image is loaded to present a label on the Spectrum
	 * screen, and the ZX is reset. Wait for the button to be released.
	 */

 	gpio_put(LED_PIN, 1);
	
	/*
	 * Run utility ROM, this isn't one of the cycled ones. The original switcher
	 * ROM image is copied into the buffer, then the label put in place, then
	 * the new switcher ROM image is converted to match the data bus pins.
	 * That's the image that runs.
	 * The string update is a hack; I just found where the original xxxxx string
	 * landed in the switcher Z80 code and hardcoded the offset here. Could do
	 * better. :)
	 */
	memcpy( sw_rom_converted, sw_rom, sw_rom_len );
	memcpy( sw_rom_converted+290, cycle_roms[ current_rom_index ].rom_switcher_label, 32 );
	preconvert_rom( sw_rom_converted, sw_rom_len );
	rom_image_ptr = sw_rom_converted;

	gpio_put( PICO_RESET_Z80_GP, 1 );
	
	/* Wait for the button to be released. Pause is to debounce */
	while( (gpio_get_all() & PICO_USER_INPUT_BIT_MASK) );
	busy_wait_us_32(600000);

	gpio_put( PICO_RESET_Z80_GP, 0 );

	gpio_put(LED_PIN, 0);

	/*
	 * Show the switcher ROM for a moment, then use an alarm to switch in the
	 * next ROM we're going to cycle to
	 */
	add_alarm_in_ms( 1200, switcher_alarm_func, NULL, 0 );

	continue;
      }
    }

#endif

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

#if ZX_IF1_VERSION

    if( (rom_address == 0x0008) || (rom_address == 0x1708) )
    {
      gpio_put(LED_PIN, 1);
      rom_image_ptr = __ROMs_if1_rom;
    }
    else if( rom_address == 0x0700 )
    {
      rom_image_ptr = __ROMs_48_original_rom;
      gpio_put(LED_PIN, 0);
    }

#endif

    /*
     * Just leave the value there. The level shifter gets turned off by hardware
     * which means the value will disappear from the Z80's view when the Z80's
     * read is complete. At which point the GPIO's state doesn't matter.
     */

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

