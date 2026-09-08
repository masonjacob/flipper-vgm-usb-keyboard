set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This firmware is intentionally a small standalone USB-host application.
# Do not glob the original VGM application sources here: frame.c, cli/*,
# imu/*, and led_state.c belong to the normal VGM display/RPC application
# and are not needed for the keyboard bridge.
set(APP_SOURCES
    main.c
    uart.c
    usb.c
)

add_executable(firmware
    ${APP_SOURCES}
)

target_include_directories(firmware PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${PICO_DVI_PATH}/assets"
    "${PICO_DVI_PATH}/include"
)

target_compile_definitions(firmware PRIVATE
    DVI_DEFAULT_SERIAL_CONFIG=${DVI_DEFAULT_SERIAL_CONFIG}
#    CFG_TUSB_OS=OPT_OS_FREERTOS
)

target_link_libraries(firmware
    pico_stdlib
    pico_multicore
    pico_util
    hardware_uart
    FreeRTOS
    protobuf
    tinyusb_host
    tinyusb_board
)

add_definitions(-DPICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

# The VGM USB-C port is being used as a USB host, not as a USB CDC device.
pico_enable_stdio_uart(firmware 0)
pico_enable_stdio_usb(firmware 0)

pico_add_extra_outputs(firmware)

# Build metadata / versioned output, matching the upstream VGM project.
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
    COMMENT "Copying 'firmware.uf2' to '${FW_PATH_UF2}'")

add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${FW_PATH}/firmware.elf"
        ${FW_PATH_ELF}
    COMMENT "Copying 'firmware.elf' to '${FW_PATH_ELF}'")

add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${FW_PATH}/firmware.bin"
        ${FW_PATH_BIN}
    COMMENT "Copying 'firmware.bin' to '${FW_PATH_BIN}'")
