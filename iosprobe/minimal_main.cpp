/*
 * Bare SDL3 probe for iOS.
 *
 * Purpose: bisect the "app dies at launch" failure. This binary links nothing
 * but SDL3 and the system frameworks -- no PS2 runtime, no graphics backend,
 * no raylib shim. If this one launches and shows a screen, SDL3 and the iOS
 * host setup are fine and the fault is inside the heavy probe. If this one
 * crashes too, the fault is in how we build/launch an SDL3 app at all.
 *
 * Draws: a green header (SDL up), a moving yellow block (render loop alive),
 * and a red header instead if any SDL step failed.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdarg>
#include <cstdio>

namespace {
const char *g_stage = "start";

void Say(const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::fprintf(stderr, "[dq8minimal] %s\n", buf);
    std::fflush(stderr);
}
} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    {
        int v = SDL_GetVersion();
        Say("entry: SDL %d.%d.%d", SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v),
            SDL_VERSIONNUM_MICRO(v));
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Say("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    Say("video ready");

    SDL_Window *window = SDL_CreateWindow("DQ8 Minimal", 1280, 720, 0);
    if (!window) {
        Say("CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 2;
    }
    Say("window ready");

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        Say("CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }
    Say("renderer: %s", SDL_GetRendererName(renderer));

    int w = 1280, h = 720;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    Say("drawable %dx%d", w, h);

    bool running = true;
    int frame = 0;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_TERMINATING ||
                ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 10, 20, 30, 255);
        SDL_RenderClear(renderer);

        SDL_FRect head{0.0f, 0.0f, static_cast<float>(w), 72.0f};
        SDL_SetRenderDrawColor(renderer, 40, 200, 120, 255);
        SDL_RenderFillRect(renderer, &head);

        SDL_FRect marker{static_cast<float>((frame * 8) % (w > 100 ? w - 80 : 1)),
                         static_cast<float>(h - 32), 80.0f, 32.0f};
        SDL_SetRenderDrawColor(renderer, 255, 220, 60, 255);
        SDL_RenderFillRect(renderer, &marker);

        SDL_RenderPresent(renderer);
        ++frame;

        if (frame % 120 == 0) {
            Say("alive: frame %d", frame);
        }
        SDL_Delay(16);
    }

    Say("exit after %d frames", frame);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
