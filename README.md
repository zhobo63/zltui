# ZLTUI

ZLTUI is a lightweight C++ terminal UI library for building ANSI-based text interfaces. It provides basic windowing, layout, text rendering, borders, colors, sliders, buttons, and simple event handling on top of a double-buffered terminal renderer.

## Features

- Cross-platform terminal abstraction for Windows and Unix-like systems
- UTF-8 text rendering with wide-character width handling
- ANSI color, style, border, and cursor helpers
- Widget tree with `Win`, `Label`, `Button`, `Check`, `Slider`, and `Edit`
- Docking and arrangement support for nested UI layouts
- Example application in `sample/hello/main.cpp`

## Repository Layout

- `include/zltui.h` - public API
- `src/zltui.cpp` - implementation
- `sample/hello/main.cpp` - sample application
- `doc/` - design notes and TODOs

## Requirements

- CMake 3.14 or newer
- A C++17-compatible compiler

## Build

```bash
cmake -S . -B build
cmake --build build
```

The build produces:

- `zltui` static library
- `hello` sample executable

## Example

```cpp
#include "zltui.h"

int main() {
    TUI::Terminal terminal;
    TUI::Mgr mgr;

    mgr.Parse(u8R"(
Object Label
{
    Name title
    Rect 1 1 30 3
    Text Hello, ZLTUI
    Bold true
}
)");

    while (true) {
        if (mgr.Update(terminal)) {
            auto& buf = terminal.GetDrawBuffer();
            buf.clear();
            mgr.Paint(buf);
            terminal.Render();
        }
        Sleep(10);
    }
}
```

## Notes

- The library uses ANSI escape sequences for rendering.
- The DSL parser used by `Mgr::Parse()` is shown in `sample/hello/main.cpp`.
- `doc/TODO.md` tracks known missing pieces and future work.
