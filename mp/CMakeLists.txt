# qosmp - 007: Quantum of Solace multiplayer (game/default_mp.xex), recompiled with ReXGlue: a second project beside
# single player (the folder above), sharing its game folder, crash handler and SDK. HANDOFF.md, "Multiplayer".
#
# SDK boilerplate lives in generated/rexglue.cmake (single player's, renamed to qosmp).

cmake_minimum_required(VERSION 3.25)
project(qosmp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

set(QOS_SP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(QOS_MP_SOURCES
    src/main.cpp
    src/relaunch_log.cpp
    src/frame_rate_fov.cpp
    src/console_log.cpp
    src/dlc_folder.cpp
    src/dlc_content.cpp
    ${QOS_SP_DIR}/src/crash_win.cpp
    ${QOS_SP_DIR}/src/kbm_binds.cpp
    ${QOS_SP_DIR}/src/pad_prompts.cpp
    ${QOS_SP_DIR}/src/dof.cpp
    src/mouse_look.cpp
    src/menu_keys.cpp
    src/hit_markers.cpp
)

if(WIN32)
    add_executable(qosmp WIN32 ${QOS_MP_SOURCES})
else()
    add_executable(qosmp ${QOS_MP_SOURCES})
endif()

rexglue_setup_target(qosmp GPU_PLUGINS xenos)
target_include_directories(qosmp PRIVATE ${QOS_SP_DIR}/src)

# Single player's renderer assets (shader refs, button pictures), so the native renderer behaves the same.
add_custom_target(qosmp_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${QOS_SP_DIR}/prompts $<TARGET_FILE_DIR:qosmp>/prompts
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${QOS_SP_DIR}/pipelines $<TARGET_FILE_DIR:qosmp>/pipelines
    COMMENT "Copying button pictures and shader refs")
add_dependencies(qosmp_assets qosmp)

# FidelityFX runtime and the native renderer from the SDK, as single player copies them.
get_filename_component(QOS_SDK_BIN "${rexglue_DIR}/../../../bin" ABSOLUTE)
file(GLOB QOS_FFX_DLLS "${QOS_SDK_BIN}/amd_fidelityfx_dx12*.dll")
if(QOS_FFX_DLLS)
    add_custom_command(TARGET qosmp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QOS_FFX_DLLS} $<TARGET_FILE_DIR:qosmp>
        COMMENT "Copying FidelityFX runtime")
endif()
file(WRITE ${CMAKE_BINARY_DIR}/copy_if_exists.cmake
    "if(EXISTS \"\${SRC}\")\n  file(COPY \"\${SRC}\" DESTINATION \"\${DST}\")\nendif()\n")
add_custom_target(qosmp_native_plugin ALL
    COMMAND ${CMAKE_COMMAND}
        "-DSRC=${QOS_SDK_BIN}/rexgpu-native$<$<CONFIG:RelWithDebInfo>:rd>$<$<CONFIG:Debug>:d>.dll"
        "-DDST=$<TARGET_FILE_DIR:qosmp>"
        -P ${CMAKE_BINARY_DIR}/copy_if_exists.cmake
    VERBATIM)
add_dependencies(qosmp_native_plugin qosmp)
