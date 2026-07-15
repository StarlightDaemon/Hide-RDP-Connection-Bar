# RAIDEN Orientation for Instance Agents

## Prompt ID

`raiden.instance.orientation.v1`

## Purpose

Use this prompt when starting an agent session in this repo that needs to understand
the RAIDEN control plane — what the `.raiden/` directory is, what each layer does,
what the updater toolchain does, and what rules apply to a local agent.

Not required for every session. Use it when the agent has no prior RAIDEN context
or when the task touches `.raiden/` files.

## Target Agent

Any agent operating in this RAIDEN Instance repo. Model-agnostic.

## Template

```text
You are operating inside a RAIDEN Instance repo. This briefing gives you the
context you need to work safely around the .raiden/ control plane.

---

## What RAIDEN Is

RAIDEN is a central toolkit/framework repo, not a running process. It authors
Edicts — versioned managed instruction packages — and installs them into
downstream repos called RAIDEN Instances via an updater toolchain. This repo
is a RAIDEN Instance. The .raiden/ directory is the local control plane
installed by that process.

RAIDEN is located at E:\Citadel/Raiden on this machine. You do not need to read
or interact with it unless the operator explicitly directs you to.

## Naming Canon

Use these terms exactly — do not paraphrase or coin alternatives:

- RAIDEN — the central framework repo and governing authority
- Edict — the versioned managed package RAIDEN authors and issues
- RAIDEN Instance — a downstream repo running the .raiden/ control plane (this repo)
- Writ — the installed managed-core payload; lives in .raiden/writ/

## Control Plane Layout

.raiden/
  writ/       — RAIDEN-managed core; do NOT edit; refreshed by Edict installs
  local/      — repo-local overlay; you may write here freely
  state/      — live continuity files; changes often; preserved during updates
  instance/   — install metadata written by the updater; do NOT edit

AGENTS.md at the repo root is the agent startup bridge — read it first.

## Three-Layer Model

1. Managed core (.raiden/writ/)
   Owned by RAIDEN. Files here are installed and updated by the central updater.
   Never manually rewrite them. If you need to change behavior governed by a writ
   file, ask the operator — the change must come through an Edict update from
   RAIDEN central.

2. Local overlay (.raiden/local/)
   Owned by this repo. Prompts, rules, repo-specific context, and exceptions live
   here. You may write here freely within normal task scope.

3. Live state (.raiden/state/)
   Fast-changing operational truth. CURRENT_STATE.md, OPEN_LOOPS.md, DECISIONS.md,
   GOALS.md, WORK_LOG.md. Update these as work progresses. They are preserved
   untouched during Edict updates.

## What the Updater Does

The updater (raiden_updater.cli) runs from RAIDEN central — not from this repo.
You do not invoke it here.

- plan  — read-only: shows what an Edict install/update would change; safe to run
- apply — writes new writ files into .raiden/writ/; only RAIDEN central runs this

The updater always preserves .raiden/local/ and .raiden/state/ on every update.
If a writ file has local edits, it stops and reports a conflict rather than
overwriting. Do not run raiden_updater.cli apply from within this repo.

## Hard Rules

- Do not edit any file inside .raiden/writ/
  Changes will be overwritten or flagged as conflicts on the next Edict update.

- Do not edit .raiden/instance/baseline.json or metadata.json
  These are written and maintained by the updater toolchain.

- Do not add Co-Authored-By or agent attribution lines to commit messages
  The commit-msg hook installed by RAIDEN enforces this at the git level.
  Do not remove or bypass it. Commits must carry only the operator's identity.

- The mainline branch must be named main — never master.

- To update the Writ, ask the operator to run the updater from RAIDEN central.
  Do not attempt it yourself.

## What You May Do Freely

- Read any file anywhere in .raiden/
- Write to .raiden/local/ (prompts, rules, context, exceptions)
- Write to .raiden/state/ (CURRENT_STATE.md, OPEN_LOOPS.md, DECISIONS.md,
  GOALS.md, WORK_LOG.md)
- Work on repo source files (hide-rdp-connection-bar.wh.cpp, README.md, etc.)
  as normal — RAIDEN does not govern those

## Read Order to Orient

1. AGENTS.md
2. .raiden/README.md
3. .raiden/state/CURRENT_STATE.md
4. .raiden/state/OPEN_LOOPS.md
5. .raiden/writ/OPERATING_RULES.md  (for the full rule set if needed)
```

## Notes

- This prompt covers RAIDEN framework orientation only. For session startup
  (git status, push flow, token request), use `instance-session-startup.md`.
- The RAIDEN central repo is at `E:\Citadel/Raiden`. The updater toolchain is at
  `toolkit/updater/` within that repo.
- Current installed Edict version: 0.4.0 (see `.raiden/instance/metadata.json`).
