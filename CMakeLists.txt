cmake_minimum_required(VERSION 3.5.0)
project(EpicNES VERSION 0.1.0 LANGUAGES C)


# ==== MAIN PROJECT ====

include_directories("${PROJECT_SOURCE_DIR}/include")
add_executable(${PROJECT_NAME} src/main.c src/emulator.c src/rom.c src/cpu.c src/ppu.c src/standard_controller.c src/apu.c src/memory.c
src/ring_buffer.c src/sdl_audio_buffer.c src/console_debug.c)


# ==== SDL2 ====

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)

find_package(SDL2 REQUIRED)

# Link SDL2 to project
target_link_libraries(${PROJECT_NAME} SDL2::Main)


# ==== TESTING ====

include(CTest)
enable_testing()

include_directories("${PROJECT_SOURCE_DIR}/test/cpu/include")

add_executable(TestCPU test/cpu/test.c src/cpu.c)
add_executable(nestest test/cpu/cputest.c test/cpu/nestest.c src/cpu.c src/rom.c)

add_test(NAME TestCPU COMMAND TestCPU)
add_test(NAME nestest COMMAND nestest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/test/cpu)
