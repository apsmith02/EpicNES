cmake_minimum_required(VERSION 3.5.0)
project(EpicNES VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === SDL2 ===

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)

list(APPEND CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/libs/SDL2") # You can put SDL2 in libs/SDL2
find_package(SDL2 REQUIRED)

# === ImGui ===

include_directories("${PROJECT_SOURCE_DIR}/imgui" "${PROJECT_SOURCE_DIR}/imgui/backends")
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_sdlrenderer2.cpp
)

# ==== MAIN PROJECT ====

set(EMU_CORE_SOURCES
    src/emulator.c src/rom.c src/cpu.c src/ppu.c src/standard_controller.c src/apu.c src/dma.c
    src/mapper/mapper.c src/mapper/nrom.c src/mapper/mmc1.c src/mapper/uxrom.c
    src/ring_buffer.c src/sdl_audio_buffer.c
)
set(EMU_APP_SOURCES
    src/app/main.cpp
    src/app/widgets/FileDialog.cpp
)

include_directories("${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/include/mapper")
add_executable(${PROJECT_NAME} ${IMGUI_SOURCES} ${EMU_CORE_SOURCES} ${EMU_APP_SOURCES})

# Link SDL2 to project
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::Main)


# ==== TESTING ====

include(CTest)
enable_testing()

include_directories("${PROJECT_SOURCE_DIR}/test/cpu/include")

add_executable(TestCPU test/cpu/test.c src/cpu.c)
add_executable(nestest test/cpu/cputest.c test/cpu/nestest.c src/cpu.c src/rom.c)

add_test(NAME TestCPU COMMAND TestCPU)
add_test(NAME nestest
    COMMAND nestest --no-illegal
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/test/cpu)
