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

# ---------------------------------------------------------------------------
# Link SDL3 statically.
#
# SDL3::SDL3 is the *shared* library when SDL is built with SDL_SHARED=ON, which
# leaves "@rpath/libSDL3.0.dylib" in the binary and makes dyld kill the app at
# launch when no copy is embedded. Always prefer SDL3::SDL3-static.
# ---------------------------------------------------------------------------
function(iosprobe_link_sdl3 target)
    if(NOT TARGET SDL3::SDL3-static AND NOT TARGET SDL3::SDL3)
        find_package(SDL3 3.2 CONFIG REQUIRED)
    endif()
    if(TARGET SDL3::SDL3-static)
        target_link_libraries(${target} PRIVATE SDL3::SDL3-static)
        message(STATUS "iosprobe: ${target} links SDL3 static")
    elseif(TARGET SDL3::SDL3)
        target_link_libraries(${target} PRIVATE SDL3::SDL3)
        message(WARNING "iosprobe: ${target} links SDL3 shared; the .app must embed it")
    else()
        message(FATAL_ERROR "iosprobe: no SDL3 target available")
    endif()

    # Objective-C classes living in a static archive are dropped by the linker
    # unless it is told to load every object file: without -ObjC, SDLUIKitDelegate
    # and SDLUIKitSceneDelegate vanish and UIApplicationMain finds no delegate.
    if(APPLE)
        target_link_options(${target} PRIVATE "-ObjC")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# The full probe: runtime + graphics backend + a status screen.
# ---------------------------------------------------------------------------
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
iosprobe_link_sdl3(dq8-ios)

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

# ---------------------------------------------------------------------------
# The bare SDL3 probe: nothing but SDL. If this one crashes too, the problem is
# SDL3 / iOS itself; if it runs, the problem is inside the runtime.
# ---------------------------------------------------------------------------
add_executable(dq8-ios-minimal minimal_main.cpp)
iosprobe_link_sdl3(dq8-ios-minimal)

if(APPLE)
    target_link_libraries(dq8-ios-minimal PRIVATE
        "-framework QuartzCore"
        "-framework UIKit"
        "-framework Foundation"
        "-framework CoreFoundation")
endif()

set_target_properties(dq8-ios-minimal PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "DQ8Minimal"
    OUTPUT_NAME "DQ8Minimal")

set_property(TARGET dq8-ios-minimal PROPERTY XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")

# get-task-allow so the app can be attached to a debugger / console when it is
# re-signed with a development profile. Harmless for ad-hoc signing tools.
set(IOS_PROBE_ENTITLEMENTS "${CMAKE_CURRENT_BINARY_DIR}/DQ8Probe.entitlements")
file(WRITE "${IOS_PROBE_ENTITLEMENTS}"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
"<dict>\n"
"    <key>get-task-allow</key>\n"
"    <true/>\n"
"    <key>com.apple.security.get-task-allow</key>\n"
"    <true/>\n"
"</dict>\n"
"</plist>\n")

# Each app gets its own Info.plist: CFBundleExecutable has to match its binary.
foreach(pair "dq8-ios;DQ8Probe;dev.minis.dq8probe;DQ8 Probe" "dq8-ios-minimal;DQ8Minimal;dev.minis.dq8minimal;DQ8 Minimal")
    list(GET pair 0 _target)
    list(GET pair 1 _exe)
    list(GET pair 2 _bundle)
    list(GET pair 3 _name)
    set(IOS_PROBE_EXECUTABLE "${_exe}")
    set(IOS_PROBE_BUNDLE_ID "${_bundle}")
    set(IOS_PROBE_DISPLAY_NAME "${_name}")
    set(_plist "${CMAKE_CURRENT_BINARY_DIR}/${_exe}-Info.plist")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in" "${_plist}" @ONLY)
    set_target_properties(${_target} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${_plist}"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${IOS_PROBE_ENTITLEMENTS}")
endforeach()
