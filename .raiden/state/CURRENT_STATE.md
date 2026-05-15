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

- Source file: `hide-rdp-connection-bar.wh.cpp` — complete and compilable
- `README.md`, `LICENSE` (MIT), `.gitignore`, `assets/` placeholder all present
- No build system, no CI configured
- 5 commits on `main`, pushed to GitHub
- Not yet installed or tested in Windhawk — operator live test pending
- Not yet published to Windhawk Marketplace

## Agent Session — 2026-05-15

RAIDEN orientation completed. Full scaffold built and published. GitHub space live. Session tokens voided by operator. Awaiting operator live test result.
