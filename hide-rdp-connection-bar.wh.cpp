// ==WindhawkMod==
// @id              hide-rdp-connection-bar
// @name            Hide RDP Connection Bar
// @description     Hides the Remote Desktop connection bar in fullscreen RDP sessions on Windows 11 and replaces it with a clean disconnect button. Adds Minimize/Restore/Disconnect buttons to the taskbar thumbnail. Shows hostname, fades when idle, supports a disconnect hotkey. Multi-monitor aware.
// @version         1.1.9
// @author          StarlightDaemon
// @github          https://github.com/StarlightDaemon
// @include         mstsc.exe
// @compilerOptions -lgdi32 -lshcore -lole32 -ladvapi32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide RDP Connection Bar

Hides the floating Remote Desktop connection bar in fullscreen sessions on
Windows 11, where the native options to hide the bar may not persist reliably.

Optionally shows a clean disconnect button pinned to any corner of the screen:

- Three stacked rows: hostname display on top, Minimize and Restore
  side by side in the middle, Disconnect on the bottom
- Minimize sends the fullscreen session to the taskbar; Restore brings
  it back from the same button, which stays on screen while the session
  is minimized. Whichever of the two does not apply to the current state
  is dimmed and clicking it does nothing
- Full border outline for visibility, or top-accent-only — your choice
- Fades to near-invisible when idle, brightens on hover
- Configurable keyboard hotkey to disconnect without touching the mouse
- Follows the RDP window if moved to a different monitor
- DPI-aware — scales correctly on 4K and HiDPI displays
- Drag the button anywhere on screen — the position persists across
  reconnects; changing the position settings in the Windhawk UI resets it
  back to the configured default

The same Minimize, Restore, and Disconnect controls also appear as buttons
under the taskbar thumbnail preview — hover the mstsc taskbar icon to use
them. They work for both fullscreen and windowed sessions, track the
window's minimized state, and survive an explorer.exe restart. They are
part of the same "Show disconnect button" setting.

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
  $description: Shows a button on the screen edge with Minimize, Disconnect, and Restore controls, plus the same three controls under the taskbar thumbnail preview. Works with or without Hide. If it does not appear, close and reopen the Remote Desktop connection.
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
#include <shobjidl.h>
#include <windowsx.h>
#include <atomic>

#ifndef THBN_CLICKED
#define THBN_CLICKED 0x1800
#endif

namespace {

// ── Constants ─────────────────────────────────────────────────────────────

constexpr int  BTN_W           = 80;
constexpr int  BTN_H           = 56;
constexpr BYTE ALPHA_FULL      = 230;
constexpr BYTE ALPHA_FADED     = 35;
constexpr UINT FADE_DELAY_MS   = 4000;
constexpr int  FADE_TIMER_ID   = 42;
constexpr int  ICONIC_TIMER_ID = 43;
constexpr UINT ICONIC_POLL_MS  = 400;

// Row layout, top to bottom (logical px, scaled at paint/click time):
// [0, ZONE_MINRESTORE_TOP) = hostname display (not clickable),
// [ZONE_MINRESTORE_TOP, ZONE_DISCONNECT_TOP) = Minimize (left half) /
// Restore (right half) side by side, [ZONE_DISCONNECT_TOP, BTN_H) =
// Disconnect.
constexpr int  ZONE_MINRESTORE_TOP = 20;
constexpr int  ZONE_DISCONNECT_TOP = 40;
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
WNDPROC                   g_origFrameWndProc = nullptr;  // guarded by g_cs

// Registered window messages for the frame subclass. Written once in
// Wh_ModInit before any hook can run, read-only afterwards — no
// synchronization needed. RegisterWindowMessage (not WM_APP offsets) so the
// values cannot collide with anything mstsc itself uses on its frame window.
UINT g_msgTaskbarButtonCreated = 0;  // Microsoft's documented taskbar signal
UINT g_msgThumbRefresh         = 0;  // re-evaluate thumb bar state/visibility
UINT g_msgThumbTeardown        = 0;  // release COM state on the frame thread
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

// ── Minimize / Restore ────────────────────────────────────────────────────

// Resolves the RDP frame for the minimize/restore zones. Deliberately never
// falls back to GetAncestor of the button itself the way DisconnectSession
// does — minimizing the wrong window is worse than a no-op.
HWND GetRdpFrameForAction() {
    EnterCriticalSection(&g_cs);
    HWND hFrame = g_hRdpFrame;
    LeaveCriticalSection(&g_cs);
    if (!hFrame || !IsWindow(hFrame))
        hFrame = FindWindowW(L"TscShellContainerClass", nullptr);
    return hFrame;
}

// Live minimize-state query. No hook tracks this: a taskbar-initiated
// minimize runs in explorer.exe and is not guaranteed to produce any
// in-process ShowWindow call this mod's hooks would see, so the state is
// sampled fresh whenever it matters (paint, zone click, poll timer).
bool IsRdpFrameIconic() {
    HWND hFrame = GetRdpFrameForAction();
    return hFrame && IsIconic(hFrame) != FALSE;
}

// ── Taskbar thumbnail toolbar ─────────────────────────────────────────────
//
// Minimize / Restore / Disconnect buttons under the taskbar hover thumbnail
// of the RDP frame, mirroring the overlay button's rows. Everything below
// runs on the RDP frame's own thread (inside FrameSubclassProc): the
// TaskbarButtonCreated message arrives there, and CLSID_TaskbarList is
// registered ThreadingModel=Apartment, so the interface pointer must be
// created, used, and released on that one thread.

constexpr UINT THUMB_ID_MINIMIZE   = 1001;
constexpr UINT THUMB_ID_RESTORE    = 1002;
constexpr UINT THUMB_ID_DISCONNECT = 1003;

// Defined locally instead of pulling in -luuid for two GUIDs.
const CLSID kCLSID_TaskbarList =
    { 0x56FDF344, 0xFD6D, 0x11D0, { 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90 } };
const IID kIID_ITaskbarList3 =
    { 0xEA1AFB91, 0x9E28, 0x4B86, { 0x90, 0xE9, 0x9E, 0x9F, 0x8A, 0x5E, 0xEF, 0xAF } };

// Frame-thread-only state. Like the helper thread's drag state, these are
// only ever touched on the thread that owns the frame window, so they need
// no synchronization. Wh_ModUninit reaches them indirectly, by sending
// g_msgThumbTeardown to the frame (see TeardownThumbBar).
ITaskbarList3* g_pTaskbarList         = nullptr;
bool           g_thumbButtonsAdded    = false;
bool           g_thumbHidden          = false;
bool           g_thumbLastIconic      = false;
bool           g_comInitedByThumbBar  = false;
bool           g_taskbarButtonCreated = false;
HICON          g_thumbIcons[3]        = {};

// The thumbnail flyout follows the taskbar's theme, not the app theme.
bool IsTaskbarLightTheme() {
    DWORD val = 0, cb = sizeof(val);
    if (RegGetValueW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &val, &cb)
            == ERROR_SUCCESS)
        return val != 0;
    return false;  // value absent → Windows defaults to a dark taskbar
}

// Renders one Segoe MDL2 Assets glyph into a 32-bpp ARGB HICON. THUMBBUTTON
// requires a real icon handle — this is a different mechanism from the
// overlay button's direct GDI text painting. GDI text output carries no
// alpha, so the glyph is drawn white-on-black and the grayscale coverage is
// reinterpreted as a premultiplied alpha channel afterwards.
HICON CreateGlyphIcon(wchar_t glyph, bool darkGlyph) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0 || cy <= 0) { cx = 16; cy = 16; }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = cx;
    bmi.bmiHeader.biHeight      = -cy;  // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc)
        return nullptr;
    void* bits = nullptr;
    HBITMAP hbmColor = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmColor || !bits) {
        if (hbmColor) DeleteObject(hbmColor);
        DeleteDC(hdc);
        return nullptr;
    }

    HBITMAP hbmOld = (HBITMAP)SelectObject(hdc, hbmColor);
    memset(bits, 0, (size_t)cx * cy * 4);

    // ANTIALIASED, not ClearType: ClearType's subpixel coloring would corrupt
    // the coverage-as-alpha conversion below.
    HFONT hFont = CreateFontW(
        -MulDiv(cy, 3, 4), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rc = { 0, 0, cx, cy };
    wchar_t text[2] = { glyph, 0 };
    DrawTextW(hdc, text, 1, &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();

    DWORD* px = (DWORD*)bits;
    for (int i = 0; i < cx * cy; i++) {
        DWORD a = px[i] & 0xFF;  // white-on-black: any channel = coverage
        // Premultiplied ARGB: white glyph = (a,a,a,a), black glyph = (a,0,0,0)
        px[i] = darkGlyph ? (a << 24)
                          : ((a << 24) | (a << 16) | (a << 8) | a);
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    SelectObject(hdc, hbmOld);
    DeleteDC(hdc);

    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, nullptr);
    ICONINFO ii = {};
    ii.fIcon    = TRUE;
    ii.hbmMask  = hbmMask;
    ii.hbmColor = hbmColor;
    HICON hIcon = CreateIconIndirect(&ii);
    if (hbmMask) DeleteObject(hbmMask);
    DeleteObject(hbmColor);
    return hIcon;
}

void FillThumbButton(THUMBBUTTON* btn, UINT id, HICON hIcon, PCWSTR tip,
                     THUMBBUTTONFLAGS flags) {
    btn->dwMask  = (THUMBBUTTONMASK)(THB_ICON | THB_TOOLTIP | THB_FLAGS);
    btn->iId     = id;
    btn->hIcon   = hIcon;
    btn->dwFlags = flags;
    wcsncpy_s(btn->szTip, tip, _TRUNCATE);
}

// Establishes (or, after an explorer.exe restart, re-establishes) the
// thumbnail toolbar. ThumbBarAddButtons is a one-shot API per taskbar
// button lifetime: buttons cannot be added, removed, or reordered after the
// first call, and 7 is the maximum — all three go in with one call, in
// their permanent left-to-right order.
void CreateOrRefreshThumbBar(HWND hwnd) {
    if (g_pTaskbarList) {
        g_pTaskbarList->Release();
        g_pTaskbarList = nullptr;
    }
    g_thumbButtonsAdded = false;
    g_thumbHidden = false;

    // Defensive COM init: this mod is injected into a process it does not
    // control, so no assumption about this thread's COM state. mstsc's UI
    // thread normally has COM already (S_FALSE, which still needs
    // balancing); RPC_E_CHANGED_MODE means the thread is already COM-
    // initialized under a different concurrency model — usable as-is for
    // CoCreateInstance, and nothing for us to balance.
    if (!g_comInitedByThumbBar) {
        HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (hrCo == S_OK || hrCo == S_FALSE) {
            g_comInitedByThumbBar = true;  // balanced in TeardownThumbBar
        } else if (hrCo != RPC_E_CHANGED_MODE) {
            Wh_Log(L"ThumbBar: CoInitializeEx failed hr=0x%08X", hrCo);
            return;
        }
    }

    HRESULT hr = CoCreateInstance(kCLSID_TaskbarList, nullptr,
        CLSCTX_INPROC_SERVER, kIID_ITaskbarList3, (void**)&g_pTaskbarList);
    Wh_Log(L"[DIAG] CoCreateInstance(CLSID_TaskbarList) hr=0x%08X", hr);
    if (FAILED(hr) || !g_pTaskbarList) {
        g_pTaskbarList = nullptr;
        Wh_Log(L"ThumbBar: CoCreateInstance failed hr=0x%08X", hr);
        return;
    }
    hr = g_pTaskbarList->HrInit();
    Wh_Log(L"[DIAG] HrInit hr=0x%08X", hr);
    if (FAILED(hr)) {
        g_pTaskbarList->Release();
        g_pTaskbarList = nullptr;
        Wh_Log(L"ThumbBar: HrInit failed hr=0x%08X", hr);
        return;
    }

    // Codepoints verified against Microsoft's Segoe Fluent/MDL2 reference:
    // E921 ChromeMinimize, E923 ChromeRestore, E8BB ChromeClose (matches the
    // overlay's "✕ Disconnect" iconography).
    static constexpr wchar_t kGlyphs[3] = { 0xE921, 0xE923, 0xE8BB };
    bool light = IsTaskbarLightTheme();
    for (int i = 0; i < 3; i++) {
        if (g_thumbIcons[i]) DestroyIcon(g_thumbIcons[i]);
        g_thumbIcons[i] = CreateGlyphIcon(kGlyphs[i], light);
    }

    bool iconic = IsIconic(hwnd) != FALSE;
    THUMBBUTTON btns[3] = {};
    FillThumbButton(&btns[0], THUMB_ID_MINIMIZE, g_thumbIcons[0],
        L"Minimize", iconic ? THBF_DISABLED : THBF_ENABLED);
    FillThumbButton(&btns[1], THUMB_ID_RESTORE, g_thumbIcons[1],
        L"Restore", iconic ? THBF_ENABLED : THBF_DISABLED);
    FillThumbButton(&btns[2], THUMB_ID_DISCONNECT, g_thumbIcons[2],
        L"Disconnect", THBF_ENABLED);

    hr = g_pTaskbarList->ThumbBarAddButtons(hwnd, 3, btns);
    Wh_Log(L"[DIAG] ThumbBarAddButtons hr=0x%08X buttonsPassed=%d", hr, 3);
    if (FAILED(hr)) {
        g_pTaskbarList->Release();
        g_pTaskbarList = nullptr;
        Wh_Log(L"ThumbBar: ThumbBarAddButtons failed hr=0x%08X", hr);
        return;
    }
    g_thumbButtonsAdded = true;
    g_thumbLastIconic   = iconic;
    Wh_Log(L"ThumbBar: buttons added to frame %p (iconic=%d)", hwnd, (int)iconic);
}

// Shows or hides all three buttons (they can never be removed — the API is
// one-shot). Used when showButton is toggled at runtime.
void SetThumbBarVisible(HWND hwnd, bool visible) {
    if (!g_pTaskbarList || !g_thumbButtonsAdded)
        return;
    if (g_thumbHidden == !visible)
        return;
    g_thumbHidden = !visible;
    bool iconic = IsIconic(hwnd) != FALSE;
    g_thumbLastIconic = iconic;
    THUMBBUTTON btns[3] = {};
    btns[0].dwMask  = THB_FLAGS;
    btns[0].iId     = THUMB_ID_MINIMIZE;
    btns[0].dwFlags = !visible ? THBF_HIDDEN
                    : (iconic ? THBF_DISABLED : THBF_ENABLED);
    btns[1].dwMask  = THB_FLAGS;
    btns[1].iId     = THUMB_ID_RESTORE;
    btns[1].dwFlags = !visible ? THBF_HIDDEN
                    : (iconic ? THBF_ENABLED : THBF_DISABLED);
    btns[2].dwMask  = THB_FLAGS;
    btns[2].iId     = THUMB_ID_DISCONNECT;
    btns[2].dwFlags = !visible ? THBF_HIDDEN : THBF_ENABLED;
    g_pTaskbarList->ThumbBarUpdateButtons(hwnd, 3, btns);
    Wh_Log(L"ThumbBar: visibility → %d", (int)visible);
}

// Keeps Minimize/Restore enabled state in sync with the frame's actual
// iconic state. Idempotent — only issues ThumbBarUpdateButtons when the
// state actually flipped, so both of its drivers (the frame's own WM_SIZE
// and the overlay's existing IsIconic poll) can fire freely.
void UpdateThumbButtonStates(HWND hwnd) {
    if (!g_pTaskbarList || !g_thumbButtonsAdded || g_thumbHidden)
        return;
    bool iconic = IsIconic(hwnd) != FALSE;
    if (iconic == g_thumbLastIconic)
        return;
    g_thumbLastIconic = iconic;
    THUMBBUTTON btns[2] = {};
    btns[0].dwMask  = THB_FLAGS;
    btns[0].iId     = THUMB_ID_MINIMIZE;
    btns[0].dwFlags = iconic ? THBF_DISABLED : THBF_ENABLED;
    btns[1].dwMask  = THB_FLAGS;
    btns[1].iId     = THUMB_ID_RESTORE;
    btns[1].dwFlags = iconic ? THBF_ENABLED : THBF_DISABLED;
    g_pTaskbarList->ThumbBarUpdateButtons(hwnd, 2, btns);
}

// Routes THBN_CLICKED to the exact action paths the overlay button's zones
// already use — no reimplementation.
void OnThumbButtonClicked(HWND hwnd, UINT id) {
    switch (id) {
    case THUMB_ID_MINIMIZE: {
        HWND hFrame = GetRdpFrameForAction();
        if (hFrame && !IsIconic(hFrame)) {
            Wh_Log(L"ThumbBar Minimize clicked — SW_MINIMIZE → %p", hFrame);
            pOrigShowWindow(hFrame, SW_MINIMIZE);
        } else {
            Wh_Log(L"ThumbBar Minimize clicked while inactive — ignored");
        }
        break;
    }
    case THUMB_ID_RESTORE: {
        HWND hFrame = GetRdpFrameForAction();
        if (hFrame && IsIconic(hFrame)) {
            Wh_Log(L"ThumbBar Restore clicked — SW_RESTORE → %p", hFrame);
            pOrigShowWindow(hFrame, SW_RESTORE);
        } else {
            Wh_Log(L"ThumbBar Restore clicked while inactive — ignored");
        }
        break;
    }
    case THUMB_ID_DISCONNECT:
        Wh_Log(L"ThumbBar Disconnect clicked");
        DisconnectSession(hwnd);
        break;
    }
}

// Frame-thread teardown: hide the buttons, release the Apartment-model COM
// pointer on its home thread, and balance the CoInitializeEx. hideButtons
// is false on WM_DESTROY, where the taskbar button disappears with the
// window anyway.
void TeardownThumbBar(HWND hwnd, bool hideButtons) {
    if (g_pTaskbarList) {
        if (hideButtons)
            SetThumbBarVisible(hwnd, false);
        g_pTaskbarList->Release();
        g_pTaskbarList = nullptr;
    }
    g_thumbButtonsAdded = false;
    g_thumbHidden = false;
    for (int i = 0; i < 3; i++) {
        if (g_thumbIcons[i]) {
            DestroyIcon(g_thumbIcons[i]);
            g_thumbIcons[i] = nullptr;
        }
    }
    if (g_comInitedByThumbBar) {
        CoUninitialize();
        g_comInitedByThumbBar = false;
    }
}

// ── RDP frame subclass — taskbar thumbnail toolbar ───────────────────────

LRESULT CALLBACK FrameSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc;
    EnterCriticalSection(&g_cs);
    origProc = g_origFrameWndProc;
    LeaveCriticalSection(&g_cs);

    if (g_msgTaskbarButtonCreated && msg == g_msgTaskbarButtonCreated) {
        Wh_Log(L"[DIAG] TaskbarButtonCreated received for HWND=%p", hwnd);
        // Microsoft's documented signal that ITaskbarList3 calls are now
        // safe for this window — sent when the taskbar button is first
        // created AND again whenever explorer.exe restarts, in which case
        // the toolbar must be re-established from scratch.
        g_taskbarButtonCreated = true;
        if (g_showButton)
            CreateOrRefreshThumbBar(hwnd);
        // fall through — mstsc may observe the message too
    } else if (msg == WM_COMMAND && HIWORD(wParam) == THBN_CLICKED) {
        UINT id = LOWORD(wParam);
        Wh_Log(L"[DIAG] THBN_CLICKED received, id=%u", id);
        if (id == THUMB_ID_MINIMIZE || id == THUMB_ID_RESTORE ||
            id == THUMB_ID_DISCONNECT) {
            OnThumbButtonClicked(hwnd, id);
            return 0;  // ours — don't feed synthetic command IDs to mstsc
        }
    } else if (msg == WM_SIZE) {
        // Direct signal for this window's own minimize/restore transitions,
        // on the exact thread the taskbar pointer lives on. The overlay's
        // existing IsIconic poll additionally posts g_msgThumbRefresh for
        // transitions this might miss; both funnel into the same idempotent
        // update. Falls through to mstsc's handler.
        UpdateThumbButtonStates(hwnd);
    } else if (g_msgThumbRefresh && msg == g_msgThumbRefresh) {
        // Posted by the overlay's poll on an iconic change and by
        // Wh_ModSettingsChanged on a showButton toggle.
        if (!g_showButton) {
            SetThumbBarVisible(hwnd, false);
        } else if (!g_thumbButtonsAdded && g_taskbarButtonCreated) {
            // showButton was off when TaskbarButtonCreated arrived; the
            // one-shot ThumbBarAddButtons was never spent for this taskbar
            // button, so it is still legal now.
            CreateOrRefreshThumbBar(hwnd);
        } else {
            SetThumbBarVisible(hwnd, true);
            UpdateThumbButtonStates(hwnd);
        }
        return 0;
    } else if (g_msgThumbTeardown && msg == g_msgThumbTeardown) {
        // Sent synchronously by Wh_ModUninit so the COM teardown runs here,
        // on the pointer's home thread.
        TeardownThumbBar(hwnd, true);
        return 0;
    } else if (msg == WM_DESTROY) {
        TeardownThumbBar(hwnd, false);
        if (origProc)
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(origProc));
        EnterCriticalSection(&g_cs);
        if (g_hRdpFrame == hwnd)
            g_hRdpFrame = nullptr;
        g_origFrameWndProc = nullptr;
        LeaveCriticalSection(&g_cs);
    }

    return origProc
        ? CallWindowProcW(origProc, hwnd, msg, wParam, lParam)
        : DefWindowProcW(hwnd, msg, wParam, lParam);
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
        // The frame outlives the bar. Clear the frame ref here only when the
        // frame subclass isn't independently tracking its lifetime (it clears
        // g_hRdpFrame itself on the frame's own WM_DESTROY).
        if (!g_origFrameWndProc)
            g_hRdpFrame = nullptr;
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

// Last minimize state observed by the helper thread (paint or poll timer),
// so the poll only triggers a repaint on an actual change. Helper-thread-only,
// like the drag state above.
bool  g_lastIconic       = false;

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

        // Row separators — hairlines marking the three stacked rows
        int ySepTop    = rc.top + ScaleY(ZONE_MINRESTORE_TOP);
        int ySepBottom = rc.top + ScaleY(ZONE_DISCONNECT_TOP);
        int xMid       = (rc.left + rc.right) / 2;
        HBRUSH hbrSep = CreateSolidBrush(RGB(70, 70, 70));
        RECT sepTop    = { rc.left + ScaleX(6), ySepTop,    rc.right - ScaleX(6), ySepTop + 1 };
        RECT sepBottom = { rc.left + ScaleX(6), ySepBottom, rc.right - ScaleX(6), ySepBottom + 1 };
        RECT sepMid    = { xMid, ySepTop + ScaleY(2), xMid + 1, ySepBottom - ScaleY(2) };
        FillRect(hdc, &sepTop,    hbrSep);
        FillRect(hdc, &sepBottom, hbrSep);
        FillRect(hdc, &sepMid,    hbrSep);
        DeleteObject(hbrSep);

        auto DrawLabel = [&](PCWSTR text, int height, int weight, COLORREF color,
                             const RECT& r, UINT format) {
            SetTextColor(hdc, color);
            HFONT hFont = CreateFontW(
                ScaleY(height), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);
            RECT rDraw = r;
            DrawTextW(hdc, text, -1, &rDraw, format);
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
        };

        // Minimize/Restore zone state — queried live; nothing tracks external
        // transitions, so every paint samples fresh (see IsRdpFrameIconic)
        bool iconic = IsRdpFrameIconic();
        g_lastIconic = iconic;
        constexpr COLORREF kZoneEnabled  = RGB(200, 200, 200);
        constexpr COLORREF kZoneDisabled = RGB(85, 85, 85);

        // Hostname — top row, display only, not clickable. Row still takes
        // up its space and is left blank when showHostname is off.
        wchar_t hostname[256];
        EnterCriticalSection(&g_cs);
        wcsncpy_s(hostname, g_hostname, _TRUNCATE);
        LeaveCriticalSection(&g_cs);

        if (g_showHostname && hostname[0]) {
            RECT rHost = { rc.left + ScaleX(4), rc.top + ScaleY(3),
                           rc.right - ScaleX(4), ySepTop };
            DrawLabel(hostname, 11, FW_NORMAL, RGB(140, 140, 140), rHost,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }

        // Minimize — middle row, left half, dimmed while already minimized
        RECT rMin = { rc.left, ySepTop + 1, xMid, ySepBottom };
        DrawLabel(L"–  Minimize", 10, FW_NORMAL,
            iconic ? kZoneDisabled : kZoneEnabled, rMin,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // Restore — middle row, right half, dimmed until minimized
        RECT rRest = { xMid, ySepTop + 1, rc.right, ySepBottom };
        DrawLabel(L"□  Restore", 10, FW_NORMAL,
            iconic ? kZoneEnabled : kZoneDisabled, rRest,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // Disconnect — bottom row
        bool hotkeyConflict = g_enableHotkey && !g_hotkeyRegistered;
        PCWSTR disconnectLabel = hotkeyConflict ? L"✕  Hotkey Failed" : L"✕  Disconnect";
        COLORREF discColor  = hotkeyConflict ? RGB(255, 100, 100) : RGB(235, 235, 235);
        int      discWeight = hotkeyConflict ? FW_BOLD : FW_NORMAL;

        RECT rDisc = { rc.left, ySepBottom + 1, rc.right, rc.bottom - ScaleY(2) };
        DrawLabel(disconnectLabel, 14, discWeight, discColor, rDisc,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER);

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
            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            UINT dpiX = 96, dpiY = 96;
            if (FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
                dpiY = 96;
            int clickY = GET_Y_LPARAM(lParam);
            int yMinRestoreTop = MulDiv(ZONE_MINRESTORE_TOP, dpiY, 96);
            int yDisconnectTop = MulDiv(ZONE_DISCONNECT_TOP, dpiY, 96);

            if (clickY < yMinRestoreTop) {
                // Hostname row — display only, not clickable
            } else if (clickY < yDisconnectTop) {
                // Minimize/Restore row — horizontal half selects the action
                RECT rcClient;
                GetClientRect(hwnd, &rcClient);
                int xMid = (rcClient.left + rcClient.right) / 2;
                int clickX = GET_X_LPARAM(lParam);

                if (clickX < xMid) {
                    // Minimize — left half. State checked live at click
                    // time; a click while already minimized is a deliberate
                    // no-op (the zone is drawn dimmed in that state)
                    HWND hFrame = GetRdpFrameForAction();
                    if (hFrame && !IsIconic(hFrame)) {
                        Wh_Log(L"Minimize zone clicked — SW_MINIMIZE → %p", hFrame);
                        pOrigShowWindow(hFrame, SW_MINIMIZE);
                    } else {
                        Wh_Log(L"Minimize zone clicked while inactive — ignored");
                    }
                } else {
                    // Restore — right half. Same live check, no-op unless
                    // minimized
                    HWND hFrame = GetRdpFrameForAction();
                    if (hFrame && IsIconic(hFrame)) {
                        Wh_Log(L"Restore zone clicked — SW_RESTORE → %p", hFrame);
                        pOrigShowWindow(hFrame, SW_RESTORE);
                    } else {
                        Wh_Log(L"Restore zone clicked while inactive — ignored");
                    }
                }
                InvalidateRect(hwnd, nullptr, TRUE);
            } else {
                HWND hRef;
                EnterCriticalSection(&g_cs);
                hRef = g_hBBar ? g_hBBar : hwnd;
                LeaveCriticalSection(&g_cs);
                DisconnectSession(hRef);
            }
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
        } else if (wParam == ICONIC_TIMER_ID) {
            // Background correctness check: nothing notifies this window when
            // the session is minimized/restored externally (e.g. from the
            // taskbar, in explorer.exe), so poll at low frequency and repaint
            // only on an actual change
            bool iconic = IsRdpFrameIconic();
            if (iconic != g_lastIconic) {
                g_lastIconic = iconic;
                InvalidateRect(hwnd, nullptr, TRUE);
                // Nudge the taskbar thumb bar off the same poll — the actual
                // COM work runs in FrameSubclassProc on the frame's thread,
                // and is a no-op if the state already matches.
                EnterCriticalSection(&g_cs);
                HWND hFrame  = g_hRdpFrame;
                bool frameSub = g_origFrameWndProc != nullptr;
                LeaveCriticalSection(&g_cs);
                if (frameSub && g_msgThumbRefresh && hFrame && IsWindow(hFrame))
                    PostMessageW(hFrame, g_msgThumbRefresh, 0, 0);
            }
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
        KillTimer(hwnd, ICONIC_TIMER_ID);
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

    // Low-frequency poll so the minimize/restore zones track external
    // minimize/restore transitions this process is never notified about
    SetTimer(g_hBtn, ICONIC_TIMER_ID, ICONIC_POLL_MS, nullptr);

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

// The RDP frame check falls back to GetClassNameW because, unlike the BBar
// case, mstsc could plausibly create its main frame from a class atom.
bool IsTscFrameClass(LPCWSTR lpClassName, HWND hwnd) {
    if (lpClassName && reinterpret_cast<ULONG_PTR>(lpClassName) > 0xFFFF)
        return lstrcmpW(lpClassName, L"TscShellContainerClass") == 0;
    wchar_t cls[64] = {};
    return GetClassNameW(hwnd, cls, ARRAYSIZE(cls)) != 0 &&
           lstrcmpW(cls, L"TscShellContainerClass") == 0;
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

    // Phase 1: latch the RDP frame directly when mstsc creates its main
    // window, independent of the connection bar (which is fullscreen-only) —
    // the taskbar thumbnail toolbar must also work for windowed sessions.
    // The bar-triggered path below is kept unchanged; for the same session
    // it re-latches the same frame handle.
    if (hwnd && (g_hideBar || g_showButton) && IsTscFrameClass(lpClassName, hwnd)) {
        Wh_Log(L"[DIAG] RDP frame class matched, HWND=%p", hwnd);

        EnterCriticalSection(&g_cs);
        bool alreadySubclassed = g_origFrameWndProc != nullptr;
        LeaveCriticalSection(&g_cs);

        if (!alreadySubclassed) {
            // Same pattern as the BBar subclass; safe here for the same
            // reason — CreateWindowExW returns on the window's own thread,
            // which is not pumping messages while we're inside the hook.
            WNDPROC origProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(FrameSubclassProc)));

            if (!origProc) {
                Wh_Log(L"[DIAG] FrameSubclassProc attach FAILED for HWND=%p, GLE=%d",
                    hwnd, GetLastError());
            } else {
                Wh_Log(L"[DIAG] FrameSubclassProc attached to HWND=%p, origProc=%p",
                    hwnd, origProc);
            }

            EnterCriticalSection(&g_cs);
            g_hRdpFrame        = hwnd;
            g_origFrameWndProc = origProc;
            LeaveCriticalSection(&g_cs);

            // If mstsc ever runs elevated, let the taskbar's medium-IL
            // TaskbarButtonCreated message through UIPI.
            if (g_msgTaskbarButtonCreated)
                ChangeWindowMessageFilterEx(hwnd, g_msgTaskbarButtonCreated,
                    MSGFLT_ALLOW, nullptr);

            Wh_Log(L"RDP frame detected HWND=%p — subclassed for thumb bar", hwnd);
        }
    }

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

    // Registered before any hook is installed, so the frame subclass only
    // ever sees these fully initialized. TaskbarButtonCreated is Microsoft's
    // documented signal that ITaskbarList3 calls are safe for a window; the
    // other two are this mod's private frame-thread requests.
    g_msgTaskbarButtonCreated = RegisterWindowMessageW(L"TaskbarButtonCreated");
    g_msgThumbRefresh  = RegisterWindowMessageW(L"WH_HideRdpBar_ThumbRefresh");
    g_msgThumbTeardown = RegisterWindowMessageW(L"WH_HideRdpBar_ThumbTeardown");

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

    Wh_Log(L"Hide RDP Connection Bar v1.1.9 initialized — "
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
    HWND hFrame = g_hRdpFrame;
    bool frameSub = g_origFrameWndProc != nullptr;
    LeaveCriticalSection(&g_cs);

    if (hBBar && IsWindow(hBBar)) {
        pOrigShowWindow(hBBar, g_hideBar ? SW_HIDE : SW_SHOWNOACTIVATE);
        DWORD helperThreadId = g_helperThreadId.load();
        if (g_showButton && helperThreadId)
            PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
    }

    // Sync taskbar thumb bar visibility with the (possibly toggled)
    // showButton setting — handled on the frame's own thread.
    if (frameSub && g_msgThumbRefresh && hFrame && IsWindow(hFrame))
        PostMessageW(hFrame, g_msgThumbRefresh, 0, 0);

    Wh_Log(L"Settings reloaded — hide=%d button=%d hotkey=%d fade=%d hostname=%d",
           (int)g_hideBar, (int)g_showButton,
           (int)g_enableHotkey, (int)g_fadeWhenIdle, (int)g_showHostname);
}

void Wh_ModUninit() {
    StopHelperThread();

    EnterCriticalSection(&g_cs);
    HWND    hBBar     = g_hBBar;
    WNDPROC origProc  = g_origBBarWndProc;
    HWND    hFrame    = g_hRdpFrame;
    WNDPROC origFrame = g_origFrameWndProc;
    LeaveCriticalSection(&g_cs);

    // Thumb bar teardown must run on the frame's own thread — the
    // TaskbarList object is ThreadingModel=Apartment, so its pointer is
    // released where it was created. Ask the still-installed subclass
    // synchronously, then restore the original wndproc. A hung frame thread
    // only costs the timeout: better a leaked pointer at unload than a
    // blocked unload or a cross-apartment Release.
    if (hFrame && origFrame && IsWindow(hFrame)) {
        DWORD_PTR result = 0;
        if (g_msgThumbTeardown)
            SendMessageTimeoutW(hFrame, g_msgThumbTeardown, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &result);
        SetWindowLongPtrW(hFrame, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(origFrame));
        EnterCriticalSection(&g_cs);
        g_origFrameWndProc = nullptr;
        LeaveCriticalSection(&g_cs);
    }

    if (hBBar && origProc && IsWindow(hBBar))
        SetWindowLongPtrW(hBBar, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(origProc));

    if (hBBar && IsWindow(hBBar))
        pOrigShowWindow(hBBar, SW_SHOWNOACTIVATE);

    DeleteCriticalSection(&g_cs);
}
