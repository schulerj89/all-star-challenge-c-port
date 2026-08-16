#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "allstar_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLSTAR_WINDOW_SCALE 3
#define ALLSTAR_FRAME_RATE 59.7275
#define ALLSTAR_FRAME_SECONDS (1.0 / ALLSTAR_FRAME_RATE)
#define ALLSTAR_MAX_CATCHUP_FRAMES 4

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Gamepad *gamepad;
    AllStarGame game;
    uint8_t raw_buttons;
    Uint64 previous_time_ns;
    double accumulator;
    bool game_initialized;
    bool suspended;
} AllStarSdlApp;

static bool file_exists(const char *path) {
    FILE *file;
    if (!path || !path[0]) return false;
    file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static bool join_path(char *out, size_t out_size,
                      const char *base, const char *relative) {
    size_t base_length;
    if (!out || out_size == 0 || !base || !relative) return false;
    base_length = strlen(base);
    if (base_length > 0 &&
        (base[base_length - 1] == '/' || base[base_length - 1] == '\\')) {
        return snprintf(out, out_size, "%s%s", base, relative) <
            (int)out_size;
    }
    return snprintf(out, out_size, "%s/%s", base, relative) <
        (int)out_size;
}

static bool find_asset_pack(int argc, char **argv,
                            char *path, size_t path_size) {
    const char *base_path;
    static const char *working_paths[] = {
        "allstar.assetpack",
        "build/allstar.assetpack",
        "build/macos/allstar.assetpack"
    };
    size_t index;

    if (argc >= 2 && file_exists(argv[1])) {
        snprintf(path, path_size, "%s", argv[1]);
        return true;
    }

    base_path = SDL_GetBasePath();
    if (base_path) {
        if (join_path(path, path_size, base_path, "allstar.assetpack") &&
            file_exists(path)) return true;
        if (join_path(path, path_size, base_path,
                      "../Resources/allstar.assetpack") &&
            file_exists(path)) return true;
    }

    for (index = 0;
         index < sizeof(working_paths) / sizeof(working_paths[0]);
         index++) {
        if (file_exists(working_paths[index])) {
            snprintf(path, path_size, "%s", working_paths[index]);
            return true;
        }
    }
    return false;
}

static uint8_t keyboard_button(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_RIGHT: return ALLSTAR_BTN_RIGHT;
        case SDL_SCANCODE_LEFT: return ALLSTAR_BTN_LEFT;
        case SDL_SCANCODE_UP: return ALLSTAR_BTN_UP;
        case SDL_SCANCODE_DOWN: return ALLSTAR_BTN_DOWN;
        case SDL_SCANCODE_Z:
        case SDL_SCANCODE_J: return ALLSTAR_BTN_A;
        case SDL_SCANCODE_X:
        case SDL_SCANCODE_K: return ALLSTAR_BTN_B;
        case SDL_SCANCODE_RETURN: return ALLSTAR_BTN_START;
        case SDL_SCANCODE_SPACE:
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT: return ALLSTAR_BTN_SELECT;
        default: return 0;
    }
}

static uint8_t gamepad_button(Uint8 button) {
    switch ((SDL_GamepadButton)button) {
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ALLSTAR_BTN_RIGHT;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return ALLSTAR_BTN_LEFT;
        case SDL_GAMEPAD_BUTTON_DPAD_UP: return ALLSTAR_BTN_UP;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return ALLSTAR_BTN_DOWN;
        case SDL_GAMEPAD_BUTTON_SOUTH: return ALLSTAR_BTN_A;
        case SDL_GAMEPAD_BUTTON_EAST: return ALLSTAR_BTN_B;
        case SDL_GAMEPAD_BUTTON_START: return ALLSTAR_BTN_START;
        case SDL_GAMEPAD_BUTTON_BACK: return ALLSTAR_BTN_SELECT;
        default: return 0;
    }
}

static void update_button(AllStarSdlApp *app, uint8_t mask, bool down) {
    if (!app || mask == 0) return;
    if (down) app->raw_buttons |= mask;
    else app->raw_buttons &= (uint8_t)~mask;
}

static void select_palette(AllStarSdlApp *app, SDL_Scancode scancode) {
    if (!app || !app->game.renderer) return;
    switch (scancode) {
        case SDL_SCANCODE_1:
            allstar_renderer_set_palette_style(
                app->game.renderer, ALLSTAR_PALETTE_DMG_ORIGINAL);
            break;
        case SDL_SCANCODE_2:
            allstar_renderer_set_palette_style(
                app->game.renderer, ALLSTAR_PALETTE_POCKET_BW);
            break;
        case SDL_SCANCODE_3:
            allstar_renderer_set_palette_style(
                app->game.renderer, ALLSTAR_PALETTE_MODERN_VIBRANT);
            break;
        case SDL_SCANCODE_P:
            allstar_renderer_cycle_palette(app->game.renderer);
            break;
        default:
            break;
    }
}

static bool present_frame(AllStarSdlApp *app) {
    if (!app || !app->game.renderer || !app->game.renderer->pixels) return false;
    if (!SDL_UpdateTexture(app->texture, NULL, app->game.renderer->pixels,
                           ALLSTAR_GB_WIDTH * (int)sizeof(AllStarColor))) {
        SDL_Log("SDL_UpdateTexture failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255) ||
        !SDL_RenderClear(app->renderer) ||
        !SDL_RenderTexture(app->renderer, app->texture, NULL, NULL) ||
        !SDL_RenderPresent(app->renderer)) {
        SDL_Log("SDL frame presentation failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    AllStarSdlApp *app;
    char asset_path[1024];

    if (!appstate) return SDL_APP_FAILURE;
    SDL_SetAppMetadata("NBA All-Star Challenge", "0.1",
                       "com.schulerj89.allstarchallenge");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app = (AllStarSdlApp *)calloc(1, sizeof(*app));
    if (!app) return SDL_APP_FAILURE;
    *appstate = app;

    if (!SDL_CreateWindowAndRenderer(
            "NBA All-Star Challenge (Native C Port)",
            ALLSTAR_GB_WIDTH * ALLSTAR_WINDOW_SCALE,
            ALLSTAR_GB_HEIGHT * ALLSTAR_WINDOW_SCALE,
            SDL_WINDOW_RESIZABLE, &app->window, &app->renderer)) {
        SDL_Log("Could not create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_SetRenderLogicalPresentation(
            app->renderer, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT,
            SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        SDL_Log("Could not configure logical presentation: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app->texture = SDL_CreateTexture(
        app->renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    if (!app->texture ||
        !SDL_SetTextureScaleMode(app->texture, SDL_SCALEMODE_NEAREST)) {
        SDL_Log("Could not create framebuffer texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!find_asset_pack(argc, argv, asset_path, sizeof(asset_path))) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "Asset pack required",
            "A version-17 allstar.assetpack was not found. Build it from "
            "your legally owned Game Boy ROM with allstar_port "
            "--build-assetpack.",
            app->window);
        return SDL_APP_FAILURE;
    }
    if (!allstar_game_init(&app->game, asset_path)) {
        SDL_Log("Could not initialize game with asset pack: %s", asset_path);
        return SDL_APP_FAILURE;
    }
    app->game_initialized = true;
    if (SDL_getenv("ALLSTAR_AUDIO_TEST")) {
        allstar_audio_play_sfx(
            &app->game.audio, ALLSTAR_SFX_MENU_SELECT);
        SDL_Log("Queued ROM-derived menu sound for audio self-test");
    }
    app->previous_time_ns = SDL_GetTicksNS();
    app->accumulator = ALLSTAR_FRAME_SECONDS;
    SDL_Log("Loaded asset pack: %s", asset_path);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AllStarSdlApp *app = (AllStarSdlApp *)appstate;
    uint8_t mask;
    if (!app || !event) return SDL_APP_CONTINUE;

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            mask = keyboard_button(event->key.scancode);
            update_button(app, mask, true);
            if (!event->key.repeat) select_palette(app, event->key.scancode);
            break;
        case SDL_EVENT_KEY_UP:
            update_button(app, keyboard_button(event->key.scancode), false);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!app->gamepad) app->gamepad = SDL_OpenGamepad(event->gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (app->gamepad &&
                SDL_GetGamepadID(app->gamepad) == event->gdevice.which) {
                SDL_CloseGamepad(app->gamepad);
                app->gamepad = NULL;
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            update_button(app, gamepad_button(event->gbutton.button), true);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            update_button(app, gamepad_button(event->gbutton.button), false);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            app->raw_buttons = 0;
            break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            app->suspended = true;
            app->raw_buttons = 0;
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            app->suspended = false;
            app->accumulator = 0.0;
            app->previous_time_ns = SDL_GetTicksNS();
            break;
        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AllStarSdlApp *app = (AllStarSdlApp *)appstate;
    Uint64 now;
    double elapsed;
    int frame_count = 0;

    if (!app || !app->game_initialized) return SDL_APP_FAILURE;
    if (!app->game.is_running) return SDL_APP_SUCCESS;
    if (app->suspended) {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    now = SDL_GetTicksNS();
    elapsed = (double)(now - app->previous_time_ns) / 1000000000.0;
    app->previous_time_ns = now;
    if (elapsed > ALLSTAR_FRAME_SECONDS * ALLSTAR_MAX_CATCHUP_FRAMES)
        elapsed = ALLSTAR_FRAME_SECONDS * ALLSTAR_MAX_CATCHUP_FRAMES;
    app->accumulator += elapsed;

    while (app->accumulator >= ALLSTAR_FRAME_SECONDS &&
           frame_count < ALLSTAR_MAX_CATCHUP_FRAMES) {
        allstar_input_update(&app->game.input, app->raw_buttons);
        allstar_game_tick(&app->game, (float)ALLSTAR_FRAME_SECONDS);
        app->accumulator -= ALLSTAR_FRAME_SECONDS;
        frame_count++;
    }

    if (!present_frame(app)) return SDL_APP_FAILURE;
    if (app->accumulator < ALLSTAR_FRAME_SECONDS * 0.5) SDL_Delay(1);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    AllStarSdlApp *app = (AllStarSdlApp *)appstate;
    (void)result;
    if (app) {
        if (app->game_initialized) allstar_game_shutdown(&app->game);
        if (app->gamepad) SDL_CloseGamepad(app->gamepad);
        if (app->texture) SDL_DestroyTexture(app->texture);
        if (app->renderer) SDL_DestroyRenderer(app->renderer);
        if (app->window) SDL_DestroyWindow(app->window);
        free(app);
    }
    SDL_Quit();
}
