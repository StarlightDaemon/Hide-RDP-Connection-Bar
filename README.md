# Hide RDP Connection Bar

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Windows-11-0078D4?logo=windows&logoColor=white)]()
[![Windhawk](https://img.shields.io/badge/Windhawk-mod-orange)](https://windhawk.net/)

A [Windhawk](https://windhawk.net/) mod that permanently hides the floating Remote Desktop connection bar in fullscreen sessions on Windows 11. The native options to keep the bar hidden often fail to persist in Windows 11 — this mod handles it at the process level so it stays gone.

Optionally provides a clean, DPI-aware disconnect button pinned to any corner of the screen, complete with idle-fading, hostname display, and a customizable global hotkey. Fully multi-monitor aware.

---

## The Problem

The native options to hide the Remote Desktop connection bar are unreliable in Windows 11. Three mechanisms that may not persist between sessions:

- The **"Show the connection bar when I use the full screen"** toggle in RDP client options
- The **pin button** on the connection bar itself
- The `.rdp` file setting `displayconnectionbar:i:0`

The bar reappears on every connection regardless of these settings.

---

## How This Mod Works

The mod hooks four Win32 APIs inside `mstsc.exe`:

- **`CreateWindowExW`** — detects when the connection bar window (`BBarWindowClass`) is created, records the bar and RDP frame handles, and hides it immediately if enabled
- **`ShowWindow`** — suppresses any subsequent attempt by mstsc to make the bar visible again
- **`SetWindowPos`** — strips the `SWP_SHOWWINDOW` flag from bar position calls, and detects when the RDP frame moves to a different monitor so the disconnect button can reposition
- **`SetWindowTextW`** — detects when mstsc updates its window title (which contains the remote hostname) and refreshes the button label in real time

All original calls are allowed to complete normally — the mod intercepts after the fact rather than blocking. This avoids any risk of destabilizing the RDP client.

The disconnect button is a separate floating window (`WS_POPUP | WS_EX_LAYERED | WS_EX_TOPMOST`) created on a dedicated helper thread, pinned to whichever corner you choose on the monitor hosting the RDP session.

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
| `showBorder` | Boolean | `true` | Draws a full outline around the button. Turn off for top-accent-only style. |
| `showHostname` | Boolean | `true` | Displays the remote hostname above the disconnect label, updated live as mstsc resolves it. |
| `fadeWhenIdle` | Boolean | `false` | Fades the button to near-invisible after a few seconds of no hover. Brightens when you move the mouse over it. |
| `enableHotkey` | Boolean | `false` | Keyboard shortcut to disconnect without clicking the button. Provides visual feedback if the hotkey fails to register. |
| `hotkeyModifier` | Dropdown | `ctrl-alt` | Modifier keys held for the hotkey. Only used when hotkey is enabled. |
| `hotkeyKey` | Dropdown | `d` | Key pressed with the modifier. Only used when hotkey is enabled. |

To temporarily restore the native bar without uninstalling the mod, open Windhawk, go to the mod's **Settings** tab, and disable `hideBar`.

---

## Known Limitations

- **Ctrl+Alt+Home** — This keyboard shortcut is hardwired into the RDP client to show a menu accessed through the connection bar. With the mod active the bar is hidden immediately after creation, so it may briefly flash into view before being hidden. This is a cosmetic artifact and does not affect functionality.
- **Settings changes take effect on the next connection** — The mod reads the setting live at window creation time, so toggling the setting mid-session will not affect an already-running session.
- **Tested on Windows 11** — Behavior on earlier Windows versions or the Windows App (formerly MSRDC) client is untested.
- **Button requires reconnect** — The connection bar is only created when a session starts. If you enable the disconnect button mid-session, you must disconnect and reconnect for it to appear.
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
