# Current State

**As of: 2026-05-31**

## What This Repo Is

A [Windhawk](https://windhawk.net/) mod (`hide-rdp-connection-bar.wh.cpp`) that
permanently hides the floating RDP connection bar (BBarWindowClass) in fullscreen
Remote Desktop sessions on Windows 11. Hooks four Win32 APIs inside `mstsc.exe`.

## RAIDEN Instance Status

- Edict v0.4.0 installed 2026-05-15 by RAIDEN central
- Control plane: `.raiden/` — standard v0.4.0 layout

## Project Status

- Source file: `hide-rdp-connection-bar.wh.cpp` — v1.1.3, complete and compilable
- `README.md`, `LICENSE` (MIT), `.gitignore`, `assets/` placeholder all present
- No build system, no CI configured
- **Pushed to GitHub** — `main` branch live at `StarlightDaemon/Hide-RDP-Connection-Bar` (HEAD `8355544`, local has unpushed changes from this session)
- **Operator live test passed** — confirmed working on Windows 11 (tested at v1.1.1; threading fixes in v1.1.3 are correctness-only)
- Not yet published to Windhawk Marketplace

## v1.1.3 feature set

- Hide native connection bar (`hideBar`) — four hooks: CreateWindowExW, ShowWindow, SetWindowPos, SetWindowTextW
- Disconnect button (`showButton`) — 80×56px floating overlay, WS_POPUP|WS_EX_LAYERED|WS_EX_TOPMOST
- Four-corner positioning (`buttonPosition`) + seven preset and custom pixel offsets
- Full border outline (`showBorder`) — 2px accent-colour edges on all four sides; toggle off for top-only style
- Live hostname display (`showHostname`) — updated via SetWindowTextW hook as mstsc resolves the title
- Fade on idle (`fadeWhenIdle`) — 4s timer, ALPHA_FADED=35, restores on hover
- Disconnect hotkey (`enableHotkey`) — RegisterHotKey with modifier + key dropdowns; visual "Hotkey Failed" label on conflict
- DPI-aware sizing — GetDpiForMonitor scales button size, offsets, fonts, corner radius
- Multi-monitor aware — MonitorFromWindow; repositions on frame monitor change
- Helper thread owns button window; CRITICAL_SECTION guards shared HWNDs and g_hostname; g_hotkeyRegistered is std::atomic<bool>
