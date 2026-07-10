# Open Loops

## LOOP-001 — Optional: Windhawk Marketplace PR

Submission staged and pushed to fork branch `add-hide-rdp-connection-bar-v1.1.4`
(commit `e7b39b8`), mod file at `mods/hide-rdp-connection-bar.wh.cpp` (v1.1.4),
built off `upstream/main` (ramensoftware/windhawk-mods @ `ae3325a`). PR title/body
drafted per repo conventions. Validated with upstream `pr_validation.py` (3
informational GWLP_WNDPROC subclassing warnings only, no errors).

Pending operator: open the PR against ramensoftware/windhawk-mods
(authorship checkbox is an operator determination), and optionally delete the
stale `add-hide-rdp-connection-bar` branch from 2026-05-15.

- Status: open
- Gate: operator

---

## Closed

- **LOOP-002 — WSL→macOS migration remediation** — completed 2026-06-07. All `/mnt/e/` path references replaced with `/Users/dante/Citadel/` equivalents across AGENTS.md, raiden-orientation.md, fresh-install-handoff.md, and CURRENT_STATE.md. `commit-msg` hook execute bit set. Global scan confirmed clean. Committed `f14651f` and pushed.
