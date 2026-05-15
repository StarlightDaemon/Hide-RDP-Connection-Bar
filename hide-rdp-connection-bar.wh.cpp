// ==WindhawkMod==
// @id              hide-rdp-connection-bar
// @name            Hide RDP Connection Bar
// @description     Hides the BBar connection bar at the top of fullscreen Remote Desktop sessions. Workaround for the broken native setting in Windows 11.
// @version         1.0.0
// @author          StarlightDaemon
// @github          https://github.com/StarlightDaemon
// @include         mstsc.exe
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide RDP Connection Bar

Permanently hides the floating connection bar (BBar) that appears at the top
of fullscreen Remote Desktop sessions in Windows 11.

## Why this exists

The native RDP setting to hide the connection bar is broken in Windows 11 24H2.
The toggle in the RDP client options, the pin button, and the `.rdp` file
setting `displayconnectionbar:i:0` all fail to persist. This mod hooks into
window creation and hides the BBar window immediately after it is created,
before it ever becomes visible.

## Settings

- **Hide RDP connection bar** (default: enabled) — Disable to temporarily
  restore the bar without uninstalling the mod.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "properties": {
    "hideConnectionBar": {
      "description": "Hide the BBar connection bar at the top of fullscreen RDP sessions",
      "title": "Hide RDP connection bar",
      "type": "boolean",
      "default": true
    }
  },
  "type": "object"
}
*/
// ==/WindhawkModSettings==

#include <windows.h>

namespace {

bool g_hideConnectionBar = true;

void LoadSettings() {
    g_hideConnectionBar = Wh_GetIntSetting(L"hideConnectionBar") != 0;
}

// Signature of the hooked function
using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t pOrigCreateWindowExW = nullptr;

HWND WINAPI CreateWindowExW_Hook(
    DWORD     dwExStyle,
    LPCWSTR   lpClassName,
    LPCWSTR   lpWindowName,
    DWORD     dwStyle,
    int       X,
    int       Y,
    int       nWidth,
    int       nHeight,
    HWND      hWndParent,
    HMENU     hMenu,
    HINSTANCE hInstance,
    LPVOID    lpParam)
{
    HWND hwnd = pOrigCreateWindowExW(
        dwExStyle, lpClassName, lpWindowName,
        dwStyle, X, Y, nWidth, nHeight,
        hWndParent, hMenu, hInstance, lpParam);

    if (hwnd && g_hideConnectionBar) {
        // lpClassName may be an atom (MAKEINTATOM) rather than a string pointer.
        // Guard against the atom case before calling lstrcmpW.
        if (lpClassName && reinterpret_cast<ULONG_PTR>(lpClassName) > 0xFFFF) {
            if (lstrcmpW(lpClassName, L"BBarWindowClass") == 0) {
                Wh_Log(L"Hiding BBarWindowClass window (HWND=%p)", hwnd);
                ShowWindow(hwnd, SW_HIDE);
            }
        }
    }

    return hwnd;
}

} // namespace

BOOL Wh_ModInit() {
    LoadSettings();

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(CreateWindowExW),
        reinterpret_cast<void*>(CreateWindowExW_Hook),
        reinterpret_cast<void**>(&pOrigCreateWindowExW));

    Wh_Log(L"Hide RDP Connection Bar initialized (hideConnectionBar=%d)",
           (int)g_hideConnectionBar);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    Wh_Log(L"Settings reloaded (hideConnectionBar=%d)", (int)g_hideConnectionBar);
}
