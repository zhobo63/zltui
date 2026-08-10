# RichEdit

`RichEdit` 是可編輯的 RichText 元件，結合文字樣式、游標、選取、鍵盤輸入、滑鼠操作與 Slider 捲動功能。

## 繼承結構

```text
RichEdit
├── Slider
│   └── Win
└── RichText
    └── Text
```

宣告如下：

```cpp
struct RichEdit : Slider, RichText
{
    RichEdit(Mgr* mgr);

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void CalRect(Win* parent) override;
    void Paint(DrawBuffer& drawbuf) override;
    void PaintText(DrawBuffer& drawbuf);
    void setText(const std::string& text);
    void Event(const TUI::Event& ev) override;
    void Copy(const Win* ob) override;
    Win* Clone() const override;

    Point cursor;
    int drag_start = -1;
    bool readonly = false;
    std::function<bool(const TUI::Event& evt)> on_key;
    RichText::Style current_style;
};
```

`RichText` 不繼承 `Win`，因此 `RichEdit : Slider, RichText` 不會產生 `Win` 菱形繼承。

## 建立 RichEdit

可以直接建立並加入容器：

```cpp
TUI::RichEdit* edit = new TUI::RichEdit(&mgr);
edit->local = { 0, 0, 70, 10 };
chatArea->AddChild(TUI::WinPtr(edit));
```

也可以透過 DSL 建立：

```text
Object RichEdit {
    Name output
    Rect 0 0 70 10
    DrawBorder true
    ScrollY true
}
```

`Mgr::Create()` 已支援：

```text
RichEdit
```

## RichText 樣式

每一個 Unicode 字元都有對應的 `RichText::Style`：

```cpp
struct RichText::Style {
    Color fg_color = AnsiColor_White;
    Color bg_color = AnsiColor_Unused;
    bool bold = false;
    bool italic = false;
    bool underline = false;
};
```

`AnsiColor_Unused` 表示不覆蓋原本的背景色。

可以使用樣式工廠建立樣式：

```cpp
auto normal = TUI::RichText::RichTextStyle(
    TUI::AnsiColor_White);

auto success = TUI::RichText::RichTextStyle(
    TUI::AnsiColor_Bright_Green,
    TUI::AnsiColor_Unused,
    true);

auto warning = TUI::RichText::RichTextStyle(
    TUI::Color(255, 220, 80),
    TUI::AnsiColor_Unused,
    true,
    false,
    true);
```

完整工廠介面：

```cpp
static Style RichTextStyle(
    const Color& fg,
    const Color& bg = AnsiColor_Unused,
    bool bold = false,
    bool italic = false,
    bool underline = false);
```

## 使用 `appendText()`

`appendText()` 會將文字追加到目前內容，並讓新增的每個字元使用指定樣式：

```cpp
edit->appendText("Build: ", normal);
edit->appendText("success", success);
edit->appendText(" warning", warning);
```

例如：

```cpp
TUI::RichText::Style normal =
    TUI::RichText::RichTextStyle(TUI::AnsiColor_White);
TUI::RichText::Style bold =
    TUI::RichText::RichTextStyle(
        TUI::Color(255, 255, 255),
        TUI::AnsiColor_Unused,
        true);
TUI::RichText::Style italic =
    TUI::RichText::RichTextStyle(
        TUI::Color(220, 90, 90),
        TUI::AnsiColor_Unused,
        false,
        true);

edit->appendText("Normal ", normal);
edit->appendText("bold ", bold);
edit->appendText("italic", italic);
```

## 設定文字

```cpp
edit->setText("Initial text");
```

`setText()` 會：

- 重新設定文字內容；
- 依目前元件寬度重新排版；
- 建立預設 RichText styles；
- 清除選取；
- 將游標重設到起點；
- 標記 Manager 為 dirty。

若要以多段樣式建立內容，應使用：

```cpp
edit->setText("");
edit->appendText("first", style1);
edit->appendText("second", style2);
```

## DSL

RichEdit 支援以下文字相關命令：

### `Text`

設定或取代整段文字：

```text
Text "Build report"
```

### `Style`

設定後續 `AppendText` 使用的目前樣式：

```text
Style Red bold italic
AppendText "error"
```

格式：

```text
Style <fgcolor> [<bgcolor>] [bold] [italic] [underline]
```

範例：

```text
Text ""

Style BrightWhite
AppendText "Build: "

Style BrightGreen bold
AppendText "success"

Style White Red bold
AppendText " error"
```

未指定的樣式旗標會是 `false`。背景色可以使用 `Unused` 表示不填背景：

```text
Style Yellow Unused italic
AppendText "warning"
```

### `AppendText`

以目前的 `Style` 追加文字：

```text
AppendText "additional text"
```

## Markdown

`Markdown()` 可以將 Markdown 內容轉換成 RichText：

```cpp
TUI::Markdown(edit, md);
```

目前支援：

- `#` 到 `######` 標題；
- 各階層標題顏色與分隔線；
- `**bold**`、`__bold__`；
- `*italic*`、`_italic_`；
- `<fg=Red>text</fg>`；
- `<bg=Blue>text</bg>`；
- `>` 區塊引用與巢狀引用；
- `- ` 無序列表 marker replacement；
- `| ... |` 表格；
- `---`、`***`、`___` 水平線；
- ````` fenced code block；
- emoji 與文字 replacement。

範例：

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

### 自訂 MarkdownStyle

```cpp
TUI::MarkdownStyle style = TUI::MarkdownStyle::DEFAULT;

style.text = TUI::RichText::RichTextStyle(
    TUI::Color(150, 150, 150));
style.bold_text = TUI::RichText::RichTextStyle(
    TUI::Color(255, 255, 255),
    TUI::AnsiColor_Unused,
    true);
style.italic_text = TUI::RichText::RichTextStyle(
    TUI::Color(220, 90, 90),
    TUI::AnsiColor_Unused,
    false,
    true);

style.heading[0] = TUI::RichText::RichTextStyle(
    TUI::Color(140, 190, 255),
    TUI::AnsiColor_Unused,
    true);
style.heading_rule_char[0] = u8"═";
style.heading_rule_enabled[3] = false;

style.quote_prefix = u8"│ ";
style.horizontal_rule_char = u8"─";

TUI::Markdown(edit, md, style);
```

### MarkdownStyle 主要欄位

| 欄位 | 用途 |
|---|---|
| `text` | 一般文字 |
| `bold_text` | `**...**` 與 `__...__` |
| `italic_text` | `*...*` 與 `_..._` |
| `code` | fenced code block |
| `quote` | `>` 引用內容 |
| `heading[6]` | 六級標題 |
| `heading_rule[6]` | 標題分隔線樣式 |
| `heading_rule_char[6]` | 各級標題分隔線字元 |
| `table_header` | 表格表頭 |
| `table_cell` | 表格資料列 |
| `table_border_style` | 表格外框與直線 |
| `table_separator` | 表頭分隔線 |
| `replacements` | emoji 或文字替換 |

## 編輯與事件

`RichEdit` 共用 `Edit` 的文字輸入行為，包括：

- Backspace、Delete、Enter；
- 左右上下方向鍵；
- Home、End；
- Shift + 方向鍵選取；
- 滑鼠點擊與拖曳選取；
- 雙擊選字；
- Ctrl+A 全選；
- Ctrl+C 複製；
- Paste 貼上；
- 唯讀模式。

`RichEdit::Event()` 與 `Edit::Event()` 使用共用的文字事件流程，但實際插入、刪除與選取操作會透過 virtual method 分派到 `RichText`，因此文字與 `styles` 會同步更新。

設定唯讀：

```cpp
edit->readonly = true;
```

自訂鍵盤事件：

```cpp
edit->on_key = [](const TUI::Event& ev) {
    // return true 表示事件已處理
    return false;
};
```

## 繪製與捲動

`RichEdit` 使用 `Slider` 的水平/垂直捲動功能：

```text
ScrollX true
ScrollY true
```

繪製時會使用 `DrawBuffer::Text(const RichText&)`，逐字元套用前景色、背景色與文字屬性。

每一幀繪製前應先清空目前 buffer：

```cpp
auto& buf = terminal.GetDrawBuffer();
buf.clear();
mgr.Paint(buf);
terminal.Render();
```

這能確保文字變短時，上一幀的寬字元 continuation cell 不會殘留。

## Unicode 索引

`RichText` 的 `chars` 與 `styles` 使用 Unicode character index：

```text
styles[i] 對應 chars[i]
```

底層 `text` 仍然是 UTF-8 byte string，因此不要用 `std::string::size()` 當成顯示字元數。中文、emoji 與寬字元會佔用多個 terminal cells，但只對應一個 Unicode character index。
