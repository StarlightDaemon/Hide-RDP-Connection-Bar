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

## 2026-05-15 — v1.5 / v1.6 rework

- Identified root cause of resize failures: mstsc continuously calls SetWindowPos on BBar, fighting any size override at that level
- Removed overlay disconnect button (complexity not justified; native bar has a close button)
- v1.5.0: switched from 8px sliver to natural-height compact bar (narrowed to 1/5 monitor width) — still unreliable due to SetWindowPos fight
- v1.6.0: complete rewrite of approach
  - Replaced two-boolean settings (hideConnectionBar + shrinkToSliver) with single `mode` string setting: `hide` / `compact` / `normal`
  - Compact mode now uses WM_WINDOWPOSCHANGING in BBar subclass to clamp cx before mstsc applies it — authoritative intercept, cannot be fought
  - Only cx clamped; x/y left alone so bar remains freely draggable
  - During drag, SWP_NOSIZE is set so clamp is skipped and drag works normally
  - Auto-hide removed (untested, adds complexity)
  - Native buttons (minimize/restore/close) remain fully functional in compact mode
  - SetWindowPos_Hook simplified to hide-mode only

**Next:** Compile v1.6.0 in Windhawk → test compact mode on secondary monitor → confirm width clamp works without fighting

## 2026-05-15 — v1.7 through v1.9 rework

- v1.7.0: replaced compact approach with hide + custom edge strip (our own window). Full-height strip confirmed visible and working by operator.
- v1.8.x: reduced strip to 48×64px floating button. Introduced WM_NCHITTEST→HTCAPTION which broke DWM painting on WS_POPUP|WS_EX_LAYERED windows — button disappeared entirely.
- v1.9.0: clean rewrite based on proven v1.7/v1.8.0 window creation path
  - Removed HTCAPTION entirely (was breaking rendering and swallowing WM_LBUTTONDOWN)
  - Button: 120×40px, pinned to top of right or left edge, dark bg, blue top border, "✕ Disconnect" label in Segoe UI
  - pOrigShowWindow used directly to bypass hook, plus explicit pOrigSetWindowPos(HWND_TOPMOST) after show
  - bbarReady check in HelperThread startup handles race where BBar detected before thread message loop starts
  - Bool settings: hideBar, showButton, buttonOnRight

**Next:** Compile v1.9.0 → confirm button appears top-right of RDP monitor → test click-to-disconnect

## 2026-05-15 — v1.0.0 release prep

- Reached v1.0.0 milestone.
- Added DPI-aware sizing using `GetDpiForMonitor`. Scaled button dimensions (`BTN_W`, `BTN_H`), offsets, corner radiuses, and fonts according to the monitor hosting the RDP frame.
- Replaced `-lgdi32` with `-lgdi32 -lshcore` compiler options.
- Added visual feedback for hotkey registration conflicts: if `RegisterHotKey` fails, the disconnect button will render its label in bold red reading "✕  Hotkey Failed".
- Updated `README.md` to fully document the available settings array and reflect v1.0.0 capabilities.
- State files tracked and updated.

**Next:** Obtain GitHub token to push v1.0.0 → Operator live test in Windhawk (focusing on DPI scaling) → Optional marketplace PR.

