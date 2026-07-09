# Work Log

## 2026-05-15 — Initial scaffold

- Created `hide-rdp-connection-bar.wh.cpp`: hooks `CreateWindowExW` in `mstsc.exe`, hides `BBarWindowClass` via `ShowWindow(hwnd, SW_HIDE)` with atom guard (`> 0xFFFF`) protecting against non-pointer `lpClassName` values
- Created `README.md`, `LICENSE` (MIT), `.gitignore`
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

## 2026-05-15 — v1.0.0 pushed, post-release polish

- v1.0.0 committed and pushed to `StarlightDaemon/Hide-RDP-Connection-Bar` (main)
- v1.0.1: narrowed disconnect button from 120px to 80px base width following operator feedback
- Wording pass: softened "native settings are broken" to "may not persist reliably"; replaced "BBar connection bar" with "Remote Desktop connection bar" in all user-facing text; broadened platform references from "Windows 11 24H2" to "Windows 11"

## 2026-05-15 — v1.1.x: hostname fix, full border

- v1.1.0: added `SetWindowTextW` hook — BBar was being created before mstsc set the window title, so `UpdateHostname()` always read "Remote Desktop Connection". Hook now detects title changes on `g_hRdpFrame`, re-parses the hostname, and repaints the button in real time. Also handles reconnect-to-different-host.
- v1.1.1: added full border outline (left/right/bottom 2px edges in accent colour). Controlled by new `showBorder` toggle (default on). Compiler options, four hooks, all 11 settings documented in README and WindhawkModReadme.
- Operator live test completed — mod confirmed working. All changes committed and pushed.

**Status:** v1.1.1 live on GitHub. Optional next step: Windhawk Marketplace PR.

## 2026-05-15 — Pre-submission review fixes

External code review surfaced three issues; all fixed and pushed as commit `3b18b83`:

1. **`@license MIT` missing from metadata block** — added to satisfy catalog submission requirements.
2. **`BBarSubclassProc` WM_DESTROY not forwarded** — `g_origBBarWndProc` was zeroed before the return check, so the original wndproc never received WM_DESTROY. Fixed by capturing `origProc` in a local at the top of the function under lock, then using the local for both the `SetWindowLongPtrW` restore and the final `CallWindowProcW`.
3. **`g_hostname` cross-thread data race** — `UpdateHostname()` wrote from the hook thread while `WM_PAINT` read from the helper thread with no synchronization. Fixed by wrapping the write in `UpdateHostname` with `g_cs`, and copying to a stack-local in `WM_PAINT` under `g_cs` before drawing.

**Status:** All three fixes live on `main`. Repo is submission-ready for Windhawk Marketplace.

## 2026-05-31 — v1.1.3 threading and correctness pass; repo cleanup

Full-scope review of the codebase identified two remaining correctness issues and several repo hygiene items. All corrected in this session:

**Code fixes (v1.1.3 — will require a version bump to v1.1.4 before next push):**
1. `g_hotkeyRegistered` promoted from `bool` to `std::atomic<bool>` — it was written by the helper thread (`CreateOrRepositionButton`) and read by the main thread (`WM_PAINT`) with no synchronization. All other shared cross-thread state was already protected; this was the only gap.
2. `g_origBBarWndProc` write moved inside `g_cs` — `SetWindowLongPtrW` is called outside the lock (correct, to avoid re-entrancy), but its return value is now assigned to `g_origBBarWndProc` inside the critical section alongside `g_hBBar` and `g_hRdpFrame`, consistent with how that value is read in `BBarSubclassProc`.

**README fixes:**
3. Removed the "Via Windhawk Marketplace" installation section — the mod has not been submitted yet; the section was incorrect.
4. Removed the redundant "Button requires reconnect" Known Limitation bullet — fully covered by the adjacent "Some settings require reconnect" bullet.

**File cleanup:**
5. Deleted `AGENT_BUGFIX_PROMPT.md` — all eight bugs it described were fixed in v1.1.3; file was spent.
6. Deleted `REVIEW_BRIEF.md` — stale pre-v1.0.0 document; described DPI and hotkey feedback as unimplemented (both shipped in v1.0.0).

**Open loop remaining:** Windhawk Marketplace PR to `ramensoftware/windhawk-mods` — no blockers.

## 2026-07-09 — Edict v2.0.0 + state normalization

- Applied RAIDEN Edict v2.0.0 (upgrade from v1.0.1): updated `README.md`,
  `OPERATING_RULES.md`, `WORKSPACE_AUDIT_PROTOCOL.md`, `FORK_REVIEW_PROTOCOL.md`,
  `AGENTS.md`; added `ROUTING_POLICY.md`; removed managed `MODEL_TIERS.md`
  (absent from the new package, flagged `managed_file_removal`, confirmed safe
  to remove). Hook `commit-msg` unchanged. Re-plan confirmed "Already up to
  date."
- Stamped `state_schema_version: 2` into `.raiden/instance/metadata.json`.
- Replaced the local routing overlay: removed `.raiden/local/MODEL_MAP.md`
  (untracked/gitignored, never committed) and added `.raiden/local/ROUTING.md`
  (ladder R1–R4 plus an offload pool and billing constraints citing
  `Raiden-ops:OPS-D-003`). Updated `.raiden/local/.gitignore` from
  `MODEL_MAP.md` to `ROUTING.md` so the overlay stays out of this public repo.
- State normalization per the new `.raiden/writ/OPERATING_RULES.md` Fact-Home
  Rule:
  - Removed the hand-written "As of: 2026-06-07" footer from
    `CURRENT_STATE.md`.
  - Relocated historical narrative out of `CURRENT_STATE.md` that was not
    previously recorded here:
    - 2026-06-07 — Edict v1.0.0 installed by RAIDEN central.
    - 2026-06-07 — Edict v1.0.0 confirmed clean, per migration audit.
    - 2026-06-21 — Maintenance sweep: audit findings F1–F9 resolved (git
      identity, unpushed commit, `.gitignore`, threading annotation,
      `TESTING.md`, README badge, state doc reconciliation).
  - Deleted the stale current-version claim "Control plane: `.raiden/` —
    standard v1.0.0 layout" from `CURRENT_STATE.md` (superseded by v2.0.0, no
    unique historical content to preserve).
  - Replaced the WSL→macOS migration restatement in `CURRENT_STATE.md` with a
    bare citation to `LOOP-002` — the full account already lives in
    `OPEN_LOOPS.md`. Assigned loop IDs `LOOP-001` (Windhawk Marketplace PR) and
    `LOOP-002` (WSL→macOS migration remediation) in `OPEN_LOOPS.md` to make the
    citation resolvable.

