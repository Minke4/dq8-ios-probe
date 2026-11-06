/*
 * iOS host shell for the DQ8 / PS2-recomp probe.
 *
 * What this proves, on a real device:
 *   * SDL3 boots through SDL_RunApp -> UIApplicationMain (SDL2/SDL3 require
 *     this on iOS; a plain main() is not a valid entry point on iOS).
 *   * SDL3's Metal GPU backend gets a device and a drawable.
 *   * An .app built only from the PS2 recomp runtime + the SDL3 GPU backend
 *     starts, reports its status on screen, and exits cleanly when the app is
 *     backgrounded/killed.
 *
 * Deliberately dependency-free: no Dear ImGui, no fonts, no asset files, so
 * there is nothing to bundle and nothing that can fail to load. Rendering is a
 * single clear pass plus a few coloured bars, drawn with SDL_Renderer (which
 * sits on the same Metal device on iOS).
 *
 * Next step after this screenshot: swap the placeholder for the SDL3 GPU GS
 * backend (dq8_gfx_sdlgpu), then link a generated corpus.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>
#include <cstdarg>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

void LogLine(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stdout, fmt, args);
    va_end(args);
    std::fflush(stdout);
}

// Draws a simple status screen: a dark background, a green/amber bar that
// encodes "we reached the render loop", and a moving highlight so a screenshot
// can tell a live loop from a single frozen frame.
void DrawStatusFrame(SDL_Renderer *renderer, int frame)
{
    SDL_SetRenderDrawColor(renderer, 12, 14, 20, 255);
    SDL_RenderClear(renderer);

    // Title bar: green once the GPU device and renderer are live.
    SDL_SetRenderDrawColor(renderer, 40, 200, 120, 255);
    SDL_FRect bar{0.0f, 0.0f, static_cast<float>(kWindowWidth), 64.0f};
    SDL_RenderFillRect(renderer, &bar);

    // Frame counter as a moving block (bottom edge) so a still screenshot shows
    // the loop really advanced.
    SDL_SetRenderDrawColor(renderer, 240, 200, 60, 255);
    const int x = (frame * 6) % (kWindowWidth - 60);
    SDL_FRect marker{static_cast<float>(x), static_cast<float>(kWindowHeight - 24), 60.0f, 24.0f};
    SDL_RenderFillRect(renderer, &marker);

    // Three horizontal lines: a cheap visual "this is the probe, not the game".
    SDL_SetRenderDrawColor(renderer, 90, 110, 150, 255);
    for (int i = 0; i < 3; ++i) {
        SDL_FRect line{120.0f, static_cast<float>(240 + i * 60),
                       static_cast<float>(800 - i * 120), 8.0f};
        SDL_RenderFillRect(renderer, &line);
    }

    SDL_RenderPresent(renderer);
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LogLine("[ios-probe] entry reached (SDL_RunApp wrapper)\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LogLine("[ios-probe] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    int sdlVersion = SDL_GetVersion();
    LogLine("[ios-probe] SDL %d.%d.%d video subsystem ready\n",
            SDL_VERSIONNUM_MAJOR(sdlVersion), SDL_VERSIONNUM_MINOR(sdlVersion),
            SDL_VERSIONNUM_MICRO(sdlVersion));

    SDL_Window *window = SDL_CreateWindow("DQ8 Probe", kWindowWidth, kWindowHeight,
                                          SDL_WINDOW_METAL);
    if (!window) {
        LogLine("[ios-probe] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        LogLine("[ios-probe] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    LogLine("[ios-probe] renderer: %s\n", SDL_GetRendererName(renderer));

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    LogLine("[ios-probe] drawable: %dx%d\n", w, h);

    bool running = true;
    int frame = 0;
    Uint64 lastLog = SDL_GetTicks();

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT ||
                ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                ev.type == SDL_EVENT_TERMINATING) {
                running = false;
            }
        }

        DrawStatusFrame(renderer, frame++);

        const Uint64 now = SDL_GetTicks();
        if (now - lastLog >= 2000) {
            LogLine("[ios-probe] alive: frame %d\n", frame);
            lastLog = now;
        }

        SDL_Delay(16);
    }

    LogLine("[ios-probe] exiting after %d frames\n", frame);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
