# Hide RDP Connection Bar

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Windows-11%2024H2-0078D4?logo=windows&logoColor=white)]()
[![Windhawk](https://img.shields.io/badge/Windhawk-mod-orange)](https://windhawk.net/)

A [Windhawk](https://windhawk.net/) mod that permanently hides the floating connection bar (BBar) at the top of fullscreen Remote Desktop sessions in Windows 11. Fixes a regression in Windows 11 24H2 where all native methods of hiding the bar stopped working.

Optionally provides a clean, DPI-aware disconnect button pinned to any corner of the screen, complete with idle-fading, hostname display, and a customizable global hotkey. Fully multi-monitor aware.

---

## The Problem

The native RDP setting to hide the connection bar is broken in Windows 11 24H2. Three mechanisms that previously worked now all fail to persist:

- The **"Show the connection bar when I use the full screen"** toggle in RDP client options
- The **pin button** on the connection bar itself
- The `.rdp` file setting `displayconnectionbar:i:0`

The bar reappears on every connection regardless of these settings.

---

## How This Mod Works

The mod hooks `CreateWindowExW` inside `mstsc.exe`. After each window is created, it checks whether the new window's class name is `BBarWindowClass` — the internal class used by the connection bar. If it matches and the mod is enabled, `ShowWindow(hwnd, SW_HIDE)` is called immediately, before the window ever becomes visible.

The original `CreateWindowExW` call is allowed to complete normally; the bar is hidden after creation rather than blocked. This avoids any risk of destabilizing the RDP client.

---

## Installation

### Via Windhawk Marketplace

1. Install [Windhawk](https://windhawk.net/)
2. Open Windhawk and search for **"Hide RDP Connection Bar"**
3. Click **Install**

### Manual Install

1. Install [Windhawk](https://windhawk.net/)
2. Open Windhawk → **Create new mod**
3. Copy the contents of [`hide-rdp-connection-bar.wh.cpp`](hide-rdp-connection-bar.wh.cpp) into the editor
4. Click **Compile mod**

---

## Settings

| Setting | Type | Default | Description |
|---|---|---|---|
| `hideBar` | Boolean | `true` | Hides the native RDP connection bar. Turn off to restore it. |
| `showButton` | Boolean | `false` | Shows a disconnect button on the screen edge. Works with or without Hide. If it does not appear, close and reopen the RDP connection. |
| `buttonPosition` | Dropdown | `top-right` | Which corner of the RDP monitor to place the button. |
| `offsetPreset` | Dropdown | `medium` | How far to nudge the button away from the corner. Use Custom offset to override with an exact value. |
| `offsetCustom` | Number | `0` | Exact pixel offset. Overrides Corner offset when non-zero. |
| `showHostname` | Boolean | `true` | Displays the remote host name above the disconnect label. |
| `fadeWhenIdle` | Boolean | `false` | Fades the button to near-invisible after a few seconds of no hover. Brightens when you move the mouse over it. |
| `enableHotkey` | Boolean | `false` | Keyboard shortcut to disconnect without clicking the button. Provides visual feedback if the hotkey fails to register. |
| `hotkeyModifier` | Dropdown | `ctrl-alt` | Modifier keys held for the hotkey. Only used when hotkey is enabled. |
| `hotkeyKey` | Dropdown | `d` | Key pressed with the modifier. Only used when hotkey is enabled. |

To temporarily restore the native bar without uninstalling the mod, open Windhawk, go to the mod's **Settings** tab, and disable `hideBar`.

---

## Known Limitations

- **Ctrl+Alt+Home** — This keyboard shortcut is hardwired into the RDP client to show a menu accessed through the connection bar. With the mod active the bar is hidden immediately after creation, so it may briefly flash into view before being hidden. This is a cosmetic artifact and does not affect functionality.
- **Settings changes take effect on the next connection** — The mod reads the setting live at window creation time, so toggling the setting mid-session will not affect an already-running session.
- **Tested on Windows 11 24H2** — Behavior on earlier Windows versions or the Windows App (formerly MSRDC) client is untested.
- **Button requires reconnect** — The BBar is only created when a session starts. If you enable the disconnect button mid-session, you must disconnect and reconnect for it to appear.
- **Hotkey Conflicts** — If the chosen hotkey is already used by another application, the mod will silently fail to register it at the OS level, but the button will display "✕  Hotkey Failed" to alert you.

---

## Links

- [Windhawk](https://windhawk.net/)
- [Windhawk mod authoring guide](https://windhawk.net/docs/)
- [Windhawk mods repository](https://github.com/ramensoftware/windhawk-mods)
- [Mod source](hide-rdp-connection-bar.wh.cpp)

---

## License

MIT — see [LICENSE](LICENSE).
