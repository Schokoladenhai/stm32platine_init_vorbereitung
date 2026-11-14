cmake_minimum_required(VERSION 3.22)

# Test-Build: Kommando-Shell statt Regelungs-Tasks, eigenes ELF (lars_fw_test.elf).
option(FW_TEST_BUILD "Test-Firmware bauen" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Der Projektname ist zugleich der Target-Name, den das CubeMX-CMake erwartet.
project(lars_fw C CXX ASM)

add_executable(${CMAKE_PROJECT_NAME})

# Von CubeMX generierte Quellen (HAL, FreeRTOS, USB, main.c). Nie von Hand ändern.
add_subdirectory(cubemx/cmake/stm32cubemx)

# Eigener Code
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    app/app.cpp
    cubemx/newlib_lock_glue.c   # von CubeMX erzeugt, aber nicht in dessen CMake-Liste
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/cubemx
)

# Versionsinfo (beim Konfigurieren ermittelt)
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE FW_GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT FW_GIT_VERSION)
    set(FW_GIT_VERSION "unbekannt")
endif()
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE FW_VERSION="${FW_GIT_VERSION}")

# Lebenszeichen-LED nur in Debug- und Test-Builds, im Release bleibt sie aus
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE FW_HEARTBEAT=$<IF:$<CONFIG:Release>,0,1>)

# Normal- oder Test-Build
if(FW_TEST_BUILD)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE test/test_main.cpp)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE FW_TEST_BUILD=1)
    set(FW_OUTPUT_NAME ${CMAKE_PROJECT_NAME}_test)
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE FW_TEST_BUILD=0)
    set(FW_OUTPUT_NAME ${CMAKE_PROJECT_NAME})
endif()
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${FW_OUTPUT_NAME})

# Linken mit dem von CubeMX erzeugten Linkerskript
set(FW_LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/cubemx/STM32F429xx_FLASH.ld)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${FW_LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_BINARY_DIR}/${FW_OUTPUT_NAME}.map)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_DEPENDS ${FW_LINKER_SCRIPT})

# .bin (für DFU) und .hex neben dem .elf erzeugen
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${FW_OUTPUT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${FW_OUTPUT_NAME}.hex
    COMMENT "Erzeuge ${FW_OUTPUT_NAME}.bin und .hex")
