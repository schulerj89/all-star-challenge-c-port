#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "allstar_game.h"
#include <stdio.h>
#include <stdbool.h>

#define WINDOW_SCALE 3
#define CLIENT_WIDTH  (ALLSTAR_GB_WIDTH * WINDOW_SCALE)
#define CLIENT_HEIGHT (ALLSTAR_GB_HEIGHT * WINDOW_SCALE)

typedef struct {
    BITMAPINFO info;
    uint32_t *pixels;
    int width;
    int height;
} Win32Framebuffer;

static Win32Framebuffer g_framebuffer;
static bool g_is_running = true;
static uint8_t g_raw_input_buttons = 0;
static AllStarGame g_game;

static void win32_init_framebuffer(Win32Framebuffer *fb, int width, int height) {
    fb->width = width;
    fb->height = height;
    fb->info.bmiHeader.biSize = sizeof(fb->info.bmiHeader);
    fb->info.bmiHeader.biWidth = width;
    fb->info.bmiHeader.biHeight = -height; /* Top-down DIB */
    fb->info.bmiHeader.biPlanes = 1;
    fb->info.bmiHeader.biBitCount = 32;
    fb->info.bmiHeader.biCompression = BI_RGB;

    fb->pixels = (uint32_t*)VirtualAlloc(0, (SIZE_T)width * (SIZE_T)height * sizeof(uint32_t),
                                         MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void win32_free_framebuffer(Win32Framebuffer *fb) {
    if (fb->pixels) {
        VirtualFree(fb->pixels, 0, MEM_RELEASE);
        fb->pixels = NULL;
    }
}

static void win32_handle_key(WPARAM key, bool is_down) {
    uint8_t mask = 0;
    switch (key) {
        case VK_RIGHT: mask = ALLSTAR_BTN_RIGHT; break;
        case VK_LEFT:  mask = ALLSTAR_BTN_LEFT; break;
        case VK_UP:    mask = ALLSTAR_BTN_UP; break;
        case VK_DOWN:  mask = ALLSTAR_BTN_DOWN; break;
        case 'Z':
        case 'J':      mask = ALLSTAR_BTN_A; break;
        case 'X':
        case 'K':      mask = ALLSTAR_BTN_B; break;
        case VK_RETURN: mask = ALLSTAR_BTN_START; break;
        case VK_SPACE:
        case VK_SHIFT:  mask = ALLSTAR_BTN_SELECT; break;
        case '1':
            if (is_down && g_game.renderer) allstar_renderer_set_palette_style(g_game.renderer, ALLSTAR_PALETTE_DMG_ORIGINAL);
            break;
        case '2':
            if (is_down && g_game.renderer) allstar_renderer_set_palette_style(g_game.renderer, ALLSTAR_PALETTE_POCKET_BW);
            break;
        case '3':
            if (is_down && g_game.renderer) allstar_renderer_set_palette_style(g_game.renderer, ALLSTAR_PALETTE_MODERN_VIBRANT);
            break;
        case 'P':
            if (is_down && g_game.renderer) allstar_renderer_cycle_palette(g_game.renderer);
            break;
        default: break;
    }

    if (mask) {
        if (is_down) g_raw_input_buttons |= mask;
        else g_raw_input_buttons &= ~mask;
    }
}

static LRESULT CALLBACK win32_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            g_is_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1; /* Prevent window background flicker */
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            win32_handle_key(wparam, true);
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            win32_handle_key(wparam, false);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            if (g_framebuffer.pixels) {
                StretchDIBits(hdc,
                              0, 0, rect.right - rect.left, rect.bottom - rect.top,
                              0, 0, g_framebuffer.width, g_framebuffer.height,
                              g_framebuffer.pixels,
                              &g_framebuffer.info,
                              DIB_RGB_COLORS,
                              SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = win32_wnd_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "AllStarGameBoyPortWindowClass";
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageA(NULL, "assets\\nba_allstar_challenge.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    RECT wr = {0, 0, CLIENT_WIDTH, CLIENT_HEIGHT};
    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_VISIBLE;
    AdjustWindowRect(&wr, style, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "NBA All-Star Challenge (Game Boy Native C Port)",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    win32_init_framebuffer(&g_framebuffer, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    char exe_dir[MAX_PATH];
    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    char *last_slash = strrchr(exe_dir, '\\');
    if (last_slash) *last_slash = '\0';

    char asset_path[MAX_PATH];
    snprintf(asset_path, sizeof(asset_path), "%s\\allstar.assetpack", exe_dir);

    FILE *f_check = fopen(asset_path, "rb");
    if (f_check) {
        fclose(f_check);
        allstar_game_init(&g_game, asset_path);
    } else {
        snprintf(asset_path, sizeof(asset_path), "build\\allstar.assetpack");
        f_check = fopen(asset_path, "rb");
        if (f_check) {
            fclose(f_check);
            allstar_game_init(&g_game, asset_path);
        } else {
            allstar_game_init(&g_game, NULL);
        }
    }

    /* Pacing timer setup (59.7275 Hz Game Boy target) */
    LARGE_INTEGER perf_freq, last_time, current_time;
    QueryPerformanceFrequency(&perf_freq);
    QueryPerformanceCounter(&last_time);

    const double target_frame_time = 1.0 / 59.7275;

    while (g_is_running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_is_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (!g_is_running) break;

        /* Update game input and step */
        allstar_input_update(&g_game.input, g_raw_input_buttons);
        allstar_game_tick(&g_game, (float)target_frame_time);

        /* Copy renderer pixels to backbuffer */
        if (g_game.renderer && g_game.renderer->pixels && g_framebuffer.pixels) {
            memcpy(g_framebuffer.pixels, g_game.renderer->pixels, ALLSTAR_GB_WIDTH * ALLSTAR_GB_HEIGHT * sizeof(uint32_t));
        }

        /* Present to window using DC */
        HDC hdc = GetDC(hwnd);
        RECT rect;
        GetClientRect(hwnd, &rect);
        StretchDIBits(hdc,
                      0, 0, rect.right - rect.left, rect.bottom - rect.top,
                      0, 0, g_framebuffer.width, g_framebuffer.height,
                      g_framebuffer.pixels,
                      &g_framebuffer.info,
                      DIB_RGB_COLORS,
                      SRCCOPY);
        ReleaseDC(hwnd, hdc);

        /* Frame pacing */
        do {
            QueryPerformanceCounter(&current_time);
            double elapsed = (double)(current_time.QuadPart - last_time.QuadPart) / (double)perf_freq.QuadPart;
            if (elapsed >= target_frame_time) {
                last_time = current_time;
                break;
            }
            Sleep(1);
        } while (1);
    }

    allstar_game_shutdown(&g_game);
    win32_free_framebuffer(&g_framebuffer);

    return 0;
}
