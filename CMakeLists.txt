# qos - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(qos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(QOS_SOURCES
    src/main.cpp
    src/crash_win.cpp
    src/fps_unlock.cpp
)

if(WIN32)
    add_executable(qos WIN32 ${QOS_SOURCES})
else()
    add_executable(qos ${QOS_SOURCES})
endif()

rexglue_setup_target(qos GPU_PLUGINS xenos)

# AMD FidelityFX runtime (FSR / CAS output filters). Present when the SDK was built with
# REXGLUE_ENABLE_FIDELITYFX (the patched SDK from the Bourne project's tools/build_sdk.bat);
# the runtime DLL needs it next to qos.exe. The stock SDK doesn't have it: nothing to copy.
get_filename_component(QOS_SDK_BIN "${rexglue_DIR}/../../../bin" ABSOLUTE)
file(GLOB QOS_FFX_DLLS "${QOS_SDK_BIN}/amd_fidelityfx_dx12*.dll")
if(QOS_FFX_DLLS)
    add_custom_command(TARGET qos POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QOS_FFX_DLLS} $<TARGET_FILE_DIR:qos>
        COMMENT "Copying FidelityFX runtime")
endif()

# Skullburglar's native Vulkan renderer (--gpu_plugin=native), when the SDK was built with it
# (the Bourne project's tools/build_native.bat). Copied when the build runs; the copy script is
# written into the build folder.
file(WRITE ${CMAKE_BINARY_DIR}/copy_if_exists.cmake
    "if(EXISTS \"\${SRC}\")\n  file(COPY \"\${SRC}\" DESTINATION \"\${DST}\")\nendif()\n")
# A target of its own that runs on every build: a POST_BUILD step only runs when qos.exe is relinked, so a rebuilt
# plugin alone (tools\build_native.bat) would otherwise never reach the build folder.
add_custom_target(qos_native_plugin ALL
    COMMAND ${CMAKE_COMMAND}
        "-DSRC=${QOS_SDK_BIN}/rexgpu-native$<$<CONFIG:RelWithDebInfo>:rd>$<$<CONFIG:Debug>:d>.dll"
        "-DDST=$<TARGET_FILE_DIR:qos>"
        -P ${CMAKE_BINARY_DIR}/copy_if_exists.cmake
    VERBATIM)
add_dependencies(qos_native_plugin qos)
