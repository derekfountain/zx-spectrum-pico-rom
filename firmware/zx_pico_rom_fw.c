#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "hardware/timer.h"


/* 1 instruction on the 133MHz microprocessor is 7.5ns */
/* 1 instruction on the 200MHz microprocessor is 5.0ns */
/* 1 instruction on the 270MHz microprocessor is 3.7ns */
/* 1 instruction on the 360MHz microprocessor is 2.8ns */

#define OVERCLOCK 140000
//#define OVERCLOCK 200000
//#define OVERCLOCK 250000
//#define OVERCLOCK 270000
//#define OVERCLOCK 360000

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
/*       Bits 0,1,2,3,4,5       Bits 6,7,8,9,10,11,12          Bit 13                            */
  return ((gpios>>9) & 0x03F) | (((gpios>>16) & 0x07F) << 6) | (((gpios>>26) & 0x001) << 13);
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

const uint32_t  A0_BIT_MASK  = ((uint32_t)1 <<  A0_GP);
const uint32_t  A1_BIT_MASK  = ((uint32_t)1 <<  A1_GP);
const uint32_t  A2_BIT_MASK  = ((uint32_t)1 <<  A2_GP);
const uint32_t  A3_BIT_MASK  = ((uint32_t)1 <<  A3_GP);
const uint32_t  A4_BIT_MASK  = ((uint32_t)1 <<  A4_GP);
const uint32_t  A5_BIT_MASK  = ((uint32_t)1 <<  A5_GP);
const uint32_t  A6_BIT_MASK  = ((uint32_t)1 <<  A6_GP);
const uint32_t  A7_BIT_MASK  = ((uint32_t)1 <<  A7_GP);
const uint32_t  A8_BIT_MASK  = ((uint32_t)1 <<  A8_GP);
const uint32_t  A9_BIT_MASK  = ((uint32_t)1 <<  A9_GP);
const uint32_t A10_BIT_MASK  = ((uint32_t)1 << A10_GP);
const uint32_t A11_BIT_MASK  = ((uint32_t)1 << A11_GP);
const uint32_t A12_BIT_MASK  = ((uint32_t)1 << A12_GP);
const uint32_t A13_BIT_MASK  = ((uint32_t)1 << A13_GP);

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

const uint32_t DBUS_MASK     = ((uint32_t)1 << D0_GP) |
                               ((uint32_t)1 << D1_GP) |
                               ((uint32_t)1 << D2_GP) |
                               ((uint32_t)1 << D3_GP) |
                               ((uint32_t)1 << D4_GP) |
                               ((uint32_t)1 << D5_GP) |
                               ((uint32_t)1 << D6_GP) |
                               ((uint32_t)1 << D7_GP);

#define STORE_SIZE 16384

uint16_t address_indirection_table[ 16384 ];

/*
 * The bits of the bytes in the ROM need shuffling around to match the
 * ordering of the D0-D7 bits on the output GPIOs. See the schematic.
 * Do this now so the pre-converted bytes can be put straight onto
 * the GPIOs at runtime.
 */
void preconvert_rom( uint8_t rom_index )
{
  uint8_t *image_ptr = roms[rom_index];

  uint16_t conv_index;
  for( conv_index=0; conv_index < STORE_SIZE; conv_index++ )
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
 * Loop over all the ROM images in the header file and convert their bit
 * patterns to match the order of bits of the data bus. It's quicker to
 * preconvert these at the start than to fiddle the bits each read cycle.
 */
void preconvert_roms( void )
{
  uint8_t rom_index;
  for( rom_index = 0; rom_index < num_roms; rom_index++ )
  {
    preconvert_rom( rom_index );
  }
}

/* From the timer_lowlevel.c example */
uint64_t get_time_us( void )
{
  uint32_t lo = timer_hw->timelr;
  uint32_t hi = timer_hw->timehr;
  return ((uint64_t)hi << 32u) | lo;
}

void create_indirection_table( void )
{
  uint32_t i;

  for( i=0; i<16384; i++ )
  {
    uint32_t raw_bit_pattern = create_gpios_for_address( i );

    uint32_t packed_bit_pattern = pack_address_gpios( raw_bit_pattern );

    address_indirection_table[packed_bit_pattern] = i;
  }

  return;
}

int main()
{
  bi_decl(bi_program_description("ZX Spectrum Pico ROM board binary."));

  /* All interrupts off */
#ifdef OVERCLOCK
#if OVERCLOCK > 270000
  vreg_set_voltage(VREG_VOLTAGE_1_20);
  sleep_ms(1000);
#endif
#endif

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

  irq_set_mask_enabled( 0xFFFFFFFF, 0 );

  /* Create address indirection table */
  create_indirection_table();

  /* Default to a copy of the ZX ROM (or whatever is in roms slot 0). */
  preconvert_roms();
  uint8_t current_rom_index = 0;
  uint8_t *rom_image_ptr = roms[ current_rom_index ];

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

  /* Ready to go, let the Z80 start */
  gpio_put( PICO_RESET_Z80_GP, 0 );

  uint64_t debounce_timestamp_us = 0;

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
	 * the ROM image is advanced and the ZX is reset. Wait for the button to
	 * be released, then we're done.
	 */

 	gpio_put(LED_PIN, 1);
	
	if( ++current_rom_index == num_roms ) current_rom_index=0;
	rom_image_ptr = roms[ current_rom_index ];

	gpio_put( PICO_RESET_Z80_GP, 1 );
	
	/* Wait for the button to be released. Pause is to debounce */
	while( (gpio_get_all() & PICO_USER_INPUT_BIT_MASK) );
	busy_wait_us_32(1000000);

	gpio_put( PICO_RESET_Z80_GP, 0 );

	gpio_put(LED_PIN, 0);

	continue;
      }
    }

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

