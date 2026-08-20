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
- [ ] Toggling "Show disconnect button" off and back on several times in quick succession, with an active RDP session, does not crash or leave the button in a broken state (exercises helper thread teardown/recreation)

**Drag-to-Reposition**

- [ ] A short click on the button still disconnects the session and does not move the button; a press-and-move past the system drag threshold starts a live-following drag instead and does not disconnect on release
- [ ] Dragging the button to each of the four screen corners and dropping it leaves it at the dropped position, not snapped back to a default
- [ ] Dropping the button very close to a screen edge, including near the taskbar if one is visible, leaves the full button on screen rather than clipped or partially off-monitor
- [ ] Disconnecting after a drag, then reconnecting, shows the button at the previously dragged position rather than the settings-derived default
- [ ] With a dragged position active, changing the Button position, Corner offset, or Custom offset setting in the Windhawk UI resets the button back to the new settings-derived default rather than leaving it at the old dragged spot
- [ ] A drag-and-reconnect check at a non-100% display scale (e.g. 150%) lands the dragged offset correctly rather than scaled on the wrong axis
- [ ] On a multi-monitor setup, dragging the button on one monitor, then reconnecting so the session appears on a different monitor with a different resolution, leaves the button in a sensible, fully on-screen position
- [ ] Switching away from the RDP window mid-drag (e.g. Alt+Tab) leaves the button in a sane, finalized position afterward rather than stuck following a cursor that is no longer tracked
- [ ] The cursor visibly changes while actively dragging (size-all) versus while idle over the button (hand)

---

## Taskbar Thumbnail Toolbar

Enable the disconnect button (`showButton`) before running these checks — the
taskbar controls ship as part of the same setting. Unlike the rest of this
protocol, the windowed-session checks here require a session that was started
windowed and never entered fullscreen.

- [ ] Hovering the mstsc taskbar icon shows Minimize / Restore / Disconnect buttons under the thumbnail for a fullscreen session
- [ ] The same three buttons appear for a windowed session that never entered fullscreen (no connection bar ever existed)
- [ ] Minimize button minimizes the session; Restore button restores it; Disconnect button disconnects it
- [ ] While the session is minimized, Minimize is disabled (dimmed) and Restore is enabled; while not minimized, the reverse — including after external transitions (minimize by clicking the taskbar icon, restore by clicking the thumbnail), verified by hovering again
- [ ] Kill and restart `explorer.exe`, then hover the taskbar icon again — the three buttons reappear and still work
- [ ] Glyphs are legible against the flyout background in both Windows light and dark taskbar themes
- [ ] With the toolbar active, bar hiding and the overlay button behave exactly as before in a fullscreen session
- [ ] Disabling the mod (uninit) with a session running leaves mstsc stable; the thumbnail buttons disappear (hidden) and no longer act

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
