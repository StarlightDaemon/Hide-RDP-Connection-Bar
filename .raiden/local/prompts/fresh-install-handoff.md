You are the Hide RDP Connection Bar Instance agent, operating inside /Users/dante/Citadel/Hide RDP Connection Bar (or the repo root wherever this repo is checked out).

Read first:
- AGENTS.md
- .raiden/README.md
- .raiden/state/CURRENT_STATE.md
- .raiden/writ/WORKSPACE_AUDIT_PROTOCOL.md

Current objective:
Verify and commit the Edict v0.4.0 fresh install that RAIDEN central performed on 2026-05-15.
No new writes are needed — RAIDEN central completed all file operations; your task is verification
and commit only.

Known constraints:
- Do not modify any managed file in .raiden/writ/
- Do not modify CURRENT_STATE.md, OPEN_LOOPS.md, DECISIONS.md, or WORK_LOG.md
- Do not push without explicit operator confirmation
- No Co-Authored-By or agent attribution lines in the commit message
- Do not run raiden_updater.cli apply — use plan only

Already true (RAIDEN central wrote these on 2026-05-15):
- .raiden/writ/ — Edict v0.4.0 writ installed (README.md, OPERATING_RULES.md,
  OWNERSHIP_BOUNDARY.md, WORKSPACE_AUDIT_PROTOCOL.md)
- .raiden/instance/baseline.json — written by install (records file hashes + version 0.4.0)
- .raiden/instance/metadata.json — installed_edict_version: "0.4.0"
- .raiden/state/CURRENT_STATE.md — populated with repo context by RAIDEN central
- .raiden/state/GOALS.md — populated with initial goals by RAIDEN central
- .raiden/local/prompts/instance-session-startup.md — standard v1 prompt
- This file (.raiden/local/prompts/fresh-install-handoff.md) — written by RAIDEN central
- .git/hooks/commit-msg — installed by updater

Verification steps:
1. Run `git status --porcelain` — confirm only RAIDEN install files appear as untracked.
   Any unexpected modifications: stop and surface to operator before proceeding.
2. Run `grep installed_edict_version .raiden/instance/metadata.json`
   → expected: "0.4.0"
3. Run from /Users/dante/Citadel/Raiden/toolkit/updater/ (RAIDEN central — ask operator to run if not in that env):
     python3 -m raiden_updater.cli plan \
       --instance "/Users/dante/Citadel/Hide RDP Connection Bar" \
       --package /Users/dante/Citadel/Raiden/toolkit/updater/fixtures/sample_package
   → expected: Block reason: Already up to date — no changes needed
   If any other result: stop and surface to operator.
4. Stage and commit all RAIDEN install files:
     AGENTS.md
     .raiden/README.md
     .raiden/instance/baseline.json
     .raiden/instance/metadata.json
     .raiden/local/README.md
     .raiden/local/prompts/fresh-install-handoff.md
     .raiden/local/prompts/instance-session-startup.md
     .raiden/state/CURRENT_STATE.md
     .raiden/state/DECISIONS.md
     .raiden/state/GOALS.md
     .raiden/state/LEGACY_REVIEW.md
     .raiden/state/OPEN_LOOPS.md
     .raiden/state/README.md
     .raiden/state/WORK_LOG.md
     .raiden/writ/OPERATING_RULES.md
     .raiden/writ/OWNERSHIP_BOUNDARY.md
     .raiden/writ/README.md
     .raiden/writ/WORKSPACE_AUDIT_PROTOCOL.md
   Suggested commit message:
     "install: RAIDEN Edict v0.4.0 fresh install"
5. Run `git status --porcelain` after commit — confirm clean.

Do not:
- Reopen settled naming or architecture
- Treat review artifacts as canon unless adopted
- Broaden the task beyond committing the files listed above
- Run the workspace audit itself

Close out with:
- result: commit SHA
- evidence checked: git diff output, plan validator output, version grep result
- remaining risks: none expected; surface any anomaly observed
