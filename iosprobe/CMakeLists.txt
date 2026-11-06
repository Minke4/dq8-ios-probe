# iOS host shell for the probe.
#
# Built only when IOS_PROBE=ON. It exists so the iOS target has something of its
# own to link while the raylib call surface inside ps2xRuntime is replaced by
# SDL3 (SDL_GetKeyboardState / SDL_GetGamepadAxis / SDL_OpenAudioDeviceStream).

option(IOS_PROBE_STUB "Link the stub host instead of a generated corpus" ON)

add_executable(dq8-ios ios_main.cpp)

if(NOT IOS_PROBE_STUB)
    message(STATUS "iosprobe: generated corpus build requested; host is still a placeholder")
endif()

# The game runtime and the graphics backend. A link failure here is the
# deliverable: the log names the raylib symbols that still need SDL3.
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
