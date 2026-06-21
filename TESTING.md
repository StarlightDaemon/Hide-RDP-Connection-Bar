# Manual Testing Protocol

Automated testing is not feasible for this project. The mod is a Windhawk-injected
Win32 GUI hook that operates inside a live `mstsc.exe` process on Windows 11. All
verification is performed manually on a Windows 11 host with Windhawk installed and
an active fullscreen RDP session running.

---

## Prerequisites

- Windhawk installed and running on Windows 11
- An active fullscreen RDP session in `mstsc.exe`
- Access to the Windhawk settings panel for this mod

---

## Core Behavior

Verify after each fresh install or mod update.

- [ ] The native RDP connection bar (`BBarWindowClass`) is hidden in fullscreen
- [ ] The mod enables without visible artifacts or host-process instability
- [ ] The mod disables (uninit path) cleanly and restores the native bar

---

## Disconnect Button

Enable the disconnect button in Windhawk settings before running these checks.

**Positioning**

- [ ] Button appears correctly at each of the four corners (Top Right, Top Left, Bottom Right, Bottom Left)
- [ ] Each of the 7 offset presets (None, Small, Medium, Large, XL, XXL, XXXL) positions the button as expected
- [ ] A non-zero custom offset value moves the button to the correct position

**Appearance**

- [ ] Full-border style renders correctly (2 px accent-colour outline on all sides)
- [ ] Top-accent-only style renders correctly (border disabled in settings)
- [ ] Hostname display reflects the active RDP session hostname
- [ ] Hostname updates if the session hostname changes (mstsc title update)

**Interaction**

- [ ] Clicking the button disconnects the session
- [ ] Idle fade activates after approximately 4 seconds with no mouse activity
- [ ] Button returns to full opacity on mouse proximity after fade

---

## Hotkey

Enable the disconnect hotkey in Windhawk settings before running these checks.

- [ ] Configured hotkey triggers a disconnect
- [ ] On registration failure, the button displays the "Hotkey Failed" indicator
- [ ] Hotkey is unregistered cleanly on mod uninit with no lingering conflicts

---

## DPI and Multi-Monitor

- [ ] Button size, offsets, fonts, and corner radius scale correctly at 150% display scaling
- [ ] Button size, offsets, fonts, and corner radius scale correctly at 200% display scaling
- [ ] On a multi-monitor setup, the button follows the `mstsc.exe` window to the correct monitor

---

## Known Limitations

Verify these are still documented behavior, not regressions.

- [ ] A brief Ctrl+Alt+Home flash may appear on session start (cosmetic, not a regression)
- [ ] Settings changes require a reconnect to take effect (button/hotkey only; hide/fade/hostname/border update live)
- [ ] Behavior on non-Windows-11 hosts is untested
- [ ] Hotkey conflicts with other registered hotkeys are the operator's responsibility

---

## Live Test Log

| Version | Date | Tester | Outcome |
|---------|------|--------|---------|
| v1.1.1  | 2026-05-15 (approx.) | operator | Pass — confirmed working on Windows 11 |
| v1.1.3  | — | — | Threading and correctness fix; not separately live-tested |
| v1.1.4  | — | — | Threading and correctness fix; not separately live-tested |
