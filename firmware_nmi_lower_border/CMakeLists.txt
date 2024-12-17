cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(zx_pico_nmi_lower_border C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(zx_pico_nmi_lower_border
  zx_pico_nmi_lower_border.c
)

target_link_libraries(zx_pico_nmi_lower_border
                      pico_multicore
                      hardware_pio
                      pico_stdlib)

pico_generate_pio_header(zx_pico_nmi_lower_border ${CMAKE_CURRENT_LIST_DIR}/lower_border_timer.pio)

pico_add_extra_outputs(zx_pico_nmi_lower_border)
