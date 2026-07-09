# Repository Audit — Hide RDP Connection Bar

- **Audited:** 2026-06-21
- **Commit:** `e6ab97e` (main, clean working tree, fully pushed to `origin/main`)
- **Instrument:** Repository Audit Instrument v4.2 (read-only)
- **Operating boundary:** `/Users/dante/Citadel/Hide RDP Connection Bar` — nothing read or written outside it; this report is the only file created.

---

## 0. Executive Summary

**Identity.** A single-file [Windhawk](https://windhawk.net/) mod (`hide-rdp-connection-bar.wh.cpp`, 765 lines, C++) that hides the floating Remote Desktop connection bar (`BBarWindowClass`) in fullscreen `mstsc.exe` sessions on Windows 11 and replaces it with an optional disconnect button. It is for Windows 11 RDP users.

**Stack composition.** Single ecosystem: **C++ / Win32** delivered as a Windhawk mod (compiled in-app, not via a local build system). No package manifests, no lockfiles, no IaC, no containers. A `.raiden/` governance/control-plane layer (Markdown + JSON state) accompanies the source.

**Maturity stage.** Mature and actively maintained. 30 commits over ~5 weeks (first commit 2026-05-15, last 2026-06-21). Source is at v1.1.4, operator live-tested at v1.1.1, all subsequent releases being threading/correctness fixes. Not a dormant repo.

**Finding counts by severity:** Critical 0 · High 0 · Medium 0 · Low 2 · Info 6.

**Repository size.** 33 working-tree files, 27 tracked. Not a large repo — this is an **exhaustive** audit, not a sampled one.

**Provenance — what was verified vs. read-only vs. not verified:**
- **Verified (executed):** `git fetch --all --prune` (exit 0, online); full git-history secret scan via bounded `git log -S` for `AKIA`/`sk_live`/`ghp_`/`*.env` (all matches confirmed to be doc pattern-text, not credentials); working-tree secret grep; env-file discovery; config-completeness cross-reference (declared settings vs. code reads); CI presence check (`gh run list` — no workflows); license inspection; declared-state reconciliation against `.raiden/state/`.
- **Read-only (inspected, not executed):** full source read; all state/governance docs; README/TESTING; `.gitignore`; tooling configs.
- **Not verified:** no automated test suite exists to run (Windows-only mod, cannot compile or execute on this macOS host); no SCA/IaC scanners present (`gitleaks`, `trufflehog`, `osv-scanner`, `checkov`, `trivy`, `cppcheck`, `clang-tidy` all absent — only `gh` and git available); no `timeout`/`gtimeout` (coreutils not installed). None of these gaps affect this stack materially — see §5/§7.

---

## 1. Identity

- **What it is:** a Windhawk mod that hooks four Win32 APIs inside `mstsc.exe` (`CreateWindowExW`, `ShowWindow`, `SetWindowPos`, `SetWindowTextW`) to suppress the RDP connection bar and render a DPI-aware disconnect overlay.
- **Who it's for:** Windows 11 Remote Desktop users for whom the native "hide connection bar" options don't persist.
- **Language/ecosystem:** C++ (Win32), `-lgdi32 -lshcore`, MIT-licensed.
- **Entry points:** Windhawk lifecycle hooks — `Wh_ModInit`, `Wh_ModSettingsChanged`, `Wh_ModUninit` (`hide-rdp-connection-bar.wh.cpp:690`, `:724`, `:749`).
- **How it runs/builds/deploys:** no local build system by design. Users paste the source into Windhawk → Compile mod. Distribution is the single `.wh.cpp` file; optional future path is a PR to `ramensoftware/windhawk-mods`.

---

## 2. Current State

- **Version:** 1.1.4 (consistent across `@version` header `:5`, init log string `:717`, README, and state docs).
- **Branch:** `main` (canonical; repo enforces `main`, not `master`).
- **Last commit:** `e6ab97e` 2026-06-21 "chore: update state docs — Edict v1.0.0, post-sweep reconciliation".
- **What works:** operator live-tested working on Windows 11 at v1.1.1; v1.1.3/v1.1.4 are threading/correctness fixes not separately field-tested (per TESTING.md:86-88).
- **Deployment status:** pushed and live at `StarlightDaemon/Hide-RDP-Connection-Bar`. Not yet published to the Windhawk Marketplace.
- **CI/CD pipeline health:** none configured — no `.github/`, `.gitlab-ci.yml`, or any pipeline definition. `gh run list` returns nothing. This is appropriate for a Windhawk mod (no buildable artifact in-repo); noted as Info, not a defect (F4).

**Declared-state reconciliation** (`.raiden/` governance layer is present and read):
- `CURRENT_STATE.md` accurately describes the v1.1.4 feature set, pushed status, and the 2026-06-21 maintenance sweep.
- Two minor drifts found — see F1 (stale HEAD reference) and F2 (stale date header).
- `OPEN_LOOPS.md` declares exactly one open loop (optional Marketplace PR, no blockers); reconciled in §4.

---

## 3. Git State & History

- **Working tree:** clean. `git status` shows only ignored entries (`.DS_Store`, `.claude/`, `.serena/`, `.raiden/local/MODEL_MAP.md`) — all intentionally gitignored.
- **Untracked files:** none requiring attention. (The `.audits/` directory holding this report is this audit's own output and is **excluded** from this finding per instrument rules — see F7 recommending it be gitignored.)
- **Staged / unpushed / unpulled:** none. `main` is level with `origin/main`.
- **Stashes:** none. **Branches:** only `main`, no divergence, no stale branches.
- **Commit cadence / dormancy:** 30 commits in the last 12 months, spanning 2026-05-15 → 2026-06-21, last commit today. Active, no dormancy signal.
- **Contributor distribution:** 21 commits `StarlightDaemon`, 5 `StarlightDaemon <…@users.noreply…>` (same GitHub ID), 4 `[redacted identity]` (placeholder email — see F3).

---

## 4. Open Loops

**Declared (from `.raiden/state/OPEN_LOOPS.md`):**
- *Optional: Windhawk Marketplace PR* — submit to `ramensoftware/windhawk-mods` when ready for public listing. Stated as having no blockers. **Status: genuinely open and untracked elsewhere** — consistent with reality; not yet done.

**Discovered (code/doc scan):**
- No `TODO`/`FIXME`/`HACK`/`XXX` markers in source code. The grep hits were checklist items in `TESTING.md`, a settings label (`XXXL`) in the source, and pattern-documentation in the `.raiden/writ/` audit protocols — none are real code debt.
- No declared-but-resolved items found. No discovered-but-untracked work items found.

---

## 5. Code Quality & Structure

- **Architecture:** clean and well-sectioned. Settings load → shared state under a `CRITICAL_SECTION` → four API hooks → a dedicated helper thread owning the overlay window and its message loop. Lifecycle (create/reposition/hide/destroy) is coherently routed through thread messages (`WM_CREATE_BTN`/`WM_HIDE_BTN`/`WM_REPAINT_BTN`).
- **Notable strengths:** GDI objects are paired with `DeleteObject`; the BBar subclass restores the original `WNDPROC` on `WM_DESTROY` and on uninit; the atom-vs-pointer guard on `lpClassName` (`:599`) is a real Win32 correctness safeguard (documented in `DECISIONS.md`); cross-thread invariants (`g_hLastMonitor`, `g_hotkeyRegistered`) use `std::atomic`.
- **Documented deliberate data race (F6, Info):** the scalar settings globals (`g_hideBar` … `g_hotkeyVk`) are written by Windhawk's settings thread and read by hooks/helper thread without synchronization. The rationale is documented in-source (`:120-130`): word-sized aligned access is atomic on x86/x64, no cross-field invariant exists, and worst case is a one-repaint-late setting. This is a conscious, reasonable trade-off — recorded as context, not a defect.
- **Doc state:** README, in-source WindhawkModReadme/Settings, and TESTING.md are accurate and match the code. The README settings table (11 settings) matches the declared settings block and the values read in `LoadSettings()` exactly (config-completeness: **verified clean**, no undocumented/stale keys).
- **Test-coverage state & confidence:** no automated tests exist, and none can run on this host (Windows-only, compiled inside Windhawk). `TESTING.md` is a manual verification protocol. **Confidence: not applicable** — this is an inherent property of the Windhawk-mod stack, not a coverage gap to flag against a normal source-to-test ratio (F5, Info).

---

## 6. Security & Compliance

- **Secrets — working tree:** none. All grep matches for `token`/`secret`/`key`/`password` are documentation references inside `.raiden/` protocol files (e.g. session-token request flow, audit pattern lists). No values to redact.
- **Secrets — git history:** none. Bounded `git log --all -S` scans for `AKIA`, `sk_live`, `ghp_`, and `*.env` were run. The only `ghp_`/`AKIA`/`sk_live` hits trace to commit `2b8bc23`, which added `.raiden/writ/WORKSPACE_AUDIT_PROTOCOL.md` containing those strings as **literal pattern documentation** ("GitHub `ghp_*`/`github_pat_*`…"), not credentials. Confirmed by inspecting only that commit's diff. No credential ever entered history.
- **Env files:** none present on disk or tracked. No leak-risk classification applies.
- **Config completeness:** verified — every setting the code reads (`LoadSettings()`) is declared in the WindhawkModSettings block and documented in the README; no undocumented-required or stale-example keys.
- **IaC / containers:** none present (no Terraform, K8s, Helm, Dockerfile, etc.). Section non-applicable; structural check confirmed absence.
- **Licensing:** `LICENSE` present, MIT, consistent with `@license MIT` in source (`:10`) and the README badge/section. No vendored third-party code, no `vendor/`/`third_party/` trees, no bundled libraries — no provenance ambiguity. Clean.

---

## 7. Dependencies & Tooling

- **Application dependencies:** none beyond the Windows SDK / Win32 (`gdi32`, `shcore`) and Windhawk's own runtime. No package manifest, no lockfile, nothing to mark outdated or run SCA against — by design for a single-file mod.
- **Shadow / OS-level dependencies:** none. No Makefile/CMake/shell build script invoking external binaries; the only "build" is Windhawk's in-app compiler.
- **Tooling available on host:** `gh` (authenticated) and git only. SCA/IaC/C++-static-analysis tools (`gitleaks`, `trufflehog`, `osv-scanner`, `checkov`, `trivy`, `cppcheck`, `clang-tidy`) and `timeout`/`gtimeout` are all absent — recorded so the not-verified items in §0 are attributable. None are required for this stack, but `cppcheck`/`clang-tidy` would be the natural way to add static verification if desired (observation, not a prescription).
- **Deprecations:** none applicable (no pinned actions, no runtimes to age out).

---

## 8. Oddities

- **Self-referential state drift (F1):** `CURRENT_STATE.md:24` records "HEAD `03153db`", but actual HEAD is `e6ab97e` — the state-doc-update commit didn't update its own HEAD reference. Cosmetic.
- **Stale date header (F2):** `CURRENT_STATE.md:3` reads "As of: 2026-06-07", yet line 17 of the same file documents a 2026-06-21 sweep. The header wasn't bumped when the sweep was recorded.
- **Placeholder author email in history (F3):** 4 historical commits are authored as `[redacted identity]`. The 2026-06-21 sweep (`CURRENT_STATE.md:17`) notes "git identity" was corrected going forward; the historical commits remain and can only be changed by a history rewrite.
- No scratch files, leftover binaries, or oversized artifacts. Largest non-`.git` file is the source itself (28 KB); `.DS_Store` (6 KB) is present but gitignored.

---

## 9. Findings Index

| ID | Severity | Effort | Blast Radius | Location | Finding | Cross-ref |
|----|----------|--------|--------------|----------|---------|-----------|
| F1 | Info | Trivial | Localized | `.raiden/state/CURRENT_STATE.md:24` | Declared HEAD `03153db` is stale; actual HEAD is `e6ab97e`. | — |
| F2 | Low | Trivial | Localized | `.raiden/state/CURRENT_STATE.md:3` | "As of: 2026-06-07" header contradicts the 2026-06-21 sweep recorded at line 17. | — |
| F3 | Info | Sprawling | Cross-cutting | git history (4 commits) | Commits authored as placeholder `[redacted identity]`; only fixable via history rewrite. | — |
| F4 | Info | n/a | Localized | repo root | No CI/CD pipeline configured. Expected for a Windhawk in-app-compiled mod; noted, no action implied. | — |
| F5 | Info | n/a | Cross-cutting | repo (Windows-only) | No automated test suite; `TESTING.md` manual protocol only. Cannot be run/verified on this host. | — |
| F6 | Info | n/a | Cross-cutting | `hide-rdp-connection-bar.wh.cpp:120-143` | Deliberate, documented data race on scalar settings globals (x86/x64 word-atomic rationale). | DECISIONS.md |
| F7 | Info | Trivial | Localized | `.gitignore` | This audit writes to `.audits/`; operator may wish to gitignore `.audits/` so reports stay untracked. (Not edited by this audit.) | — |
| F8 | Low | Bounded | Localized | `.raiden/state/OPEN_LOOPS.md:3-5` | One genuinely-open declared loop: optional Windhawk Marketplace PR (no blockers). Listed for completeness, not a defect. | OPEN_LOOPS |

---

*End of report. No roadmap, prioritization, or remediation order is included by design — triage belongs to the downstream planning layer.*
