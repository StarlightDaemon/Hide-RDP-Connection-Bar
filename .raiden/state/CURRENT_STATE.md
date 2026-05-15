# Current State

**As of: 2026-05-15**

## What This Repo Is

A [Windhawk](https://windhawk.net/) mod (`hide-rdp-connection-bar.wh.cpp`) that
permanently hides the floating RDP connection bar (BBarWindowClass) in fullscreen
Remote Desktop sessions on Windows 11 24H2. Hooks `CreateWindowExW` in
`mstsc.exe` and calls `ShowWindow(hwnd, SW_HIDE)` immediately after bar creation.

## RAIDEN Instance Status

- Edict v0.4.0 installed 2026-05-15 by RAIDEN central
- Control plane: `.raiden/` — standard v0.4.0 layout
- No open loops at install time

## Project Status

- Source file: `hide-rdp-connection-bar.wh.cpp` — v1.0.0, complete and compilable
- `README.md`, `LICENSE` (MIT), `.gitignore`, `assets/` placeholder all present
- No build system, no CI configured
- **Pushed to GitHub** — `main` branch live at `StarlightDaemon/Hide-RDP-Connection-Bar`
- Not yet installed or tested in Windhawk — operator live test pending
- Not yet published to Windhawk Marketplace

## v1.0.0 feature set

- Hide native BBar (`hideBar`) — three-hook approach (CreateWindowExW, ShowWindow, SetWindowPos)
- Disconnect button (`showButton`) — 120×56px floating overlay, WS_POPUP|WS_EX_LAYERED|WS_EX_TOPMOST
- Four-corner positioning (`buttonPosition`) + preset and custom pixel offsets
- Hostname display (`showHostname`) — parsed from mstsc window title
- Fade on idle (`fadeWhenIdle`) — 4s timer, ALPHA_FADED=35, restores on hover
- Disconnect hotkey (`enableHotkey`) — RegisterHotKey with modifier + key dropdowns; visual "Hotkey Failed" label on conflict
- DPI-aware sizing — GetDpiForMonitor scales BTN_W, BTN_H, offsets, fonts, corner radius
- Multi-monitor aware — MonitorFromWindow; repositions on frame monitor change
- Helper thread owns button window; CRITICAL_SECTION guards shared HWNDs
