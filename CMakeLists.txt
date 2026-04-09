cmake_minimum_required(VERSION 3.13)

# Must be set before pico_sdk_import and project() so the SDK picks up
# the correct board headers (CYW43 WiFi, lwIP, etc.)
set(PICO_BOARD pico_w CACHE STRING "Target board")

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_water C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

execute_process(
    COMMAND git -C ${CMAKE_CURRENT_LIST_DIR} describe --tags --always --dirty
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown")
endif()

add_executable(pico_water
    src/main.c
    src/gpio_control.c
    src/rest_api.c
    src/led.c
    src/wifi_manager.c
    src/status.c
    src/log_buffer.c
)

target_include_directories(pico_water PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}        # config.h, lwipopts.h
    ${CMAKE_CURRENT_LIST_DIR}/src    # module headers
)

target_compile_definitions(pico_water PRIVATE FW_VERSION="${GIT_VERSION}")

target_link_libraries(pico_water
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_adc
    hardware_watchdog
)

target_compile_options(pico_water PRIVATE -fstack-usage)
target_compile_definitions(pico_water PRIVATE PICO_STACK_SIZE=4096)

# Output over USB serial; disable UART to keep pin 0/1 free
pico_enable_stdio_usb(pico_water 1)
pico_enable_stdio_uart(pico_water 0)

# Generate .uf2, .bin, .hex etc.
pico_add_extra_outputs(pico_water)
