# Dock & Arrange 佈局系統

## 概述

Dock & Arrange 是 zltui 的 TUI 佈局核心，採用兩層機制：

- **Dock** — 控制控件自身如何錨定到父容器邊緣（"我在哪裡"）
- **Arrange** — 控制容器內子元素如何排列（"我的孩子怎麼排"）

兩者正交，一個控件可以同時擁有 Dock 和 Arrange。

```
┌────────────────────────────── parent ────────────────────────┐
│ [Header - Dock_Top]                                          │
│                                                              │
│ ┌──────┐  ┌──────────────────────────────────────────────┐   │
│ │Nav   │  │ Main Area (Dock_All)                         │   │
│ │Dock_ │  │                                              │   │
│ │Left  │  │ Arrange_Vertical:                            │   │
│ │      │  │ [item1] [item2] [item3]                      │   │
│ └──────┘  └──────────────────────────────────────────────┘   │
│ [Status - Dock_Down]                                         │
└──────────────────────────────┘
```

## 運作流程

佈局計算在 `Win::CalRect(Win* parent)` 中執行，自上而下遞迴：

```
Mgr (root)
  └─ CalRect(nullptr)          ← 根節點，screen = terminal size
       ├─ child1.CalRect(this)  ← Dock: 決定自身位置
       │    └─ Arrange: 計算子元素位置
       ├─ child2.CalRect(this)
       │    └─ Arrange: ...
       └─ child3.CalRect(this)
```

### 階段劃分

每個控件的 `CalRect` 分為三個階段：

1. **Dock 階段** — 根據 Dock 配置，計算自身在父容器中的位置
2. **Clip 階段** — 扣除邊框，確定可繪製區域（已有邏輯）
3. **Arrange 階段** — 如果有子元素且 Arrange 模式非 None，計算每個子元素的 `local` rect

## Dock 實作

### 核心概念

Dock 是位標記系統，支援組合錨定。計算順序：**Top → Left → Right → Down**。

### 計算規則

#### Dock_None
控件使用 `local` rect 的絕對位置，不做任何調整。

```cpp
// local = {10, 5, 30, 15} → screen = parent->clip + (10, 5)
```

#### Dock_Top / Dock_Left / Dock_Right / Dock_Down（單邊錨定）

錨定的邊緣貼合父容器，對向邊緣由 `dock` rect 的尺寸決定。

```cpp
// Dock_Top: 貼頂部，高度固定
screen.x   = parent_clip.x + offset.x;
screen.y   = parent_clip.y + offset.y;
screen.x2  = screen.x + dock.width() - 1;    // 寬度由 dock 決定
screen.y2  = screen.y + dock.height() - 1;   // 高度由 dock 決定
```

#### Dock_All（四邊錨定）

填充父容器剩餘空間，扣除 offset。

```cpp
screen.x   = parent_clip.x + offset.x;
screen.y   = parent_clip.y + offset.y;
screen.x2  = parent_clip.x2 - offset.x2;
screen.y2  = parent_clip.y2 - offset.y2;
```

#### 組合錨定（如 Dock_Top | Dock_Left）

錨定的邊緣貼合，未錨定的方向由 `dock` rect 的尺寸決定。

```cpp
// Dock_Top | Dock_Left: 貼左上角，寬度和高度由 dock 決定
screen.x   = parent_clip.x + offset.x;
screen.y   = parent_clip.y + offset.y;
screen.x2  = screen.x + dock.width() - 1;
screen.y2  = screen.y + dock.height() - 1;
```

### Dock 計算偽碼

```cpp
void Win::CalRectDock(Win* parent)
{
    Rect pc = parent ? parent->clip : local;  // parent clip area
    Point pt = parent ? parent->GetClipPos() : Point{0, 0};

    switch (dock.mode) {
        case Dock_None:
            screen = local.move(pt.x, pt.y);
            break;

        case Dock_All:
            screen.set(
                pc.x + dock.offset.x,
                pc.y + dock.offset.y,
                pc.x2 - dock.offset.x2,
                pc.y2 - dock.offset.y2
            );
            break;

        default:  // bit flags combination
            // X position
            if (dock.mode & Dock_Left)
                screen.x = pc.x + dock.offset.x;
            else if (dock.mode & Dock_Right)
                screen.x = pc.x2 - dock.width() + 1 - dock.offset.x2;
            else
                screen.x = pt.x + local.x;  // use local position

            // Y position
            if (dock.mode & Dock_Top)
                screen.y = pc.y + dock.offset.y;
            else if (dock.mode & Dock_Down)
                screen.y = pc.y2 - dock.height() + 1 - dock.offset.y2;
            else
                screen.y = pt.y + local.y;  // use local position

            // Size: anchored edges stretch, unanchored edges use dock size
            if (dock.mode & Dock_Right)
                screen.x2 = pc.x2 - dock.offset.x2;
            else
                screen.x2 = screen.x + dock.width() - 1;

            if (dock.mode & Dock_Down)
                screen.y2 = pc.y2 - dock.offset.y2;
            else
                screen.y2 = screen.y + dock.height() - 1;
    }
}
```

### Offset 的意義

`offset` 是 `Rect` 類型，四邊各一個值：

| 字段 | 含義 |
|---|---|
| `offset.x`   | 左側內縮（Dock_Left / Dock_All 時有效） |
| `offset.y`   | 頂部內縮（Dock_Top / Dock_All 時有效） |
| `offset.x2`  | 右側內縮（Dock_Right / Dock_All 時有效） |
| `offset.y2`  | 底部內縮（Dock_Down / Dock_All 時有效） |

```cpp
// Dock_All with offset = {5, 3, 5, 3} → 四邊各留 5/3 格空白
screen.x   = pc.x + 5;
screen.y   = pc.y + 3;
screen.x2  = pc.x2 - 5;
screen.y2  = pc.y2 - 3;
```

## Arrange 實作

### 核心概念

Arrange 在父控件的 `CalRect` 中執行，負責計算所有子元素的 `local` rect。子元素隨後在自己的 `CalRect` 中被轉換為 screen 座標。

### Arrange_None
不處理子元素排列，子元素使用各自的 `local` rect（手動設定）。

### Arrange_Item
每個子元素使用固定的 `item_size`，按方向排列。

```cpp
void Win::ArrangeItem()
{
    int gap = 0;  // item spacing, future extension
    Point pos;

    if (is_vertical) {
        pos.x = clip.x - GetClipPos().x;  // relative to parent local
        pos.y = clip.y - GetClipPos().y;
        for (int i = 0; i < (int)child.size(); i++) {
            child[i]->local.set(
                pos.x,
                pos.y,
                pos.x + item_size.x - 1,
                pos.y + item_size.y - 1
            );
            pos.y += item_size.y + gap;
        }
    } else {  // horizontal
        for (int i = 0; i < (int)child.size(); i++) {
            child[i]->local.set(
                pos.x,
                pos.y,
                pos.x + item_size.x - 1,
                pos.y + item_size.y - 1
            );
            pos.x += item_size.x + gap;
        }
    }
}
```

### Arrange_Content
根據子元素的內容尺寸自動排列。每個子元素先計算自身需要的最小尺寸，然後按方向堆疊。

```cpp
void Win::ArrangeContent()
{
    Point pos;
    int total_size = 0;  // accumulated size in arrange direction

    if (is_vertical) {
        pos.x = clip.x - GetClipPos().x;
        pos.y = clip.y - GetClipPos().y;
        for (auto& ch : child) {
            int h = ch->GetContentHeight();  // virtual, subclass implements
            int w = std::min(ch->GetContentWidth(), clip.width());
            ch->local.set(pos.x, pos.y, pos.x + w - 1, pos.y + h - 1);
            pos.y += h;
        }
    } else {  // horizontal
        for (auto& ch : child) {
            int w = ch->GetContentWidth();
            int h = std::min(ch->GetContentHeight(), clip.height());
            ch->local.set(pos.x, pos.y, pos.x + w - 1, pos.y + h - 1);
            pos.x += w;
        }
    }
}
```

## Win::CalRect 整合

將 Dock 和 Arrange 整合到現有的 `CalRect` 流程中：

```cpp
void Win::CalRect(Win* parent)
{
    // ── Stage 1: Dock (position self) ───────────────
    CalRectDock(parent);

    // ── Stage 2: Clip (account for border) ──────────
    if (draw_border && border_style != BorderStyle_None)
        clip = screen.expand(-1, -1);
    else
        clip = screen;

    // ── Stage 3: Arrange (position children) ────────
    CalRectArrange();
}
```

## Win 結構體新增欄位

```cpp
struct Win {
    // ... existing fields ...

    Dock dock;       // docking configuration
    Arrange arrange; // arrangement configuration for children
};
```

## 與現有 Display_ 的關係

| Display_ | 對應的 Dock/Arrange |
|---|---|
| `Display_User` | `Dock_None` + `Arrange_None`（手動指定 rect） |
| `Display_Block` | `Dock_Left \| Dock_Right` + 高度由內容決定 |
| `Display_Flex` | 可透過 Dock/Arrange 組合實現，但非直接對應 |
| `Display_Grid` | 不適合用 Dock/Arrange 表達 |

建議：
- **保留** `Display_User` / `Display_Block`（簡單場景）
- **棄用** `Display_Flex` / `Display_Grid`（TUI 中過度設計，Dock/Arrange 組合已足夠）

## 典型使用模式

### 1. 標準 TUI 框架

```
┌───────────────────────────────┐
│ Title Bar (Dock_Top, h=2)     │
│                               │
│ ┌───┐ ┌───────────────────┐   │
│ │Nav│ │                   │   │
│ │   │ │  Main Content      │   │
│ │   │ │  (Dock_All)        │   │
│ │   │ │                   │   │
│ └───┘ └───────────────────┘   │
│ Status Bar (Dock_Down, h=1)   │
└───────────────────────────────┘
```

- Title: `dock.mode = Dock_Top`, `dock.dock.height() = 2`
- Nav: `dock.mode = Dock_Left`, `dock.dock.width() = 3`
- Main: `dock.mode = Dock_All`
- Status: `dock.mode = Dock_Down`, `dock.dock.height() = 1`

### 2. 文件列表（側邊欄內）

```
┌─────────────── Nav (Dock_Left) ────────────────┐
│ Arrange_Vertical, Arrange_Item                 │
│ [File1.txt]                                    │
│ [File2.txt]                                    │
│ [File3.txt]                                    │
└────────────────────────────────────────────────┘
```

- Nav: `arrange.mode = Arrange_Item`, `arrange.is_vertical = true`, `item_size = {15, 1}`

### 3. 表單輸入

```
┌─────────────── Form (Dock_All) ────────────────┐
│ Arrange_Vertical, Arrange_Content              │
│ Name: [____________]                           │
│ Email: [___________________]                   │
│ Bio:  [_________________________]              │
│         [_________________________]            │
└────────────────────────────────────────────────┘
```

- Form: `arrange.mode = Arrange_Content`, `is_vertical = true`
- 每個輸入欄位回報自身內容高度，自動堆疊

## 擴充方向

### Gap（間距）

Arrange 支援 item 之間的間距：

```cpp
struct Arrange {
    // ...
    int gap = 0;        // spacing between items
};
```

### Align（對齊）

當子元素尺寸小於可用空間時，控制對齊方式：

```cpp
struct Arrange {
    // ...
    Align_ align_main   = Align_Start;  // along arrange direction
    Align_ align_cross  = Align_Start;  // perpendicular to arrange direction
};
```

### Wrap（換行）

當子元素超出容器時自動換行：

```cpp
struct Arrange {
    // ...
    bool wrap = false;
};
```

## 與 Slider 的整合

`Slider::CalRect` 目前手動計算 `content_length`。Arrange 可以統一這個邏輯：

- Slider 內部使用 `Arrange_Item`（垂直）排列子元素
- `content_length` = 所有 item 總高度 + gap
- `scroll_value` 控制 clip 的偏移量

```cpp
void Slider::CalRect(Win* parent)
{
    Win::CalRect(parent);  // Dock + Arrange already handled

    // content_length is now computed by Arrange_Item
    // scroll_value adjusts the effective clip offset
}
```

## 解析器（EditLine / ParseCmd）

支援在 DSL 中設定 Dock 和 Arrange：

```cpp
// Dock parsing
bool Win::ParseDock(EditLine& el)
{
    std::string tok = el.next_tok();
    if (eqi(tok, "None"))   dock.mode = Dock_None;
    else if (eqi(tok, "Top"))  dock.mode = Dock_Top;
    else if (eqi(tok, "Left")) dock.mode = Dock_Left;
    else if (eqi(tok, "Right"))dock.mode = Dock_Right;
    else if (eqi(tok, "Down")) dock.mode = Dock_Down;
    else if (eqi(tok, "All"))  dock.mode = Dock_All;

    // optional: size and offset
    std::string next = el.tok();
    if (!next.empty()) {
        // parse dock rect or offset
    }
    return true;
}

// Arrange parsing
bool Win::ParseArrange(EditLine& el)
{
    std::string tok = el.next_tok();
    if (eqi(tok, "None"))      arrange.mode = Arrange_None;
    else if (eqi(tok, "Item"))  arrange.mode = Arrange_Item;
    else if (eqi(tok, "Content"))arrange.mode = Arrange_Content;

    // optional: direction and item_size
    return true;
}
```

DSL 範例：

```
Dock Top 2
Dock Left 15
Dock All
Arrange Item Vertical 10x1
```
