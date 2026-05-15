# Work Log

## 2026-05-15 — Initial scaffold

- Created `hide-rdp-connection-bar.wh.cpp`: hooks `CreateWindowExW` in `mstsc.exe`, hides `BBarWindowClass` via `ShowWindow(hwnd, SW_HIDE)` with atom guard (`> 0xFFFF`) protecting against non-pointer `lpClassName` values
- Created `README.md`, `LICENSE` (MIT), `.gitignore`, `assets/` placeholder
- RAIDEN Edict v0.4.0 installed; control plane oriented
- No git commit yet; not yet installed or tested in Windhawk

**Next:** git init + initial commit → Windhawk install → live test → optional marketplace PR

## 2026-05-15 — GitHub space configured and pushed

- README updated: badge strip added (license, platform, Windhawk), intro tightened
- RAIDEN state files committed (CURRENT_STATE, DECISIONS, WORK_LOG)
- Remote added: `https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar.git`
- All 5 commits pushed to `main`
- Repo description and 8 topics set via GitHub API (required Administration:write token)
- Session tokens voided by operator per RAIDEN rotation policy
- Repo is fully live and public

**Next:** Operator live test in Windhawk → report results → optional marketplace PR

