cmake_minimum_required(VERSION 3.22)
project(editorLER)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-std=c++20 -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++")
endif()

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Vulkan)

include_directories(libs/imgui)
include_directories(libs/imgui/backends)

set(IMGUI
    "libs/imgui/imconfig.h"
    "libs/imgui/imgui.h"
    "libs/imgui/imgui_internal.h"
    "libs/imgui/imstb_rectpack.h"
    "libs/imgui/imstb_textedit.h"
    "libs/imgui/imstb_truetype.h"
    "libs/imgui/imgui.cpp"
    "libs/imgui/imgui_demo.cpp"
    "libs/imgui/imgui_draw.cpp"
    "libs/imgui/imgui_tables.cpp"
    "libs/imgui/imgui_widgets.cpp"
    "libs/imgui/backends/imgui_impl_vulkan.h"
    "libs/imgui/backends/imgui_impl_vulkan.cpp"
    "libs/imgui/backends/imgui_impl_glfw.h"
    "libs/imgui/backends/imgui_impl_glfw.cpp"
)

set(LER
    "src/ler.hpp"
    "src/ler_app.cpp"
    "src/ler_dev.cpp"
    "src/ler_gui.cpp"
    "src/ler_sys.cpp"
)

add_executable(editorLER src/main.cpp ${LER} ${IMGUI})
target_link_libraries(editorLER Vulkan::Vulkan ${CONAN_LIBS})
