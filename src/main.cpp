#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d9.h>
#include <wincodec.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <avrt.h>
#include <string>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <atomic>
#include <intrin.h>

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx9.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "avrt.lib")

#define IDR_SOUND 2
#define IDR_LOGO  3

#ifndef USE_UNDOCUMENTED_BLUR
#define USE_UNDOCUMENTED_BLUR 1
#endif

#ifndef ADAPTIVE_THEME_FROM_SCREEN
#define ADAPTIVE_THEME_FROM_SCREEN 1
#endif

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// default config
enum class SpamMode : int { Hold = 0, Toggle = 1 };

static ImFont* g_fontRegular = nullptr;
static ImFont* g_fontBold    = nullptr;

static const int kMaxScansPerSec = 4000;

struct Settings
{
    WORD     sendKeys[2]   = { 'E', 'F' };
    WORD     triggerKey    = VK_XBUTTON1;
    WORD     overlayKey    = VK_INSERT;
    double   delayMs       = 4.0;
    SpamMode mode          = SpamMode::Toggle;
    bool     enabled       = true;
    int      used          = 0;

    // trigger
    bool     tbEnabled       = false;
    bool     tbToggle        = false;
    WORD     tbTriggerKey    = VK_LCONTROL;
    int      tbR             = 255;
    int      tbG             = 0;
    int      tbB             = 0;
    int      tbTolerance     = 120;
    double   tbClickDelayMs  = 80.0;
    int      tbScansPerSec   = 120;
};
static Settings g_cfg;

static std::wstring GetSettingsPath()
{
    wchar_t buf[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (len == 0) return L"ParadiseMacroConfig.txt";
    std::wstring path = buf;
    path += L"\\ParadiseMacroConfig.txt";
    return path;
}

static void SaveSettings()
{
    FILE* f = nullptr;
    _wfopen_s(&f, GetSettingsPath().c_str(), L"w");
    if (!f) return;
    fprintf(f, "key1=%u\n",    (unsigned)g_cfg.sendKeys[0]);
    fprintf(f, "key2=%u\n",    (unsigned)g_cfg.sendKeys[1]);
    fprintf(f, "trigger=%u\n", (unsigned)g_cfg.triggerKey);
    fprintf(f, "overlay=%u\n", (unsigned)g_cfg.overlayKey);
    fprintf(f, "delay=%.3f\n", g_cfg.delayMs);
    fprintf(f, "mode=%d\n",    (int)g_cfg.mode);
    fprintf(f, "enabled=%d\n", g_cfg.enabled ? 1 : 0);
    fprintf(f, "used=%d\n",    g_cfg.used);
    fprintf(f, "tb_enabled=%d\n",  g_cfg.tbEnabled ? 1 : 0);
    fprintf(f, "tb_toggle=%d\n",   g_cfg.tbToggle ? 1 : 0);
    fprintf(f, "tb_trigger=%u\n",  (unsigned)g_cfg.tbTriggerKey);
    fprintf(f, "tb_r=%d\n",        g_cfg.tbR);
    fprintf(f, "tb_g=%d\n",        g_cfg.tbG);
    fprintf(f, "tb_b=%d\n",        g_cfg.tbB);
    fprintf(f, "tb_tol=%d\n",      g_cfg.tbTolerance);
    fprintf(f, "tb_delay=%.3f\n",  g_cfg.tbClickDelayMs);
    fprintf(f, "tb_sps=%d\n",      g_cfg.tbScansPerSec);
    fclose(f);
}

static void LoadSettings()
{
    FILE* f = nullptr;
    _wfopen_s(&f, GetSettingsPath().c_str(), L"r");
    if (!f) { SaveSettings(); return; }
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        unsigned v; double d; int i;
        if (sscanf_s(line, "key1=%u",    &v) == 1) g_cfg.sendKeys[0] = (WORD)v;
        if (sscanf_s(line, "key2=%u",    &v) == 1) g_cfg.sendKeys[1] = (WORD)v;
        if (sscanf_s(line, "trigger=%u", &v) == 1) g_cfg.triggerKey  = (WORD)v;
        if (sscanf_s(line, "overlay=%u", &v) == 1) g_cfg.overlayKey  = (WORD)v;
        if (sscanf_s(line, "delay=%lf",  &d) == 1) g_cfg.delayMs     = d;
        if (sscanf_s(line, "mode=%d",    &i) == 1) g_cfg.mode        = (SpamMode)i;
        if (sscanf_s(line, "enabled=%d", &i) == 1) g_cfg.enabled     = (i != 0);
        if (sscanf_s(line, "used=%d",    &i) == 1) g_cfg.used        = i;
        if (sscanf_s(line, "tb_enabled=%d", &i) == 1) g_cfg.tbEnabled      = (i != 0);
        if (sscanf_s(line, "tb_toggle=%d",  &i) == 1) g_cfg.tbToggle       = (i != 0);
        if (sscanf_s(line, "tb_trigger=%u", &v) == 1) g_cfg.tbTriggerKey   = (WORD)v;
        if (sscanf_s(line, "tb_r=%d",       &i) == 1) g_cfg.tbR            = i;
        if (sscanf_s(line, "tb_g=%d",       &i) == 1) g_cfg.tbG            = i;
        if (sscanf_s(line, "tb_b=%d",       &i) == 1) g_cfg.tbB            = i;
        if (sscanf_s(line, "tb_tol=%d",     &i) == 1) g_cfg.tbTolerance    = i;
        if (sscanf_s(line, "tb_delay=%lf",  &d) == 1) g_cfg.tbClickDelayMs = d;
        if (sscanf_s(line, "tb_sps=%d",     &i) == 1) g_cfg.tbScansPerSec  = i;
    }
    fclose(f);
}

// key naming
static std::wstring VkToString(WORD vk)
{
    if (vk == VK_LBUTTON)  return L"Mouse Left";
    if (vk == VK_RBUTTON)  return L"Mouse Right";
    if (vk == VK_MBUTTON)  return L"Mouse Middle";
    if (vk == VK_XBUTTON1) return L"Mouse 4 (Back)";
    if (vk == VK_XBUTTON2) return L"Mouse 5 (Fwd)";

    wchar_t buf[64] = {};
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG lParam = (scanCode << 16);
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN ||
        vk == VK_HOME || vk == VK_END   || vk == VK_PRIOR || vk == VK_NEXT ||
        vk == VK_INSERT || vk == VK_DELETE || vk == VK_NUMLOCK ||
        vk == VK_RCONTROL || vk == VK_RMENU)
        lParam |= (1 << 24);
    if (GetKeyNameTextW(lParam, buf, 64) > 0) return buf;
    if (vk >= 0x20 && vk <= 0x7E) { buf[0] = (wchar_t)vk; buf[1] = 0; return buf; }
    swprintf_s(buf, L"0x%02X", vk);
    return buf;
}

enum class CaptureTarget { None, Key1, Key2, Trigger, Overlay, TBTrigger };
static std::atomic<CaptureTarget> g_capturing{ CaptureTarget::None };
static std::atomic<bool>          g_captureGot{ false };
static std::atomic<WORD>          g_captureVk{ 0 };

static void BeginCapture(CaptureTarget t)
{
    g_captureGot = false;
    g_capturing  = t;
}

static bool CaptureFromMessage(UINT msg, WPARAM wParam)
{
    if (g_capturing.load() == CaptureTarget::None) return false;

    WORD vk = 0;
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:   vk = (WORD)wParam; break;
    case WM_RBUTTONDOWN:  vk = VK_RBUTTON;   break;
    case WM_MBUTTONDOWN:  vk = VK_MBUTTON;   break;
    case WM_XBUTTONDOWN:  vk = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2; break;
    default: return false;
    }

    if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
        vk == VK_LSHIFT || vk == VK_RSHIFT ||
        vk == VK_LCONTROL || vk == VK_RCONTROL ||
        vk == VK_LMENU || vk == VK_RMENU)
        return true;

    g_captureVk  = vk;
    g_captureGot = true;
    return true;
}

// macro thread
static LARGE_INTEGER     g_freq;
static std::atomic<bool> g_running{ true };
static std::atomic<bool> g_isSpamming{ false };
static std::atomic<bool> g_tbActive{ false };

struct MacroConfig
{
    WORD     sendKeys[2];
    WORD     triggerKey;
    double   delayMs;
    SpamMode mode;
    bool     enabled;
};

static SRWLOCK     g_cfgLock  = SRWLOCK_INIT;
static MacroConfig g_macroCfg = { { 'E', 'F' }, VK_XBUTTON1, 4.0,
                                  SpamMode::Toggle, true };

static void PublishMacroConfig()
{
    MacroConfig c;
    c.sendKeys[0] = g_cfg.sendKeys[0];
    c.sendKeys[1] = g_cfg.sendKeys[1];
    c.triggerKey  = g_cfg.triggerKey;
    c.delayMs     = g_cfg.delayMs;
    c.mode        = g_cfg.mode;
    c.enabled     = g_cfg.enabled;

    AcquireSRWLockExclusive(&g_cfgLock);
    g_macroCfg = c;
    ReleaseSRWLockExclusive(&g_cfgLock);
}

static MacroConfig ReadMacroConfig()
{
    MacroConfig c;
    AcquireSRWLockShared(&g_cfgLock);
    c = g_macroCfg;
    ReleaseSRWLockShared(&g_cfgLock);
    return c;
}

// trigger cfg
struct TriggerConfig
{
    WORD     triggerKey;
    WORD     clickKey;
    bool     enabled;
    bool     toggleMode;
    int      r, g, b;
    int      tolerance;
    double   clickDelayMs;
    int      scansPerSec;
};

static SRWLOCK         g_tbLock = SRWLOCK_INIT;
static TriggerConfig g_tbCfg = { VK_LCONTROL, 'E', false, false,
                                    180, 40, 40, 45, 80.0, 120 };

static void PublishTriggerConfig()
{
    TriggerConfig c;
    c.triggerKey   = g_cfg.tbTriggerKey;
    c.clickKey     = g_cfg.sendKeys[0];
    c.enabled      = g_cfg.tbEnabled;
    c.toggleMode   = g_cfg.tbToggle;
    c.r            = g_cfg.tbR;
    c.g            = g_cfg.tbG;
    c.b            = g_cfg.tbB;
    c.tolerance    = g_cfg.tbTolerance;
    c.clickDelayMs = g_cfg.tbClickDelayMs;
    c.scansPerSec  = g_cfg.tbScansPerSec;

    AcquireSRWLockExclusive(&g_tbLock);
    g_tbCfg = c;
    ReleaseSRWLockExclusive(&g_tbLock);
}

static TriggerConfig ReadTriggerConfig()
{
    TriggerConfig c;
    AcquireSRWLockShared(&g_tbLock);
    c = g_tbCfg;
    ReleaseSRWLockShared(&g_tbLock);
    return c;
}

static inline bool IsMouseVk(WORD vk)
{
    return vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
           vk == VK_XBUTTON1 || vk == VK_XBUTTON2;
}

static void FillKeyInput(INPUT (&in)[2], WORD vk)
{
    ZeroMemory(in, sizeof(INPUT) * 2);

    if (IsMouseVk(vk))
    {
        DWORD down = 0, up = 0, data = 0;
        switch (vk)
        {
        case VK_LBUTTON:  down = MOUSEEVENTF_LEFTDOWN;   up = MOUSEEVENTF_LEFTUP;   break;
        case VK_RBUTTON:  down = MOUSEEVENTF_RIGHTDOWN;  up = MOUSEEVENTF_RIGHTUP;  break;
        case VK_MBUTTON:  down = MOUSEEVENTF_MIDDLEDOWN; up = MOUSEEVENTF_MIDDLEUP; break;
        case VK_XBUTTON1: down = MOUSEEVENTF_XDOWN; up = MOUSEEVENTF_XUP; data = XBUTTON1; break;
        case VK_XBUTTON2: down = MOUSEEVENTF_XDOWN; up = MOUSEEVENTF_XUP; data = XBUTTON2; break;
        }
        in[0].type           = INPUT_MOUSE;
        in[0].mi.dwFlags     = down;
        in[0].mi.mouseData   = data;
        in[1].type           = INPUT_MOUSE;
        in[1].mi.dwFlags     = up;
        in[1].mi.mouseData   = data;
        return;
    }

    UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
    DWORD flags = KEYEVENTF_SCANCODE;
    if ((sc & 0xFF00) == 0xE000) flags |= KEYEVENTF_EXTENDEDKEY;

    in[0].type       = INPUT_KEYBOARD;
    in[0].ki.wScan   = (WORD)(sc & 0xFF);
    in[0].ki.dwFlags = flags;
    in[1].type       = INPUT_KEYBOARD;
    in[1].ki.wScan   = (WORD)(sc & 0xFF);
    in[1].ki.dwFlags = flags | KEYEVENTF_KEYUP;
}

static void SendClickKey(WORD vk)
{
    INPUT in[2];
    FillKeyInput(in, vk);
    SendInput(2, in, sizeof(INPUT));
}

static void BuildInputs(INPUT (&in)[4], const WORD keys[2])
{
    INPUT a[2], b[2];
    FillKeyInput(a, keys[0]);
    FillKeyInput(b, keys[1]);
    memcpy(in,     a, sizeof(a));
    memcpy(in + 2, b, sizeof(b));
}

static void WaitUntil(LONGLONG deadline, HANDLE timer)
{
    const LONGLONG spinTicks    = g_freq.QuadPart / 1000;
    const LONGLONG maxWaitTicks = g_freq.QuadPart / 50;

    while (g_running)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        LONGLONG remaining = deadline - now.QuadPart;
        if (remaining <= 0) return;

        if (remaining <= spinTicks * 2) break;

        LONGLONG waitTicks = remaining - spinTicks;
        if (waitTicks > maxWaitTicks) waitTicks = maxWaitTicks;

        if (timer)
        {
            LARGE_INTEGER due;
            due.QuadPart = -(waitTicks * 10000000LL / g_freq.QuadPart);
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) &&
                WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0)
                continue;
        }
        DWORD ms = (DWORD)(waitTicks * 1000 / g_freq.QuadPart);
        Sleep(ms ? ms : 1);
    }

    if (!g_running) return;
    LARGE_INTEGER now;
    do { _mm_pause(); QueryPerformanceCounter(&now); }
    while (now.QuadPart < deadline && g_running);
}

static inline LONGLONG QpcNow()
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart;
}

static inline LONGLONG MsToTicks(double ms)
{
    return (LONGLONG)(ms / 1000.0 * (double)g_freq.QuadPart);
}

static DWORD WINAPI MacroThread(LPVOID)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer)
        timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);

    INPUT inputs[4];
    WORD  builtFor[2] = { 0, 0 };
    bool  toggleState = false, prevTriggerDown = false, wasActive = false;
    LONGLONG next = 0;

    while (g_running)
    {
        MacroConfig cfg = ReadMacroConfig();

        bool triggerDown = (GetAsyncKeyState(cfg.triggerKey) & 0x8000) != 0;
        if (g_capturing.load() != CaptureTarget::None) triggerDown = false;

        bool active;
        if (cfg.mode == SpamMode::Hold) active = cfg.enabled && triggerDown;
        else {
            if (triggerDown && !prevTriggerDown) toggleState = !toggleState;
            active = cfg.enabled && toggleState;
        }
        prevTriggerDown = triggerDown;
        g_isSpamming    = active;

        if (active)
        {
            WORD k[2] = { cfg.sendKeys[0], cfg.sendKeys[1] };
            if (k[0] != builtFor[0] || k[1] != builtFor[1])
            {
                BuildInputs(inputs, k);
                builtFor[0] = k[0];
                builtFor[1] = k[1];
            }

            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            if (!wasActive) next = now.QuadPart;

            SendInput(4, inputs, sizeof(INPUT));

            next += (LONGLONG)(cfg.delayMs / 1000.0 * (double)g_freq.QuadPart);
            QueryPerformanceCounter(&now);
            if (next < now.QuadPart) next = now.QuadPart;

            WaitUntil(next, timer);
        }
        else
        {
            Sleep(1);
        }
        wasActive = active;
    }

    if (timer) CloseHandle(timer);
    return 0;
}

struct CenterColorSampler
{
    static constexpr int SW = 16;
    static constexpr int SH = 24;

    HDC       hScreen = nullptr;
    HDC       hMem    = nullptr;
    HBITMAP   hBmp    = nullptr;
    HGDIOBJ   hOld    = nullptr;
    uint32_t* px      = nullptr;

    int screenW = 0, screenH = 0, startX = 0, startY = 0;
    LONGLONG lastMetricsCheck = 0;

    bool Init()
    {
        hScreen = GetDC(nullptr);
        if (!hScreen) return false;

        hMem = CreateCompatibleDC(hScreen);
        if (!hMem) { Shutdown(); return false; }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = SW;
        bmi.bmiHeader.biHeight      = -SH;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        hBmp = CreateDIBSection(hMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hBmp || !bits) { Shutdown(); return false; }

        px   = static_cast<uint32_t*>(bits);
        hOld = SelectObject(hMem, hBmp);

        screenW = GetSystemMetrics(SM_CXSCREEN);
        screenH = GetSystemMetrics(SM_CYSCREEN);
        startX  = (screenW - SW) / 2;
        startY  = (screenH - SH) / 2 + screenH * (25 / 1600);
        return true;
    }

    void Shutdown()
    {
        if (hMem && hOld) SelectObject(hMem, hOld);
        if (hBmp)    { DeleteObject(hBmp);          hBmp    = nullptr; }
        if (hMem)    { DeleteDC(hMem);              hMem    = nullptr; }
        if (hScreen) { ReleaseDC(nullptr, hScreen); hScreen = nullptr; }
        hOld = nullptr;
        px   = nullptr;
    }

    bool Sample(int tr, int tg, int tb, int tol, int minPixels)
    {
        if (!hMem || !hScreen || !px) return false;

        const LONGLONG nowT = QpcNow();
        if (nowT - lastMetricsCheck > g_freq.QuadPart / 2)
        {
            lastMetricsCheck = nowT;
            int w = GetSystemMetrics(SM_CXSCREEN);
            int h = GetSystemMetrics(SM_CYSCREEN);
            if (w != screenW || h != screenH)
            {
                screenW = w; screenH = h;
                startX = (screenW - SW) / 2;
                startY = (screenH - SH) / 2 + screenH * (25 / 1600);
            }
        }

        if (!BitBlt(hMem, 0, 0, SW, SH, hScreen, startX, startY, SRCCOPY))
            return false;

        const unsigned lim = 2u * (unsigned)tol;
        int matches = 0;
        for (int i = 0; i < SW * SH; ++i)
        {
            uint32_t p = px[i];
            int b = (int)( p        & 0xFF);
            int g = (int)((p >>  8) & 0xFF);
            int r = (int)((p >> 16) & 0xFF);
            // Branchless |x - t| <= tol
            if ((unsigned)(r - tr + tol) <= lim &&
                (unsigned)(g - tg + tol) <= lim &&
                (unsigned)(b - tb + tol) <= lim)
            {
                if (++matches >= minPixels) return true;
            }
        }
        return false;
    }
};

static CenterColorSampler g_centerSampler;

static DWORD WINAPI TriggerThread(LPVOID)
{
    // timeBeginPeriod(1) is already active process-wide (set in WinMain).
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    SetThreadPriorityBoost(GetCurrentThread(), TRUE);

    DWORD  mmTask = 0;
    HANDLE mmcss  = AvSetMmThreadCharacteristicsW(L"Games", &mmTask);

    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer)
        timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);

    bool prevKeyDown = false;
    bool toggleState = false;

    bool     wasMatching = false;
    bool     absentValid = false;
    LONGLONG absentSince = 0;
    const LONGLONG kLockoutTicks = MsToTicks(250.0);
    const LONGLONG kRearmTicks   = MsToTicks(80.0);
    LONGLONG lockoutUntil = 0;

    const int kMinPixels = 20;

    LONGLONG nextScan = QpcNow();

    while (g_running)
    {
        TriggerConfig cfg = ReadTriggerConfig();

        bool keyDown = (GetAsyncKeyState(cfg.triggerKey) & 0x8000) != 0;
        if (g_capturing.load() != CaptureTarget::None) keyDown = false;

        bool active;
        if (!cfg.toggleMode) active = cfg.enabled && keyDown;
        else {
            if (keyDown && !prevKeyDown) toggleState = !toggleState;
            active = cfg.enabled && toggleState;
        }
        prevKeyDown = keyDown;
        g_tbActive  = active;

        if (!active)
        {
            wasMatching = false;
            absentValid = false;
            lockoutUntil = 0;
            Sleep(2);
            nextScan = QpcNow();
            continue;
        }

        const bool     matching = g_centerSampler.Sample(cfg.r, cfg.g, cfg.b, cfg.tolerance, kMinPixels);
        const LONGLONG detected = QpcNow();

        if (detected < lockoutUntil)
        {
            wasMatching = true;
            absentValid = false;
        }
        else if (matching)
        {
            absentValid = false;

            if (!wasMatching)
            {
                if (cfg.clickDelayMs > 0.0)
                    WaitUntil(detected + MsToTicks(cfg.clickDelayMs), timer);

                if (!g_running) break;
                SendClickKey(cfg.clickKey);
                lockoutUntil = QpcNow() + kLockoutTicks;
            }
            wasMatching = true;
        }
        else if (wasMatching)
        {
            if (!absentValid) { absentSince = detected; absentValid = true; }
            else if (detected - absentSince >= kRearmTicks)
            {
                wasMatching = false;
                absentValid = false;
            }
        }

        int sps = cfg.scansPerSec < 1 ? 1 : cfg.scansPerSec;
        if (sps > kMaxScansPerSec) sps = kMaxScansPerSec;

        nextScan += (LONGLONG)((double)g_freq.QuadPart / (double)sps);
        const LONGLONG now = QpcNow();
        if (nextScan < now) nextScan = now;
        WaitUntil(nextScan, timer);
    }

    if (timer)  CloseHandle(timer);
    if (mmcss)  AvRevertMmThreadCharacteristics(mmcss);
    return 0;
}

// window
static HINSTANCE         g_hInst = nullptr;
static HWND              g_hwnd = nullptr;
static LPDIRECT3D9       g_pD3D = nullptr;
static LPDIRECT3DDEVICE9 g_pd3dDevice = nullptr;
static D3DPRESENT_PARAMETERS g_d3dpp = {};
static IDirect3DTexture9* g_logoTexture = nullptr;
static int                g_logoW = 0, g_logoH = 0;

static const int kOverlayW = 450;
static const int kOverlayH = 400;

// Height of the main panel when the Trigger section is disabled
// (Color + Timing sections hidden).
static const int kCollapsedH = 305;

static float g_anchorX = 0.0f, g_anchorY = 0.0f;

static bool  g_overlayVisible = true;
static float g_fadeAlpha      = 0.0f;
static float g_fadeTarget     = 0.0f;
static LARGE_INTEGER g_lastAnimTime = {};

static float      g_themeMix       = 0.0f;
static float      g_themeTarget    = 0.0f;
static float      g_bgLuma         = 1.0f;
static ULONGLONG  g_lastLumaSample = 0;

static inline float LerpF(float a, float b, float t) { return a + (b - a) * t; }

static inline ImU32 LerpU32(ImU32 a, ImU32 b, float t)
{
    int ar = (a >> IM_COL32_R_SHIFT) & 0xFF, ag = (a >> IM_COL32_G_SHIFT) & 0xFF,
        ab = (a >> IM_COL32_B_SHIFT) & 0xFF, aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
    int br = (b >> IM_COL32_R_SHIFT) & 0xFF, bg = (b >> IM_COL32_G_SHIFT) & 0xFF,
        bb = (b >> IM_COL32_B_SHIFT) & 0xFF, ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32(
        (int)(ar + (br - ar) * t + 0.5f),
        (int)(ag + (bg - ag) * t + 0.5f),
        (int)(ab + (bb - ab) * t + 0.5f),
        (int)(aa + (ba - aa) * t + 0.5f));
}

static inline ImVec4 LerpV4(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(LerpF(a.x, b.x, t), LerpF(a.y, b.y, t),
                  LerpF(a.z, b.z, t), LerpF(a.w, b.w, t));
}

// Multiplies a packed color's alpha by a (0..1).
static inline ImU32 ScaleAlpha(ImU32 c, float a)
{
    int al = (int)((float)((c >> IM_COL32_A_SHIFT) & 0xFF) * a + 0.5f);
    if (al < 0)   al = 0;
    if (al > 255) al = 255;
    return (c & ~IM_COL32_A_MASK) | ((ImU32)al << IM_COL32_A_SHIFT);
}

static void EnableWindowBlur(HWND hwnd)
{
#if USE_UNDOCUMENTED_BLUR
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (!hUser) return;
    typedef BOOL(WINAPI* PFN_SetWindowCompositionAttribute)(HWND, void*);
    auto SetWCA = (PFN_SetWindowCompositionAttribute)
        GetProcAddress(hUser, "SetWindowCompositionAttribute");
    if (!SetWCA) return;

    struct ACCENT_POLICY_ { int AccentState; DWORD AccentFlags; DWORD GradientColor; DWORD AnimationId; };
    struct WCAD_ { DWORD Attrib; void* pvData; SIZE_T cbData; };

    ACCENT_POLICY_ policy = { 3, 0, 0, 0 };
    WCAD_ wcad = { 19, &policy, sizeof(policy) };
    SetWCA(hwnd, &wcad);
#else
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &bb);
#endif
}

#if ADAPTIVE_THEME_FROM_SCREEN
struct LumaSampler
{
    static constexpr int SW = 24;
    static constexpr int SH = 12;

    HDC       hScreen = nullptr;
    HDC       hMem    = nullptr;
    HBITMAP   hBmp    = nullptr;
    HGDIOBJ   hOld    = nullptr;
    uint32_t* px      = nullptr;

    bool Init()
    {
        hScreen = GetDC(nullptr);
        if (!hScreen) return false;

        hMem = CreateCompatibleDC(hScreen);
        if (!hMem) { Shutdown(); return false; }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = SW;
        bmi.bmiHeader.biHeight      = -SH;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        hBmp = CreateDIBSection(hMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hBmp || !bits) { Shutdown(); return false; }

        px   = static_cast<uint32_t*>(bits);
        hOld = SelectObject(hMem, hBmp);

        SetStretchBltMode(hMem, HALFTONE);
        SetBrushOrgEx(hMem, 0, 0, nullptr);
        return true;
    }

    void Shutdown()
    {
        if (hMem && hOld) SelectObject(hMem, hOld);
        if (hBmp)    { DeleteObject(hBmp);          hBmp    = nullptr; }
        if (hMem)    { DeleteDC(hMem);              hMem    = nullptr; }
        if (hScreen) { ReleaseDC(nullptr, hScreen); hScreen = nullptr; }
        hOld = nullptr;
        px   = nullptr;
    }

    bool Sample(float& outLuma, const RECT& rc)
    {
        if (!hMem || !hScreen || !px) return false;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) return false;

        if (!StretchBlt(hMem, 0, 0, SW, SH, hScreen,
                        rc.left, rc.top, w, h, SRCCOPY))
            return false;

        double sum = 0.0;
        for (int i = 0; i < SW * SH; ++i)
        {
            uint32_t p = px[i];
            int b  = (int)( p        & 0xFF);
            int g  = (int)((p >>  8) & 0xFF);
            int rv = (int)((p >> 16) & 0xFF);
            sum += 0.2126 * rv + 0.7152 * g + 0.0722 * b;
        }
        outLuma = (float)(sum / (double)(SW * SH * 255.0));
        return true;
    }
};

static LumaSampler g_lumaSampler;

static void SampleBackgroundLuma()
{
    if (!g_hwnd) return;

    RECT rc;
    if (!GetWindowRect(g_hwnd, &rc)) return;

    float luma;
    if (g_lumaSampler.Sample(luma, rc))
        g_bgLuma = luma;
}
#else
static void SampleBackgroundLuma()
{
    DWORD v = 1, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        g_bgLuma = v ? 1.0f : 0.0f;
}
#endif

static IDirect3DTexture9* LoadTextureFromResource(int resId, int* w, int* h)
{
    HRSRC res = FindResourceW(g_hInst, MAKEINTRESOURCEW(resId), MAKEINTRESOURCEW(10));
    if (!res) return nullptr;
    HGLOBAL hGlob = LoadResource(g_hInst, res);
    if (!hGlob) return nullptr;
    const void* ptr = LockResource(hGlob);
    DWORD size = SizeofResource(g_hInst, res);
    if (!ptr || size == 0) return nullptr;

    IWICImagingFactory* pFactory = nullptr;
    if (CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IWICImagingFactory, (void**)&pFactory) != S_OK)
        return nullptr;

    IWICStream* pStream = nullptr;
    if (pFactory->CreateStream(&pStream) != S_OK) { pFactory->Release(); return nullptr; }
    pStream->InitializeFromMemory((BYTE*)ptr, size);

    IWICBitmapDecoder* pDecoder = nullptr;
    if (pFactory->CreateDecoderFromStream(pStream, nullptr,
        WICDecodeMetadataCacheOnDemand, &pDecoder) != S_OK)
    { pStream->Release(); pFactory->Release(); return nullptr; }

    IWICBitmapFrameDecode* pFrame = nullptr;
    if (pDecoder->GetFrame(0, &pFrame) != S_OK)
    { pDecoder->Release(); pStream->Release(); pFactory->Release(); return nullptr; }

    UINT px = 0, py = 0;
    pFrame->GetSize(&px, &py);

    IWICFormatConverter* pConv = nullptr;
    if (pFactory->CreateFormatConverter(&pConv) != S_OK)
    { pFrame->Release(); pDecoder->Release(); pStream->Release(); pFactory->Release(); return nullptr; }

    if (pConv->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA,
                          WICBitmapDitherTypeNone, nullptr, 0.0,
                          WICBitmapPaletteTypeCustom) != S_OK)
    {
        pConv->Release(); pFrame->Release(); pDecoder->Release();
        pStream->Release(); pFactory->Release(); return nullptr;
    }

    uint32_t* pixels = new uint32_t[px * py];
    if (pConv->CopyPixels(nullptr, px * 4, px * py * 4, (BYTE*)pixels) != S_OK)
    {
        delete[] pixels; pConv->Release(); pFrame->Release(); pDecoder->Release();
        pStream->Release(); pFactory->Release(); return nullptr;
    }

    IDirect3DTexture9* tex = nullptr;
    if (g_pd3dDevice->CreateTexture(px, py, 1, 0, D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED, &tex, nullptr) != D3D_OK)
    {
        delete[] pixels; pConv->Release(); pFrame->Release(); pDecoder->Release();
        pStream->Release(); pFactory->Release(); return nullptr;
    }

    D3DLOCKED_RECT lr;
    if (tex->LockRect(0, &lr, nullptr, 0) == D3D_OK)
    {
        const BYTE* src = (const BYTE*)pixels;
        BYTE* dst = (BYTE*)lr.pBits;
        for (UINT y = 0; y < py; ++y)
            memcpy(dst + y * lr.Pitch, src + y * px * 4, px * 4);
        tex->UnlockRect(0);
    }

    delete[] pixels;
    pConv->Release(); pFrame->Release(); pDecoder->Release();
    pStream->Release(); pFactory->Release();

    if (w) *w = (int)px;
    if (h) *h = (int)py;
    return tex;
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (CaptureFromMessage(msg, wParam))
        return (msg == WM_XBUTTONDOWN) ? TRUE : 0;

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCHITTEST:
        return HTCLIENT;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool CreateDeviceD3D(HWND hWnd)
{
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (g_pD3D == nullptr) return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    // COPY lets us present a sub-rectangle of the back buffer, so the OS window
    // (and the blur behind it) can be resized without resetting the device.
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
    g_d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    g_d3dpp.BackBufferWidth  = kOverlayW;
    g_d3dpp.BackBufferHeight = kOverlayH;
    g_d3dpp.EnableAutoDepthStencil = FALSE;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

static void CleanupDeviceD3D()
{
    if (g_logoTexture) { g_logoTexture->Release(); g_logoTexture = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

static void ApplyGrayscaleTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 8.0f;
    s.FrameRounding     = 4.0f;
    s.GrabRounding      = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ChildRounding     = 4.0f;
    s.WindowBorderSize  = 0.0f;
    s.FrameBorderSize   = 1.0f;
    s.PopupBorderSize   = 0.0f;
    s.FramePadding      = ImVec2(8, 5);
}

static void UpdateThemeColors(float t)
{
    ImVec4* c = ImGui::GetStyle().Colors;

    const ImVec4 ltText  = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    const ImVec4 ltDis   = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    const ImVec4 ltCheck = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    const ImVec4 dkText  = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    const ImVec4 dkDis   = ImVec4(0.62f, 0.62f, 0.62f, 1.00f);
    const ImVec4 dkCheck = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);

    c[ImGuiCol_WindowBg]       = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    c[ImGuiCol_Border]         = LerpV4(ImVec4(0, 0, 0, 0.10f), ImVec4(1, 1, 1, 0.14f), t);
    c[ImGuiCol_FrameBg]        = LerpV4(ImVec4(0, 0, 0, 0.04f), ImVec4(1, 1, 1, 0.06f), t);
    c[ImGuiCol_FrameBgHovered] = LerpV4(ImVec4(0, 0, 0, 0.08f), ImVec4(1, 1, 1, 0.12f), t);
    c[ImGuiCol_FrameBgActive]  = LerpV4(ImVec4(0, 0, 0, 0.14f), ImVec4(1, 1, 1, 0.18f), t);
    c[ImGuiCol_Button]         = LerpV4(ImVec4(0, 0, 0, 0.04f), ImVec4(1, 1, 1, 0.06f), t);
    c[ImGuiCol_ButtonHovered]  = LerpV4(ImVec4(0, 0, 0, 0.10f), ImVec4(1, 1, 1, 0.14f), t);
    c[ImGuiCol_ButtonActive]   = LerpV4(ImVec4(0, 0, 0, 0.18f), ImVec4(1, 1, 1, 0.22f), t);
    c[ImGuiCol_CheckMark]      = LerpV4(ltCheck, dkCheck, t);
    c[ImGuiCol_Text]           = LerpV4(ltText,  dkText,  t);
    c[ImGuiCol_TextDisabled]   = LerpV4(ltDis,   dkDis,   t);
}

static void ShowOverlay(bool show)
{
    g_overlayVisible = show;
    g_fadeTarget     = show ? 1.0f : 0.0f;
}

static bool g_dragRequest = false;

// color picker pop-out
static bool  g_pickerOpen   = false;  // what the user wants
static bool  g_pickerShown  = false;  // whether the OS window is currently widened
static float g_pickerAlpha  = 0.0f;   // fade animation 0..1
static const int kPickerGap = 6;
static const int kPickerW   = 280;

// animated height of the main panel
static float g_menuH = (float)kOverlayH;

static std::string Utf8(const std::wstring& w)
{
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], n, nullptr, nullptr);
    out.resize(n > 0 ? n - 1 : 0);
    return out;
}

static bool KeyButton(const char* id, WORD& key, CaptureTarget t, float x, float y, float w)
{
    bool changed = false;
    bool capturing = (g_capturing.load() == t);
    std::string text = capturing ? "press a key..." : Utf8(VkToString(key));
    std::string label = text + "###" + id;
    ImGui::SetCursorPos(ImVec2(x, y));
    if (ImGui::Button(label.c_str(), ImVec2(w, 28))) BeginCapture(t);
    if (capturing && g_captureGot.exchange(false))
    {
        key = g_captureVk.load();
        g_capturing = CaptureTarget::None;
        changed = true;
    }
    return changed;
}

static void LabelAt(float x, float y, const char* text)
{
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::TextUnformatted(text);
}

static void DrawUI()
{
    bool changed = false;
    const float W  = (float)kOverlayW;
    const float H  = g_menuH;                 // animated main panel height
    const float PH = (float)kOverlayH;        // picker panel height
    const float titleH = 44.0f, sideW = 75.0f;

    const ImU32 line      = LerpU32(IM_COL32(0, 0, 0, 140),   IM_COL32(255, 255, 255, 175), g_themeMix);
    const ImU32 fg        = LerpU32(IM_COL32(0, 0, 0, 240),   IM_COL32(242, 242, 242, 250), g_themeMix);
    const ImU32 ctrlHover = LerpU32(IM_COL32(0, 0, 0, 25),    IM_COL32(255, 255, 255, 45),  g_themeMix);

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(W, H), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Paradise", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRect(ImVec2(1, 1), ImVec2(W - 1, H - 1), line, 7.0f, 0, 1.5f);
    dl->AddLine(ImVec2(12, titleH), ImVec2(W - 12, titleH), line, 1.5f);
    dl->AddLine(ImVec2(sideW, titleH + 12), ImVec2(sideW, H - 14), line, 1.5f);

    if (g_logoTexture)
        dl->AddImage((ImTextureID)(intptr_t)g_logoTexture, ImVec2(14, 8), ImVec2(14 + 30, 8 + 30));
    dl->AddText(ImGui::GetFont(), 26.0f, ImVec2(58, 8), fg, "Paradise Macro");

    auto ctrlBtn = [&](const char* id, float x, bool close) -> bool
    {
        ImGui::SetCursorPos(ImVec2(x, 8));
        bool clicked = ImGui::InvisibleButton(id, ImVec2(28, 28));
        bool hov = ImGui::IsItemHovered();
        ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
        if (hov)
            dl->AddRectFilled(p0, p1,
                close ? IM_COL32(220, 60, 60, 200) : ctrlHover,
                7.5f);
        ImVec2 c((p0.x + p1.x) * 0.4995f, (p0.y + p1.y) * 0.5f);
        if (close)
        {
            dl->AddLine(ImVec2(c.x - 6, c.y - 6), ImVec2(c.x + 6, c.y + 6), fg, 2.0f);
            dl->AddLine(ImVec2(c.x - 6, c.y + 6), ImVec2(c.x + 6, c.y - 6), fg, 2.0f);
        }
        else
            dl->AddLine(ImVec2(c.x - 7, c.y), ImVec2(c.x + 7, c.y), fg, 2.0f);
        return clicked;
    };
    if (ctrlBtn("##close", W - 42, true)) PostQuitMessage(0);

    auto BoldLabelAt = [&](float x, float y, const char* text)
    {
        ImGui::SetCursorPos(ImVec2(x, y));
        ImGui::PushFont(g_fontBold);
        ImGui::TextUnformatted(text);
        ImGui::PopFont();
    };

    BoldLabelAt(20, 66,  "Mode");
    BoldLabelAt(20, 111, "Binds");
    BoldLabelAt(20, 201, "Speed");

    int mode = (int)g_cfg.mode;
    ImGui::SetCursorPos(ImVec2(100, 61));
    if (ImGui::RadioButton("Hold", &mode, (int)SpamMode::Hold)) changed = true;
    ImGui::SameLine(0, 40);
    if (ImGui::RadioButton("Toggle", &mode, (int)SpamMode::Toggle)) changed = true;
    g_cfg.mode = (SpamMode)mode;

    LabelAt(100, 111, "Trigger");
    if (KeyButton("trigger", g_cfg.triggerKey, CaptureTarget::Trigger, 152, 106, 108)) changed = true;
    LabelAt(282, 111, "Key 1");
    if (KeyButton("key1", g_cfg.sendKeys[0], CaptureTarget::Key1, 322, 106, 114)) changed = true;

    LabelAt(100, 156, "UI Key");
    if (KeyButton("ui", g_cfg.overlayKey, CaptureTarget::Overlay, 152, 151, 108)) changed = true;
    LabelAt(282, 156, "Key 2");
    if (KeyButton("key2", g_cfg.sendKeys[1], CaptureTarget::Key2, 322, 151, 114)) changed = true;

    static double kpsBuf = 0.0;
    static bool kpsInit = false;
    if (!kpsInit) { kpsBuf = (g_cfg.delayMs > 0.0) ? (2000.0 / g_cfg.delayMs) : 333.0; kpsInit = true; }
    ImGui::SetCursorPos(ImVec2(100, 196));
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputDouble("##kps", &kpsBuf, 0.0, 0.0, "%.1f"))
    {
        if (kpsBuf < 1.0)    kpsBuf = 1.0;
        if (kpsBuf > 1000.0) kpsBuf = 1000.0;
        changed = true;
    }
    if (kpsBuf > 0.0) g_cfg.delayMs = 2000.0 / kpsBuf;
    ImGui::SetCursorPos(ImVec2(282, 201));
    ImGui::Text("kps = %.3f ms", g_cfg.delayMs);

    dl->AddLine(ImVec2(12, 240), ImVec2(W - 12, 240), line, 1.5f);
    BoldLabelAt(20, 262, "Trigger");

    ImGui::SetCursorPos(ImVec2(100, 257));
    if (ImGui::Checkbox("##tb_en", &g_cfg.tbEnabled)) changed = true;

    if (KeyButton("tb_bind", g_cfg.tbTriggerKey, CaptureTarget::TBTrigger, 140, 257, 108)) changed = true;
    int tbMode = g_cfg.tbToggle ? 1 : 0;
    ImGui::SetCursorPos(ImVec2(265, 257));
    if (ImGui::RadioButton("Hold##tb", &tbMode, 0)) { g_cfg.tbToggle = false; changed = true; }
    ImGui::SameLine(0, 20);
    if (ImGui::RadioButton("Toggle##tb", &tbMode, 1)) { g_cfg.tbToggle = true; changed = true; }

    // Color + Timing sections: fade with the collapse animation and are
    // skipped entirely (no input) once nearly hidden.
    const float lowerA = (H - (float)kCollapsedH) / (float)(kOverlayH - kCollapsedH);
    if (lowerA > 0.02f)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, lowerA > 1.0f ? 1.0f : lowerA);

        BoldLabelAt(20, 307, "Color");
        ImGui::SetCursorPos(ImVec2(100, 302));
        {
            ImVec4 sw(g_cfg.tbR / 255.0f, g_cfg.tbG / 255.0f, g_cfg.tbB / 255.0f, 1.0f);
            if (ImGui::ColorButton("##tb_swatch", sw,
                    ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_Uint8, ImVec2(28, 28)))
                g_pickerOpen = !g_pickerOpen;
        }
        LabelAt(135, 307, "Tolerance");
        ImGui::SetCursorPos(ImVec2(200, 302));
        ImGui::SetNextItemWidth(40);
        if (ImGui::InputInt("##tb_tol", &g_cfg.tbTolerance, 0, 0))
        {
            if (g_cfg.tbTolerance < 0)   g_cfg.tbTolerance = 0;
            if (g_cfg.tbTolerance > 255) g_cfg.tbTolerance = 255;
            changed = true;
        }

        BoldLabelAt(20, 352, "Timing");
        LabelAt(100, 352, "Frequency");
        ImGui::SetCursorPos(ImVec2(170, 347));
        ImGui::SetNextItemWidth(40);
        if (ImGui::InputInt("##tb_sps", &g_cfg.tbScansPerSec, 0, 0))
        {
            if (g_cfg.tbScansPerSec < 1)               g_cfg.tbScansPerSec = 1;
            if (g_cfg.tbScansPerSec > kMaxScansPerSec) g_cfg.tbScansPerSec = kMaxScansPerSec;
            changed = true;
        }
        LabelAt(220, 352, "/s");

        LabelAt(260, 352, "Delay");
        ImGui::SetCursorPos(ImVec2(300, 347));
        ImGui::SetNextItemWidth(40);
        if (ImGui::InputDouble("##tb_delay", &g_cfg.tbClickDelayMs, 0.0, 0.0, "%.0f"))
        {
            if (g_cfg.tbClickDelayMs < 0.0) g_cfg.tbClickDelayMs = 0.0;
            changed = true;
        }
        LabelAt(350, 352, "ms");

        ImGui::PopStyleVar();
    }

    ImVec2 mp = ImGui::GetMousePos();
    if (ImGui::IsMouseClicked(0) && mp.y >= 0 && mp.y < titleH && !ImGui::IsAnyItemHovered())
        g_dragRequest = true;

    ImGui::End();

    // ---- pop-out color picker panel (second ImGui window, right of the main one) ----
    if (g_pickerShown)
    {
        const float pa  = g_pickerAlpha;               // fade factor
        const float px0 = (float)(kOverlayW + kPickerGap);
        const float PW  = (float)kPickerW;

        const ImU32 pLine = ScaleAlpha(line, pa);
        const ImU32 pFg   = ScaleAlpha(fg,   pa);

        ImGui::SetNextWindowPos(ImVec2(px0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(PW, PH), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pa);   // fades widgets, text and window bg
        ImGui::Begin("ParadisePicker", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar(); // WindowPadding (Alpha stays until after End)

        ImDrawList* pdl = ImGui::GetWindowDrawList();

        // draw-list coordinates are absolute (screen/client space)
        pdl->AddRect(ImVec2(px0 + 1, 1), ImVec2(px0 + PW - 1, PH - 1), pLine, 7.0f, 0, 1.5f);
        pdl->AddLine(ImVec2(px0 + 12, titleH), ImVec2(px0 + PW - 12, titleH), pLine, 1.5f);
        pdl->AddText(ImGui::GetFont(), 26.0f, ImVec2(px0 + 20, 8), pFg, "Color");

        // close (X) button -- cursor positions are window-local
        ImGui::SetCursorPos(ImVec2(PW - 42, 8));
        bool xClicked = ImGui::InvisibleButton("##picker_close", ImVec2(28, 28));
        bool xHover   = ImGui::IsItemHovered();
        ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
        if (xHover) pdl->AddRectFilled(p0, p1, ScaleAlpha(IM_COL32(220, 60, 60, 200), pa), 7.5f);
        ImVec2 c((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
        pdl->AddLine(ImVec2(c.x - 6, c.y - 6), ImVec2(c.x + 6, c.y + 6), pFg, 2.0f);
        pdl->AddLine(ImVec2(c.x - 6, c.y + 6), ImVec2(c.x + 6, c.y - 6), pFg, 2.0f);
        if (xClicked) g_pickerOpen = false;

        // the picker itself
        float pc[3] = { g_cfg.tbR / 255.0f, g_cfg.tbG / 255.0f, g_cfg.tbB / 255.0f };
        ImGui::SetCursorPos(ImVec2(22, titleH + 14));
        ImGui::SetNextItemWidth(PW - 44);
        if (ImGui::ColorPicker3("##tb_picker", pc,
                ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_PickerHueBar))
        {
            g_cfg.tbR = (int)(pc[0] * 255.0f + 0.5f);
            g_cfg.tbG = (int)(pc[1] * 255.0f + 0.5f);
            g_cfg.tbB = (int)(pc[2] * 255.0f + 0.5f);
            changed = true;
        }

        ImGui::End();
        ImGui::PopStyleVar(); // Alpha
    }

    if (changed)
    {
        PublishMacroConfig();
        PublishTriggerConfig();
        SaveSettings();
    }
}

// Switches the picker's widened layout on/off. Only the back buffer is reset
// here (to the maximum size it will ever need); the actual OS window size is
// handled by UpdateWindowRegion.
static void ApplyPickerWindowState()
{
    const bool wantExpanded = g_pickerOpen || g_pickerAlpha > 0.0f;
    if (wantExpanded == g_pickerShown || !g_hwnd) return;
    g_pickerShown = wantExpanded;

    if (g_pickerShown)
    {
        RECT wr; GetWindowRect(g_hwnd, &wr);
        int x = wr.left, y = wr.top;
        int totalW = kOverlayW + kPickerGap + kPickerW;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        if (x + totalW > sw) x = sw - totalW - 20;   // keep the picker on screen
        if (x < 0) x = 0;
        SetWindowPos(g_hwnd, nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        g_anchorX = (float)x;
        g_anchorY = (float)y;
    }

    // Back buffer: explicit size, big enough for the full layout.
    ImGui_ImplDX9_InvalidateDeviceObjects();
    g_d3dpp.BackBufferWidth  = g_pickerShown ? (kOverlayW + kPickerGap + kPickerW) : kOverlayW;
    g_d3dpp.BackBufferHeight = kOverlayH;
    g_pd3dDevice->Reset(&g_d3dpp);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Resizes the actual OS window to the visible shape so the blur behind it
// follows (the blur ignores window regions, but always covers the full window
// rect). Main panel only: window = kOverlayW x animated height.
// With the picker: window = full width x kOverlayH, clipped to the two panels.
static void UpdateWindowRegion(bool force)
{
    static int lastW = -1, lastH = -1;

    if (!g_hwnd) return;

    const int h    = (int)(g_menuH + 0.5f);
    const int winW = g_pickerShown ? (kOverlayW + kPickerGap + kPickerW) : kOverlayW;
    const int winH = g_pickerShown ? kOverlayH : h;

    if (!force && winW == lastW && winH == lastH) return;
    lastW = winW; lastH = winH;

    SetWindowPos(g_hwnd, nullptr, 0, 0, winW, winH,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    HRGN a = CreateRoundRectRgn(0, 0, kOverlayW + 1, h + 1, 16, 16);
    if (g_pickerShown)
    {
        HRGN b = CreateRoundRectRgn(kOverlayW + kPickerGap, 0, winW + 1, kOverlayH + 1, 16, 16);
        CombineRgn(a, a, b, RGN_OR);
        DeleteObject(b);
    }
    SetWindowRgn(g_hwnd, a, TRUE);   // system owns the region afterwards
}

// Winmain
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"ParadiseMacroMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hMutex);
        MessageBoxW(nullptr, L"Already running.", L"ParadiseMacro", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    g_hInst = hInst;
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_lastAnimTime);
    timeBeginPeriod(1);
    LoadSettings();
    PublishMacroConfig();
    PublishTriggerConfig();

    // start at the correct height for the saved trigger state
    g_menuH = g_cfg.tbEnabled ? (float)kOverlayH : (float)kCollapsedH;

    // Initialize persistent samplers once.
    g_centerSampler.Init();
    #if ADAPTIVE_THEME_FROM_SCREEN
        g_lumaSampler.Init();
    #endif

    g_cfg.used++;
    SaveSettings();

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hIconSm       = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"ParadiseMacroOverlay";
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    g_anchorX = (float)(sw - kOverlayW - 20);
    g_anchorY = 60.0f;

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        wc.lpszClassName, L"ParadiseMacro",
        WS_POPUP,
        (int)g_anchorX, (int)g_anchorY, kOverlayW, kOverlayH,
        nullptr, nullptr, hInst, nullptr);

    if (!g_hwnd)
    {
        UnregisterClassW(wc.lpszClassName, hInst);
        return 1;
    }

    SetLayeredWindowAttributes(g_hwnd, 0, 0, LWA_ALPHA);
    g_fadeAlpha  = 0.0f;
    g_fadeTarget = 0.0f;

    EnableWindowBlur(g_hwnd);

    // Device is created while the window is still full size so the back buffer
    // is kOverlayW x kOverlayH; the window is shrunk afterwards.
    if (!CreateDeviceD3D(g_hwnd))
    {
        CleanupDeviceD3D();
        DestroyWindow(g_hwnd);
        UnregisterClassW(wc.lpszClassName, hInst);
        return 1;
    }

    UpdateWindowRegion(true);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    {
        wchar_t winDir[MAX_PATH] = {};
        GetWindowsDirectoryW(winDir, MAX_PATH);

        auto load = [&](const wchar_t* rel, float size) -> ImFont*
        {
            std::wstring wpath = winDir;
            wpath += rel;
            char path[MAX_PATH] = {};
            WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, path, sizeof(path), nullptr, nullptr);
            return io.Fonts->AddFontFromFileTTF(path, size);
        };

        g_fontRegular = load(L"\\Fonts\\segoeui.ttf", 17.0f);
        if (!g_fontRegular) g_fontRegular = load(L"\\Fonts\\arial.ttf", 17.0f);
        if (!g_fontRegular) g_fontRegular = io.Fonts->AddFontDefault();

        g_fontBold = load(L"\\Fonts\\segoeuib.ttf", 17.0f);
        if (!g_fontBold) g_fontBold = load(L"\\Fonts\\arialbd.ttf", 17.0f);
        if (!g_fontBold) g_fontBold = g_fontRegular;
    }

    ApplyGrayscaleTheme();
    UpdateThemeColors(g_themeMix);
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    g_logoTexture = LoadTextureFromResource(IDR_LOGO, &g_logoW, &g_logoH);

    PlaySoundW(MAKEINTRESOURCEW(IDR_SOUND), GetModuleHandleW(nullptr), SND_RESOURCE | SND_ASYNC);

    HANDLE hMacroThread = CreateThread(nullptr, 0, MacroThread,   nullptr, 0, nullptr);
    HANDLE hTbThread    = CreateThread(nullptr, 0, TriggerThread, nullptr, 0, nullptr);

    ShowWindow(g_hwnd, SW_SHOW);
    ShowOverlay(true);

    bool prevOverlayKeyDown = false;
    MSG msg;
    while (g_running)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;

        bool overlayKeyDown = (GetAsyncKeyState(g_cfg.overlayKey) & 0x8000) != 0;
        if (g_capturing.load() != CaptureTarget::None) overlayKeyDown = false;
        if (overlayKeyDown && !prevOverlayKeyDown)
            ShowOverlay(!g_overlayVisible);
        prevOverlayKeyDown = overlayKeyDown;

        LARGE_INTEGER nowT;
        QueryPerformanceCounter(&nowT);
        double dt = (double)(nowT.QuadPart - g_lastAnimTime.QuadPart)
                  / (double)g_freq.QuadPart;
        g_lastAnimTime = nowT;
        if (dt > 0.10) dt = 0.10;

        if (g_overlayVisible &&
            (GetTickCount64() - g_lastLumaSample) > 350)
        {
            g_lastLumaSample = GetTickCount64();
            SampleBackgroundLuma();

            if      (g_bgLuma < 0.50f) g_themeTarget = 1.0f;
            else if (g_bgLuma > 0.62f) g_themeTarget = 0.0f;
        }

        if (g_themeMix != g_themeTarget)
        {
            const float speed = 3.0f;
            float d    = g_themeTarget - g_themeMix;
            float step = speed * (float)dt;
            if (std::fabs(d) <= step) g_themeMix = g_themeTarget;
            else                      g_themeMix += (d > 0.0f) ? step : -step;
        }

        if (g_fadeAlpha != g_fadeTarget)
        {
            const float speed = 11.0f;
            float diff = g_fadeTarget - g_fadeAlpha;
            float step = speed * (float)dt;
            if (std::fabs(diff) <= step) g_fadeAlpha = g_fadeTarget;
            else                         g_fadeAlpha += (diff > 0.0f) ? step : -step;

            BYTE a = (BYTE)(g_fadeAlpha * 255.0f + 0.5f);
            SetLayeredWindowAttributes(g_hwnd, 0, a, LWA_ALPHA);
        }

        if (g_fadeAlpha == 0.0f && g_fadeTarget == 0.0f) { Sleep(10); continue; }

        // trigger disabled -> picker must close
        if (!g_cfg.tbEnabled) g_pickerOpen = false;

        // color picker fade in / out
        {
            const float target = g_pickerOpen ? 1.0f : 0.0f;
            if (g_pickerAlpha != target)
            {
                const float speed = 9.0f;
                float diff = target - g_pickerAlpha;
                float step = speed * (float)dt;
                if (std::fabs(diff) <= step) g_pickerAlpha = target;
                else                         g_pickerAlpha += (diff > 0.0f) ? step : -step;
            }
        }

        // main panel height: collapses only once the picker has fully faded out
        {
            const bool pickerBusy = g_pickerOpen || g_pickerAlpha > 0.0f;
            float target = (g_cfg.tbEnabled || pickerBusy) ? (float)kOverlayH
                                                           : (float)kCollapsedH;
            if (g_menuH != target)
            {
                float diff = target - g_menuH;
                g_menuH += diff * (1.0f - std::exp(-12.0f * (float)dt));  // ease-out
                if (std::fabs(target - g_menuH) < 0.5f) g_menuH = target;
            }
        }

        ApplyPickerWindowState();
        UpdateWindowRegion(false);

        UpdateThemeColors(g_themeMix);

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawUI();

        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }

        // Present only the top-left client-sized part of the back buffer.
        RECT clientRc = {};
        GetClientRect(g_hwnd, &clientRc);
        HRESULT r = g_pd3dDevice->Present(&clientRc, nullptr, nullptr, nullptr);
        if (r == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
        {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            g_pd3dDevice->Reset(&g_d3dpp);
            ImGui_ImplDX9_CreateDeviceObjects();
        }
        if (g_dragRequest)
        {
            g_dragRequest = false;
            ReleaseCapture();
            SendMessageW(g_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            RECT wr; GetWindowRect(g_hwnd, &wr);
            g_anchorX = (float)wr.left;
            g_anchorY = (float)wr.top;
            #if IMGUI_VERSION_NUM >= 18700
                io.AddMouseButtonEvent(0, false);
            #else
                io.MouseDown[0] = false;
            #endif
        }
    }
    g_running = false;
    if (hMacroThread)
    {
        WaitForSingleObject(hMacroThread, 2000);
        CloseHandle(hMacroThread);
    }
    if (hTbThread)
    {
        WaitForSingleObject(hTbThread, 2000);
        CloseHandle(hTbThread);
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();

    g_centerSampler.Shutdown();
#if ADAPTIVE_THEME_FROM_SCREEN
    g_lumaSampler.Shutdown();
#endif
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, hInst);

    timeEndPeriod(1);
    CloseHandle(hMutex);
    CoUninitialize();
    return 0;
}