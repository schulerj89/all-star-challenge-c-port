#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
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

    bool game_initialized = false;
    FILE *f_check = NULL;
    fopen_s(&f_check, asset_path, "rb");
    if (f_check) {
        fclose(f_check);
        game_initialized = allstar_game_init(&g_game, asset_path);
    }
    if (!game_initialized) {
        snprintf(asset_path, sizeof(asset_path), "build\\allstar.assetpack");
        fopen_s(&f_check, asset_path, "rb");
        if (f_check) {
            fclose(f_check);
            game_initialized = allstar_game_init(&g_game, asset_path);
        }
    }
    if (!game_initialized) {
        MessageBoxA(
            hwnd,
            "Gameplay asset pack v15 is missing or stale.\n\n"
            "Run build.ps1 -RomPath \"path\\to\\game.gb\" to rebuild it.",
            "NBA All-Star Challenge - Asset Pack Required",
            MB_OK | MB_ICONERROR);
        win32_free_framebuffer(&g_framebuffer);
        DestroyWindow(hwnd);
        return 1;
    }

    /* Pacing timer setup (59.7275 Hz Game Boy target) */
    LARGE_INTEGER perf_freq, current_time;
    LONGLONG frame_ticks;
    LONGLONG next_frame_tick;
    QueryPerformanceFrequency(&perf_freq);
    const double target_frame_time = 1.0 / 59.7275;
    frame_ticks = (LONGLONG)(target_frame_time * (double)perf_freq.QuadPart);
    QueryPerformanceCounter(&current_time);
    next_frame_tick = current_time.QuadPart + frame_ticks;
    /* Sleep(1) can otherwise use a coarse Windows scheduler quantum, and
       resetting the deadline to the overshot time accumulates that error.
       A 1 ms timer period plus an absolute deadline keeps the host at the
       DMG's 59.7275 Hz while the gameplay logic remains one ROM step/frame. */
    timeBeginPeriod(1);

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

        /* Absolute-deadline frame pacing: sleep for the coarse part and
           yield through the final sub-millisecond interval. */
        for (;;) {
            QueryPerformanceCounter(&current_time);
            if (current_time.QuadPart >= next_frame_tick) break;
            if (next_frame_tick - current_time.QuadPart >
                    perf_freq.QuadPart / 500) {
                Sleep(1);
            } else {
                SwitchToThread();
            }
        }
        next_frame_tick += frame_ticks;
        /* A breakpoint/window drag must not trigger an unbounded catch-up. */
        if (current_time.QuadPart > next_frame_tick + frame_ticks * 4) {
            next_frame_tick = current_time.QuadPart + frame_ticks;
        }
    }

    timeEndPeriod(1);
    allstar_game_shutdown(&g_game);
    win32_free_framebuffer(&g_framebuffer);

    return 0;
}
