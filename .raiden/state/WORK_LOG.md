# Work Log

## 2026-05-15 — Initial scaffold

- Created `hide-rdp-connection-bar.wh.cpp`: hooks `CreateWindowExW` in `mstsc.exe`, hides `BBarWindowClass` via `ShowWindow(hwnd, SW_HIDE)` with atom guard (`> 0xFFFF`) protecting against non-pointer `lpClassName` values
- Created `README.md`, `LICENSE` (MIT), `.gitignore`, `assets/` placeholder
- RAIDEN Edict v0.4.0 installed; control plane oriented
- No git commit yet; not yet installed or tested in Windhawk

**Next:** git init + initial commit → Windhawk install → live test → optional marketplace PR

