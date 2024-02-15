cmake_minimum_required(VERSION 3.5.1)
project(penplotter)

find_package(PythonInterp 3.8 REQUIRED)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts.inc
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/convert.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/convert.py
    COMMENT "Generating font data"
)

add_executable(penplotter
    src/main.cpp src/fonts.cpp src/fonts.h
    src/motion/planner.cpp src/motion/planner.h src/motion/plannerConfig.cpp src/motion/plannerConfig.h src/config/planner.h
    src/arch/sim/arch.h
    ${CMAKE_CURRENT_BINARY_DIR}/fonts.inc
)
target_include_directories(penplotter PUBLIC src/arch/sim ${CMAKE_CURRENT_BINARY_DIR})
