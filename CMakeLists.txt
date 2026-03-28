cmake_minimum_required(VERSION 3.13)

# Must be set before pico_sdk_import and project() so the SDK picks up
# the correct board headers (CYW43 WiFi, lwIP, etc.)
set(PICO_BOARD pico_w CACHE STRING "Target board")

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_water C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(pico_water
    main.c
    gpio_control.c
    rest_api.c
    led.c
)

target_include_directories(pico_water PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pico_water
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
)

# Output over USB serial; disable UART to keep pin 0/1 free
pico_enable_stdio_usb(pico_water 1)
pico_enable_stdio_uart(pico_water 0)

# Generate .uf2, .bin, .hex etc.
pico_add_extra_outputs(pico_water)
