cmake_minimum_required(VERSION 3.5.1)
if (PICO_SDK_PATH)
  include(pico_sdk_import.cmake)
endif()
project(penplotter)

if (PICO_SDK_PATH)
  set(TARGET_ARCH "rp2040")
  pico_sdk_init()
else()
  set(TARGET_ARCH "sim")
endif()

find_package(PythonInterp 3.8 REQUIRED)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts.inc
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/convert.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/convert.py
    COMMENT "Generating font data"
)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")
list(FILTER SOURCES EXCLUDE REGEX src/arch/[^/]+/)
file(GLOB_RECURSE ARCH_SOURCES "src/arch/${TARGET_ARCH}/*.cpp" "src/arch/${TARGET_ARCH}/*.h")

add_executable(penplotter
    ${SOURCES} ${ARCH_SOURCES}
    ${CMAKE_CURRENT_BINARY_DIR}/fonts.inc
)
target_compile_options(penplotter PUBLIC -Wall -Wextra -Wshadow)
target_include_directories(penplotter PUBLIC src src/arch/${TARGET_ARCH} ${CMAKE_CURRENT_BINARY_DIR})
if (PICO_SDK_PATH)
    add_subdirectory(Pico-PIO-USB)
    pico_enable_stdio_usb(penplotter 1)
    pico_enable_stdio_uart(penplotter 0)
    target_link_libraries(penplotter PUBLIC pico_stdlib pico_multicore pico_pio_usb)
    pico_add_extra_outputs(penplotter)
endif()
