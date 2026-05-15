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

- Source file: `hide-rdp-connection-bar.wh.cpp` (single-file Windhawk mod)
- README and LICENSE present
- No build system, no CI configured
- Not yet published to Windhawk Marketplace (status unknown at RAIDEN install time)
