// ==WindhawkMod==
// @id              hide-rdp-connection-bar
// @name            Hide RDP Connection Bar
// @description     Hides the Remote Desktop connection bar in fullscreen RDP sessions on Windows 11 and replaces it with a clean disconnect button. Shows hostname, fades when idle, supports a disconnect hotkey. Multi-monitor aware.
// @version         1.1.4
// @author          StarlightDaemon
// @github          https://github.com/StarlightDaemon
// @include         mstsc.exe
// @compilerOptions -lgdi32 -lshcore
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide RDP Connection Bar

Hides the floating Remote Desktop connection bar in fullscreen sessions on
Windows 11, where the native options to hide the bar may not persist reliably.

Optionally shows a clean disconnect button pinned to any corner of the screen:

- Displays the remote hostname above the disconnect label, updated live
- Full border outline for visibility, or top-accent-only — your choice
- Fades to near-invisible when idle, brightens on hover
- Configurable keyboard hotkey to disconnect without touching the mouse
- Follows the RDP window if moved to a different monitor
- DPI-aware — scales correctly on 4K and HiDPI displays
- Drag the button anywhere on screen — the position persists across
  reconnects; changing the position settings in the Windhawk UI resets it
  back to the configured default

## Requirements

Requires Windhawk 1.6 or later — earlier versions lack the
`Wh_GetModStoragePath` API used to persist the dragged button position.

## Note

If the disconnect button does not appear after enabling it, close and reopen
the Remote Desktop connection. The button is created when the session starts;
it cannot appear for a session that is already running.

Click and drag the button to reposition it anywhere on screen; the dragged
position persists across reconnects. Changing the Button position, Corner
offset, or Custom offset setting in the Windhawk UI resets it back to the
configured default.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- hideBar: true
  $name: Hide connection bar
  $description: Hides the native RDP connection bar. Turn off to restore it.
- showButton: true
  $name: Show disconnect button
  $description: Shows a disconnect button on the screen edge. Works with or without Hide. If it does not appear, close and reopen the Remote Desktop connection.
- buttonPosition: top-right
  $name: Button position
  $description: Which corner of the RDP monitor to place the button.
  $options:
  - top-right: Top Right
  - top-left: Top Left
  - bottom-right: Bottom Right
  - bottom-left: Bottom Left
- offsetPreset: medium
  $name: Corner offset
  $description: How far to nudge the button away from the corner. Use Custom offset to override with an exact value.
  $options:
  - none: None (0 px)
  - small: Small (16 px)
  - medium: Medium (32 px)
  - large: Large (64 px)
  - xlarge: XL (96 px)
  - xxlarge: XXL (256 px)
  - xxxlarge: XXXL (512 px)
- offsetCustom: 0
  $name: Custom offset (pixels)
  $description: Exact pixel offset. Overrides Corner offset when non-zero.
- showBorder: true
  $name: Show full border
  $description: Draws a full outline around the button. Turn off for top-accent-only style.
- showHostname: true
  $name: Show hostname on button
  $description: Displays the remote host name above the disconnect label.
- fadeWhenIdle: false
  $name: Fade when idle
  $description: Fades the button to near-invisible after a few seconds of no hover. Brightens when you move the mouse over it.
- enableHotkey: false
  $name: Enable disconnect hotkey
  $description: Keyboard shortcut to disconnect without clicking the button.
- hotkeyModifier: ctrl-alt
  $name: Hotkey modifier keys
  $description: Modifier keys held for the hotkey. Only used when hotkey is enabled.
  $options:
  - ctrl-alt: Ctrl + Alt
  - ctrl-shift: Ctrl + Shift
  - alt-shift: Alt + Shift
- hotkeyKey: d
  $name: Hotkey key
  $description: Key pressed with the modifier. Only used when hotkey is enabled.
  $options:
  - d: D
  - q: Q
  - f4: F4
  - end: End
  - pause: Pause / Break
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <atomic>

namespace {

// ── Constants ─────────────────────────────────────────────────────────────

constexpr int  BTN_W           = 80;
constexpr int  BTN_H           = 56;
constexpr BYTE ALPHA_FULL      = 230;
constexpr BYTE ALPHA_FADED     = 35;
constexpr UINT FADE_DELAY_MS   = 4000;
constexpr int  FADE_TIMER_ID   = 42;
constexpr int  HOTKEY_ID       = 1;
constexpr auto BTN_CLASS       = L"WH_RdpDisconnectBtn";
constexpr UINT WM_CREATE_BTN   = WM_APP + 1;
constexpr UINT WM_HIDE_BTN     = WM_APP + 2;
constexpr UINT WM_REPAINT_BTN  = WM_APP + 3;

// ── Settings ──────────────────────────────────────────────────────────────

/*
 * These globals are non-atomic scalars written by LoadSettings() on Windhawk's
 * settings-changed thread and read concurrently by hook callbacks and the helper
 * thread without synchronization. This is a data race and technically undefined
 * behavior per the C++ standard. The decision is deliberate: on x86/x64 all
 * naturally-aligned word-sized loads and stores are atomic at the hardware level;
 * no cross-field invariant exists across these flags; and the worst observable
 * outcome is a setting taking effect one repaint late, which already matches the
 * mod's recreate-on-change model. The fields that carry real cross-thread invariants
 * (g_hLastMonitor, g_hotkeyRegistered) are separately guarded with std::atomic.
 */
bool g_hideBar        = true;
bool g_showButton     = false;
bool g_buttonOnRight  = true;
bool g_buttonAtBottom = false;
int  g_buttonOffset   = 32;
bool g_showBorder     = true;
bool g_showHostname   = true;
bool g_fadeWhenIdle   = false;
bool g_enableHotkey   = false;
std::atomic<bool> g_hotkeyRegistered { false };
UINT g_hotkeyMod      = MOD_CONTROL | MOD_ALT;
UINT g_hotkeyVk       = 'D';

void LoadSettings() {
    g_hideBar      = Wh_GetIntSetting(L"hideBar")      != 0;
    g_showButton   = Wh_GetIntSetting(L"showButton")   != 0;
    g_showBorder   = Wh_GetIntSetting(L"showBorder")   != 0;
    g_showHostname = Wh_GetIntSetting(L"showHostname") != 0;
    g_fadeWhenIdle = Wh_GetIntSetting(L"fadeWhenIdle") != 0;
    g_enableHotkey = Wh_GetIntSetting(L"enableHotkey") != 0;

    // Position dropdown
    PCWSTR pos = Wh_GetStringSetting(L"buttonPosition");
    g_buttonOnRight  = lstrcmpW(pos, L"top-left")    != 0
                    && lstrcmpW(pos, L"bottom-left")  != 0;
    g_buttonAtBottom = lstrcmpW(pos, L"bottom-right") == 0
                    || lstrcmpW(pos, L"bottom-left")  == 0;
    Wh_FreeStringSetting(pos);

    // Offset — custom overrides preset when non-zero
    int custom = Wh_GetIntSetting(L"offsetCustom");
    if (custom != 0) {
        g_buttonOffset = custom;
    } else {
        PCWSTR preset = Wh_GetStringSetting(L"offsetPreset");
        if      (lstrcmpW(preset, L"none")     == 0) g_buttonOffset =   0;
        else if (lstrcmpW(preset, L"small")    == 0) g_buttonOffset =  16;
        else if (lstrcmpW(preset, L"large")    == 0) g_buttonOffset =  64;
        else if (lstrcmpW(preset, L"xlarge")   == 0) g_buttonOffset =  96;
        else if (lstrcmpW(preset, L"xxlarge")  == 0) g_buttonOffset = 256;
        else if (lstrcmpW(preset, L"xxxlarge") == 0) g_buttonOffset = 512;
        else                                          g_buttonOffset =  32;
        Wh_FreeStringSetting(preset);
    }

    // Hotkey modifier
    PCWSTR mod = Wh_GetStringSetting(L"hotkeyModifier");
    if      (lstrcmpW(mod, L"ctrl-shift") == 0) g_hotkeyMod = MOD_CONTROL | MOD_SHIFT;
    else if (lstrcmpW(mod, L"alt-shift")  == 0) g_hotkeyMod = MOD_ALT     | MOD_SHIFT;
    else                                         g_hotkeyMod = MOD_CONTROL | MOD_ALT;
    Wh_FreeStringSetting(mod);

    // Hotkey key
    PCWSTR key = Wh_GetStringSetting(L"hotkeyKey");
    if      (lstrcmpW(key, L"q")     == 0) g_hotkeyVk = 'Q';
    else if (lstrcmpW(key, L"f4")    == 0) g_hotkeyVk = VK_F4;
    else if (lstrcmpW(key, L"end")   == 0) g_hotkeyVk = VK_END;
    else if (lstrcmpW(key, L"pause") == 0) g_hotkeyVk = VK_PAUSE;
    else                                   g_hotkeyVk = 'D';
    Wh_FreeStringSetting(key);
}

// ── Shared state ──────────────────────────────────────────────────────────

CRITICAL_SECTION          g_cs;
HWND                      g_hBBar           = nullptr;
HWND                      g_hRdpFrame       = nullptr;
WNDPROC                   g_origBBarWndProc = nullptr;
std::atomic<HMONITOR>     g_hLastMonitor    { nullptr };
wchar_t                   g_hostname[256]   = {};
HANDLE                    g_hHelperThread   = nullptr;
std::atomic<DWORD>        g_helperThreadId  { 0 };

// Dragged button position, overriding the settings-derived default when set.
// Guarded by g_cs: written by the helper thread on drag finalize and cleared
// by Wh_ModSettingsChanged on a relevant settings change (different thread).
bool                      g_hasDragPos      = false;
bool                      g_dragOnRight     = true;
bool                      g_dragAtBottom    = false;
int                       g_dragDx          = 0;
int                       g_dragDy          = 0;

// ── Hook originals ────────────────────────────────────────────────────────

using CreateWindowExW_t  = decltype(&CreateWindowExW);
using ShowWindow_t       = decltype(&ShowWindow);
using SetWindowPos_t     = decltype(&SetWindowPos);
using SetWindowTextW_t   = decltype(&SetWindowTextW);

CreateWindowExW_t  pOrigCreateWindowExW  = nullptr;
ShowWindow_t       pOrigShowWindow       = nullptr;
SetWindowPos_t     pOrigSetWindowPos     = nullptr;
SetWindowTextW_t   pOrigSetWindowTextW   = nullptr;

// ── Monitor helper ────────────────────────────────────────────────────────

RECT GetMonitorRect(HWND hRef) {
    HMONITOR hMon = hRef && IsWindow(hRef)
        ? MonitorFromWindow(hRef, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    if (hMon && GetMonitorInfoW(hMon, &mi))
        return mi.rcMonitor;
    return { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

RECT GetRdpMonitorRect() {
    EnterCriticalSection(&g_cs);
    HWND hRef = g_hRdpFrame ? g_hRdpFrame : g_hBBar;
    LeaveCriticalSection(&g_cs);
    return GetMonitorRect(hRef);
}

HMONITOR GetRdpMonitor() {
    EnterCriticalSection(&g_cs);
    HWND hRef = g_hRdpFrame ? g_hRdpFrame : g_hBBar;
    LeaveCriticalSection(&g_cs);
    return hRef && IsWindow(hRef)
        ? MonitorFromWindow(hRef, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

// Constrains a w×h rect at (x, y) to stay fully within mon, preferring to
// keep the top-left corner in bounds first. Shared by the live WM_MOUSEMOVE
// drag-follow code and FinalizeDragPosition's end-of-drag safety net so both
// clamp against the same math.
POINT ClampToMonitorRect(int x, int y, int w, int h, const RECT& mon) {
    if (x < mon.left) x = mon.left;
    if (y < mon.top) y = mon.top;
    if (x + w > mon.right)  x = mon.right  - w;
    if (y + h > mon.bottom) y = mon.bottom - h;
    return { x, y };
}

// ── Button position persistence ─────────────────────────────────────────────

struct PersistedButtonPos {
    DWORD magic;
    DWORD version;
    BOOL  onRight;
    BOOL  atBottom;
    int   dx;
    int   dy;
};

constexpr DWORD kButtonPosMagic   = 0x50425244; // 'DRBP'
constexpr DWORD kButtonPosVersion = 1;
constexpr PCWSTR kButtonPosFileName = L"button-pos.dat";

bool GetButtonPosFilePath(wchar_t* pathBuffer, size_t bufferChars) {
    wchar_t dir[MAX_PATH] = {};
    if (Wh_GetModStoragePath(dir, ARRAYSIZE(dir)) == 0) {
        Wh_Log(L"Wh_GetModStoragePath failed");
        return false;
    }
    CreateDirectoryW(dir, nullptr);

    if (wcslen(dir) + 1 + wcslen(kButtonPosFileName) >= bufferChars)
        return false;

    wcscpy_s(pathBuffer, bufferChars, dir);
    size_t len = wcslen(pathBuffer);
    if (len > 0 && pathBuffer[len - 1] != L'\\')
        wcscat_s(pathBuffer, bufferChars, L"\\");
    wcscat_s(pathBuffer, bufferChars, kButtonPosFileName);
    return true;
}

void PersistDragPosition(bool onRight, bool atBottom, int dx, int dy) {
    wchar_t path[MAX_PATH + 32];
    if (!GetButtonPosFilePath(path, ARRAYSIZE(path)))
        return;

    PersistedButtonPos data{ kButtonPosMagic, kButtonPosVersion,
        onRight ? TRUE : FALSE, atBottom ? TRUE : FALSE, dx, dy };

    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        Wh_Log(L"Failed to open button position file for writing, GLE=%d", GetLastError());
        return;
    }
    DWORD written = 0;
    WriteFile(hFile, &data, sizeof(data), &written, nullptr);
    CloseHandle(hFile);
}

void ClearPersistedDragPosition() {
    wchar_t path[MAX_PATH + 32];
    if (GetButtonPosFilePath(path, ARRAYSIZE(path)))
        DeleteFileW(path);
}

bool LoadPersistedDragPosition(bool* onRight, bool* atBottom, int* dx, int* dy) {
    wchar_t path[MAX_PATH + 32];
    if (!GetButtonPosFilePath(path, ARRAYSIZE(path)))
        return false;

    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    PersistedButtonPos data{};
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, &data, sizeof(data), &read, nullptr);
    CloseHandle(hFile);

    if (!ok || read != sizeof(data) || data.magic != kButtonPosMagic ||
        data.version != kButtonPosVersion)
        return false;

    *onRight  = data.onRight  != FALSE;
    *atBottom = data.atBottom != FALSE;
    *dx = data.dx;
    *dy = data.dy;
    return true;
}

// Clamps the button to the RDP monitor's full rect, derives the nearest
// corner and (dx, dy) offset from it, and persists the result. Called on
// drag finalize (button-up past the drag threshold, or capture loss).
void FinalizeDragPosition(HWND hwnd) {
    HMONITOR hMon = GetRdpMonitor();
    // Matches CreateOrRepositionButton's reference rect (mi.rcMonitor, not
    // rcWork) so the offset computed here reproduces the same on-screen
    // position when reapplied — the button is meant to sit flush against
    // the physical screen edge, same as the settings-driven default.
    MONITORINFO mi = { sizeof(mi) };
    RECT mon = (hMon && GetMonitorInfoW(hMon, &mi))
        ? mi.rcMonitor
        : RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

    UINT dpiX = 96, dpiY = 96;
    if (hMon && FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiX = 96; dpiY = 96;
    }

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    POINT clamped = ClampToMonitorRect(rc.left, rc.top, w, h, mon);
    int x = clamped.x, y = clamped.y;

    if (x != rc.left || y != rc.top) {
        pOrigSetWindowPos(hwnd, nullptr, x, y, 0, 0,
            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    int centerX = x + w / 2;
    int centerY = y + h / 2;
    bool onRight  = centerX > (mon.left + mon.right)  / 2;
    bool atBottom = centerY > (mon.top  + mon.bottom) / 2;

    int dxPx = onRight  ? (mon.right  - (x + w)) : (x - mon.left);
    int dyPx = atBottom ? (mon.bottom - (y + h)) : (y - mon.top);
    int dx = MulDiv(dxPx, 96, dpiX);
    int dy = MulDiv(dyPx, 96, dpiY);

    EnterCriticalSection(&g_cs);
    g_hasDragPos   = true;
    g_dragOnRight  = onRight;
    g_dragAtBottom = atBottom;
    g_dragDx       = dx;
    g_dragDy       = dy;
    LeaveCriticalSection(&g_cs);

    PersistDragPosition(onRight, atBottom, dx, dy);

    Wh_Log(L"Button drag finalized: onRight=%d atBottom=%d dx=%d dy=%d",
        (int)onRight, (int)atBottom, dx, dy);
}

// ── Hostname ──────────────────────────────────────────────────────────────

void UpdateHostname() {
    EnterCriticalSection(&g_cs);
    HWND hFrame = g_hRdpFrame;
    LeaveCriticalSection(&g_cs);

    wchar_t title[512] = {};
    if (hFrame && IsWindow(hFrame))
        GetWindowTextW(hFrame, title, 512);

    // Strip " - Remote Desktop Connection" suffix
    wchar_t* sep = wcsstr(title, L" - ");
    if (sep) *sep = L'\0';

    EnterCriticalSection(&g_cs);
    wcsncpy_s(g_hostname, title[0] ? title : L"", _TRUNCATE);
    LeaveCriticalSection(&g_cs);
    Wh_Log(L"Hostname: %s", g_hostname);
}

// ── Disconnect ────────────────────────────────────────────────────────────

void DisconnectSession(HWND hRef) {
    EnterCriticalSection(&g_cs);
    HWND hFrame = g_hRdpFrame;
    LeaveCriticalSection(&g_cs);
    if (!hFrame || !IsWindow(hFrame))
        hFrame = GetAncestor(hRef, GA_ROOT);
    if (!hFrame || !IsWindow(hFrame))
        hFrame = FindWindowW(L"TscShellContainerClass", nullptr);
    Wh_Log(L"Disconnect: WM_CLOSE → %p", hFrame);
    if (hFrame)
        PostMessageW(hFrame, WM_CLOSE, 0, 0);
}

// ── BBar subclass — cleanup only ─────────────────────────────────────────

LRESULT CALLBACK BBarSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc;
    EnterCriticalSection(&g_cs);
    origProc = g_origBBarWndProc;
    LeaveCriticalSection(&g_cs);

    if (msg == WM_DESTROY) {
        if (origProc)
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(origProc));
        EnterCriticalSection(&g_cs);
        g_hBBar           = nullptr;
        g_hRdpFrame       = nullptr;
        g_origBBarWndProc = nullptr;
        bool showBtn = g_showButton;
        LeaveCriticalSection(&g_cs);
        g_hLastMonitor.store(nullptr);
        DWORD helperThreadId = g_helperThreadId.load();
        if (showBtn && helperThreadId)
            PostThreadMessageW(helperThreadId, WM_HIDE_BTN, 0, 0);
    }

    return origProc
        ? CallWindowProcW(origProc, hwnd, msg, wParam, lParam)
        : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ── Disconnect button window ──────────────────────────────────────────────

HWND g_hBtn = nullptr;

// Click/drag disambiguation state. Only ever touched on the helper thread
// (the thread that owns g_hBtn and runs its message loop), so — like g_hBtn
// itself — these need no synchronization.
bool  g_btnPotentialDrag = false;
bool  g_btnDragging      = false;
POINT g_btnDragStart     = {};  // screen coords of the WM_LBUTTONDOWN
POINT g_btnWindowStart   = {};  // window top-left (screen coords) at grab time

LRESULT CALLBACK BtnWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 96, dpiY = 96;
        if (FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
            dpiX = 96; dpiY = 96;
        }

        auto ScaleX = [dpiX](int v) { return MulDiv(v, dpiX, 96); };
        auto ScaleY = [dpiY](int v) { return MulDiv(v, dpiY, 96); };

        // Background
        HBRUSH hbrBg = CreateSolidBrush(RGB(24, 24, 24));
        FillRect(hdc, &rc, hbrBg);
        DeleteObject(hbrBg);

        // Blue accent / border
        HBRUSH hbrAccent = CreateSolidBrush(RGB(0, 120, 212));
        RECT accent = { rc.left, rc.top, rc.right, rc.top + ScaleY(3) };
        FillRect(hdc, &accent, hbrAccent);
        if (g_showBorder) {
            RECT left   = { rc.left,           rc.top, rc.left  + ScaleX(2), rc.bottom };
            RECT right  = { rc.right - ScaleX(2), rc.top, rc.right,          rc.bottom };
            RECT bottom = { rc.left, rc.bottom - ScaleY(2), rc.right,        rc.bottom };
            FillRect(hdc, &left,   hbrAccent);
            FillRect(hdc, &right,  hbrAccent);
            FillRect(hdc, &bottom, hbrAccent);
        }
        DeleteObject(hbrAccent);

        SetBkMode(hdc, TRANSPARENT);

        bool hotkeyConflict = g_enableHotkey && !g_hotkeyRegistered;
        PCWSTR disconnectLabel = hotkeyConflict ? L"✕  Hotkey Failed" : L"✕  Disconnect";

        wchar_t hostname[256];
        EnterCriticalSection(&g_cs);
        wcsncpy_s(hostname, g_hostname, _TRUNCATE);
        LeaveCriticalSection(&g_cs);

        if (g_showHostname && hostname[0]) {
            // Hostname — small, dimmed, top half
            SetTextColor(hdc, RGB(140, 140, 140));
            HFONT hSmall = CreateFontW(
                ScaleY(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HFONT hOld = (HFONT)SelectObject(hdc, hSmall);
            RECT rHost = { rc.left + ScaleX(4), rc.top + ScaleY(5), rc.right - ScaleX(4), rc.top + ScaleY(26) };
            DrawTextW(hdc, hostname, -1, &rHost,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            SelectObject(hdc, hOld);
            DeleteObject(hSmall);

            // Disconnect — normal, bottom half
            SetTextColor(hdc, hotkeyConflict ? RGB(255, 100, 100) : RGB(235, 235, 235));
            HFONT hMain = CreateFontW(
                ScaleY(14), 0, 0, 0, hotkeyConflict ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            hOld = (HFONT)SelectObject(hdc, hMain);
            RECT rDisc = { rc.left, rc.top + ScaleY(26), rc.right, rc.bottom };
            DrawTextW(hdc, disconnectLabel, -1, &rDisc,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, hOld);
            DeleteObject(hMain);
        } else {
            // Disconnect only — vertically centered
            SetTextColor(hdc, hotkeyConflict ? RGB(255, 100, 100) : RGB(235, 235, 235));
            HFONT hMain = CreateFontW(
                ScaleY(14), 0, 0, 0, hotkeyConflict ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HFONT hOld = (HFONT)SelectObject(hdc, hMain);
            RECT rDisc = { rc.left, rc.top + ScaleY(3), rc.right, rc.bottom };
            DrawTextW(hdc, disconnectLabel, -1, &rDisc,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, hOld);
            DeleteObject(hMain);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        g_btnPotentialDrag = true;
        g_btnDragging = false;

        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hwnd, &pt);
        g_btnDragStart = pt;

        RECT rc;
        GetWindowRect(hwnd, &rc);
        g_btnWindowStart = { rc.left, rc.top };
        return 0;
    }

    case WM_LBUTTONUP: {
        bool wasDragging  = g_btnDragging;
        bool wasPotential = g_btnPotentialDrag;
        ReleaseCapture(); // synchronously sends WM_CAPTURECHANGED, which
                           // finalizes the drag and resets both flags
        if (!wasDragging && wasPotential) {
            HWND hRef;
            EnterCriticalSection(&g_cs);
            hRef = g_hBBar ? g_hBBar : hwnd;
            LeaveCriticalSection(&g_cs);
            DisconnectSession(hRef);
        }
        return 0;
    }

    case WM_CAPTURECHANGED: {
        if (g_btnDragging) {
            g_btnDragging = false;
            FinalizeDragPosition(hwnd);
        }
        g_btnPotentialDrag = false;
        return 0;
    }

    case WM_HOTKEY: {
        if (wParam == HOTKEY_ID) {
            HWND hRef;
            EnterCriticalSection(&g_cs);
            hRef = g_hBBar ? g_hBBar : hwnd;
            LeaveCriticalSection(&g_cs);
            DisconnectSession(hRef);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_btnPotentialDrag || g_btnDragging) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &pt);
            int totalDx = pt.x - g_btnDragStart.x;
            int totalDy = pt.y - g_btnDragStart.y;

            if (!g_btnDragging) {
                int absDx = totalDx < 0 ? -totalDx : totalDx;
                int absDy = totalDy < 0 ? -totalDy : totalDy;
                if (absDx > GetSystemMetrics(SM_CXDRAG) ||
                    absDy > GetSystemMetrics(SM_CYDRAG)) {
                    g_btnDragging = true;
                    g_btnPotentialDrag = false;
                }
            }

            if (g_btnDragging) {
                RECT rc;
                GetWindowRect(hwnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;

                // Same monitor reference as FinalizeDragPosition (the RDP
                // frame's monitor via GetRdpMonitor(), not the cursor's
                // current monitor) so the live clamp and the end-of-drag
                // clamp never disagree and cause a snap-back.
                HMONITOR hMon = GetRdpMonitor();
                MONITORINFO mi = { sizeof(mi) };
                RECT mon = (hMon && GetMonitorInfoW(hMon, &mi))
                    ? mi.rcMonitor
                    : RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

                POINT clamped = ClampToMonitorRect(
                    g_btnWindowStart.x + totalDx, g_btnWindowStart.y + totalDy,
                    w, h, mon);

                pOrigSetWindowPos(hwnd, nullptr, clamped.x, clamped.y,
                    0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }

        if (g_fadeWhenIdle) {
            KillTimer(hwnd, FADE_TIMER_ID);
            SetLayeredWindowAttributes(hwnd, 0, ALPHA_FULL, LWA_ALPHA);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        break;

    case WM_MOUSELEAVE:
        if (g_fadeWhenIdle)
            SetTimer(hwnd, FADE_TIMER_ID, FADE_DELAY_MS, nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == FADE_TIMER_ID) {
            KillTimer(hwnd, FADE_TIMER_ID);
            SetLayeredWindowAttributes(hwnd, 0, ALPHA_FADED, LWA_ALPHA);
        }
        return 0;

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, g_btnDragging ? IDC_SIZEALL : IDC_HAND));
        return TRUE;

    case WM_DISPLAYCHANGE: {
        g_hLastMonitor.store(nullptr);
        DWORD helperThreadId = g_helperThreadId.load();
        if (helperThreadId)
            PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, FADE_TIMER_ID);
        if (g_enableHotkey)
            UnregisterHotKey(hwnd, HOTKEY_ID);
        g_hBtn = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void CreateOrRepositionButton() {
    HMONITOR hMon = GetRdpMonitor();
    MONITORINFO mi = { sizeof(mi) };
    RECT mon = (hMon && GetMonitorInfoW(hMon, &mi)) ? mi.rcMonitor : 
               RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

    UINT dpiX = 96, dpiY = 96;
    if (hMon && FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiX = 96; dpiY = 96;
    }

    int scaledW = MulDiv(BTN_W, dpiX, 96);
    int scaledH = MulDiv(BTN_H, dpiY, 96);

    // A dragged position (corner + two-axis offset) overrides the
    // settings-derived default when one has been persisted. The default
    // path (dx=0, dy=g_buttonOffset) reproduces the original flush-edge,
    // vertical-offset-only placement exactly.
    bool hasDragPos;
    bool onRight, atBottom;
    int  offsetDx, offsetDy;
    EnterCriticalSection(&g_cs);
    hasDragPos = g_hasDragPos;
    onRight    = g_dragOnRight;
    atBottom   = g_dragAtBottom;
    offsetDx   = g_dragDx;
    offsetDy   = g_dragDy;
    LeaveCriticalSection(&g_cs);
    if (!hasDragPos) {
        onRight  = g_buttonOnRight;
        atBottom = g_buttonAtBottom;
        offsetDx = 0;
        offsetDy = g_buttonOffset;
    }

    int scaledDx = MulDiv(offsetDx, dpiX, 96);
    int scaledDy = MulDiv(offsetDy, dpiY, 96);

    int btnX = onRight  ? (mon.right  - scaledW - scaledDx) : (mon.left + scaledDx);
    int btnY = atBottom ? (mon.bottom - scaledH - scaledDy) : (mon.top  + scaledDy);

    Wh_Log(L"Button: x=%d y=%d w=%d h=%d (monitor %d,%d-%d,%d)",
        btnX, btnY, scaledW, scaledH,
        mon.left, mon.top, mon.right, mon.bottom);

    if (g_hBtn && IsWindow(g_hBtn)) {
        pOrigSetWindowPos(g_hBtn, HWND_TOPMOST,
            btnX, btnY, scaledW, scaledH,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        
        HRGN hRgn = CreateRoundRectRgn(0, 0, scaledW + 1, scaledH + 1, MulDiv(8, dpiX, 96), MulDiv(8, dpiY, 96));
        SetWindowRgn(g_hBtn, hRgn, FALSE);
        UpdateHostname();
        InvalidateRect(g_hBtn, nullptr, TRUE);
        return;
    }

    g_hBtn = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        BTN_CLASS, L"",
        WS_POPUP,
        btnX, btnY, scaledW, scaledH,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!g_hBtn) {
        Wh_Log(L"Button CreateWindowExW FAILED GLE=%d", GetLastError());
        return;
    }

    // Rounded corners
    HRGN hRgn = CreateRoundRectRgn(0, 0, scaledW + 1, scaledH + 1, MulDiv(8, dpiX, 96), MulDiv(8, dpiY, 96));
    SetWindowRgn(g_hBtn, hRgn, FALSE);

    // Start faded if idle-fade is on, otherwise full opacity
    SetLayeredWindowAttributes(g_hBtn, 0,
        g_fadeWhenIdle ? ALPHA_FADED : ALPHA_FULL, LWA_ALPHA);

    pOrigShowWindow(g_hBtn, SW_SHOWNOACTIVATE);
    pOrigSetWindowPos(g_hBtn, HWND_TOPMOST,
        btnX, btnY, scaledW, scaledH,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if (g_enableHotkey) {
        if (RegisterHotKey(g_hBtn, HOTKEY_ID, g_hotkeyMod, g_hotkeyVk)) {
            Wh_Log(L"Hotkey registered mod=0x%x vk=0x%x", g_hotkeyMod, g_hotkeyVk);
            g_hotkeyRegistered = true;
        } else {
            Wh_Log(L"Hotkey registration FAILED GLE=%d", GetLastError());
            g_hotkeyRegistered = false;
        }
    }

    UpdateHostname();
    InvalidateRect(g_hBtn, nullptr, TRUE);

    Wh_Log(L"Button created HWND=%p at (%d,%d)", g_hBtn, btnX, btnY);
}

// ── Helper thread ─────────────────────────────────────────────────────────

DWORD WINAPI HelperThread(LPVOID) {
    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = BtnWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = BTN_CLASS;
    wc.hCursor       = LoadCursorW(nullptr, IDC_HAND);
    RegisterClassExW(&wc);

    EnterCriticalSection(&g_cs);
    bool bbarReady = (g_hBBar != nullptr);
    LeaveCriticalSection(&g_cs);
    if (bbarReady)
        CreateOrRepositionButton();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_CREATE_BTN) {
            CreateOrRepositionButton();
        } else if (msg.message == WM_HIDE_BTN) {
            if (g_hBtn && IsWindow(g_hBtn))
                pOrigShowWindow(g_hBtn, SW_HIDE);
        } else if (msg.message == WM_REPAINT_BTN) {
            if (g_hBtn && IsWindow(g_hBtn))
                InvalidateRect(g_hBtn, nullptr, TRUE);
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_hBtn && IsWindow(g_hBtn))
        DestroyWindow(g_hBtn);
    UnregisterClassW(BTN_CLASS, GetModuleHandleW(nullptr));
    return 0;
}

void StartHelperThread() {
    if (g_hHelperThread) return;
    DWORD threadId = 0;
    g_hHelperThread = CreateThread(
        nullptr, 0, HelperThread, nullptr, 0, &threadId);
    g_helperThreadId.store(threadId);
}

void StopHelperThread() {
    DWORD threadId = g_helperThreadId.load();
    if (threadId)
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    if (g_hHelperThread) {
        WaitForSingleObject(g_hHelperThread, 3000);
        CloseHandle(g_hHelperThread);
        g_hHelperThread = nullptr;
        g_helperThreadId.store(0);
    }
}

// ── Hooks ─────────────────────────────────────────────────────────────────

bool IsBBarClass(LPCWSTR lpClassName) {
    if (!lpClassName) return false;
    if (reinterpret_cast<ULONG_PTR>(lpClassName) <= 0xFFFF) return false;
    return lstrcmpW(lpClassName, L"BBarWindowClass") == 0;
}

HWND WINAPI CreateWindowExW_Hook(
    DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
    DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    HWND hwnd = pOrigCreateWindowExW(
        dwExStyle, lpClassName, lpWindowName,
        dwStyle, X, Y, nWidth, nHeight,
        hWndParent, hMenu, hInstance, lpParam);

    if (hwnd && (g_hideBar || g_showButton) && IsBBarClass(lpClassName)) {
        HWND hFrame = hWndParent ? GetAncestor(hWndParent, GA_ROOT) : nullptr;
        RECT mon    = GetMonitorRect(hFrame ? hFrame : hwnd);
        Wh_Log(L"BBar detected HWND=%p frame=%p monitor=%d,%d-%d,%d",
            hwnd, hFrame, mon.left, mon.top, mon.right, mon.bottom);

        WNDPROC origProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(BBarSubclassProc)));

        EnterCriticalSection(&g_cs);
        g_hBBar           = hwnd;
        g_hRdpFrame       = hFrame;
        g_origBBarWndProc = origProc;
        LeaveCriticalSection(&g_cs);

        if (g_hideBar)
            pOrigShowWindow(hwnd, SW_HIDE);

        DWORD helperThreadId = g_helperThreadId.load();
        if (g_showButton && helperThreadId)
            PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
    }

    return hwnd;
}

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    if (g_hideBar && hWnd && hWnd == g_hBBar) {
        Wh_Log(L"ShowWindow: suppressing nCmdShow=%d on BBar", nCmdShow);
        return pOrigShowWindow(hWnd, SW_HIDE);
    }
    return pOrigShowWindow(hWnd, nCmdShow);
}

BOOL WINAPI SetWindowPos_Hook(
    HWND hWnd, HWND hWndInsertAfter,
    int X, int Y, int cx, int cy, UINT uFlags)
{
    if (g_hideBar && hWnd && hWnd == g_hBBar) {
        if (uFlags & SWP_SHOWWINDOW) {
            Wh_Log(L"SetWindowPos: stripping SWP_SHOWWINDOW from BBar");
            uFlags = (uFlags & ~SWP_SHOWWINDOW) | SWP_HIDEWINDOW;
        }
    }

    BOOL result = pOrigSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);

    if (g_showButton && hWnd && hWnd == g_hRdpFrame && !(uFlags & SWP_NOMOVE)) {
        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        if (hMon && hMon != g_hLastMonitor.load()) {
            g_hLastMonitor.store(hMon);
            Wh_Log(L"RDP frame changed monitor — repositioning button");
            DWORD helperThreadId = g_helperThreadId.load();
            if (helperThreadId)
                PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
        }
    }

    return result;
}

BOOL WINAPI SetWindowTextW_Hook(HWND hWnd, LPCWSTR lpString) {
    BOOL result = pOrigSetWindowTextW(hWnd, lpString);

    EnterCriticalSection(&g_cs);
    bool isFrame = (hWnd == g_hRdpFrame);
    LeaveCriticalSection(&g_cs);

    if (isFrame && g_showButton && g_showHostname) {
        UpdateHostname();
        DWORD helperThreadId = g_helperThreadId.load();
        if (helperThreadId)
            PostThreadMessageW(helperThreadId, WM_REPAINT_BTN, 0, 0);
    }
    return result;
}

} // namespace

BOOL Wh_ModInit() {
    InitializeCriticalSection(&g_cs);
    LoadSettings();

    bool dragOnRight, dragAtBottom;
    int  dragDx, dragDy;
    if (LoadPersistedDragPosition(&dragOnRight, &dragAtBottom, &dragDx, &dragDy)) {
        EnterCriticalSection(&g_cs);
        g_hasDragPos   = true;
        g_dragOnRight  = dragOnRight;
        g_dragAtBottom = dragAtBottom;
        g_dragDx       = dragDx;
        g_dragDy       = dragDy;
        LeaveCriticalSection(&g_cs);
        Wh_Log(L"Loaded persisted button position: onRight=%d atBottom=%d dx=%d dy=%d",
            (int)dragOnRight, (int)dragAtBottom, dragDx, dragDy);
    }

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(CreateWindowExW),
        reinterpret_cast<void*>(CreateWindowExW_Hook),
        reinterpret_cast<void**>(&pOrigCreateWindowExW));

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(ShowWindow),
        reinterpret_cast<void*>(ShowWindow_Hook),
        reinterpret_cast<void**>(&pOrigShowWindow));

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(SetWindowPos),
        reinterpret_cast<void*>(SetWindowPos_Hook),
        reinterpret_cast<void**>(&pOrigSetWindowPos));

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(SetWindowTextW),
        reinterpret_cast<void*>(SetWindowTextW_Hook),
        reinterpret_cast<void**>(&pOrigSetWindowTextW));

    if (g_showButton)
        StartHelperThread();

    Wh_Log(L"Hide RDP Connection Bar v1.1.4 initialized — "
           L"hide=%d button=%d hotkey=%d fade=%d hostname=%d",
           (int)g_hideBar, (int)g_showButton,
           (int)g_enableHotkey, (int)g_fadeWhenIdle, (int)g_showHostname);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    bool prevButton   = g_showButton;
    bool prevOnRight  = g_buttonOnRight;
    bool prevAtBottom = g_buttonAtBottom;
    int  prevOffset   = g_buttonOffset;

    LoadSettings();

    // buttonPosition, offsetPreset, and offsetCustom all funnel into these
    // three derived values — if none of them changed, the settings-driven
    // default didn't change either, so there's nothing to reset.
    if (g_buttonOnRight != prevOnRight || g_buttonAtBottom != prevAtBottom ||
        g_buttonOffset != prevOffset) {
        EnterCriticalSection(&g_cs);
        g_hasDragPos = false;
        LeaveCriticalSection(&g_cs);
        ClearPersistedDragPosition();
        Wh_Log(L"Button position settings changed — cleared dragged position");
    }

    if (prevButton || g_showButton) {
        StopHelperThread();
        if (g_showButton)
            StartHelperThread();
    }

    EnterCriticalSection(&g_cs);
    HWND hBBar = g_hBBar;
    LeaveCriticalSection(&g_cs);

    if (hBBar && IsWindow(hBBar)) {
        pOrigShowWindow(hBBar, g_hideBar ? SW_HIDE : SW_SHOWNOACTIVATE);
        DWORD helperThreadId = g_helperThreadId.load();
        if (g_showButton && helperThreadId)
            PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
    }

    Wh_Log(L"Settings reloaded — hide=%d button=%d hotkey=%d fade=%d hostname=%d",
           (int)g_hideBar, (int)g_showButton,
           (int)g_enableHotkey, (int)g_fadeWhenIdle, (int)g_showHostname);
}

void Wh_ModUninit() {
    StopHelperThread();

    EnterCriticalSection(&g_cs);
    HWND    hBBar    = g_hBBar;
    WNDPROC origProc = g_origBBarWndProc;
    LeaveCriticalSection(&g_cs);

    if (hBBar && origProc && IsWindow(hBBar))
        SetWindowLongPtrW(hBBar, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(origProc));

    if (hBBar && IsWindow(hBBar))
        pOrigShowWindow(hBBar, SW_SHOWNOACTIVATE);

    DeleteCriticalSection(&g_cs);
}
