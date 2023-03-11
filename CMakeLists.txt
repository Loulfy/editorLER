cmake_minimum_required(VERSION 3.22)
project(editorLER)

set(CMAKE_CXX_STANDARD 20)
set(SPIRV_REFLECT_EXAMPLES OFF)
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
set(RTXMU_WITH_D3D12 OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-std=c++20 -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++")
endif()

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Vulkan)

add_subdirectory(libs/SPIRV-Reflect)
add_subdirectory(libs/glslang)
include_directories(libs/imgui)
include_directories(libs/imgui/backends)

add_definitions(-DPROJECT_DIR=\"${PROJECT_SOURCE_DIR}\")

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
    "src/common.hpp"
    "src/ler_sys.hpp"
    "src/ler_spv.hpp"
    "src/ler_app.cpp"
    "src/ler_dev.cpp"
    "src/ler_gui.cpp"
    "src/ler_sys.cpp"
    "src/ler_spv.cpp"
    "src/format.cpp"
)

add_executable(editorLER src/main.cpp ${LER} ${IMGUI})
target_link_libraries(editorLER Vulkan::Vulkan ${CONAN_LIBS} spirv-reflect-static glslang glslang-default-resource-limits SPIRV)
