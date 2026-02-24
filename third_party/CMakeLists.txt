# Dear ImGui CMake integration

cmake_minimum_required(VERSION 3.20)

# Check if ImGui exists
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.h)
    message(WARNING "ImGui not found in third_party/imgui/. Please download from https://github.com/ocornut/imgui")
    message(WARNING "Build will fail without ImGui.")
endif()

# ImGui core sources (platform/renderer neutral)
set(IMGUI_CORE_SOURCES
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)

# Select backends based on the parent project's window/graphics backend choice.
# The variables SHADERLAB_GRAPHICS_BACKEND and SHADERLAB_WINDOW_BACKEND are set
# in the parent CMakeLists.txt before add_subdirectory(third_party) is called.
if(NOT DEFINED SHADERLAB_GRAPHICS_BACKEND)
    set(SHADERLAB_GRAPHICS_BACKEND "d3d12")
endif()
if(NOT DEFINED SHADERLAB_WINDOW_BACKEND)
    set(SHADERLAB_WINDOW_BACKEND "win32")
endif()

if(SHADERLAB_WINDOW_BACKEND STREQUAL "win32" AND SHADERLAB_GRAPHICS_BACKEND STREQUAL "d3d12")
    # Windows: Win32 platform backend + D3D12 renderer backend
    set(IMGUI_BACKEND_SOURCES
        imgui/backends/imgui_impl_win32.cpp
        imgui/backends/imgui_impl_dx12.cpp
    )
    set(IMGUI_BACKEND_LIBS d3d12.lib dxgi.lib)
elseif(SHADERLAB_WINDOW_BACKEND STREQUAL "sdl2" AND SHADERLAB_GRAPHICS_BACKEND STREQUAL "vulkan")
    # Linux / macOS: SDL2 platform backend + Vulkan renderer backend
    find_package(SDL2 REQUIRED)
    find_package(Vulkan REQUIRED)
    set(IMGUI_BACKEND_SOURCES
        imgui/backends/imgui_impl_sdl2.cpp
        imgui/backends/imgui_impl_vulkan.cpp
    )
    set(IMGUI_BACKEND_LIBS SDL2::SDL2 Vulkan::Vulkan)
else()
    message(WARNING "Unrecognised ImGui backend combination: "
        "window=${SHADERLAB_WINDOW_BACKEND}, graphics=${SHADERLAB_GRAPHICS_BACKEND}. "
        "Falling back to core ImGui only (no backends).")
    set(IMGUI_BACKEND_SOURCES "")
    set(IMGUI_BACKEND_LIBS "")
endif()

add_library(imgui STATIC
    ${IMGUI_CORE_SOURCES}
    ${IMGUI_BACKEND_SOURCES}
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

if(IMGUI_BACKEND_LIBS)
    target_link_libraries(imgui PUBLIC ${IMGUI_BACKEND_LIBS})
endif()
