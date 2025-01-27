/*
 * ZX Pico Lower Border Experimentation Firmware, a Raspberry Pi Pico
 * based ZX Spectrum research project
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
 * cmake -DCMAKE_BUILD_TYPE=Debug ..
 * make -j10
 * sudo openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -c "program ./zx_pico_nmi_lower_border.elf verify reset exit"
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"

#include "lower_border_timer.pio.h"

const uint8_t LED_PIN = PICO_DEFAULT_LED_PIN;

/* This pin sits high; it's switched low to trigger an NMI */
const uint8_t  NMI_GP                   = 27;

/* INT input from the Spectrum */
const uint8_t  INT_GP                   = 15;

int main()
{
  bi_decl(bi_program_description("ZX Spectrum Pico NMI lower border binary."));

  int timer_sm;
  PIO timer_pio = pio1;
  uint timer_offset;

  gpio_set_function(INT_GP, GPIO_FUNC_PIO1);    
  gpio_set_function(NMI_GP, GPIO_FUNC_PIO1);    

  /* Set up the PIO state machine */
  timer_sm        = pio_claim_unused_sm(timer_pio, true);
  timer_offset    = pio_add_program(timer_pio, &lower_border_timer_program);
  lower_border_timer_program_init(timer_pio, timer_sm, timer_offset, INT_GP, NMI_GP);

  /* Set the clock divider to get a more manageable frequency (must be done ater initialisation) */
  pio_sm_set_clkdiv(timer_pio, timer_sm, 1000.0);

  /* Set it running */
  pio_sm_set_enabled(timer_pio, timer_sm, true);

  /* Blip LED to show we're running */
  gpio_init(LED_PIN);  gpio_set_dir(LED_PIN, GPIO_OUT);
  while(1)
  {
    gpio_put(LED_PIN, 1);
    busy_wait_us_32(250000);
    gpio_put(LED_PIN, 0);
    busy_wait_us_32(250000);
  }
}



#if 0
/* Blip the result pin, shows on scope */
gpio_put( TEST_OUTPUT_GP, 1 ); busy_wait_us_32(5);
gpio_put( TEST_OUTPUT_GP, 0 ); busy_wait_us_32(5);

gpio_put( TEST_OUTPUT_GP, 1 ); busy_wait_us_32(1000);
gpio_put( TEST_OUTPUT_GP, 0 ); busy_wait_us_32(1000);
__asm volatile ("nop");
#endif

