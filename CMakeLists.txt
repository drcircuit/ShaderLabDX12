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
option(SHADERLAB_USE_CRINKLER "Use Crinkler as the linker (self-contained builds)" OFF)
option(SHADERLAB_CRINKLER_TINYIMPORT "Enable Crinkler /TINYIMPORT optimization" ON)
option(SHADERLAB_RUNTIME_IMGUI "Enable ImGui debug overlay in runtime player" ON)
option(SHADERLAB_COMPACT_TRACK_DEBUG "Enable compact track runtime debug logs" OFF)
option(SHADERLAB_RUNTIME_DEBUG_LOG "Enable general runtime debug logs" OFF)
option(SHADERLAB_TINY_PLAYER "Enable ultra-minimal runtime profile (strips optional subsystems)" OFF)
option(SHADERLAB_BUILD_MICRO_PLAYER "Build ultra-minimal benchmark runtime target" OFF)
set(CRINKLER_PATH "" CACHE FILEPATH "Path to crinkler.exe")

# Platform check
if(NOT WIN32)
    message(FATAL_ERROR "ShaderLab currently supports Windows only")
endif()

if(SHADERLAB_BUILD_MICRO_PLAYER)
    add_executable(ShaderLabMicroPlayer WIN32
        src/app/runtime/main_micro_player.cpp
    )

    target_include_directories(ShaderLabMicroPlayer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MSVC)
        target_compile_options(ShaderLabMicroPlayer PRIVATE
            /W4
            /WX-
            /Os
            /Oy
            /Oi
            /GS-
            /GR-
            /Gy
            /Gw
            /GF
            /Zc:threadSafeInit-
        )

        if(SHADERLAB_USE_CRINKLER)
            target_link_options(ShaderLabMicroPlayer PRIVATE
                /ENTRY:WinMainCRTStartup
                /INCREMENTAL:NO
                /MANIFEST:NO
                /RELEASE
                /COMPMODE:SLOW
                /ORDERTRIES:120
                /UNALIGNCODE
            )
            if(SHADERLAB_CRINKLER_TINYIMPORT)
                target_link_options(ShaderLabMicroPlayer PRIVATE /TINYIMPORT)
            endif()
        else()
            target_link_options(ShaderLabMicroPlayer PRIVATE
                /ENTRY:WinMainCRTStartup
                /INCREMENTAL:NO
                /OPT:REF
                /OPT:ICF
                /LTCG
                /MANIFEST:NO
                /RELEASE
            )
        endif()
    endif()
endif()

if(MSVC AND SHADERLAB_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MSVC AND SHADERLAB_USE_CRINKLER)
    if(CRINKLER_PATH)
        if(EXISTS "${CRINKLER_PATH}")
            set(CMAKE_LINKER "${CRINKLER_PATH}" CACHE FILEPATH "Crinkler linker" FORCE)
        else()
            message(WARNING "CRINKLER_PATH does not exist: ${CRINKLER_PATH}")
        endif()
    endif()
endif()

# Third-party dependencies
add_subdirectory(third_party)

# Core modules
set(SHADERLAB_CORE_RUNTIME_SOURCES
    src/graphics/Device.cpp
    src/graphics/Swapchain.cpp
    src/graphics/CommandQueue.cpp
    src/graphics/PreviewRenderer.cpp
    src/shader/ShaderCompiler.cpp
    src/audio/BeatClock.cpp
    src/core/Serializer.cpp
    src/core/PackageManager.cpp
)

if(NOT SHADERLAB_TINY_PLAYER OR SHADERLAB_BUILD_EDITOR)
    list(APPEND SHADERLAB_CORE_RUNTIME_SOURCES
        src/audio/AudioSystem.cpp
    )
endif()

set(SHADERLAB_CORE_EDITOR_SOURCES
    src/ui/UISystem.cpp
    src/ui/UISystem.Project.cpp
    src/ui/UISystem.Demo.cpp
    src/ui/UISystem.Render.cpp
    src/ui/UISystem.Editor.cpp
    src/ui/UISystemAssets.cpp
    src/ui/UISystem.About.cpp
    src/core/BuildPipeline.cpp
    src/core/RuntimeExporter.cpp
    third_party/ImGuiColorTextEdit/TextEditor.cpp
)

set(SHADERLAB_CORE_HEADER_SOURCES
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
    third_party/ImGuiColorTextEdit/TextEditor.h
)

set(SHADERLAB_CORE_SOURCES ${SHADERLAB_CORE_RUNTIME_SOURCES} ${SHADERLAB_CORE_HEADER_SOURCES})
if(SHADERLAB_BUILD_EDITOR)
    list(APPEND SHADERLAB_CORE_SOURCES ${SHADERLAB_CORE_EDITOR_SOURCES})
endif()

add_library(ShaderLabCore STATIC ${SHADERLAB_CORE_SOURCES})

target_include_directories(ShaderLabCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit
)

if(NOT SHADERLAB_TINY_PLAYER OR SHADERLAB_BUILD_EDITOR)
    target_include_directories(ShaderLabCore PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    )
endif()

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
if(SHADERLAB_BUILD_EDITOR OR (SHADERLAB_BUILD_RUNTIME AND SHADERLAB_RUNTIME_IMGUI))
    target_link_libraries(ShaderLabCore PUBLIC imgui)
endif()

if(SHADERLAB_RUNTIME_IMGUI)
    target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_RUNTIME_IMGUI=1)
else()
    target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_RUNTIME_IMGUI=0)
endif()

if(SHADERLAB_COMPACT_TRACK_DEBUG)
    target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_COMPACT_TRACK_DEBUG=1)
else()
    target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_COMPACT_TRACK_DEBUG=0)
endif()

if(SHADERLAB_RUNTIME_DEBUG_LOG)
    target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_RUNTIME_DEBUG_LOG=1)
else()
    target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_RUNTIME_DEBUG_LOG=0)
endif()

if(SHADERLAB_TINY_PLAYER)
    target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_TINY_PLAYER=1)
else()
    target_compile_definitions(ShaderLabCore PUBLIC SHADERLAB_TINY_PLAYER=0)
endif()

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
        target_compile_options(ShaderLabCore PRIVATE /O2)
        if(NOT SHADERLAB_USE_CRINKLER)
            target_compile_options(ShaderLabCore PRIVATE /GL) # Whole program optimization
        endif()
    endif()

    if(SHADERLAB_TINY_PLAYER)
        target_compile_options(ShaderLabCore PRIVATE
            /Os
            /Oy
            /Oi
            /fp:fast
            /GS-
            /GR-
            /Gy
            /Gw
            /GF
            /Zc:threadSafeInit-
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

    if(MSVC AND SHADERLAB_TINY_PLAYER)
        if(SHADERLAB_USE_CRINKLER)
            target_link_options(ShaderLabPlayer PRIVATE
                /INCREMENTAL:NO
                /MANIFEST:NO
                /RELEASE
                /COMPMODE:SLOW
                /ORDERTRIES:80
                /UNALIGNCODE
            )
        else()
            target_link_options(ShaderLabPlayer PRIVATE
                /INCREMENTAL:NO
                /OPT:REF
                /OPT:ICF
                /LTCG
                /MANIFEST:NO
                /RELEASE
            )
        endif()
    endif()
endif()
