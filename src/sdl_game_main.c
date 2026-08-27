#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "allstar_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLSTAR_WINDOW_SCALE 3
#define ALLSTAR_FRAME_RATE 59.7275
#define ALLSTAR_FRAME_SECONDS (1.0 / ALLSTAR_FRAME_RATE)
#define ALLSTAR_MAX_CATCHUP_FRAMES 4
#define ALLSTAR_MAX_TOUCHES 10

typedef struct {
    SDL_FingerID finger_id;
    uint8_t buttons;
    bool palette;
    bool active;
} AllStarTouchSlot;

typedef struct {
    SDL_FRect safe_area;
    SDL_FRect game;
    SDL_FRect select_button;
    SDL_FRect start_button;
    SDL_FRect palette_button;
    float dpad_x;
    float dpad_y;
    float dpad_radius;
    float button_a_x;
    float button_a_y;
    float button_b_x;
    float button_b_y;
    float face_button_radius;
    bool valid;
} AllStarTouchLayout;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Gamepad *gamepad;
    AllStarGame game;
    uint8_t keyboard_buttons;
    uint8_t gamepad_buttons;
    uint8_t touch_buttons;
    AllStarTouchSlot touches[ALLSTAR_MAX_TOUCHES];
    AllStarTouchLayout touch_layout;
    Uint64 previous_time_ns;
    double accumulator;
    bool game_initialized;
    bool suspended;
    bool touch_controls;
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

static void update_button(uint8_t *buttons, uint8_t mask, bool down) {
    if (!buttons || mask == 0) return;
    if (down) *buttons |= mask;
    else *buttons &= (uint8_t)~mask;
}

static float minimum_float(float left, float right) {
    return left < right ? left : right;
}

static bool point_in_rect(float x, float y, const SDL_FRect *rect) {
    return rect && x >= rect->x && x <= rect->x + rect->w &&
        y >= rect->y && y <= rect->y + rect->h;
}

static bool point_in_circle(float x, float y, float center_x,
                            float center_y, float radius) {
    float dx = x - center_x;
    float dy = y - center_y;
    return dx * dx + dy * dy <= radius * radius;
}

static void update_touch_layout(AllStarSdlApp *app) {
    SDL_Rect safe_window;
    int window_width;
    int window_height;
    int output_width;
    int output_height;
    float scale_x;
    float scale_y;
    float game_width;
    float game_height;
    float max_game_width;
    float left_width;
    float right_start;
    float right_width;
    float pill_width;
    float pill_height;
    AllStarTouchLayout *layout;
    if (!app || !app->window || !app->renderer) return;
    if (!SDL_GetWindowSize(app->window, &window_width, &window_height) ||
        !SDL_GetRenderOutputSize(
            app->renderer, &output_width, &output_height) ||
        window_width <= 0 || window_height <= 0 ||
        output_width <= 0 || output_height <= 0) return;
    if (!SDL_GetWindowSafeArea(app->window, &safe_window)) {
        safe_window.x = 0;
        safe_window.y = 0;
        safe_window.w = window_width;
        safe_window.h = window_height;
    }
    scale_x = (float)output_width / (float)window_width;
    scale_y = (float)output_height / (float)window_height;
    layout = &app->touch_layout;
    layout->safe_area.x = safe_window.x * scale_x;
    layout->safe_area.y = safe_window.y * scale_y;
    layout->safe_area.w = safe_window.w * scale_x;
    layout->safe_area.h = safe_window.h * scale_y;

    game_height = layout->safe_area.h * 0.92f;
    game_width = game_height *
        ((float)ALLSTAR_GB_WIDTH / (float)ALLSTAR_GB_HEIGHT);
    max_game_width = layout->safe_area.w * 0.56f;
    if (game_width > max_game_width) {
        game_width = max_game_width;
        game_height = game_width *
            ((float)ALLSTAR_GB_HEIGHT / (float)ALLSTAR_GB_WIDTH);
    }
    layout->game.w = game_width;
    layout->game.h = game_height;
    layout->game.x = layout->safe_area.x +
        (layout->safe_area.w - game_width) * 0.5f;
    layout->game.y = layout->safe_area.y +
        (layout->safe_area.h - game_height) * 0.5f;

    left_width = layout->game.x - layout->safe_area.x;
    right_start = layout->game.x + layout->game.w;
    right_width = layout->safe_area.x + layout->safe_area.w - right_start;
    layout->dpad_x = layout->safe_area.x + left_width * 0.5f;
    layout->dpad_y = layout->safe_area.y + layout->safe_area.h * 0.56f;
    layout->dpad_radius = minimum_float(
        left_width * 0.30f, layout->safe_area.h * 0.17f);
    layout->face_button_radius = minimum_float(
        right_width * 0.17f, layout->safe_area.h * 0.105f);
    layout->button_b_x = right_start + right_width * 0.34f;
    layout->button_b_y = layout->safe_area.y + layout->safe_area.h * 0.66f;
    layout->button_a_x = right_start + right_width * 0.70f;
    layout->button_a_y = layout->safe_area.y + layout->safe_area.h * 0.53f;

    pill_width = minimum_float(left_width * 0.42f,
                               layout->safe_area.h * 0.16f);
    pill_height = layout->safe_area.h * 0.055f;
    layout->select_button.x = layout->dpad_x - pill_width * 0.5f;
    layout->select_button.y = layout->safe_area.y +
        layout->safe_area.h * 0.86f;
    layout->select_button.w = pill_width;
    layout->select_button.h = pill_height;
    layout->start_button.x = right_start + right_width * 0.5f -
        pill_width * 0.5f;
    layout->start_button.y = layout->select_button.y;
    layout->start_button.w = pill_width;
    layout->start_button.h = pill_height;
    layout->palette_button.w = minimum_float(
        right_width * 0.54f, layout->safe_area.h * 0.22f);
    layout->palette_button.h = layout->safe_area.h * 0.070f;
    layout->palette_button.x = right_start + right_width * 0.5f -
        layout->palette_button.w * 0.5f;
    layout->palette_button.y = layout->safe_area.y +
        layout->safe_area.h * 0.055f;
    layout->valid = true;
}

static uint8_t touch_buttons_at(const AllStarTouchLayout *layout,
                                float x, float y) {
    float hit_radius;
    float dx;
    float dy;
    uint8_t buttons = 0;
    if (!layout || !layout->valid) return 0;
    hit_radius = layout->face_button_radius * 1.35f;
    if (point_in_circle(x, y, layout->button_a_x, layout->button_a_y,
                        hit_radius)) return ALLSTAR_BTN_A;
    if (point_in_circle(x, y, layout->button_b_x, layout->button_b_y,
                        hit_radius)) return ALLSTAR_BTN_B;
    if (point_in_rect(x, y, &layout->start_button))
        return ALLSTAR_BTN_START;
    if (point_in_rect(x, y, &layout->select_button))
        return ALLSTAR_BTN_SELECT;

    hit_radius = layout->dpad_radius * 1.35f;
    dx = (x - layout->dpad_x) / hit_radius;
    dy = (y - layout->dpad_y) / hit_radius;
    if (dx * dx + dy * dy > 1.35f ||
        (fabsf(dx) < 0.20f && fabsf(dy) < 0.20f)) return 0;
    if (dx < -0.24f) buttons |= ALLSTAR_BTN_LEFT;
    if (dx > 0.24f) buttons |= ALLSTAR_BTN_RIGHT;
    if (dy < -0.24f) buttons |= ALLSTAR_BTN_UP;
    if (dy > 0.24f) buttons |= ALLSTAR_BTN_DOWN;
    return buttons;
}

static void refresh_touch_buttons(AllStarSdlApp *app) {
    size_t index;
    uint8_t buttons = 0;
    if (!app) return;
    for (index = 0; index < ALLSTAR_MAX_TOUCHES; index++) {
        if (app->touches[index].active)
            buttons |= app->touches[index].buttons;
    }
    app->touch_buttons = buttons;
}

static AllStarTouchSlot *find_touch_slot(AllStarSdlApp *app,
                                         SDL_FingerID finger_id,
                                         bool create) {
    size_t index;
    AllStarTouchSlot *free_slot = NULL;
    if (!app) return NULL;
    for (index = 0; index < ALLSTAR_MAX_TOUCHES; index++) {
        AllStarTouchSlot *slot = &app->touches[index];
        if (slot->active && slot->finger_id == finger_id) return slot;
        if (!slot->active && !free_slot) free_slot = slot;
    }
    if (!create || !free_slot) return NULL;
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->active = true;
    free_slot->finger_id = finger_id;
    return free_slot;
}

static void update_touch(AllStarSdlApp *app,
                         const SDL_TouchFingerEvent *event,
                         bool finger_down) {
    AllStarTouchSlot *slot;
    int output_width;
    int output_height;
    float x;
    float y;
    if (!app || !event || !app->touch_controls) return;
    update_touch_layout(app);
    if (!SDL_GetRenderOutputSize(
            app->renderer, &output_width, &output_height)) return;
    slot = find_touch_slot(app, event->fingerID, true);
    if (!slot) return;
    x = event->x * output_width;
    y = event->y * output_height;
    slot->palette = point_in_rect(
        x, y, &app->touch_layout.palette_button);
    if (finger_down && slot->palette && app->game.renderer)
        allstar_renderer_cycle_palette(app->game.renderer);
    slot->buttons = slot->palette ? 0 :
        touch_buttons_at(&app->touch_layout, x, y);
    refresh_touch_buttons(app);
}

static bool touch_palette_pressed(const AllStarSdlApp *app) {
    size_t index;
    if (!app) return false;
    for (index = 0; index < ALLSTAR_MAX_TOUCHES; index++) {
        if (app->touches[index].active && app->touches[index].palette)
            return true;
    }
    return false;
}

static void end_touch(AllStarSdlApp *app, SDL_FingerID finger_id) {
    AllStarTouchSlot *slot = find_touch_slot(app, finger_id, false);
    if (!slot) return;
    memset(slot, 0, sizeof(*slot));
    refresh_touch_buttons(app);
}

static void clear_all_buttons(AllStarSdlApp *app) {
    if (!app) return;
    app->keyboard_buttons = 0;
    app->gamepad_buttons = 0;
    app->touch_buttons = 0;
    memset(app->touches, 0, sizeof(app->touches));
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

static bool draw_filled_circle(SDL_Renderer *renderer, float center_x,
                               float center_y, float radius) {
    int y;
    int integer_radius = (int)radius;
    if (!renderer || radius <= 0.0f) return false;
    for (y = -integer_radius; y <= integer_radius; y++) {
        float half_width = sqrtf(
            radius * radius - (float)(y * y));
        if (!SDL_RenderLine(renderer, center_x - half_width,
                            center_y + y, center_x + half_width,
                            center_y + y)) return false;
    }
    return true;
}

static bool draw_circle_outline(SDL_Renderer *renderer, float center_x,
                                float center_y, float radius) {
    SDL_FPoint points[49];
    size_t index;
    if (!renderer || radius <= 0.0f) return false;
    for (index = 0; index < 49; index++) {
        float angle = ((float)index / 48.0f) * 6.28318530718f;
        points[index].x = center_x + cosf(angle) * radius;
        points[index].y = center_y + sinf(angle) * radius;
    }
    return SDL_RenderLines(renderer, points, 49);
}

static bool draw_horizontal_capsule(SDL_Renderer *renderer,
                                    const SDL_FRect *rect) {
    SDL_FRect middle;
    float radius;
    if (!renderer || !rect || rect->w < rect->h || rect->h <= 0.0f)
        return false;
    radius = rect->h * 0.5f;
    middle.x = rect->x + radius;
    middle.y = rect->y;
    middle.w = rect->w - rect->h;
    middle.h = rect->h;
    return SDL_RenderFillRect(renderer, &middle) &&
        draw_filled_circle(renderer, rect->x + radius,
                           rect->y + radius, radius) &&
        draw_filled_circle(renderer, rect->x + rect->w - radius,
                           rect->y + radius, radius);
}

static bool draw_vertical_capsule(SDL_Renderer *renderer,
                                  const SDL_FRect *rect) {
    SDL_FRect middle;
    float radius;
    if (!renderer || !rect || rect->h < rect->w || rect->w <= 0.0f)
        return false;
    radius = rect->w * 0.5f;
    middle.x = rect->x;
    middle.y = rect->y + radius;
    middle.w = rect->w;
    middle.h = rect->h - rect->w;
    return SDL_RenderFillRect(renderer, &middle) &&
        draw_filled_circle(renderer, rect->x + radius,
                           rect->y + radius, radius) &&
        draw_filled_circle(renderer, rect->x + radius,
                           rect->y + rect->h - radius, radius);
}

static const uint8_t *pixel_glyph(char character) {
    static const uint8_t glyph_a[5] = {2, 5, 7, 5, 5};
    static const uint8_t glyph_b[5] = {6, 5, 6, 5, 6};
    static const uint8_t glyph_c[5] = {3, 4, 4, 4, 3};
    static const uint8_t glyph_e[5] = {7, 4, 6, 4, 7};
    static const uint8_t glyph_l[5] = {4, 4, 4, 4, 7};
    static const uint8_t glyph_o[5] = {2, 5, 5, 5, 2};
    static const uint8_t glyph_r[5] = {6, 5, 6, 5, 5};
    static const uint8_t glyph_s[5] = {3, 4, 2, 1, 6};
    static const uint8_t glyph_t[5] = {7, 2, 2, 2, 2};
    switch (character) {
        case 'A': return glyph_a;
        case 'B': return glyph_b;
        case 'C': return glyph_c;
        case 'E': return glyph_e;
        case 'L': return glyph_l;
        case 'O': return glyph_o;
        case 'R': return glyph_r;
        case 'S': return glyph_s;
        case 'T': return glyph_t;
        default: return NULL;
    }
}

static bool draw_pixel_text(SDL_Renderer *renderer, const char *text,
                            float center_x, float center_y, float scale) {
    size_t character_index;
    size_t length;
    float text_width;
    float origin_x;
    if (!renderer || !text || scale <= 0.0f) return false;
    length = strlen(text);
    if (length == 0) return true;
    text_width = ((float)length * 4.0f - 1.0f) * scale;
    origin_x = center_x - text_width * 0.5f;
    for (character_index = 0; character_index < length;
         character_index++) {
        const uint8_t *glyph = pixel_glyph(text[character_index]);
        size_t row;
        if (!glyph) continue;
        for (row = 0; row < 5; row++) {
            size_t column;
            for (column = 0; column < 3; column++) {
                SDL_FRect pixel;
                if ((glyph[row] & (uint8_t)(1u << (2u - column))) == 0)
                    continue;
                pixel.x = origin_x +
                    ((float)character_index * 4.0f + (float)column) * scale;
                pixel.y = center_y + ((float)row - 2.5f) * scale;
                pixel.w = scale;
                pixel.h = scale;
                if (!SDL_RenderFillRect(renderer, &pixel)) return false;
            }
        }
    }
    return true;
}

static bool draw_dpad_arrow(SDL_Renderer *renderer, float center_x,
                            float center_y, float size,
                            uint8_t direction) {
    SDL_FPoint points[3];
    if (!renderer) return false;
    if (direction == ALLSTAR_BTN_UP) {
        points[0] = (SDL_FPoint){center_x - size, center_y + size * 0.6f};
        points[1] = (SDL_FPoint){center_x, center_y - size * 0.4f};
        points[2] = (SDL_FPoint){center_x + size, center_y + size * 0.6f};
    } else if (direction == ALLSTAR_BTN_DOWN) {
        points[0] = (SDL_FPoint){center_x - size, center_y - size * 0.6f};
        points[1] = (SDL_FPoint){center_x, center_y + size * 0.4f};
        points[2] = (SDL_FPoint){center_x + size, center_y - size * 0.6f};
    } else if (direction == ALLSTAR_BTN_LEFT) {
        points[0] = (SDL_FPoint){center_x + size * 0.6f, center_y - size};
        points[1] = (SDL_FPoint){center_x - size * 0.4f, center_y};
        points[2] = (SDL_FPoint){center_x + size * 0.6f, center_y + size};
    } else {
        points[0] = (SDL_FPoint){center_x - size * 0.6f, center_y - size};
        points[1] = (SDL_FPoint){center_x + size * 0.4f, center_y};
        points[2] = (SDL_FPoint){center_x - size * 0.6f, center_y + size};
    }
    return SDL_RenderLines(renderer, points, 3);
}

static bool draw_touch_controls(AllStarSdlApp *app) {
    const AllStarTouchLayout *layout;
    SDL_FRect horizontal;
    SDL_FRect vertical;
    SDL_FRect capsule;
    uint8_t directions;
    float arm_width;
    float arrow_offset;
    float label_scale;
    uint8_t visual_buttons;
    bool palette_pressed;
    if (!app || !app->renderer) return false;
    layout = &app->touch_layout;
    if (!layout->valid) return false;
    if (!SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND))
        return false;

    visual_buttons = app->keyboard_buttons | app->gamepad_buttons |
        app->touch_buttons;
    palette_pressed = touch_palette_pressed(app);

    capsule = layout->palette_button;
    capsule.x += 3.0f;
    capsule.y += 4.0f;
    if (!SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 115) ||
        !draw_horizontal_capsule(app->renderer, &capsule)) return false;
    switch (app->game.renderer->palette_style) {
        case ALLSTAR_PALETTE_DMG_ORIGINAL:
            if (!SDL_SetRenderDrawColor(app->renderer,
                    palette_pressed ? 164 : 119,
                    palette_pressed ? 189 : 151,
                    palette_pressed ? 105 : 78, 245)) return false;
            break;
        case ALLSTAR_PALETTE_POCKET_BW:
            if (!SDL_SetRenderDrawColor(app->renderer,
                    palette_pressed ? 190 : 132,
                    palette_pressed ? 194 : 139,
                    palette_pressed ? 199 : 148, 245)) return false;
            break;
        case ALLSTAR_PALETTE_MODERN_VIBRANT:
        default:
            if (!SDL_SetRenderDrawColor(app->renderer,
                    palette_pressed ? 78 : 42,
                    palette_pressed ? 156 : 108,
                    palette_pressed ? 225 : 173, 245)) return false;
            break;
    }
    if (!draw_horizontal_capsule(
            app->renderer, &layout->palette_button) ||
        !SDL_SetRenderDrawColor(app->renderer, 249, 250, 252, 255) ||
        !draw_pixel_text(app->renderer, "COLOR",
            layout->palette_button.x + layout->palette_button.w * 0.5f,
            layout->palette_button.y + layout->palette_button.h * 0.5f,
            layout->palette_button.h * 0.105f)) return false;

    directions = visual_buttons &
        (ALLSTAR_BTN_UP | ALLSTAR_BTN_DOWN |
         ALLSTAR_BTN_LEFT | ALLSTAR_BTN_RIGHT);
    arm_width = layout->dpad_radius * 0.68f;
    horizontal.x = layout->dpad_x - layout->dpad_radius;
    horizontal.y = layout->dpad_y - arm_width * 0.5f;
    horizontal.w = layout->dpad_radius * 2.0f;
    horizontal.h = arm_width;
    vertical.x = layout->dpad_x - arm_width * 0.5f;
    vertical.y = layout->dpad_y - layout->dpad_radius;
    vertical.w = arm_width;
    vertical.h = layout->dpad_radius * 2.0f;
    capsule = horizontal;
    capsule.x += layout->dpad_radius * 0.05f;
    capsule.y += layout->dpad_radius * 0.07f;
    if (!SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 120) ||
        !draw_horizontal_capsule(app->renderer, &capsule)) return false;
    capsule = vertical;
    capsule.x += layout->dpad_radius * 0.05f;
    capsule.y += layout->dpad_radius * 0.07f;
    if (!draw_vertical_capsule(app->renderer, &capsule)) return false;

    if (!SDL_SetRenderDrawColor(app->renderer, 48, 57, 70, 245) ||
        !draw_horizontal_capsule(app->renderer, &horizontal)) return false;
    if (!draw_vertical_capsule(app->renderer, &vertical)) return false;

    if (!SDL_SetRenderDrawColor(app->renderer, 103, 117, 137, 220))
        return false;
    if ((directions & ALLSTAR_BTN_LEFT) != 0) {
        if (!draw_filled_circle(app->renderer,
                layout->dpad_x - layout->dpad_radius * 0.66f,
                layout->dpad_y, arm_width * 0.48f)) return false;
    }
    if ((directions & ALLSTAR_BTN_RIGHT) != 0) {
        if (!draw_filled_circle(app->renderer,
                layout->dpad_x + layout->dpad_radius * 0.66f,
                layout->dpad_y, arm_width * 0.48f)) return false;
    }
    if ((directions & ALLSTAR_BTN_UP) != 0) {
        if (!draw_filled_circle(app->renderer, layout->dpad_x,
                layout->dpad_y - layout->dpad_radius * 0.66f,
                arm_width * 0.48f)) return false;
    }
    if ((directions & ALLSTAR_BTN_DOWN) != 0) {
        if (!draw_filled_circle(app->renderer, layout->dpad_x,
                layout->dpad_y + layout->dpad_radius * 0.66f,
                arm_width * 0.48f)) return false;
    }
    arrow_offset = layout->dpad_radius * 0.63f;
    if (!SDL_SetRenderDrawColor(app->renderer, 212, 220, 230, 235) ||
        !draw_dpad_arrow(app->renderer, layout->dpad_x,
                         layout->dpad_y - arrow_offset,
                         arm_width * 0.17f, ALLSTAR_BTN_UP) ||
        !draw_dpad_arrow(app->renderer, layout->dpad_x,
                         layout->dpad_y + arrow_offset,
                         arm_width * 0.17f, ALLSTAR_BTN_DOWN) ||
        !draw_dpad_arrow(app->renderer,
                         layout->dpad_x - arrow_offset, layout->dpad_y,
                         arm_width * 0.17f, ALLSTAR_BTN_LEFT) ||
        !draw_dpad_arrow(app->renderer,
                         layout->dpad_x + arrow_offset, layout->dpad_y,
                         arm_width * 0.17f, ALLSTAR_BTN_RIGHT)) return false;
    if (!SDL_SetRenderDrawColor(app->renderer, 24, 29, 38, 235) ||
        !draw_filled_circle(app->renderer, layout->dpad_x,
                            layout->dpad_y, arm_width * 0.31f) ||
        !SDL_SetRenderDrawColor(app->renderer, 96, 109, 128, 220) ||
        !draw_circle_outline(app->renderer, layout->dpad_x,
                             layout->dpad_y, arm_width * 0.31f)) return false;

    if (!SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 125) ||
        !draw_filled_circle(app->renderer,
                            layout->button_b_x + 4.0f,
                            layout->button_b_y + 5.0f,
                            layout->face_button_radius) ||
        !SDL_SetRenderDrawColor(app->renderer,
            (visual_buttons & ALLSTAR_BTN_B) != 0 ? 225 : 155,
            (visual_buttons & ALLSTAR_BTN_B) != 0 ? 65 : 34,
            (visual_buttons & ALLSTAR_BTN_B) != 0 ? 124 : 78, 245) ||
        !draw_filled_circle(app->renderer, layout->button_b_x,
                            layout->button_b_y,
                            layout->face_button_radius) ||
        !SDL_SetRenderDrawColor(app->renderer, 245, 238, 242, 255) ||
        !draw_pixel_text(app->renderer, "B", layout->button_b_x,
                         layout->button_b_y,
                         layout->face_button_radius * 0.22f)) return false;
    if (!SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 125) ||
        !draw_filled_circle(app->renderer,
                            layout->button_a_x + 4.0f,
                            layout->button_a_y + 5.0f,
                            layout->face_button_radius) ||
        !SDL_SetRenderDrawColor(app->renderer,
            (visual_buttons & ALLSTAR_BTN_A) != 0 ? 225 : 155,
            (visual_buttons & ALLSTAR_BTN_A) != 0 ? 65 : 34,
            (visual_buttons & ALLSTAR_BTN_A) != 0 ? 124 : 78, 245) ||
        !draw_filled_circle(app->renderer, layout->button_a_x,
                            layout->button_a_y,
                            layout->face_button_radius) ||
        !SDL_SetRenderDrawColor(app->renderer, 245, 238, 242, 255) ||
        !draw_pixel_text(app->renderer, "A", layout->button_a_x,
                         layout->button_a_y,
                         layout->face_button_radius * 0.22f)) return false;

    if (!SDL_SetRenderDrawColor(app->renderer,
            (visual_buttons & ALLSTAR_BTN_SELECT) != 0 ? 100 : 55,
            (visual_buttons & ALLSTAR_BTN_SELECT) != 0 ? 114 : 64,
            (visual_buttons & ALLSTAR_BTN_SELECT) != 0 ? 134 : 78, 245) ||
        !draw_horizontal_capsule(app->renderer, &layout->select_button))
        return false;
    label_scale = layout->select_button.h * 0.105f;
    if (!SDL_SetRenderDrawColor(app->renderer, 225, 231, 239, 255) ||
        !draw_pixel_text(app->renderer, "SELECT",
            layout->select_button.x + layout->select_button.w * 0.5f,
            layout->select_button.y + layout->select_button.h * 0.5f,
            label_scale) ||
        !SDL_SetRenderDrawColor(app->renderer,
            (visual_buttons & ALLSTAR_BTN_START) != 0 ? 100 : 55,
            (visual_buttons & ALLSTAR_BTN_START) != 0 ? 114 : 64,
            (visual_buttons & ALLSTAR_BTN_START) != 0 ? 134 : 78, 245) ||
        !draw_horizontal_capsule(app->renderer, &layout->start_button) ||
        !SDL_SetRenderDrawColor(app->renderer, 225, 231, 239, 255) ||
        !draw_pixel_text(app->renderer, "START",
            layout->start_button.x + layout->start_button.w * 0.5f,
            layout->start_button.y + layout->start_button.h * 0.5f,
            label_scale))
        return false;
    return true;
}

static bool present_frame(AllStarSdlApp *app) {
    if (!app || !app->game.renderer || !app->game.renderer->pixels) return false;
    if (!SDL_UpdateTexture(app->texture, NULL, app->game.renderer->pixels,
                           ALLSTAR_GB_WIDTH * (int)sizeof(AllStarColor))) {
        SDL_Log("SDL_UpdateTexture failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderDrawColor(app->renderer, 8, 12, 20, 255) ||
        !SDL_RenderClear(app->renderer)) {
        SDL_Log("SDL frame presentation failed: %s", SDL_GetError());
        return false;
    }
    if (app->touch_controls) {
        update_touch_layout(app);
        if (!SDL_RenderTexture(app->renderer, app->texture, NULL,
                               &app->touch_layout.game) ||
            !SDL_SetRenderDrawColor(app->renderer, 85, 96, 114, 210) ||
            !SDL_RenderRect(app->renderer, &app->touch_layout.game) ||
            !draw_touch_controls(app)) {
            SDL_Log("SDL touch presentation failed: %s", SDL_GetError());
            return false;
        }
    } else if (!SDL_RenderTexture(
                   app->renderer, app->texture, NULL, NULL)) {
        SDL_Log("SDL game presentation failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_RenderPresent(app->renderer)) {
        SDL_Log("SDL present failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    AllStarSdlApp *app;
    char asset_path[1024];
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE;

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
#if defined(SDL_PLATFORM_IOS)
    app->touch_controls = true;
    window_flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
    app->touch_controls = SDL_getenv("ALLSTAR_TOUCH_CONTROLS") != NULL;
#endif

    if (!SDL_CreateWindowAndRenderer(
            "NBA All-Star Challenge (Native C Port)",
            ALLSTAR_GB_WIDTH * ALLSTAR_WINDOW_SCALE,
            ALLSTAR_GB_HEIGHT * ALLSTAR_WINDOW_SCALE,
            window_flags, &app->window, &app->renderer)) {
        SDL_Log("Could not create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!app->touch_controls && !SDL_SetRenderLogicalPresentation(
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
            "A version-19 allstar.assetpack was not found. Build it from "
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
            update_button(&app->keyboard_buttons, mask, true);
            if (!event->key.repeat) select_palette(app, event->key.scancode);
            break;
        case SDL_EVENT_KEY_UP:
            update_button(&app->keyboard_buttons,
                          keyboard_button(event->key.scancode), false);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!app->gamepad) app->gamepad = SDL_OpenGamepad(event->gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (app->gamepad &&
                SDL_GetGamepadID(app->gamepad) == event->gdevice.which) {
                SDL_CloseGamepad(app->gamepad);
                app->gamepad = NULL;
                app->gamepad_buttons = 0;
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            update_button(&app->gamepad_buttons,
                          gamepad_button(event->gbutton.button), true);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            update_button(&app->gamepad_buttons,
                          gamepad_button(event->gbutton.button), false);
            break;
        case SDL_EVENT_FINGER_DOWN:
            update_touch(app, &event->tfinger, true);
            break;
        case SDL_EVENT_FINGER_MOTION:
            update_touch(app, &event->tfinger, false);
            break;
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            end_touch(app, event->tfinger.fingerID);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            clear_all_buttons(app);
            break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            app->suspended = true;
            clear_all_buttons(app);
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
        uint8_t buttons = app->keyboard_buttons |
            app->gamepad_buttons | app->touch_buttons;
        allstar_input_update(&app->game.input, buttons);
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
