set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB APP_SOURCES
    "*.c*"
)

add_executable(firmware
    ${APP_SOURCES}
)

target_compile_definitions(firmware PRIVATE
    DVI_DEFAULT_SERIAL_CONFIG=${DVI_DEFAULT_SERIAL_CONFIG}
)

target_link_libraries(firmware
    pico_stdlib
    pico_multicore
    pico_util
    hardware_gpio
    hardware_uart
    FreeRTOS
    tinyusb_host
    tinyusb_board
    protobuf
)

add_definitions(-DPICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

# No USB CDC device: the USB-C connector is reserved for keyboard host mode.
pico_enable_stdio_uart(firmware 0)
pico_enable_stdio_usb(firmware 0)

pico_add_extra_outputs(firmware)

# Build date / commit metadata is retained from the upstream VGM build.
string(TIMESTAMP FW_BUILD_DATE "%Y-%m-%d")
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE FW_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE FW_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE FW_GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE)

if ("${FW_GIT_TAG}" STREQUAL "")
    set(FW_GIT_TAG "none")
endif()

set(INFO_FLAGS
    -DFW_BUILD_DATE="${FW_BUILD_DATE}"
    -DFW_GIT_COMMIT="${FW_GIT_COMMIT}"
    -DFW_GIT_BRANCH="${FW_GIT_BRANCH}"
    -DFW_GIT_TAG="${FW_GIT_TAG}"
)

set(FW_NAME "vgm-fw")
set(FW_PATH "${CMAKE_SOURCE_DIR}/build/app")
set(FW_NEW_PATH "${FW_PATH}/${FW_NAME}-${FW_GIT_TAG}")
set(FW_PATH_UF2 "${FW_NEW_PATH}.uf2")
set(FW_PATH_ELF "${FW_NEW_PATH}.elf")
set(FW_PATH_BIN "${FW_NEW_PATH}.bin")

add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${FW_PATH}/firmware.uf2"
        ${FW_PATH_UF2}
    COMMENT "Copying versioned UF2")
add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${FW_PATH}/firmware.elf"
        ${FW_PATH_ELF}
    COMMENT "Copying versioned ELF")
add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${FW_PATH}/firmware.bin"
        ${FW_PATH_BIN}
    COMMENT "Copying versioned BIN")
