# iOS host shell for the probe.
#
# Built only when IOS_PROBE=ON.
#
# Two things this links together:
#   * ios_main.cpp  -- the SDL3 entry point. SDL3 on iOS needs SDL_RunApp /
#                      UIApplicationMain, which SDL_main.h provides by renaming
#                      main(); this file just uses SDL normally.
#   * rl_shim.cpp   -- ps2xRuntime calls a small slice of raylib for
#                      window/keyboard/gamepad/audio, and raylib's desktop
#                      OpenGL path cannot build for iOS. The shim implements
#                      those symbols on SDL3.
#
# Real rendering is dq8_gfx_sdlgpu (SDL3 GPU -> Metal); the probe screen is a
# dependency-free SDL_Renderer status frame.

option(IOS_PROBE_STUB "Link the stub host instead of a generated corpus" ON)

set(IOS_PROBE_RAYLIB_HEADERS "${CMAKE_SOURCE_DIR}/thirdparty/raylib-headers")

add_executable(dq8-ios ios_main.cpp rl_shim.cpp)

if(IOS_PROBE_STUB)
    target_compile_definitions(dq8-ios PRIVATE IOS_PROBE_STUB=1)
endif()

# The runtime includes "raylib.h" through ps2_host_backend.h; point it at the
# upstream header we vendor so the declarations match the shim exactly.
target_include_directories(dq8-ios PRIVATE "${IOS_PROBE_RAYLIB_HEADERS}")
target_link_libraries(dq8-ios PRIVATE ps2_runtime)
if(TARGET dq8_gfx_sdlgpu)
    target_link_libraries(dq8-ios PRIVATE dq8_gfx_sdlgpu)
endif()
# Imported targets belong to the directory that found them, so SDL3::SDL3 is
# not visible here even though src/gfx found it. Resolve it locally.
if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 3.2 CONFIG REQUIRED)
endif()
target_link_libraries(dq8-ios PRIVATE SDL3::SDL3)

if(APPLE)
    target_link_libraries(dq8-ios PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework UIKit"
        "-framework Foundation"
        "-framework CoreFoundation"
        "-framework AVFAudio"
        "-framework AudioToolbox"
        "-framework GameController")
endif()

set_target_properties(dq8-ios PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "DQ8Probe"
    OUTPUT_NAME "DQ8Probe")

set_property(TARGET dq8-ios PROPERTY XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")

# Launch screen / orientation / minimum OS come from a generated Info.plist so
# the unsigned IPA is installable by anything that only re-signs it.
set(IOS_PROBE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/DQ8Probe-Info.plist")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in" "${IOS_PROBE_INFO_PLIST}" @ONLY)

set_target_properties(dq8-ios PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${IOS_PROBE_INFO_PLIST}")
