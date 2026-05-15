# Hide RDP Connection Bar

A [Windhawk](https://windhawk.net/) mod that permanently hides the floating connection bar (BBar) at the top of fullscreen Remote Desktop sessions in Windows 11.

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
| `hideConnectionBar` | Boolean | `true` | Hides the BBar connection bar at the top of fullscreen RDP sessions |

To temporarily restore the bar without uninstalling the mod, open Windhawk, go to the mod's **Settings** tab, and disable the toggle.

---

## Known Limitations

- **Ctrl+Alt+Home** — This keyboard shortcut is hardwired into the RDP client to show a menu accessed through the connection bar. With the mod active the bar is hidden immediately after creation, so it may briefly flash into view before being hidden. This is a cosmetic artifact and does not affect functionality.
- **Settings changes take effect on the next connection** — The mod reads the setting live at window creation time, so toggling the setting mid-session will not affect an already-running session.
- **Tested on Windows 11 24H2** — Behavior on earlier Windows versions or the Windows App (formerly MSRDC) client is untested.

---

## Links

- [Windhawk](https://windhawk.net/)
- [Windhawk mod authoring guide](https://windhawk.net/docs/)
- [Windhawk mods repository](https://github.com/ramensoftware/windhawk-mods)
- [Mod source](hide-rdp-connection-bar.wh.cpp)

---

## License

MIT — see [LICENSE](LICENSE).
