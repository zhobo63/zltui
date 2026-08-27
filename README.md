# ZLTUI

ZLTUI is a lightweight C++ terminal UI library for building ANSI-based text interfaces. It provides basic windowing, layout, text rendering, borders, colors, sliders, buttons, and simple event handling on top of a double-buffered terminal renderer.

## Features

- Cross-platform terminal abstraction for Windows and Unix-like systems
- UTF-8 text rendering with wide-character width handling
- ANSI color, style, border, and cursor helpers
- Widget tree with `Win`, `Label`, `Button`, `Check`, `Slider`, `Edit`, and `RichEdit`
- Rich text rendering with per-character foreground/background colors, bold, italic, and underline styles
- Markdown rendering with headings, styled heading rules, inline emphasis, color tags, block quotes, lists, tables, horizontal rules, and fenced code blocks
- Configurable `MarkdownStyle` themes and text/emoji replacements
- UTF-8 copy-to-clipboard support through Windows `CF_UNICODETEXT`
- Docking and arrangement support for nested UI layouts
- Example application in `sample/hello/main.cpp`

## Repository Layout

- `include/zltui.h` - public API
- `src/zltui.cpp` - implementation
- `sample/hello/main.cpp` - sample application and RichText/Markdown examples
- `doc/dsl.md` - DSL syntax, layout, and widget configuration
- `doc/richedit.md` - RichEdit, RichText styles, and Markdown usage
- `doc/TODO.md` - design notes and TODOs

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

### Basic widget

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

### RichText

`RichText` stores a style for each decoded character. `RichEdit` combines it with the editable, scrollable behavior of `Edit`.

```cpp
TUI::RichEdit* edit = new TUI::RichEdit(&mgr);
edit->local = { 0, 0, 70, 10 };

TUI::RichText::Style normal =
    TUI::RichText::RichTextStyle(TUI::AnsiColor_White);
TUI::RichText::Style success =
    TUI::RichText::RichTextStyle(TUI::AnsiColor_Bright_Green,
                                 TUI::AnsiColor_Unused,
                                 true);

edit->appendText("Build: ", normal);
edit->appendText("success", success);
```

`RichText::Style` supports:

- `fg_color`
- `bg_color` (`AnsiColor_Unused` means keep the existing background)
- `bold`
- `italic`
- `underline`

### Markdown

````cpp
std::string md = R"(
# Build report

- Build: **success**
- Tests: *running*

> This is a quote.

| File | Status |
|---|---|
| main.cpp | **OK** |
| test.cpp | failed |

---

```cpp
RichText::Style style;
style.bold = true;
```
)";

TUI::Markdown(edit, md);
````

Supported Markdown features include:

- `#` through `######` headings with configurable colors and rules;
- `**bold**`, `__bold__`, `*italic*`, and `_italic_`;
- `<fg=Red>colored text</fg>` and `<bg=Blue>highlighted text</bg>`;
- `>` block quotes, including nested quotes;
- unordered list marker replacement (`- ` becomes `● ` in the default theme);
- tables with Unicode borders and per-column width calculation;
- `---`, `***`, and `___` horizontal rules;
- fenced code blocks using triple backticks;
- configurable emoji/text replacements.

`MarkdownStyle::DEFAULT` provides the default theme. A custom theme can be passed to the overload:

```cpp
TUI::MarkdownStyle style = TUI::MarkdownStyle::DEFAULT;
style.heading[0] = TUI::RichText::RichTextStyle(
    TUI::Color(140, 190, 255),
    TUI::AnsiColor_Unused,
    true);
style.quote_prefix = u8"│ ";
style.horizontal_rule_char = u8"═";

TUI::Markdown(edit, md, style);
```

Markdown parsing is performed into RichText spans/character styles; fenced code blocks are emitted literally and do not parse inline Markdown tags.

For the complete RichEdit API, DSL commands, style options, and Markdown details, see [`doc/richedit.md`](doc/richedit.md) and [`doc/dsl.md`](doc/dsl.md).

## Documentation

- [DSL reference](doc/dsl.md) - object declarations, layout, colors, scrolling, and widget properties.
- [RichEdit guide](doc/richedit.md) - editable rich text, per-character styles, and Markdown rendering.
- [TODO list](doc/TODO.md) - known limitations and planned work.

## Notes

- The library uses ANSI escape sequences for rendering.
- Clear the active draw buffer before painting each frame:
  ```cpp
  auto& buf = terminal.GetDrawBuffer();
  buf.clear();
  mgr.Paint(buf);
  terminal.Render();
  ```
  This is important for clearing old wide characters when text becomes shorter.
- Text indices are Unicode character indices, while the underlying `std::string` is UTF-8.
- The DSL parser used by `Mgr::Parse()` is shown in `sample/hello/main.cpp`.
- `doc/TODO.md` tracks known missing pieces and future work.
