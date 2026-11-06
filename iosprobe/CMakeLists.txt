# iOS host shell for the probe.
#
# Built only when IOS_PROBE=ON. Its reason to exist: ps2xRuntime includes
# raylib.h and calls a small slice of raylib for window/keyboard/gamepad/audio,
# and raylib's desktop OpenGL path cannot build for iOS. rl_shim.cpp supplies
# those symbols on top of SDL3, and the real rendering is dq8_gfx_sdlgpu
# (SDL3 GPU -> Metal).

option(IOS_PROBE_STUB "Link the stub host instead of a generated corpus" ON)

set(IOS_PROBE_RAYLIB_HEADERS "${CMAKE_SOURCE_DIR}/thirdparty/raylib-headers")

add_executable(dq8-ios ios_main.cpp rl_shim.cpp)

# The runtime includes "raylib.h" through ps2_host_backend.h; point it at the
# upstream header we vendor, so the declarations match the shim exactly.
target_include_directories(dq8-ios PRIVATE "${IOS_PROBE_RAYLIB_HEADERS}")
target_link_libraries(dq8-ios PRIVATE ps2_runtime)
if(TARGET dq8_gfx_sdlgpu)
    target_link_libraries(dq8-ios PRIVATE dq8_gfx_sdlgpu)
endif()

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
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.minis.dq8probe"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    OUTPUT_NAME "DQ8Probe")

set_property(TARGET dq8-ios PROPERTY XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
