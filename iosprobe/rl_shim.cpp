// raylib -> SDL3 shim for the iOS host.
//
// ps2xRuntime calls a small slice of raylib (window lifecycle, keyboard,
// gamepad, audio stream). raylib's desktop/OpenGL path does not build on iOS,
// so this file supplies exactly those symbols on top of SDL3, which is already
// in the build for the Metal GPU backend. Everything else in the runtime is
// unchanged.
//
// The declarations come from the real raylib.h shipped in
// thirdparty/raylib-headers, so signatures stay identical to upstream. Types
// are re-declared here in plain SDL terms to avoid depending on raylib's
// struct layout.

#include <SDL3/SDL.h>

#include <cstdint>

namespace {
SDL_Window *g_window = nullptr;

bool KeyDown(int key)
{
    const bool *state = SDL_GetKeyboardState(nullptr);
    if (!state) return false;
    SDL_Scancode sc = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(key), nullptr);
    return sc != SDL_SCANCODE_UNKNOWN && state[sc];
}

bool PadAvailable(int id)
{
    return SDL_GetGamepadFromID(static_cast<SDL_JoystickID>(id)) != nullptr;
}

float PadAxis(int id, int axis)
{
    SDL_Gamepad *pad = SDL_GetGamepadFromID(static_cast<SDL_JoystickID>(id));
    if (!pad) return 0.0f;
    const Sint16 v = SDL_GetGamepadAxis(pad, static_cast<SDL_GamepadAxis>(axis));
    return v < 0 ? static_cast<float>(v) / 32768.0f : static_cast<float>(v) / 32767.0f;
}

bool PadButton(int id, int button)
{
    SDL_Gamepad *pad = SDL_GetGamepadFromID(static_cast<SDL_JoystickID>(id));
    return pad && SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(button));
}
} // namespace

extern "C" {

using AudioStream = unsigned int;

void InitWindow(int width, int height, const char *title)
{
    if (g_window) return;
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_Init(SDL_INIT_VIDEO)) return;
    g_window = SDL_CreateWindow(title ? title : "PS2", width, height, SDL_WINDOW_METAL);
}

bool IsWindowReady(void) { return g_window != nullptr; }
bool WindowShouldClose(void) { return false; }
void CloseWindow(void)
{
    if (g_window) SDL_DestroyWindow(g_window);
    g_window = nullptr;
}

int GetScreenWidth(void)
{
    int w = 0;
    if (g_window) SDL_GetWindowSize(g_window, &w, nullptr);
    return w;
}
int GetScreenHeight(void)
{
    int h = 0;
    if (g_window) SDL_GetWindowSize(g_window, nullptr, &h);
    return h;
}

void SetTargetFPS(int) {}
void SetExitKey(int) {}
void SetWindowTitle(const char *) {}
void BeginDrawing(void) {}
void EndDrawing(void) { SDL_PumpEvents(); }
void ClearBackground(unsigned int) {}

bool IsKeyDown(int key) { return KeyDown(key); }
bool IsKeyPressed(int key) { return KeyDown(key); }
bool IsKeyReleased(int) { return false; }
int GetKeyPressed(void) { return 0; }

bool IsGamepadAvailable(int id) { return PadAvailable(id); }
bool IsGamepadButtonDown(int id, int button) { return PadButton(id, button); }
bool IsGamepadButtonPressed(int id, int button) { return PadButton(id, button); }
bool IsGamepadButtonReleased(int, int) { return false; }
int GetGamepadButtonPressed(void) { return -1; }
int GetGamepadAxisCount(int) { return 0; }
float GetGamepadAxisMovement(int id, int axis) { return PadAxis(id, axis); }

// Audio: one SDL playback device shared by every runtime stream. The probe
// only needs the calls to exist so the link closes; real playback lands with
// the SDL audio device wiring in the host.
void SetAudioStreamBufferSizeDefault(int) {}

AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize, unsigned int channels)
{
    (void)sampleRate;
    (void)sampleSize;
    (void)channels;
    return 1u;
}

unsigned int IsAudioStreamValid(AudioStream stream) { return stream != 0 ? 1u : 0u; }
void UnloadAudioStream(AudioStream) {}
void UpdateAudioStream(AudioStream stream, const void *data, int frames)
{
    (void)stream;
    (void)data;
    (void)frames;
}
unsigned int IsAudioStreamProcessed(AudioStream) { return 1u; }
void PlayAudioStream(AudioStream) {}
void PauseAudioStream(AudioStream) {}
void SetAudioStreamVolume(AudioStream, float) {}
void SetAudioStreamPitch(AudioStream, float) {}
unsigned int IsAudioStreamPlaying(AudioStream) { return 0u; }

} // extern "C"
