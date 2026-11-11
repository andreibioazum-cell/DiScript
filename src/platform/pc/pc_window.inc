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

/* Autorepeat for the keys that benefit from holding them down. */
static void pc_apply_held_input(void) {
    static const struct {
        int vk;
        const char *name;
    } repeatable[] = {
        {VK_LEFT, "left"},   {VK_RIGHT, "right"}, {VK_UP, "up"},     {VK_DOWN, "down"},
        {VK_BACK, "backspace"}, {VK_DELETE, "delete"},
    };
    static uint64_t next_repeat[6];
    IDEApp *app = pc_ide_app();
    uint64_t now = monotonic_ns();
    if (!app)
        return;
    for (size_t i = 0; i < sizeof(repeatable) / sizeof(repeatable[0]); i++) {
        if (!pc_key_down(repeatable[i].vk)) {
            next_repeat[i] = 0;
            continue;
        }
        if (now < next_repeat[i])
            continue;
        /* ~30 keys per second once the initial delay has passed. */
        next_repeat[i] = now + 33000000ull;
        if (strcmp(repeatable[i].name, "backspace") == 0)
            ide_app_backspace(app, 1);
        else
            ide_app_special_key(app, repeatable[i].name);
    }
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
        if (pc_mouse_down) {
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
        pc_touch(mx, my, 0, PC_TOUCH_MOUSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        float mx, my;
        pc_mouse_xy(lp, &mx, &my);
        pc_mouse_down = 0;
        ReleaseCapture();
        pc_touch(mx, my, 1, PC_TOUCH_MOUSE);
        return 0;
    }
    case 0x020A: /* WM_MOUSEWHEEL */ {
        short z = (short)HIWORD(wp);
        IDEApp *app = pc_ide_app();
        if (z != 0 && app)
            ide_app_scroll(app, -(float)z / 120.0f, g_frame.width, g_frame.height);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        IDEApp *app = pc_ide_app();
        int ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        int shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        pc_keys[wp & 0xff] = 1;
        if (msg == WM_SYSKEYDOWN)
            return 0; /* Alt+Tab etc. stay with the system */
        if (!app)
            return 0;
        /* Shortcuts first: they are the only place Ctrl changes meaning. */
        if (ctrl) {
            switch (wp) {
            case 'S':
                if (shift)
                    ide_app_save_as(app);
                else
                    ide_app_save(app);
                return 0;
            case 'O':
                ide_app_open_dialog(app);
                return 0;
            case 'N':
                ide_app_new(app);
                return 0;
            case 'R':
                ide_app_run(app);
                return 0;
            case 'Z':
                ide_app_special_key(app, "undo");
                return 0;
            case 'Y':
                ide_app_special_key(app, "redo");
                return 0;
            case 'A':
                ide_app_special_key(app, "select_all");
                return 0;
            case VK_LEFT:
                ide_app_special_key(app, "word_left");
                return 0;
            case VK_RIGHT:
                ide_app_special_key(app, "word_right");
                return 0;
            case VK_HOME:
                ide_app_special_key(app, "doc_start");
                return 0;
            case VK_END:
                ide_app_special_key(app, "doc_end");
                return 0;
            default:
                return 0;
            }
        }
        switch (wp) {
        case VK_F12:
            (void)pc_write_bmp("discript_shot.bmp", g_frame.pixels, g_frame.width, g_frame.height, g_frame.stride);
            ds_log("screenshot saved to discript_shot.bmp");
            return 0;
        case VK_F5:
            ide_app_run(app);
            return 0;
        case VK_ESCAPE:
            keyboard_hide();
            return 0;
        case VK_BACK:
            ide_app_backspace(app, 1);
            return 0;
        case VK_RETURN:
            ide_app_enter(app);
            return 0;
        case VK_TAB:
            ide_app_tab(app);
            return 0;
        case VK_LEFT:
            ide_app_special_key(app, shift ? "left" : "left");
            return 0;
        case VK_RIGHT:
            ide_app_special_key(app, "right");
            return 0;
        case VK_UP:
            ide_app_special_key(app, "up");
            return 0;
        case VK_DOWN:
            ide_app_special_key(app, "down");
            return 0;
        case VK_HOME:
            ide_app_special_key(app, "home");
            return 0;
        case VK_END:
            ide_app_special_key(app, "end");
            return 0;
        case VK_DELETE:
            ide_app_special_key(app, "delete");
            return 0;
        default:
            return 0;
        }
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
        pc_keys[wp & 0xff] = 0;
        if (msg == WM_SYSKEYUP)
            return 0;
        return 0;
    case WM_CHAR: {
        IDEApp *app = pc_ide_app();
        char utf8[8];
        pc_utf8_from_wchar((WCHAR)wp, utf8, sizeof(utf8));
        /* Enter, Tab and Backspace arrive here too; they were handled above. */
        if (app && utf8[0] && !(wp == '\r' || wp == '\n' || wp == '\b' || wp == '\t'))
            ide_app_text(app, utf8);
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (pc_mouse_down) {
            pc_mouse_down = 0;
            pc_touch(0.0f, 0.0f, 3, PC_TOUCH_MOUSE);
        }
        return 0;
    case WM_KILLFOCUS:
        memset(pc_keys, 0, sizeof(pc_keys));
        keyboard_cancel_repeat();
        if (pc_mouse_down)
            pc_touch(0.0f, 0.0f, 3, PC_TOUCH_MOUSE);
        pc_mouse_down = 0;
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
    pc_hwnd = CreateWindowExW(0, PC_WIN_CLASS, L"DiScript", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, inst, NULL);
    if (!pc_hwnd)
        return 2;
    ShowWindow(pc_hwnd, SW_SHOW);
    UpdateWindow(pc_hwnd);
    pc_boot(args);
    if (!init_done) {
        MessageBoxA(pc_hwnd, ds_graphics_failure(), "DiScript: renderer", MB_ICONERROR | MB_OK);
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
