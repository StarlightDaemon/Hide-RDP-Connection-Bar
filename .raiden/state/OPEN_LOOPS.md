# Open Loops

## LOOP-001 — Optional: Windhawk Marketplace PR

Submission prepared on fork `StarlightDaemon/windhawk-mods`, local branch
`add-hide-rdp-connection-bar` (commit `e7b39b8`), mod file staged at
`mods/hide-rdp-connection-bar.wh.cpp` (v1.1.4), built off current
`upstream/main` (ramensoftware/windhawk-mods @ `ae3325a`). PR title/body
drafted per repo conventions. Local `pr_validation.py` run clean (3
informational GWLP_WNDPROC subclassing warnings only, no errors).

Blocked on push: the fork already has a branch named
`add-hide-rdp-connection-bar` from a prior, never-PR'd staging attempt
(2026-05-15, mod v1.1.1, based on the fork's stale `main`). Publishing the
current commit requires either a force-push over that unauthored remote
branch or deleting/renaming it first — both need explicit operator
authorization before this loop can proceed to opening the PR.

- Status: open
- Gate: operator

---

## Closed

- **LOOP-002 — WSL→macOS migration remediation** — completed 2026-06-07. All `/mnt/e/` path references replaced with `/Users/dante/Citadel/` equivalents across AGENTS.md, raiden-orientation.md, fresh-install-handoff.md, and CURRENT_STATE.md. `commit-msg` hook execute bit set. Global scan confirmed clean. Committed `f14651f` and pushed.
