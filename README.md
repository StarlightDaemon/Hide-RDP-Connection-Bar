# Hide RDP Connection Bar

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Platform](https://img.shields.io/badge/Windows-11-0078D4?logo=windows&logoColor=white)
[![Windhawk](https://img.shields.io/badge/Windhawk-mod-orange)](https://windhawk.net/)

A [Windhawk](https://windhawk.net/) mod that permanently hides the floating Remote Desktop connection bar in fullscreen sessions on Windows 11. The native options to keep the bar hidden often fail to persist in Windows 11 — this mod handles it at the process level so it stays gone.

Optionally provides a clean, DPI-aware disconnect button pinned to any corner of the screen, complete with idle-fading, hostname display, and a customizable global hotkey. The button also carries Minimize and Restore controls side by side above the disconnect action — minimize the fullscreen session to the taskbar and bring it back from the same button, which stays on screen while the session is minimized. The button can also be dragged to any position, which persists across reconnects. Fully multi-monitor aware.

The same three controls also appear as buttons under the taskbar thumbnail preview — hover the mstsc taskbar icon to reach Minimize, Restore, and Disconnect without the session being fullscreen at all.

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

- **`CreateWindowExW`** — detects when the RDP frame window (`TscShellContainerClass`) is created and latches it directly (so features that are not fullscreen-only work for windowed sessions too), then detects the connection bar window (`BBarWindowClass`), records the bar handle, and hides the bar immediately if enabled
- **`ShowWindow`** — suppresses any subsequent attempt by mstsc to make the bar visible again
- **`SetWindowPos`** — strips the `SWP_SHOWWINDOW` flag from bar position calls, and detects when the RDP frame moves to a different monitor so the disconnect button can reposition
- **`SetWindowTextW`** — detects when mstsc updates its window title (which contains the remote hostname) and refreshes the button label in real time

All original calls are allowed to complete normally — the mod intercepts after the fact rather than blocking. This avoids any risk of destabilizing the RDP client.

The disconnect button is a separate floating window (`WS_POPUP | WS_EX_LAYERED | WS_EX_TOPMOST`) created on a dedicated helper thread, pinned to whichever corner you choose on the monitor hosting the RDP session. Click and drag the button to reposition it anywhere on screen — the dragged position persists across reconnects. Changing the `buttonPosition`, `offsetPreset`, or `offsetCustom` setting in the Windhawk UI resets it back to the configured default.

The button is divided into three stacked rows. The top row shows the hostname (display only, not clickable — it stays reserved even when the hostname is hidden). The middle row is split left/right between **Minimize** and **Restore**. The bottom row is **Disconnect**. Minimize sends the fullscreen session to the taskbar; Restore brings it back — labeled Restore rather than Maximize because a fullscreen RDP session has no separate maximized state to expand into. Whichever of Minimize/Restore does not apply to the current state is dimmed and clicking it does nothing. The minimize state is queried live (`IsIconic`) at paint and click time, backed by a low-frequency poll, because a taskbar-initiated minimize happens in `explorer.exe` and produces no notification inside `mstsc.exe`. The button intentionally stays visible while the session is minimized so Restore remains reachable.

The taskbar thumbnail toolbar mirrors those controls on the mstsc taskbar button itself. The mod subclasses the RDP frame window, waits for the taskbar's documented `TaskbarButtonCreated` message (the first-time-safe point for `ITaskbarList3` calls, re-sent whenever `explorer.exe` restarts — the toolbar is rebuilt on each arrival), and registers three thumbnail buttons in one `ThumbBarAddButtons` call: Minimize, Restore, Disconnect, drawn as Segoe MDL2 Assets glyphs matched to the taskbar's light/dark theme. Button clicks route to the exact same action paths as the overlay button's zones, and Minimize/Restore enabled states track the window's real minimized state (via the frame's own `WM_SIZE` plus the same `IsIconic` poll that drives the overlay). This works for fullscreen **and** windowed sessions — it does not depend on the connection bar ever existing — and ships as part of the same `showButton` setting.

---

## Installation

> Requires Windhawk 1.6 or later — earlier versions lack the `Wh_GetModStoragePath` API used to persist the dragged button position.

1. Install [Windhawk](https://windhawk.net/)
2. Open Windhawk → **Create new mod**
3. Copy the contents of [`hide-rdp-connection-bar.wh.cpp`](hide-rdp-connection-bar.wh.cpp) into the editor
4. Click **Compile mod**

---

## Settings

| Setting | Type | Default | Description |
|---|---|---|---|
| `hideBar` | Boolean | `true` | Hides the native RDP connection bar. Turn off to restore it. |
| `showButton` | Boolean | `true` | Shows a button on the screen edge with Minimize, Disconnect, and Restore controls, plus the same three controls under the taskbar thumbnail preview. Works with or without Hide. If it does not appear, close and reopen the RDP connection. |
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
- **Some settings require reconnect** — The disconnect button and hotkey are created when the session starts and cannot be added to an already-running session. All other settings (hide bar, fade, hostname, border) take effect immediately via the Windhawk settings panel.
- **Tested on Windows 11** — Behavior on earlier Windows versions or the Windows App (formerly MSRDC) client is untested.
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
