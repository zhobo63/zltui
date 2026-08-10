# zltui DSL 使用說明

## 概述

zltui DSL 是一種宣告式語言，用於描述終端 UI 的元件結構與樣式。所有關鍵字不區分大小寫。

```cpp
mgr.Parse(R"(
    Object Slider {
        Name pane1
        Rect 0 0 80 24
        ...
    }
)"");
```

## 語法規則

- `Object <Type> { ... }` — 建立元件，大括號內為子元件或屬性
- 註解：行首 `#` 或 `//`，區塊 `/* ... */`
- 字串：支援 `u8"..."`（UTF-8）或直接寫文字
- 布林值：`true` / `false`
- 數字：整數

## 元件類型

| 類型 | 說明 | 繼承 |
|---|---|---|
| `Slider` | 可滾動容器，支援垂直/水平捲軸 | Win |
| `Label` | 文字標籤 | Win + Text |
| `Button` | 按鈕（含 hover/down 狀態） | Label |
| `Check` | 核取方塊（切換 checked） | Button |
| `Edit` | 可編輯文字元件，支援游標、選取與輸入 | Slider + Text |
| `RichEdit` | 可編輯 RichText 元件，每個字元可有獨立樣式 | Slider + RichText |

## 通用屬性（Win）

所有元件都支援以下屬性：

### 基本

```dsl
Name myWidget          # 元件名稱，用於 GetUI() 查詢
Rect x y x2 y2         # 位置與大小（相對於父容器 clip 區域）
Visible true            # 是否可見
Notify true             # 是否接收事件
Autosize None           # None / TextWidth / TextHeight / TextSize
```

### 邊框

```dsl
DrawBorder true                 # 繪製邊框
BorderStyle Single              # None / Single / Double / Round
Title <string>                  # 標題
```

### 顏色

```dsl
fgColor BrightGreen             # 前景色
bgColor RGB(40,60,80)           # 背景色
```

**顏色格式：**
- ANSI 名稱：`Black`, `Red`, `Green`, `Yellow`, `Blue`, `Magenta`, `Cyan`, `White`
- 亮色：`BrightBlack`, `BrightRed`, ... , `BrightWhite`
- RGB：`RGB(r,g,b)`，例如 `RGB(0,250,250)`
- #rrggbb #rgb

### Dock（錨定佈局）

```dsl
Dock <mode> x y x2 y2
```

Mode 支援以 `|` 組合：

| Mode | 說明 |
|---|---|
| `None` | 不錨定，使用 Rect |
| `Top` | 貼頂部 |
| `Left` | 貼左側 |
| `Right` | 貼右側 |
| `Down` | 貼底部 |
| `All` | 四邊拉伸填滿 |
| `Top_Pane` | 頂部面板錨定 |
| `Left_Pane` | 左側面板錨定 |
| `Right_Pane` | 右側面板錨定 |
| `Down_Pane` | 底部面板錨定 |

`x y x2 y2` 為百分比（0–100），定義元件在父容器中的位置。

```dsl
# 貼頂部，橫跨全寬，佔前 5% 高度
Dock All 0 0 100 5

# 左側 + 右側同時錨定（水平拉伸）
Dock Left|Right 0 0 100 100
```

微調偏移：
```dsl
DockOffset ox oy ox2 oy2       # 從錨定位置偏移的像素值
```

### Autosize（內容自動調整大小）

```dsl
Autosize None        # 使用 Rect，不自動調整
Autosize TextWidth   # 依文字內容寬度調整寬度
Autosize TextHeight  # 依文字內容高度調整高度
Autosize TextSize    # 同時依文字寬度與高度調整
```

`Autosize` 會在元件重新計算 layout 時，依 `GetTextSize()` 的結果調整 `Rect`。對文字元件使用時，文字必須先設定，才能得到正確的內容尺寸。

### Arrange（子元件排列）

```dsl
Arrange <mode> <vertical>
```

| Mode | 參數 | 說明 |
|---|---|---|
| `None` | — | 不自動排列，子元件使用自己的 Rect |
| `Item` | `true/false items itemW itemH` | 固定欄位或固定尺寸排列 |
| `Content` | `true/false` | 依內容大小流式排列 |

**Arrange_Item（items > 0）：**
```dsl
Arrange Item true 3             # 垂直方向分 3 欄，子元素置中於欄位內
```

**Arrange_Item（items = 0，固定尺寸）：**
```dsl
Arrange Item true 0 10 1        # 每個項目 10x1，橫向排列自動換行
```

**Arrange_Content：**
```dsl
Arrange Content true            # 子元素按自身尺寸排列，超出寬度自動換行
```

## Label / Button / Check 專屬屬性

繼承自 Text：

```dsl
Text u8"這是🔥 label\n第二行"    # 設定文字（支援 \n 換行）
TextAlign Center                 # Start / Center / End
Bold true                        # 粗體
Italic true                      # 斜體
Underline true                   # 底線
```

Button 額外：
```dsl
ColorHover RGB(70,70,70)         # hover 背景色
ColorDown RGB(90,90,90)          # 按下背景色
```

## Slider 專屬屬性

```dsl
Vertical true                    # true=垂直捲軸，false=水平
TrackColor RGB(44,44,44)         # 軌道顏色
ThumbColor BrightWhite           # 滑塊顏色
```

## Edit 專屬屬性

`Edit` 支援文字輸入、游標、選取、複製、貼上與捲動：

```dsl
Object Edit {
    Name input
    Rect 0 0 60 4
    ScrollY true
    Text "Type here..."
}
```

目前可用的互動包括：

- Backspace、Delete、Enter；
- 方向鍵、Home、End；
- Shift + 方向鍵選取；
- 滑鼠點擊、拖曳選取與雙擊選字；
- Ctrl+A 全選、Ctrl+C 複製；
- Windows 使用 `CF_UNICODETEXT` 複製 Unicode 文字。

## RichEdit 專屬屬性

`RichEdit` 使用 `RichText::Style` 儲存每個字元的樣式。除了普通 `Text` 外，還支援以目前樣式追加文字。

### Text

```dsl
Text "Build report"
```

`Text` 會取代目前內容並重設 RichText 的字元樣式。

### Style

```dsl
Style <fgcolor> [<bgcolor>] [bold] [italic] [underline]
```

`Style` 設定後續 `AppendText` 使用的目前樣式。樣式旗標可以任意組合，未指定的旗標為 `false`：

```dsl
Object RichEdit {
    Name output
    Rect 0 0 70 10
    ScrollY true

    Text ""

    Style BrightWhite
    AppendText "Build: "

    Style BrightGreen bold
    AppendText "success"

    Style Yellow Unused italic
    AppendText " warning"

    Style White Red bold underline
    AppendText " error"
}
```

範例中的：

```dsl
Style BrightGreen bold
```

表示前景色為 `BrightGreen`、粗體開啟；

```dsl
Style Yellow Unused italic
```

表示黃色斜體，且不填背景色。

### AppendText

```dsl
AppendText "文字內容"
```

`AppendText` 將文字追加到目前內容，並套用最近一次 `Style` 指定的樣式。文字支援 `u8"..."` 與 `\\n` 換行：

```dsl
Object RichEdit {
    Rect 0 0 70 6
    Text ""
    Style BrightCyan
    AppendText u8"第一行\\n"
    Style BrightYellow bold
    AppendText u8"第二行"
}
```

### RichEdit 與 Markdown

Markdown 目前透過 C++ API 使用，不是 RichEdit DSL 命令：

```cpp
TUI::RichEdit* edit = new TUI::RichEdit(&mgr);
TUI::Markdown(edit, markdown_text);
```

Markdown 支援標題、粗體、斜體、顏色 tag、引用、列表、表格、水平線與 fenced code block。詳細說明請參考 [`doc/richedit.md`](richedit.md)。

## 完整範例

```dsl
Object Slider {
    Name pane1
    DrawBorder true
    FgColor RGB(0,250,250)
    Rect 0 0 80 24
    Dock All 0 0 100 100

    Object Label {
        Name label1
        Rect 0 0 79 2
        Text u8"這是🔥 label\n隨著螢幕大小自動縮放"
        TextAlign Center
    }

    Object Button {
        Name btn1
        Rect 0 4 12 4
        Text "Button 1"
        FgColor BrightGreen
        ColorHover RGB(70,70,70)
    }

    Object Check {
        Name chk1
        Rect 0 6 15 6
        Text "選項 A"
    }
}
```

## 程式碼中使用

```cpp
TUI::Mgr mgr;
mgr.Parse(R"( ... DSL content ... )");

// 取得元件
auto* btn = mgr.GetUI("btn1");
if (btn) {
    btn->on_click = []() { /* handle click */ };
}
```
