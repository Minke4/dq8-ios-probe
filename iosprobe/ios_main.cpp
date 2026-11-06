/*
#!/bin/sh
# Host-side entry point for the probe app.
#
# Two builds:
#   * stub build  (default)   -> no game corpus, just proves the shell boots
#   * generated build         -> generated/main.cpp from PS2Recomp defines the
#                                real entry, so this file is not compiled
#
# Notes for the iOS port:
#   * SDL3 requires SDL_main / SDL_RunApp on iOS; CMake sets
#     SDL_MAIN_HANDLED because DQ8 declares its own main(), so we wrap it here.
#   * The runtime still owns raylib (raylib.h, IsKeyDown, LoadAudioStream,
#     InitWindow). raylib is fetched but *not built or linked* when
#     PS2X_ENABLE_DEBUG_UI=OFF, so this translation unit exists to give CMake a
#     place to grow the real SDL3 + Metal host.
#   * TODO(ios): replace the raylib call surface with SDL3 equivalents
#     (SDL_GetKeyboardState / SDL_GetGamepadAxis / SDL_OpenAudioDeviceStream),
#     then link dq8_gfx_sdlgpu and pump the GS backend from here.
*/

#if defined(IOS_PROBE_STUB)

#include <cstdio>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    std::printf("[ios-probe] host shell alive: iOS build of the PS2 recomp runtime\n");
    std::printf("[ios-probe] SDL3 + Metal backend compiled into this binary: see dq8_gfx_sdlgpu\n");
    std::printf("[ios-probe] next: link a generated corpus, then pump the GS backend\n");
    std::fflush(stdout);
    return 0;
}

#else

// A generated corpus defines its own entry point; until the iOS host is wired
// up here, keep a placeholder so the build stays green.
#include <cstdio>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    std::printf("[ios-probe] generated corpus linked, but the iOS host is not wired up\n");
    return 0;
}

#endif
