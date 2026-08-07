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
└──────────────────────────────────────────────────────────────┘

Header: pos(0,0) size(parent_width, 2)
  Rect 0 0 2 2
  Dock right 0 0 100 100

Nav: pos(0,2) size(30, parent_height - 3 - 2)
  Rect 0 2 30 30
  Dock down 0 0 100 100
  DockOffset 0 0 0 -3

Main Area: pos(30, 2) size(parent_width - 30, parent_height - 3 - 2)
  Rect 30 2 30 30
  Dock right|down 0 0 100 100
  DockOffset 0 0 -3 0
  
Status: pos(0, parent_height - 3) size(parent_width, 3)
  Rect 0 0 30 3
  Dock right|down 0 100 100 100
  DockOffset 0 -3 0 0
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

Dock 是**百分比錨定系統**，四邊（Left / Top / Right / Down）可獨立設定。
每個錨定的邊緣會根據 `dock_.dock` rect 的四個值（以 **0–100 的百分比**）來決定該邊緣在父容器 clip 區域中的位置，再加上 `dock_.offset` 做像素級微調。

未錨定的邊緣則保持 `local` rect 原有的相對位置。

最終 `screen = local.move(pt.x, pt.y)`，將調整後的 local 座標轉換為 screen 座標。

### dock_.dock 欄位含義

| 字段 | 控制的邊 | 說明 |
|---|---|---|
| `dock_.dock.x`   | Left（當 `Dock_Left` 設定時） | 左邊緣在父容器寬度的百分比位置 |
| `dock_.dock.y`   | Top（當 `Dock_Top` 設定時）  | 上邊緣在父容器高度的百分比位置 |
| `dock_.dock.x2`  | Right（當 `Dock_Right` 設定時） | 右邊緣在父容器寬度的百分比位置 |
| `dock_.dock.y2`  | Down（當 `Dock_Down` 設定時）  | 下邊緣在父容器高度的百分比位置 |

### dock_.offset 欄位含義

| 字段 | 說明 |
|---|---|
| `dock_.offset.x`   | Left 錨定後額外偏移的像素數 |
| `dock_.offset.y`   | Top 錨定後額外偏移的像素數 |
| `dock_.offset.x2`  | Right 錨定後額外偏移的像素數 |
| `dock_.offset.y2`  | Down 錨定後額外偏移的像素數 |

### 計算規則

- Dock_None

  不設定任何錨定旗標，控件使用 `local` rect 的原始位置。

- Dock_Left / Dock_Top（單邊錨定）

  將對應邊緣固定在父容器 clip 區域的百分比位置上：

- Dock_Right / Dock_Down（對向錨定）

  將對應邊緣固定在父容器 clip 區域的百分比位置上：

- Dock_All（四邊錨定）

  同時設定 `Dock_Left | Dock_Top | Dock_Right | Dock_Down`，四個邊緣都被百分比錨定。
這相當於讓控件填滿由四個百分比所定義的區域

- 組合錨定（如 Dock_Left | Dock_Top）

  只錨定左邊和上邊，右邊和下邊保持 `local` rect 的原始尺寸：

### Dock 計算流程

- void Win::CalRect(Win* parent)


## Arrange 實作

### 核心概念

Arrange 在父控件的 `CalRect` 中執行（Stage 3），負責計算所有子元素的 `local` rect。
子元素隨後在自己的 `CalRect` 中被轉換為 screen 座標。

**重要**：Arrange 會將子元素的 `dock_.mode` 設為 `Dock_None`，覆蓋子元素自身的 Dock 設定。

- Arrange_None

  不處理子元素排列，子元素使用各自的 `local` rect（手動設定）。

- Arrange_Item

  Arrange_Item 有**兩種模式**，由 `arrange_.items` 決定：

    - 模式 A：欄位模式（`items > 0`）

      將容器分割為固定數量的欄位，子元素在欄位內**置中排列**，超出欄位數量時自動換行。

**Vertical（垂直方向）** — 橫向排 `items` 個欄位，滿列後換行：

```
|    container width    |
┌───────┬───────┬───────┐
│ item1 │ item2 │ item3 │   ← items=3, 每欄置中
├───────┼───────┼───────┤
│ item4 │ item5 │       │   ← 換行，maxh 取該列最大高度
└───────┴───────┴───────┘
```

**Horizontal（水平方向）** — 縱向排 `items` 個欄位，滿欄後換列：

  -
    - 模式 B：固定尺寸模式（`items == 0`）

每個子元素使用固定的 `item_size`，按方向排列並自動換行。

**Vertical（垂直方向）** — 橫向排固定寬度的項目，超出容器寬度時換行：

```
|  container width    |
┌──────────┬──────────┐
│ item1    │ item2    │   ← 每個 item_size.x 寬
├──────────┴──────────┤
│ item3               │   ← 換行
└─────────────────────┘
```

**Horizontal（水平方向）** — 縱向排固定高度的項目，超出容器高度時換列：

### Arrange_Content

根據子元素的內容尺寸自動排列。當下一個元素放不進當前行/欄時自動換行。
與 Arrange_Item 不同：
- **不使用固定尺寸**，每個子元素使用自身的 `local.width()` / `local.height()`
- **不會置中**，靠左（或靠上）緊貼排列
- **自動換行**，當當前行/欄放不下時才換行

**Vertical（垂直方向）** — 橫向流動，超出容器寬度時換行：

```
|    container width    |
┌──────────┬────────────┐
│ btn1     │ check2     │   ← 超過寬度換到下行
├──────────┼────────────┤
│ arrange1 │ btn3       │   ← 超過寬度換到下行
├──────────┴────────────┤
│ chk1 │ chk2 │ chk3    │
└───────────────────────┘
```

**Horizontal（水平方向）** — 縱向流動，超出容器高度時換列：

## Win::CalRect 整合

將 Dock 和 Arrange 整合到現有的 `CalRect` 流程中：

> Win::CalRect()

1. ── 準備父容器資訊 ───────────────────────
2. ── Stage 1: Dock（百分比錨定）───────────
3. ── Stage 2: Screen + Clip（邊框扣除）────
4. ── Stage 3: Arrange（子元素排列）────────

> **注意**：`CalRect` 直接修改 `local` rect（而非先計算再賦值給 `screen`），
> 因為 Dock 的百分比錨定需要基於父容器 clip 動態計算。

## Win 結構體新增欄位

```cpp
struct Dock_ {
    int mode;     // Dock_None / Dock_Left / Dock_Top / Dock_Right / Dock_Down / Dock_All (bit flags)
    Rect dock;    // {x, y, x2, y2} — 百分比錨定值（0-100）
                  //   x: Left 的百分比, y: Top 的百分比
                  //   x2: Right 的百分比, y2: Down 的百分比
    Rect offset;  // {x, y, x2, y2} — 像素級偏移量
};

struct Arrange_ {
    int mode;         // Arrange_None / Arrange_Item / Arrange_Content
    bool is_vertical; // true=垂直方向（橫向流動）, false=水平方向（縱向流動）
    int items;        // >0: 欄位模式（分割為 N 個等寬/等高欄位）
                      // ==0: 固定尺寸模式
    Point item_size;  // 固定尺寸模式下的每個項目大小 (x=width, y=height)
};

struct Win {
    // ... existing fields ...
    Dock_ dock_;       // docking configuration
    Arrange_ arrange_; // arrangement configuration for children
};
```

## 典型使用模式

### 1. 標準 TUI 框架

```
┌───────────────────────────────┐
│ Title Bar (Dock_Top, h=2)     │
│                               │
│ ┌───┐ ┌───────────────────┐   │
│ │Nav│ │                   │   │
│ │   │ │  Main Content     │   │
│ │   │ │  (Dock_All)       │   │
│ │   │ │                   │   │
│ └───┘ └───────────────────┘   │
│ Status Bar (Dock_Down, h=1)   │
└───────────────────────────────┘
```

- Title: `dock_.mode = Dock_Top | Dock_Left | Dock_Right`, `dock_.dock.y = 0, x = 0, x2 = 100`（貼頂部，橫跨全寬）
- Nav: `dock_.mode = Dock_Left | Dock_Top | Dock_Down`, `dock_.dock.x = 0, y = 2, y2 = 98`, `dock_.dock.x2 = 15`（左側 15% 寬，上下留白）
- Main: `dock_.mode = Dock_All`, `dock_.dock = {15, 2, 100, 98}`（填滿剩餘空間）
- Status: `dock_.mode = Dock_Down | Dock_Left | Dock_Right`, `dock_.dock.y2 = 100, x = 0, x2 = 100`（貼底部，橫跨全寬）

### 2. 文件列表（側邊欄內）

```
┌─────────────── Nav (Dock_Left) ────────────────┐
│ Arrange_Vertical, Arrange_Item                 │
│ [File1.txt]                                    │
│ [File2.txt]                                    │
│ [File3.txt]                                    │
└────────────────────────────────────────────────┘
```

- Nav: `arrange_.mode = Arrange_Item`, `arrange_.is_vertical = true`, `arrange_.items = 1`（單欄，項目置中）

> 若使用固定尺寸模式：`arrange_.items = 0`, `arrange_.item_size = {15, 1}`

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

- Form: `arrange_.mode = Arrange_Content`, `arrange_.is_vertical = true`
- 每個輸入欄位使用自身的 `local.width()` / `local.height()`，自動換行

## 與 Slider 的整合

`Slider::CalRect` 呼叫 `Win::CalRect(parent)` 完成 Dock + Arrange 後，再計算滾動相關參數：

- Slider 的 Arrange（通常是 `Arrange_Item`）已經將子元素的 `local` rect 排列好
- `content_length` = 所有子元素在滾動方向上的最大延伸範圍
- `scroll_max` = `content_length - clip.size()`（可滾動的總距離）
- `GetClipPos()` 被覆寫，回傳帶有 `scroll_value` 偏移的座標，使子元素的 Dock 計算自動應用滾動偏移

## 解析器（EditLine / ParseCmd）

實際的解析器實作在 `Win::ParseCmd`：

### DSL 範例

**標準 TUI 框架：**
```
# Title bar — 貼頂部，橫跨全寬
Dock All 0 0 100 5
Rect 0 0 80 2

# Navigation — 左側 15% 寬度
Dock All 0 5 15 95
Rect 0 0 15 1

# Main content — 填滿剩餘空間
Dock All 15 5 100 95
Arrange Item true 3
# items=3: 垂直方向分 3 欄，子元素置中排列

# Status bar — 貼底部
Dock All 0 95 100 100
Rect 0 0 80 1
```

**固定尺寸模式（items = 0）：**
```
Arrange Item true 0 10 1
# items=0, item_size={10, 1}: 每個項目 10x1，橫向排列自動換行
```

**內容流式佈局：**
```
Arrange Content true
# 子元素按自身尺寸排列，超出容器寬度時自動換行
```

**DockOffset 微調：**
```
Dock All 0 0 100 100
DockOffset 2 2 2 2
# 四邊各內縮 2 像素
```
