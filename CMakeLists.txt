cmake_minimum_required(VERSION 3.20)
project(ShaderLab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Build options
option(SHADERLAB_BUILD_EDITOR "Build the editor application" ON)
option(SHADERLAB_BUILD_RUNTIME "Build the standalone runtime" ON)
option(SHADERLAB_ENABLE_DXC "Enable DXC shader compiler (requires dxcompiler.dll)" ON)
option(SHADERLAB_STATIC_RUNTIME "Link MSVC runtime statically" OFF)

# Platform check
if(NOT WIN32)
    message(FATAL_ERROR "ShaderLab currently supports Windows only")
endif()

if(MSVC AND SHADERLAB_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Third-party dependencies
add_subdirectory(third_party)

# Core modules
add_library(ShaderLabCore STATIC
    src/graphics/Device.cpp
    src/graphics/Swapchain.cpp
    src/graphics/CommandQueue.cpp
    src/graphics/PreviewRenderer.cpp
    src/shader/ShaderCompiler.cpp
    src/audio/AudioSystem.cpp
    src/audio/BeatClock.cpp
    src/ui/UISystem.cpp
    src/ui/UISystem.Demo.cpp
    src/ui/UISystem.Render.cpp
    src/ui/UISystem.Editor.cpp
    src/ui/UISystemAssets.cpp
    src/ui/UISystem.About.cpp
    src/core/Serializer.cpp
    src/core/BuildPipeline.cpp
    src/core/RuntimeExporter.cpp
    src/core/PackageManager.cpp
    include/ShaderLab/Graphics/Device.h
    include/ShaderLab/Graphics/Swapchain.h
    include/ShaderLab/Graphics/CommandQueue.h
    include/ShaderLab/Graphics/PreviewRenderer.h
    include/ShaderLab/Shader/ShaderCompiler.h
    include/ShaderLab/Audio/AudioSystem.h
    include/ShaderLab/Audio/BeatClock.h
    include/ShaderLab/UI/UISystem.h
    include/ShaderLab/Core/Serializer.h
    include/ShaderLab/Core/BuildPipeline.h
    include/ShaderLab/Core/RuntimeExporter.h
    include/ShaderLab/Core/PackageManager.h
    include/ShaderLab/Core/ShaderLabData.h
    third_party/ImGuiColorTextEdit/TextEditor.cpp
    third_party/ImGuiColorTextEdit/TextEditor.h
)

target_include_directories(ShaderLabCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit
)

# Link D3D12 and related Windows libraries
target_link_libraries(ShaderLabCore PUBLIC
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
)

if(SHADERLAB_ENABLE_DXC)
    target_link_libraries(ShaderLabCore PUBLIC dxcompiler.lib)
endif()

# ImGui integration
target_link_libraries(ShaderLabCore PUBLIC imgui)

# Compiler flags
if(MSVC)
    target_compile_options(ShaderLabCore PRIVATE
        /W4           # Warning level 4
        /WX-          # Don't treat warnings as errors (for now)
        /permissive-  # Standards conformance
        /Zc:__cplusplus
    )

    if(SHADERLAB_ENABLE_DXC)
        target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_ENABLE_DXC=1)
    else()
        target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_ENABLE_DXC=0)
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(ShaderLabCore PUBLIC
            SHADERLAB_DEBUG=1
            SHADERLAB_VALIDATION=1
        )
    else()
        target_compile_options(ShaderLabCore PRIVATE
            /O2       # Optimize for speed
            /GL       # Whole program optimization
        )
    endif()
endif()

# Editor executable
if(SHADERLAB_BUILD_EDITOR)
    add_executable(ShaderLabEditor WIN32
        src/app/editor/main.cpp
        src/app/editor/EditorApp.cpp
        include/ShaderLab/App/EditorApp.h
    )
    
    target_link_libraries(ShaderLabEditor PRIVATE ShaderLabCore)
    
    target_include_directories(ShaderLabEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # Show console window in debug builds for error messages
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(ShaderLabEditor PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:WinMainCRTStartup")
    endif()

    # Deploy Development Kit (Code + CMake) for standalone builds
    add_custom_command(TARGET ShaderLabEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/src
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/third_party $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/third_party
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/templates/Standalone_CMakeLists.txt $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/CMakeLists.txt
    )

    # Copy DXC DLLs to output directory
    if(SHADERLAB_ENABLE_DXC)
        add_custom_command(TARGET ShaderLabEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxcompiler.dll"
                $<TARGET_FILE_DIR:ShaderLabEditor>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxil.dll"
                $<TARGET_FILE_DIR:ShaderLabEditor>
        )
    endif()
endif()

# Runtime executable
if(SHADERLAB_BUILD_RUNTIME)
    add_executable(ShaderLabPlayer WIN32
        src/app/runtime/main_player.cpp
        src/app/runtime/DemoPlayer.cpp
        include/ShaderLab/App/DemoPlayer.h
    )
    
    target_link_libraries(ShaderLabPlayer PRIVATE ShaderLabCore)
    
    target_include_directories(ShaderLabPlayer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()
