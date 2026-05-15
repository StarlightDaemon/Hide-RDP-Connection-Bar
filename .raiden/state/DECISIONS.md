# Decisions

## 2026-05-15 — Hook on CreateWindowExW, hide after creation

Hide the BBar after `CreateWindowExW` returns rather than blocking the call. Blocking risks crashing `mstsc.exe` if the bar is used internally; post-creation hide is safe and sufficient.

## 2026-05-15 — Atom guard on lpClassName

`lstrcmpW` is guarded with `reinterpret_cast<ULONG_PTR>(lpClassName) > 0xFFFF` before use. Win32 allows `lpClassName` to carry a packed 16-bit atom instead of a real pointer; calling `lstrcmpW` on an atom value reads from an unmapped address and crashes the process.

