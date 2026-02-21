cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
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
set(SHADERLAB_CRINKLER_HASH_SIZE_MB "64" CACHE STRING "Crinkler hash table size in MB")
set(SHADERLAB_CRINKLER_HASH_TRIES "1" CACHE STRING "Crinkler hash optimization tries")
set(SHADERLAB_CRINKLER_ORDER_TRIES "1" CACHE STRING "Crinkler code reordering tries")
option(SHADERLAB_RUNTIME_IMGUI "Enable ImGui debug overlay in runtime player" ON)
option(SHADERLAB_COMPACT_TRACK_DEBUG "Enable compact track runtime debug logs" OFF)
option(SHADERLAB_RUNTIME_DEBUG_LOG "Enable general runtime debug logs" OFF)
option(SHADERLAB_TINY_PLAYER "Enable ultra-minimal runtime profile (strips optional subsystems)" OFF)
option(SHADERLAB_BUILD_MICRO_PLAYER "Build ultra-minimal benchmark runtime target" OFF)
option(SHADERLAB_TINY_RUNTIME_COMPILE "Enable runtime shader compilation in tiny player" ON)
option(SHADERLAB_TINY_TRACE "Enable lean tiny-player diagnostics via OutputDebugString" OFF)
option(SHADERLAB_TINY_DEV_OVERLAY "Enable tiny-player on-screen diagnostic overlay" OFF)
set(CRINKLER_PATH "" CACHE FILEPATH "Path to crinkler.exe")

# Platform check
if(NOT WIN32)
    message(FATAL_ERROR "ShaderLab currently supports Windows only")
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

function(shaderlab_assert_target_has_no_sources target)
    if(NOT TARGET ${target})
        return()
    endif()

    get_target_property(_sources ${target} SOURCES)
    if(NOT _sources)
        return()
    endif()

    foreach(_source IN LISTS _sources)
        file(TO_CMAKE_PATH "${_source}" _source_norm)
        foreach(_forbidden IN LISTS ARGN)
            string(FIND "${_source_norm}" "${_forbidden}" _match)
            if(NOT _match EQUAL -1)
                message(FATAL_ERROR
                    "Layering violation: target '${target}' contains forbidden source '${_source_norm}' (matched '${_forbidden}').")
            endif()
        endforeach()
    endforeach()
endfunction()

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
    src/audio/AudioSystem.cpp
)


set(SHADERLAB_COREAPI_HEADER_SOURCES
    include/ShaderLab/Graphics/Device.h
    include/ShaderLab/Graphics/Swapchain.h
    include/ShaderLab/Graphics/CommandQueue.h
    include/ShaderLab/Graphics/PreviewRenderer.h
    include/ShaderLab/Shader/ShaderCompiler.h
    include/ShaderLab/Audio/AudioSystem.h
    include/ShaderLab/Audio/BeatClock.h
    include/ShaderLab/Core/Serializer.h
    include/ShaderLab/Core/PackageManager.h
    include/ShaderLab/Core/ShaderLabData.h
)

set(SHADERLAB_PLAYER_RUNTIME_SOURCES
    src/app/runtime/DemoPlayer.cpp
    src/app/runtime/PlayerApp.cpp
    src/app/runtime/RuntimeStartupPolicy.cpp
    src/app/runtime/RuntimeWindowPolicy.cpp
    include/ShaderLab/App/DemoPlayer.h
    include/ShaderLab/App/PlayerApp.h
    include/ShaderLab/Runtime/RuntimeStartupPolicy.h
    include/ShaderLab/Runtime/RuntimeWindowPolicy.h
)

set(SHADERLAB_DEVKIT_BUILDTOOLS_SOURCES
    src/core/BuildPipeline.cpp
    src/core/RuntimeExporter.cpp
    include/ShaderLab/DevKit/BuildPipeline.h
    include/ShaderLab/DevKit/RuntimeExporter.h
)

set(SHADERLAB_EDITORLIB_SOURCES
    src/ui/UISystem.cpp
    src/ui/UISystem.Project.cpp
    src/ui/UISystem.Demo.cpp
    src/ui/UISystem.Render.cpp
    src/ui/UISystem.Editor.cpp
    src/ui/UISystemAssets.cpp
    src/ui/UISystem.About.cpp
    third_party/ImGuiColorTextEdit/TextEditor.cpp
    include/ShaderLab/UI/UISystem.h
    third_party/ImGuiColorTextEdit/TextEditor.h
)

set(SHADERLAB_COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit
)

add_library(ShaderLabCoreApi STATIC
    ${SHADERLAB_CORE_RUNTIME_SOURCES}
    ${SHADERLAB_COREAPI_HEADER_SOURCES}
)

target_include_directories(ShaderLabCoreApi PUBLIC
    ${SHADERLAB_COMMON_INCLUDE_DIRS}
)

target_include_directories(ShaderLabCoreApi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
)

target_link_libraries(ShaderLabCoreApi PUBLIC
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
)

if(SHADERLAB_BUILD_EDITOR OR (SHADERLAB_BUILD_RUNTIME AND SHADERLAB_RUNTIME_IMGUI))
    target_link_libraries(ShaderLabCoreApi PUBLIC imgui)
endif()

add_library(ShaderLabDevKit STATIC ${SHADERLAB_PLAYER_RUNTIME_SOURCES})

target_include_directories(ShaderLabDevKit PUBLIC
    ${SHADERLAB_COMMON_INCLUDE_DIRS}
)

target_include_directories(ShaderLabDevKit PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
)

target_link_libraries(ShaderLabDevKit PUBLIC
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
)

if(SHADERLAB_BUILD_EDITOR OR (SHADERLAB_BUILD_RUNTIME AND SHADERLAB_RUNTIME_IMGUI))
    target_link_libraries(ShaderLabDevKit PUBLIC imgui)
endif()

if(SHADERLAB_RUNTIME_IMGUI)
    target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_RUNTIME_IMGUI=1)
else()
    target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_RUNTIME_IMGUI=0)
endif()

if(SHADERLAB_COMPACT_TRACK_DEBUG)
    target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_COMPACT_TRACK_DEBUG=1)
else()
    target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_COMPACT_TRACK_DEBUG=0)
endif()

if(SHADERLAB_RUNTIME_DEBUG_LOG)
    target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_RUNTIME_DEBUG_LOG=1)
else()
    target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_RUNTIME_DEBUG_LOG=0)
endif()

target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_TINY_PLAYER=0)

target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_TINY_RUNTIME_COMPILE=0)

target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_TINY_TRACE=0)

target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_TINY_DEV_OVERLAY=0)

if(SHADERLAB_ENABLE_DXC)
    target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_ENABLE_DXC=1)
else()
    target_compile_definitions(ShaderLabDevKit PUBLIC SHADERLAB_ENABLE_DXC=0)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ShaderLabDevKit PUBLIC
        SHADERLAB_DEBUG=1
        SHADERLAB_VALIDATION=1
    )
endif()

if(MSVC)
    target_compile_options(ShaderLabDevKit PRIVATE
        /W4
        /WX-
        /permissive-
        /Zc:__cplusplus
    )
endif()

add_library(ShaderLabDevKitBuildTools STATIC ${SHADERLAB_DEVKIT_BUILDTOOLS_SOURCES})

target_include_directories(ShaderLabDevKitBuildTools PUBLIC
    ${SHADERLAB_COMMON_INCLUDE_DIRS}
)

target_include_directories(ShaderLabDevKitBuildTools PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
)

target_link_libraries(ShaderLabDevKitBuildTools PUBLIC ShaderLabCoreApi)

if(SHADERLAB_BUILD_EDITOR)
    add_library(ShaderLabEditorLib STATIC ${SHADERLAB_EDITORLIB_SOURCES})

    target_include_directories(ShaderLabEditorLib PUBLIC
        ${SHADERLAB_COMMON_INCLUDE_DIRS}
    )

    target_include_directories(ShaderLabEditorLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    )

    target_link_libraries(ShaderLabEditorLib PUBLIC
        d3d12.lib
        dxgi.lib
        dxguid.lib
        d3dcompiler.lib
        imgui
    )
endif()

if(MSVC)
    set_source_files_properties(
        third_party/ImGuiColorTextEdit/TextEditor.cpp
        PROPERTIES
        COMPILE_OPTIONS "/wd4101;/wd4244"
    )
endif()

# Editor executable
if(SHADERLAB_BUILD_EDITOR)
    add_executable(ShaderLabEditor WIN32
        src/app/editor/main.cpp
        src/app/editor/EditorApp.cpp
        src/app/editor/ShaderLabEditor.rc
        include/ShaderLab/App/EditorApp.h
    )
    
    target_link_libraries(ShaderLabEditor PRIVATE ShaderLabEditorLib ShaderLabDevKitBuildTools ShaderLabCoreApi)
    
    target_include_directories(ShaderLabEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # Show console window in debug builds for error messages
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(ShaderLabEditor PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:WinMainCRTStartup")
    endif()

    # Deploy Development Kit (Code + CMake) for standalone builds
    add_custom_command(TARGET ShaderLabEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/editor_assets $<TARGET_FILE_DIR:ShaderLabEditor>/editor_assets

        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include/ShaderLab
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderLab/App $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include/ShaderLab/App
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderLab/Audio $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include/ShaderLab/Audio
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderLab/Core $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include/ShaderLab/Core
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderLab/DevKit $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include/ShaderLab/DevKit
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderLab/Graphics $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include/ShaderLab/Graphics
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderLab/Runtime $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include/ShaderLab/Runtime
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderLab/Shader $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/include/ShaderLab/Shader

        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/src
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/app/runtime $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/src/app/runtime
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/app/tiny $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/src/app/tiny
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/audio $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/src/audio
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/core $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/src/core
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/src/graphics
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/shader $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/src/shader

        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/third_party
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMakeLists.txt $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/third_party/CMakeLists.txt
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/third_party/imgui
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/third_party/json
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/third_party/miniaudio
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/third_party/stb
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Crinkler.exe $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/third_party/Crinkler.exe

        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/templates/Standalone_CMakeLists.txt $<TARGET_FILE_DIR:ShaderLabEditor>/dev_kit/CMakeLists.txt
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ShaderLabEditor>/editor_assets/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenFontIcons/OpenFontIcons.ttf
            $<TARGET_FILE_DIR:ShaderLabEditor>/editor_assets/fonts/OpenFontIcons.ttf
    )

    # Copy DXC DLLs to editor output (required for shader compilation/build flow)
    if(SHADERLAB_ENABLE_DXC)
        set(SHADERLAB_DXC_BIN_DIR "" CACHE PATH "Directory containing dxcompiler.dll and dxil.dll")
        set(_shaderlab_dxc_bin_dir "${SHADERLAB_DXC_BIN_DIR}")

        if(NOT _shaderlab_dxc_bin_dir)
            file(GLOB _shaderlab_windows_sdk_bin_dirs "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
            if(_shaderlab_windows_sdk_bin_dirs)
                list(SORT _shaderlab_windows_sdk_bin_dirs ORDER DESCENDING)
                list(GET _shaderlab_windows_sdk_bin_dirs 0 _shaderlab_dxc_bin_dir)
            endif()
        endif()

        if(_shaderlab_dxc_bin_dir
           AND EXISTS "${_shaderlab_dxc_bin_dir}/dxcompiler.dll"
           AND EXISTS "${_shaderlab_dxc_bin_dir}/dxil.dll")
            add_custom_command(TARGET ShaderLabEditor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_shaderlab_dxc_bin_dir}/dxcompiler.dll"
                    $<TARGET_FILE_DIR:ShaderLabEditor>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_shaderlab_dxc_bin_dir}/dxil.dll"
                    $<TARGET_FILE_DIR:ShaderLabEditor>
            )
        else()
            message(WARNING "DXC runtime DLLs not found. Set SHADERLAB_DXC_BIN_DIR to a folder containing dxcompiler.dll and dxil.dll.")
        endif()
    endif()

endif()

if(SHADERLAB_BUILD_EDITOR)
    add_executable(ShaderLabBuildCli
        src/app/tools/build_cli.cpp
    )

    target_link_libraries(ShaderLabBuildCli PRIVATE ShaderLabDevKitBuildTools ShaderLabCoreApi)

    target_include_directories(ShaderLabBuildCli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()

# Runtime executable
if(SHADERLAB_BUILD_RUNTIME)
    add_executable(ShaderLabPlayer WIN32
        src/app/runtime/main_player.cpp
        include/ShaderLab/App/PlayerApp.h
    )
    
    target_link_libraries(ShaderLabPlayer PRIVATE ShaderLabDevKit ShaderLabCoreApi)
    
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

    add_executable(ShaderLabScreenSaver WIN32
        src/app/runtime/main_screensaver.cpp
        include/ShaderLab/App/PlayerApp.h
    )

    target_link_libraries(ShaderLabScreenSaver PRIVATE ShaderLabDevKit ShaderLabCoreApi)

    target_include_directories(ShaderLabScreenSaver PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    set_target_properties(ShaderLabScreenSaver PROPERTIES SUFFIX ".scr")

    if(MSVC AND SHADERLAB_TINY_PLAYER)
        if(SHADERLAB_USE_CRINKLER)
            target_link_options(ShaderLabScreenSaver PRIVATE
                /INCREMENTAL:NO
                /MANIFEST:NO
                /RELEASE
                /COMPMODE:SLOW
                /ORDERTRIES:80
                /UNALIGNCODE
            )
        else()
            target_link_options(ShaderLabScreenSaver PRIVATE
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

if(SHADERLAB_BUILD_MICRO_PLAYER)
    add_subdirectory(src/app/tiny)
endif()

shaderlab_assert_target_has_no_sources(ShaderLabDevKit
    "src/core/BuildPipeline.cpp"
    "src/core/RuntimeExporter.cpp"
    "src/ui/")

shaderlab_assert_target_has_no_sources(ShaderLabDevKitBuildTools
    "src/app/runtime/"
    "src/ui/")

shaderlab_assert_target_has_no_sources(ShaderLabEditorLib
    "src/app/runtime/"
    "src/core/BuildPipeline.cpp"
    "src/core/RuntimeExporter.cpp")

shaderlab_assert_target_has_no_sources(ShaderLabEditor
    "src/app/runtime/"
    "src/core/BuildPipeline.cpp"
    "src/core/RuntimeExporter.cpp")

shaderlab_assert_target_has_no_sources(ShaderLabPlayer
    "src/core/BuildPipeline.cpp"
    "src/core/RuntimeExporter.cpp"
    "src/ui/")

shaderlab_assert_target_has_no_sources(ShaderLabScreenSaver
    "src/core/BuildPipeline.cpp"
    "src/core/RuntimeExporter.cpp"
    "src/ui/")

shaderlab_assert_target_has_no_sources(ShaderLabMicroPlayer
    "src/core/BuildPipeline.cpp"
    "src/core/RuntimeExporter.cpp"
    "src/ui/")
