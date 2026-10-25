/* Win32 GDI window, mouse pointer/combat input, and keyboard event loop. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#define PC_WIN_CLASS L"CubicBattle4PC"
#define PC_TOUCH_MOUSE 500
#define PC_MAX_RENDER_DIM 1280

static HWND pc_hwnd = NULL;
static int pc_win_w = 540, pc_win_h = 960;
static int pc_mouse_down = 0;  /* Left mouse button held. */
static int pc_mouse_punch = 0; /* Current press routed to combat punch. */
static unsigned char pc_keys[256];
static uint32_t *pc_conv = NULL;
static int pc_conv_w = 0, pc_conv_h = 0;

/* Cap software framebuffer dimensions while preserving window aspect ratio. */
static void pc_update_window_size(int w, int h) {
    int rw, rh;
    if (w < 1 || h < 1)
        return;
    pc_win_w = w;
    pc_win_h = h;
    rw = w;
    rh = h;
    if (rw > PC_MAX_RENDER_DIM || rh > PC_MAX_RENDER_DIM) {
        if (rw >= rh) {
            rh = (int)((long long)rh * PC_MAX_RENDER_DIM / rw);
            rw = PC_MAX_RENDER_DIM;
        } else {
            rw = (int)((long long)rw * PC_MAX_RENDER_DIM / rh);
            rh = PC_MAX_RENDER_DIM;
        }
        if (rw < 1)
            rw = 1;
        if (rh < 1)
            rh = 1;
    }
    phys_w = rw;
    phys_h = rh;
    screen_w = rw;
    screen_h = rh;
}

/* Map client-area mouse coordinates into internal render-space coordinates. */
static void pc_mouse_xy(LPARAM lp, float *ox, float *oy) {
    float x = (float)GET_X_LPARAM(lp);
    float y = (float)GET_Y_LPARAM(lp);
    if (pc_win_w > 0 && screen_w > 0)
        x = x * (float)screen_w / (float)pc_win_w;
    if (pc_win_h > 0 && screen_h > 0)
        y = y * (float)screen_h / (float)pc_win_h;
    *ox = x;
    *oy = y;
}

static int pc_key_down(int vk) { return pc_keys[vk & 0xff] != 0; }

/* Return 1 when active battle maps left mouse clicks to punch attacks. */
static int pc_mouse_combat(void) {
    return game_pc_in_battle() && game_pc_finished() == 0.0 && !keyboard_visible();
}

static void pc_apply_held_input(void) {
    float dx = 0, dy = 0;
    if (keyboard_visible()) {
        pc_ctl_release_all();
        return;
    }
    if (pc_key_down('A') || pc_key_down(VK_LEFT))
        dx -= 1.0f;
    if (pc_key_down('D') || pc_key_down(VK_RIGHT))
        dx += 1.0f;
    if (pc_key_down('W') || pc_key_down(VK_UP))
        dy -= 1.0f;
    if (pc_key_down('S') || pc_key_down(VK_DOWN))
        dy += 1.0f;
    pc_ctl_stick(dx, dy);
    pc_ctl_attack(pc_mouse_punch && pc_mouse_combat());
    pc_ctl_block(pc_key_down('E'));
    pc_ctl_super(pc_key_down('Q'));
}

static void pc_blit(void) {
    HDC dc;
    RECT rc;
    int cw, ch;
    int x, y;
    if (!pc_hwnd || !g_frame.pixels)
        return;
    GetClientRect(pc_hwnd, &rc);
    cw = rc.right - rc.left;
    ch = rc.bottom - rc.top;
    if (cw < 1 || ch < 1)
        return;
    if (pc_conv_w != g_frame.width || pc_conv_h != g_frame.height) {
        free(pc_conv);
        pc_conv = (uint32_t *)malloc((size_t)g_frame.width * (size_t)g_frame.height * sizeof(uint32_t));
        pc_conv_w = g_frame.width;
        pc_conv_h = g_frame.height;
        if (!pc_conv)
            return;
    }
    for (y = 0; y < g_frame.height; y++) {
        const uint32_t *src = g_frame.pixels + (size_t)y * (size_t)g_frame.stride;
        uint32_t *dst = pc_conv + (size_t)y * (size_t)pc_conv_w;
        for (x = 0; x < g_frame.width; x++) {
            uint32_t c = src[x]; /* r | g<<8 | b<<16 | a<<24 */
            dst[x] = 0xff000000u | ((c & 0xffu) << 16) | (c & 0xff00u) | ((c >> 16) & 0xffu);
        }
    }
    dc = GetDC(pc_hwnd);
    {
        BITMAPINFO bi;
        memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = pc_conv_w;
        bi.bmiHeader.biHeight = -pc_conv_h; /* top-down */
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        /* Nearest-neighbor stretch blit to preserve sharp framebuffer pixels. */
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, 0, 0, cw, ch, 0, 0, pc_conv_w, pc_conv_h, pc_conv, &bi, DIB_RGB_COLORS, SRCCOPY);
    }
    ReleaseDC(pc_hwnd, dc);
}

static void pc_utf8_from_wchar(WCHAR wc, char *out, size_t cap) {
    wchar_t wbuf[2];
    int n;
    if (cap < 8)
        return;
    /* Ignore UTF-16 surrogates; input requires only BMP characters. */
    if (wc >= 0xD800 && wc <= 0xDFFF) {
        out[0] = '\0';
        return;
    }
    wbuf[0] = (wchar_t)wc;
    wbuf[1] = 0;
    n = WideCharToMultiByte(CP_UTF8, 0, wbuf, 1, out, (int)cap - 1, NULL, NULL);
    out[n > 0 ? n : 0] = '\0';
}

static LRESULT CALLBACK pc_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
        pc_quit = 1;
        return 0;
    case WM_DESTROY:
        pc_quit = 1;
        return 0;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED)
            pc_update_window_size((int)LOWORD(lp), (int)HIWORD(lp));
        return 0;
    case WM_MOUSEMOVE:
        if (pc_mouse_down && !pc_mouse_punch) {
            float mx, my;
            pc_mouse_xy(lp, &mx, &my);
            pc_touch(mx, my, 2, PC_TOUCH_MOUSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        float mx, my;
        pc_mouse_xy(lp, &mx, &my);
        SetCapture(hwnd);
        pc_mouse_down = 1;
        /* Route clicks outside battle chat controls to punch attacks during combat. */
        pc_mouse_punch = pc_mouse_combat() && game_pc_battle_mouse_ui(mx, my) == 0;
        if (!pc_mouse_punch)
            pc_touch(mx, my, 0, PC_TOUCH_MOUSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        float mx, my;
        pc_mouse_xy(lp, &mx, &my);
        pc_mouse_down = 0;
        ReleaseCapture();
        if (!pc_mouse_punch)
            pc_touch(mx, my, 1, PC_TOUCH_MOUSE);
        pc_mouse_punch = 0;
        return 0;
    }
    case 0x0215: /* WM_CAPTURECHANGED */
    case 0x0008: /* WM_KILLFOCUS */
        if (pc_mouse_down) {
            pc_mouse_down = 0;
            if (!pc_mouse_punch)
                pc_touch(0.0f, 0.0f, 3, PC_TOUCH_MOUSE);
            pc_mouse_punch = 0;
        }
        if (msg == 0x0008)
            memset(pc_keys, 0, sizeof(pc_keys));
        return 0;
    case 0x020A: /* WM_MOUSEWHEEL */ {
        short z = (short)HIWORD(wp);
        if (z != 0)
            game_pc_wheel((float)z / 120.0f);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        pc_keys[wp & 0xff] = 1;
        if (msg == WM_SYSKEYDOWN)
            return 0; /* Alt+Tab etc. stay with the system */
        if (wp == VK_F12) {
            (void)pc_write_bmp("cb4_shot.bmp", g_frame.pixels, g_frame.width, g_frame.height, g_frame.stride);
            ds_log("screenshot saved to cb4_shot.bmp");
        } else if (wp == VK_ESCAPE) {
            if (keyboard_visible())
                keyboard_hide();
            else
                (void)pc_back();
        } else if (wp == VK_BACK) {
            if (keyboard_visible())
                keyboard_backspace();
        } else if (wp == VK_RETURN) {
            if (keyboard_visible())
                pc_keyboard_enter();
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        pc_keys[wp & 0xff] = 0;
        if (msg == WM_SYSKEYUP)
            return 0;
        return 0;
    case WM_CHAR: {
        char utf8[8];
        pc_utf8_from_wchar((WCHAR)wp, utf8, sizeof(utf8));
        if (utf8[0] && !(wp == '\r' || wp == '\n' || wp == '\b'))
            keyboard_type(utf8);
        return 0;
    }
    case WM_KILLFOCUS:
        memset(pc_keys, 0, sizeof(pc_keys));
        pc_mouse_down = 0; /* the punch is the mouse now: it must not stick */
        pc_mouse_punch = 0;
        pc_ctl_release_all();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

static int pc_run_window(PcArgs *args) {
    WNDCLASSEXW wc;
    HINSTANCE inst = GetModuleHandleW(NULL);
    RECT rc;
    MSG msg;
    int running = 1;
    SetProcessDPIAware();
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = pc_wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = PC_WIN_CLASS;
    if (!RegisterClassExW(&wc))
        return 2;
    rc.left = 0;
    rc.top = 0;
    rc.right = args->width;
    rc.bottom = args->height;
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    pc_hwnd = CreateWindowExW(0, PC_WIN_CLASS, L"Cubic Battle 4", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, inst, NULL);
    if (!pc_hwnd)
        return 2;
    ShowWindow(pc_hwnd, SW_SHOW);
    UpdateWindow(pc_hwnd);
    pc_boot(args);
    if (!init_done) {
        MessageBoxA(pc_hwnd, ds_graphics_failure(), "Cubic Battle 4: renderer", MB_ICONERROR | MB_OK);
        return 2;
    }
    if (GetClientRect(pc_hwnd, &rc))
        pc_update_window_size(rc.right - rc.left, rc.bottom - rc.top);
    timeBeginPeriod(1);
    while (running && !pc_quit) {
        uint64_t t0 = monotonic_ns();
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running || pc_quit)
            break;
        pc_apply_held_input();
        (void)pc_step();
        pc_blit();
        {
            uint64_t elapsed_ns = monotonic_ns() - t0;
            if (elapsed_ns < 15000000ull) {
                DWORD rem_ms = (DWORD)((16000000ull - elapsed_ns) / 1000000ull);
                if (rem_ms > 0)
                    Sleep(rem_ms);
            }
        }
    }
    timeEndPeriod(1);
    pc_shutdown();
    if (pc_conv) {
        free(pc_conv);
        pc_conv = NULL;
    }
    return 0;
}
