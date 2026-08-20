# Current State

## What This Repo Is

A [Windhawk](https://windhawk.net/) mod (`hide-rdp-connection-bar.wh.cpp`) that
permanently hides the floating RDP connection bar (BBarWindowClass) in fullscreen
Remote Desktop sessions on Windows 11. Hooks four Win32 APIs inside `mstsc.exe`.

## RAIDEN Instance Status

- WSL→macOS migration remediation: LOOP-002 (closed)

## Project Status

- Source file: `hide-rdp-connection-bar.wh.cpp` — v1.1.9, complete and compilable
- `README.md`, `LICENSE` (MIT), `.gitignore` present
- No build system, no CI configured
- **Pushed to GitHub** — `main` branch live at `StarlightDaemon/Hide-RDP-Connection-Bar`
- **Operator live test passed** — confirmed working on Windows 11 (tested at v1.1.1; all subsequent releases are correctness/threading fixes)
- Not yet published to Windhawk Marketplace

## v1.1.4 feature set

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

## Since v1.1.4, through v1.1.9

- Thread-safety fix: `g_helperThreadId` made `std::atomic` to close a data race (`94155e3`)
- Drag-to-reposition the disconnect button, with a manual test checklist (`5ac2152`, `7a4042a`)
- Disconnect button now defaults to visible (`ff7f771`)
- Drag-clamp logic extracted to `ClampToMonitorRect`, applied during live drag (`2174e57`)
- Thumbnail toolbar plus Minimize/Restore rows added, cut as v1.1.9 (`16f429d`)
