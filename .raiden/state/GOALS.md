# Goals

- Maintain a working Windhawk mod that reliably hides the RDP connection bar on Windows 11
- Keep the implementation minimal — single `.wh.cpp` file, no external dependencies
- Track any Windows or mstsc.exe changes that break the `BBarWindowClass` hook
