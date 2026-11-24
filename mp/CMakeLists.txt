# qos_mp - 007: Quantum of Solace multiplayer (game/default_mp.xex), recompiled with ReXGlue: a second project beside
# single player (the folder above), sharing its game folder, crash handler and SDK. HANDOFF.md, "Multiplayer".
#
# SDK boilerplate lives in generated/rexglue.cmake (single player's, renamed to qos_mp).

cmake_minimum_required(VERSION 3.25)
project(qos_mp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

set(QOS_SP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(QOS_MP_SOURCES
    src/main.cpp
    ${QOS_SP_DIR}/src/crash_win.cpp
)

if(WIN32)
    add_executable(qos_mp WIN32 ${QOS_MP_SOURCES})
else()
    add_executable(qos_mp ${QOS_MP_SOURCES})
endif()

rexglue_setup_target(qos_mp GPU_PLUGINS xenos)
target_include_directories(qos_mp PRIVATE ${QOS_SP_DIR}/src)

# Single player's renderer assets (shader refs, button pictures), so the native renderer behaves the same.
add_custom_target(qos_mp_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${QOS_SP_DIR}/prompts $<TARGET_FILE_DIR:qos_mp>/prompts
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${QOS_SP_DIR}/pipelines $<TARGET_FILE_DIR:qos_mp>/pipelines
    COMMENT "Copying button pictures and shader refs")
add_dependencies(qos_mp_assets qos_mp)

# FidelityFX runtime and the native renderer from the SDK, as single player copies them.
get_filename_component(QOS_SDK_BIN "${rexglue_DIR}/../../../bin" ABSOLUTE)
file(GLOB QOS_FFX_DLLS "${QOS_SDK_BIN}/amd_fidelityfx_dx12*.dll")
if(QOS_FFX_DLLS)
    add_custom_command(TARGET qos_mp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QOS_FFX_DLLS} $<TARGET_FILE_DIR:qos_mp>
        COMMENT "Copying FidelityFX runtime")
endif()
file(WRITE ${CMAKE_BINARY_DIR}/copy_if_exists.cmake
    "if(EXISTS \"\${SRC}\")\n  file(COPY \"\${SRC}\" DESTINATION \"\${DST}\")\nendif()\n")
add_custom_target(qos_mp_native_plugin ALL
    COMMAND ${CMAKE_COMMAND}
        "-DSRC=${QOS_SDK_BIN}/rexgpu-native$<$<CONFIG:RelWithDebInfo>:rd>$<$<CONFIG:Debug>:d>.dll"
        "-DDST=$<TARGET_FILE_DIR:qos_mp>"
        -P ${CMAKE_BINARY_DIR}/copy_if_exists.cmake
    VERBATIM)
add_dependencies(qos_mp_native_plugin qos_mp)
