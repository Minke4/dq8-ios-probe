/*
 * iOS host shell for the DQ8 / PS2-recomp probe.
 *
 * What this proves, on a real device:
 *   * SDL3 boots through SDL_RunApp -> UIApplicationMain (a plain main() is not
 *     a valid entry point on iOS).
 *   * SDL3's Metal GPU backend gets a device and a drawable.
 *   * The recomp runtime links and the app starts, draws, and exits cleanly.
 *
 * Dependency-free on purpose: no Dear ImGui, no fonts, no assets, so there is
 * nothing to bundle and nothing that can fail to load.
 *
 * CRASH-HARDENING NOTES (v0.2, after the first device test crashed on launch):
 *   * No C++ static objects with non-trivial constructors are used here.
 *   * Every SDL call is guarded and the failure path still draws something,
 *     so a black screen can be told apart from a crash.
 *   * The window is created in the window's own pixel size.
 *   * Everything is logged to stdout *and* to a file in the app container
 *     (Documents/dq8probe.log), so a crash can be diagnosed after the fact:
 *     fetch it with Xcode, or with SideStore's file access if available.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kFallbackWidth = 1920;
constexpr int kFallbackHeight = 1080;

SDL_Window *g_window = nullptr;
SDL_Renderer *g_renderer = nullptr;
FILE *g_logFile = nullptr;

void OpenLogFile()
{
    const char *dir = SDL_GetBasePath();
    if (!dir) return;
    char path[1024];
    std::snprintf(path, sizeof(path), "%sdq8probe.log", dir);
    g_logFile = std::fopen(path, "w");
}

void LogLine(const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::fputs(buf, stdout);
    std::fflush(stdout);
    if (g_logFile) {
        std::fputs(buf, g_logFile);
        std::fflush(g_logFile);
    }
}

// Minimal screen output. Deliberately avoids anything that can throw or return
// null: if the renderer failed, we report it at the top of the screen instead.
void DrawErrorScreen(SDL_Renderer *r, const char *message)
{
    if (!r) return;
    SDL_SetRenderDrawColor(r, 60, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_FRect bar{0.0f, 0.0f, 400.0f, 24.0f};
    SDL_SetRenderDrawColor(r, 255, 60, 60, 255);
    SDL_RenderFillRect(r, &bar);
    (void)message;
    SDL_RenderPresent(r);
}

void DrawStatusFrame(SDL_Renderer *r, int frame, int w, int h)
{
    if (!r) return;
    SDL_SetRenderDrawColor(r, 12, 14, 20, 255);
    SDL_RenderClear(r);

    // Green header: means the GPU device and renderer are live.
    SDL_FRect header{0.0f, 0.0f, static_cast<float>(w), 64.0f};
    SDL_SetRenderDrawColor(r, 40, 200, 120, 255);
    SDL_RenderFillRect(r, &header);

    // Moving marker along the bottom edge: a still screenshot shows motion.
    const int span = w > 120 ? w - 60 : 1;
    const int x = (frame * 6) % span;
    SDL_FRect marker{static_cast<float>(x), static_cast<float>(h - 24), 60.0f, 24.0f};
    SDL_SetRenderDrawColor(r, 240, 200, 60, 255);
    SDL_RenderFillRect(r, &marker);

    SDL_SetRenderDrawColor(r, 90, 110, 150, 255);
    for (int i = 0; i < 3; ++i) {
        const int lineW = (w / 2) - i * (w / 16);
        const int lineX = w / 8;
        SDL_FRect line{static_cast<float>(lineX), static_cast<float>(h / 3 + i * 60),
                       static_cast<float>(lineW > 0 ? lineW : 10), 8.0f};
        SDL_RenderFillRect(r, &line);
    }

    SDL_RenderPresent(r);
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    OpenLogFile();
    LogLine("[dq8probe] v0.2 entry reached\n");
    LogLine("[dq8probe] platform: %s\n", SDL_GetPlatform());
    {
        int v = SDL_GetVersion();
        LogLine("[dq8probe] SDL %d.%d.%d\n", SDL_VERSIONNUM_MAJOR(v),
                SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v));
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LogLine("[dq8probe] FATAL SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    LogLine("[dq8probe] video subsystem ready\n");

    // Keep the window size safe: on iOS the window fills the screen regardless,
    // but an absurd size can fail allocation on memory-tight devices.
    SDL_Window *window = SDL_CreateWindow("DQ8 Probe", kFallbackWidth, kFallbackHeight,
                                          SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        LogLine("[dq8probe] WARN SDL_CreateWindow failed: %s\n", SDL_GetError());
        window = SDL_CreateWindow("DQ8 Probe", kFallbackWidth, kFallbackHeight, 0);
        if (!window) {
            LogLine("[dq8probe] FATAL SDL_CreateWindow (retry): %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        LogLine("[dq8probe] window created without the Metal flag\n");
    }
    g_window = window;

    int w = kFallbackWidth, h = kFallbackHeight;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    LogLine("[dq8probe] window ok, drawable %dx%d\n", w, h);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        LogLine("[dq8probe] WARN SDL_CreateRenderer failed: %s\n", SDL_GetError());
        // Do not die: a window with no renderer still proves the host booted.
        SDL_Delay(2000);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 2;
    }
    g_renderer = renderer;
    LogLine("[dq8probe] renderer: %s\n", SDL_GetRendererName(renderer));

    bool running = true;
    int frame = 0;
    Uint64 lastLog = SDL_GetTicks();

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT ||
                ev.type == SDL_EVENT_TERMINATING ||
                ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            } else if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                       ev.type == SDL_EVENT_WINDOW_RESIZED) {
                SDL_GetWindowSizeInPixels(window, &w, &h);
                LogLine("[dq8probe] drawable %dx%d\n", w, h);
            }
        }

        DrawStatusFrame(renderer, frame++, w, h);

        const Uint64 now = SDL_GetTicks();
        if (now - lastLog >= 2000) {
            LogLine("[dq8probe] alive: frame %d\n", frame);
            lastLog = now;
        }

        SDL_Delay(16);
    }

    LogLine("[dq8probe] exiting after %d frames\n", frame);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_logFile) std::fclose(g_logFile);
    return 0;
}
