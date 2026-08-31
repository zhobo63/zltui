#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>

// Keep the public Event key values portable.  On Windows these are supplied
// by windows.h; Unix terminals use the same values when reporting special
// keys through VT sequences.
#ifndef VK_BACK
#define VK_BACK    0x08
#define VK_TAB     0x09
#define VK_RETURN  0x0D
#define VK_ESCAPE  0x1B
#define VK_SPACE   0x20
#define VK_PRIOR   0x21
#define VK_NEXT    0x22
#define VK_END     0x23
#define VK_HOME    0x24
#define VK_LEFT    0x25
#define VK_UP      0x26
#define VK_RIGHT   0x27
#define VK_DOWN    0x28
#define VK_INSERT  0x2D
#define VK_DELETE  0x2E
#define VK_F1      0x70
#define VK_F2      0x71
#define VK_F3      0x72
#define VK_F4      0x73
#define VK_F5      0x74
#define VK_F6      0x75
#define VK_F7      0x76
#define VK_F8      0x77
#define VK_F9      0x78
#define VK_F10     0x79
#define VK_F11     0x7A
#define VK_F12     0x7B
#endif
#endif

#include <thread>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>

#define NAMESPACE_BEGIN(n) namespace n {
#define NAMESPACE_END };

NAMESPACE_BEGIN(TUI)

// ── Style ─────────────────────────────────────
static constexpr const char* ANSI_RESET = "\033[0m";  // reset all
static constexpr const char* ANSI_BOLD = "\033[1m";  // bold
static constexpr const char* ANSI_DIM = "\033[2m";  // dim
static constexpr const char* ANSI_ITALIC = "\033[3m";  // italic
static constexpr const char* ANSI_UNDER = "\033[4m";  // underline
static constexpr const char* ANSI_STRIKE = "\033[9m";  // strikethrough

// ── Foreground colors ─────────────────────────
static constexpr const char* ANSI_FG_BLACK = "\033[30m";
static constexpr const char* ANSI_FG_RED = "\033[31m";
static constexpr const char* ANSI_FG_GREEN = "\033[32m";
static constexpr const char* ANSI_FG_YELLOW = "\033[33m";
static constexpr const char* ANSI_FG_BLUE = "\033[34m";
static constexpr const char* ANSI_FG_MAGENTA = "\033[35m";
static constexpr const char* ANSI_FG_CYAN = "\033[36m";
static constexpr const char* ANSI_FG_WHITE = "\033[37m";

// ── Bright foreground colors ──────────────────
static constexpr const char* ANSI_BRIGHT_BLACK = "\033[90m";  // gray
static constexpr const char* ANSI_BRIGHT_RED = "\033[91m";
static constexpr const char* ANSI_BRIGHT_GREEN = "\033[92m";
static constexpr const char* ANSI_BRIGHT_YELLOW = "\033[93m";
static constexpr const char* ANSI_BRIGHT_BLUE = "\033[94m";
static constexpr const char* ANSI_BRIGHT_MAGENTA = "\033[95m";
static constexpr const char* ANSI_BRIGHT_CYAN = "\033[96m";
static constexpr const char* ANSI_BRIGHT_WHITE = "\033[97m";

// ── Background colors ─────────────────────────
static constexpr const char* ANSI_BG_BLACK = "\033[40m";
static constexpr const char* ANSI_BG_RED = "\033[41m";
static constexpr const char* ANSI_BG_GREEN = "\033[42m";
static constexpr const char* ANSI_BG_YELLOW = "\033[43m";
static constexpr const char* ANSI_BG_BLUE = "\033[44m";
static constexpr const char* ANSI_BG_MAGENTA = "\033[45m";
static constexpr const char* ANSI_BG_CYAN = "\033[46m";
static constexpr const char* ANSI_BG_WHITE = "\033[47m";

// ── Bright background colors ──────────────────
static constexpr const char* ANSI_BRIGHT_BG_BLACK = "\033[100m";
static constexpr const char* ANSI_BRIGHT_BG_RED = "\033[101m";
static constexpr const char* ANSI_BRIGHT_BG_GREEN = "\033[102m";
static constexpr const char* ANSI_BRIGHT_BG_YELLOW = "\033[103m";
static constexpr const char* ANSI_BRIGHT_BG_BLUE = "\033[104m";
static constexpr const char* ANSI_BRIGHT_BG_MAGENTA = "\033[105m";
static constexpr const char* ANSI_BRIGHT_BG_CYAN = "\033[106m";
static constexpr const char* ANSI_BRIGHT_BG_WHITE = "\033[107m";

// ── Cursor control ────────────────────────────
static constexpr const char* ANSI_CURSOR_HOME = "\033[H";       // move to top-left
static constexpr const char* ANSI_CLEAR_LINE = "\033[K";       // clear from cursor to end of line
static constexpr const char* ANSI_CLEAR_SCREEN = "\033[2J";      // clear entire screen
static constexpr const char* ANSI_SCROLL_UP = "\033[1A";      // move cursor up one line
static constexpr const char* ANSI_SCROLL_DOWN = "\033[1B";      // move cursor down one line
static constexpr const char* ANSI_CLEAR_TO_END = "\x1B[0J";	    // clears from cursor until end of screen
static constexpr const char* ANSI_CLEAR_TO_BEGIN = "\x1B[1J";      // clears from cursor to beginning of screen
static constexpr const char* SHOW_CURSOR = "\033[?25h";
static constexpr const char* HIDE_CURSOR = "\033[?25l";

#undef min
#undef max

enum AnsiColor_ : uint8_t {
    AnsiColor_None = 0,

    AnsiColor_Black,
    AnsiColor_Red,
    AnsiColor_Green,
    AnsiColor_Yellow,
    AnsiColor_Blue,
    AnsiColor_Magenta,
    AnsiColor_Cyan,
    AnsiColor_White,

    AnsiColor_Bright_Black,
    AnsiColor_Bright_Red,
    AnsiColor_Bright_Green,
    AnsiColor_Bright_Yellow,
    AnsiColor_Bright_Blue,
    AnsiColor_Bright_Magenta,
    AnsiColor_Bright_Cyan,
    AnsiColor_Bright_White,

    AnsiColor_Unused,
    AnsiColor_Max,
};

struct Point {
    int x = 0, y = 0;

    void set(int _x, int _y) { x = _x; y = _y; }
    bool operator==(const Point& pt) const { return x == pt.x && y == pt.y; }
    bool operator!=(const Point& pt) const { return x != pt.x || y != pt.y; }
};

struct Rect {
    int x = 0, y = 0;
    int x2 = 0, y2 = 0;

    int width() const { return x2 - x + 1; }
    int height() const { return y2 - y + 1; }
    Point size() const { return { x2 - x + 1 , y2 - y + 1 }; }
    void set(int _x, int _y, int _x2, int _y2) { x = _x; y = _y; x2 = _x2; y2 = _y2; }
    Rect move(int ox, int oy) const { return { x + ox, y + oy, x2 + ox, y2 + oy }; }
    Rect expand(int ox, int oy) const { return { x - ox, y - oy, x2 + ox, y2 + oy }; }
    Rect intersect(const Rect& r) const { return { std::max(x, r.x), std::max(y, r.y), std::min(x2, r.x2), std::min(y2, r.y2)}; }
    bool inside(const Point& pt) const { return pt.x >= x && pt.x <= x2 && pt.y >= y && pt.y <= y2; }
    bool inside(const Rect& r) const { return r.x >= x && r.x2 <= x2 && r.y >= y && r.y2 <= y2; }
    bool collide(const Rect& r) const { return !(r.x2 < x || r.x > x2 || r.y2 < y || r.y > y2); }
};

struct Color {
    uint8_t r = 255, g = 255, b = 255;
    uint8_t ansi = AnsiColor_None;

    Color(uint8_t r, uint8_t g, uint8_t b) :r(r), g(g), b(b), ansi(AnsiColor_None) {}
    Color(AnsiColor_ ansi) :ansi(ansi) {}
    Color() {}

    std::string toAnsi(bool fg) const;
    inline bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && ansi == o.ansi; }
    inline bool operator!=(const Color& o) const { return !operator==(o); }

    static Color Parse(const std::string& param);
};

#define VK_EOF 0x07

struct Selection {
    int start = -1;
    int end = -1;

    bool has_selection() const { return start >= 0 && end >= 0; }
    bool is_selected(int pos) const { return start >= 0 && end >= 0 && pos >= start && pos <= end; }
    void unselect() { start = -1; end = -1; }
    void valid() { if (start > end) std::swap(start, end); }
};

struct Text {
    struct Char {
        union {
            uint8_t code[4];
            uint32_t ch;
        };
        char utf8[4];
        int size;
        int char_width;

        static Char from_code(uint32_t cp);
    };

    std::string text;
    std::vector<Char> chars;
    std::vector<Point> position;

    bool bold = false;
    bool italic = false;
    bool underline = false;
    int text_width = 0;
    int text_height = 0;
    int wrap_width = 0;

    Selection selected;
    Color color_selected;

    virtual void setText(const std::string& _text, int wrap);

    std::string get_selected() const;
    int char_at(int x, int y) const;
    Point pos_of(int idx) const;                // Helper: get display position from char index
    size_t byte_offset_of(int idx) const;       // Helper: get byte offset of char index in the string
    int cur_idx_of(const Point& cursor) const;  // Helper: find current char index from cursor position
    void select_word(int x, int y);             // select word at clicked position
    void reparse();                             // Helper: reparse text without clearing selection
    virtual int delete_selected(int idx);
    virtual int enter(int idx);
    virtual int backspace(int idx);
    virtual int del(int idx);
    virtual int insert(int idx, const std::string text);
    Point home(int idx);
    Point end(int idx);
    Point left(int idx);
    Point right(int idx);
    Point up(int idx);
    Point down(int idx);
};

struct RichText: Text
{
    struct Style {
        Color fg_color = AnsiColor_White;
        Color bg_color = AnsiColor_Unused;      // AnsiColor_Unused: DO NOT fill bg_color
        bool bold = false;
        bool italic = false;
        bool underline = false;
    };

    std::vector<Style> styles;       //style for each character, size=chars.size()

    // [start, end) `start` 和 `end` 是字元索引
    void setStyle(int start, int end, const Color &fgColor, const Color &bgColor, bool bold, bool italic, bool underline);
    void setStyle(int start, int end, const Style &style);
    void setText(const std::string& _text, int wrap) override;
    int delete_selected(int idx) override;
    int enter(int idx) override;
    int backspace(int idx) override;
    int del(int idx) override;
    int insert(int idx, const std::string text) override;

    virtual void appendText(const std::string& _text, const Style &style);

    static Style RichTextStyle(const Color& fg, const Color& bg = AnsiColor_Unused, bool bold = false, bool italic = false, bool underline = false);
};

struct Cell {
    std::string content = " ";
    int size = 1;
    Color fg_color = { AnsiColor_White };
    Color bg_color = { AnsiColor_Black };
    bool bold = false;
    bool italic = false;
    bool underline = false;

    void reset();
    bool operator==(const Cell& o) const;
};

enum BorderStyle_
{
    BorderStyle_None,
    BorderStyle_Single,
    BorderStyle_Double,
    BorderStyle_Round,
};

BorderStyle_ ParseBorderStyle(const std::string& param);

struct DrawBuffer {
    std::vector<Cell> cells_;
    std::vector<Rect> clips_;

    int width_ = 0;
    int height_ = 0;

    void PushClip(const Rect& clip);
    void PopClip();

    void resize(int w, int h);
    void clear();
    void Text(const std::string& text, const Point& pos, const Color& color = AnsiColor_White, bool bold = false, bool italic = false, bool underline = false);
    void Text(const Point& pos, const TUI::Text& text, const Color& color = AnsiColor_White);
    void Text(const Point& pos, const TUI::RichText& text);
    void Border(const Rect& r, const Color& bgcolor, BorderStyle_ style = BorderStyle_Round, const Color& color = AnsiColor_Bright_White);
    void ScrollBar(const Point& pos, int length, int offset, int content_length, bool vertical, const Color& track_color = AnsiColor_White, const Color& thumb_color = AnsiColor_Bright_White);
    void SetColor(const Point& pos, const Color& fgColor, const Color& bgColor);
    void SetBgColor(const Point& pos, const Color& bgColor);
    void FillBgColor(const Rect& r, const Color& bgColor);
};

std::string CursorMove(int x, int y);

enum EventType_
{
    EventType_None,
    EventType_Key,
    EventType_Mouse,
    EventType_Paste,
};

enum Button_
{
    Button_None,
    Button_Left,
    Button_Right,
    Button_Middle,
    Button_ScrollUp,
    Button_ScrollDown,
};

struct Event
{
    EventType_ type = EventType_None;
    uint32_t key = 0;
    uint32_t vkey = 0;
    int button = 0;             // button: 1=left, 2=right, 3=middle, 4=scroll up, 5=scroll down, 6=h-scroll left, 7=h-scroll right
    int x = 0;
    int y = 0;
    int clicks = 0;             // clicks: number of clicks (1=single, 2=double)
    bool first_down[3] = { false };
    bool mouse_motion = false;

    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool is_vt = false;
    bool is_paste_bracket = false;

    std::string paste_text;
    std::string seq;

    void set_key(uint32_t _key, uint32_t _vkey);
    bool any_button_down() const;
    bool any_first_down() const;
    void reset();
    bool parse_sequence(uint32_t ch);
    void parse_csi(uint32_t ch);
    void parse_sgr(uint32_t ch);
};

class Terminal
{
public:
    Terminal();
    ~Terminal() { DisableRawMode(); }

    void EnableRawMode();
    void DisableRawMode();
    Point GetSize();
    void Resize();

    DrawBuffer& GetDrawBuffer() { return drawbuffers[current_drawbuffer]; }
    void Render();
    static void GetEvent(std::vector<Event> &events);
private:
    int current_drawbuffer = 0;
    DrawBuffer drawbuffers[2];

public:
    /// Whether the watcher thread should keep running.
    static std::atomic<bool> s_running;
    static std::thread* s_event_thread;
    static std::mutex   s_event_mutex;
    static std::vector<Event> s_events;
    static void event_thread();
};

struct EditLine {
    std::vector<std::string> lines;
    int current = -1;
    int tok_ = 0;

    /// Read from file — uses split_lines semantics.
    bool read_file(const std::string& path);

    /// Parse text into lines (e.g. new_text input). Uses split_lines.
    void parse(const std::string& text);

    std::string next_line();
    std::string next_tok(std::string delims = " \t");
    std::string tok(std::string delims = " \t");
    std::string tok_line();
    int tok_int(std::string delims = " \t");
    bool tok_bool(std::string delims = " \t");
};

struct Mgr;
struct Win;
using WinPtr = std::shared_ptr<Win>;

enum Align_
{
    Align_Start,
    Align_Center,
    Align_End,
};

Align_ ParseAlign(const std::string& tok);

// Dock direction flags (bitwise combinable)
enum Dock_
{
    Dock_None,        // no docking
    Dock_Top = 1,     // dock to top edge
    Dock_Left = 2,    // dock to left edge
    Dock_Right = 4,   // dock to right edge
    Dock_Down = 8,    // dock to bottom edge
    Dock_All = Dock_Top | Dock_Left | Dock_Right | Dock_Down,  // stretch fill all edges
    Dock_Top_Pane = 16,
    Dock_Left_Pane = 32,
    Dock_Right_Pane = 64,
    Dock_Down_Pane = 128,
};

Dock_ ParseDock(const std::string& tok);

// Docking configuration: how a widget anchors inside its parent
struct Dock {
    Dock_ mode = Dock_None;       // docking direction(s)
    Rect dock = { 0,0,100,100 };  // widget rect within parent (x,y,w,h) in percent
    Rect offset = { 0,0,0,0 };    // offset from anchored position
};

// Arrange mode: how child items are laid out inside a container
enum Arrange_
{
    Arrange_None,       // no arrangement
    Arrange_Item,       // fixed-size item layout
    Arrange_Content,    // auto-fit by content size
};

Arrange_ ParseArrange(const std::string& tok);

// Arrangement configuration for child widgets
struct Arrange
{
    Arrange_ mode = Arrange_None;  // arrangement mode
    bool is_vertical = true;       // vertical (true) or horizontal (false)
    int items = 0;                 // number of items
    Point item_size = { 0,0 };     // size per item (w,h)
};

enum Autosize_
{
    Autosize_None,
    Autosize_TextWidth,
    Autosize_TextHeight,
    Autosize_TextSize,
};

Autosize_ ParseAutosize(const std::string& tok);

struct Win
{
    Win(Mgr* mgr) : mgr(mgr) {}

    virtual bool Parse(EditLine& el);
    virtual bool ParseCmd(const std::string& cmd, EditLine& el);
    virtual void CalRect(Win* parent);
    virtual void Paint(DrawBuffer& drawbuf);
    virtual void PaintBorder(DrawBuffer& drawbuf);
    virtual void PaintChild(DrawBuffer& drawbuf);
    virtual WinPtr GetNotify(const Point& pt);
    virtual WinPtr GetSlider(const Point& pt);
    virtual WinPtr GetUI(const std::string &name);
    virtual void Copy(const Win* ob);
    virtual WinPtr Clone() const;
    virtual void AddChild(WinPtr obj);
    virtual void RemoveChild(const WinPtr& obj);
    virtual void Click() { if (on_click) on_click(); }
    virtual bool IsSlider() const { return false; }
    virtual bool Event(const Event& ev) { return false; }
    virtual Point GetClipPos() const;
    virtual Point GetTextSize() const { return { 0,0 }; }
    virtual void SetVisible(bool visible);
    virtual void OnSize() {}

    template<class T>
    std::shared_ptr<T> GetUI(const char* name) {
        return std::dynamic_pointer_cast<T>(GetUI(std::string(name)));
    }
    template<class T>
    std::shared_ptr<T> Create(const std::string& name = "", const Rect& r = {}) {
        auto ptr = WinPtr(new T(mgr));
        ptr->name = name;
        ptr->local = r;
        AddChild(ptr);
        return std::dynamic_pointer_cast<T>(ptr);
    }

    std::string name = "";
    std::string title = "";
    bool is_visible = true;
    bool is_notifiable = true;
    bool is_down = false;
    bool draw_border = false;
    bool is_cloneable = true;
    BorderStyle_ border_style = BorderStyle_Round;
    Color bg_color = COLOR_BG;
    Color fg_color = AnsiColor_White;
    Dock dock_;
    Arrange arrange_;
    Autosize_ autosize_ = Autosize_None;

    Rect local;
    Rect screen;
    Rect clip;

    std::vector<WinPtr> child;

    Mgr* mgr = nullptr;
    
    using fn_click = std::function<void()>;
    using fn_key = std::function<bool(const TUI::Event& evt)>;

    fn_click on_click;
    fn_key on_key;

    static Color COLOR_BG;
    static Color COLOR_HOVER;
    static Color COLOR_DOWN;
    static Color COLOR_BTN;
    static Color COLOR_TRACK;
    static Color COLOR_THUMB;
    static Color COLOR_SELECTED;
    static Color COLOR_CURSOR;
    static Color COLOR_CURSOR_BG;
    static Color COLOR_LABEL;
    static Color COLOR_BUTTON;
    static Color COLOR_CHECKED;
};

struct Label : Win, Text
{
    Label(Mgr* mgr);

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void Paint(DrawBuffer& drawbuf) override;
    virtual void PaintText(DrawBuffer& drawbuf);
    Point GetTextSize() const override;
    void setText(const std::string& _text);
    void Copy(const Win* ob) override;
    WinPtr Clone() const override;

    Align_ text_algn = Align_Start;

    using fn_text = std::function<void(const std::string &text)>;
    fn_text on_text;
};

using LabelPtr = std::shared_ptr<Label>;

struct Button : Label
{
    Button(Mgr* mgr);
    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void PaintBorder(DrawBuffer& drawbuf) override;
    void Copy(const Win* ob) override;
    WinPtr Clone() const override;

    Color bg_color_hover = COLOR_HOVER;
    Color bg_color_down = COLOR_DOWN;
};

using ButtonPtr = std::shared_ptr<Button>;

struct Check : Button
{
    Check(Mgr* mgr);
    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void PaintText(DrawBuffer& drawbuf) override;
    void Click() override;
    void Copy(const Win* ob) override;
    WinPtr Clone() const override;
    void SetChecked(bool checked);

    struct CheckText {
        std::string checked;
        std::string unchecked;
    };

    bool checked = false;
    Color fg_color_checked = COLOR_CHECKED;
    CheckText check_text = {};

    using fn_check = std::function<void(bool)>;
    fn_check on_check;
};

using CheckPtr = std::shared_ptr<Check>;

struct Combo : Button
{
    Combo(Mgr* mgr);

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void PaintText(DrawBuffer& drawbuf) override;
    void Click() override;
    void Copy(const Win* ob) override;
    WinPtr Clone() const override;
    void SetValue(int value);

    int value = -1;
    std::vector<std::string> items;
    WinPtr menu;

    using fn_selected = std::function<void(int)>;
    fn_selected on_selected;
};

using ComboPtr = std::shared_ptr<Combo>;

struct Slider : Win
{
    Slider(Mgr* mgr);

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void CalRect(Win* parent) override;
    void Paint(DrawBuffer& drawbuf) override;
    bool IsSlider() const override { return (is_scroll_x || is_scroll_y); }
    bool Event(const TUI::Event& ev) override;
    Point GetClipPos() const override;
    virtual void PaintScrollBar(DrawBuffer& drawbuf);
    virtual void UpdateScrollMax();
    virtual int ScrollTo(int percent);  // 0~100 percent
    void Copy(const Win* ob) override;
    WinPtr Clone() const override;
    void AddChild(WinPtr obj) override;

    bool is_scroll_x = false;
    bool is_scroll_y = true;
    Point scroll_value = { 0,0 };
    Point scroll_max = { 0,0 };
    Point content_length = { 0,0 };
    Color color_track = COLOR_TRACK;
    Color color_thumb = COLOR_THUMB;
    int scroll_speed = 3;
};

using SliderPtr = std::shared_ptr<Slider>;

struct Edit : Slider, Text
{
    Edit(Mgr* mgr);

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void CalRect(Win* parent) override;
    Point GetTextSize() const override;
    void Paint(DrawBuffer& drawbuf) override;
    virtual void PaintText(DrawBuffer& drawbuf);
    void setText(const std::string& _text);
    void OnSize() override;
    bool Event(const TUI::Event& ev) override;
    void Copy(const Win* ob) override;
    WinPtr Clone() const override;
    void UpdateScrollMax() override;

    Point cursor;
    int drag_start = -1;  // character index where drag selection started
    bool readonly = false;

    using fn_edit = std::function<void (Edit* edit, const std::string& text)>;    
    fn_edit on_edit;
};

using EditPtr = std::shared_ptr<Edit>;

struct LabelEdit : Label
{
    LabelEdit(Mgr* mgr);

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void Copy(const Win* ob) override;
    WinPtr Clone() const override;

    void SetValue(const std::string& value);
    void Text(const std::string& label, std::string& value, Label::fn_text ontext = nullptr);
    void Input(const std::string& label, std::string& value, Edit::fn_edit onedit = nullptr);
    void Input(const std::string& label, uint32_t& value, uint32_t step = 1, Edit::fn_edit onedit = nullptr);
    void Button(const std::string& label, const std::string& value, fn_click onclick);
    void Check(const std::string& label, bool& value, const Check::CheckText & check_text = {}, Check::fn_check oncheck = nullptr);
    void Combo(const std::string& label, int& value, const std::vector<std::string>& items, Combo::fn_selected onselect = nullptr);

    enum Type_ {
        Type_None,
        Type_Label,
        Type_Button,
        Type_Edit,
        Type_Check,
        Type_Combo,
    };

    int label_width = 16;
    Color bg_color_hover = COLOR_HOVER;
    Type_ type = Type_None;
    WinPtr control;
};

using LabelEditPtr = std::shared_ptr<LabelEdit>;

struct RichEdit : Slider, RichText
{
    RichEdit(Mgr *mgr);

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void CalRect(Win* parent) override;
    Point GetTextSize() const override;
    void Paint(DrawBuffer& drawbuf) override;
    void PaintText(DrawBuffer& drawbuf);
    void setText(const std::string& _text);
    void appendText(const std::string& _text, const Style& style) override;
    void OnSize() override;
    bool Event(const TUI::Event& ev) override;
    void Copy(const Win* ob) override;
    WinPtr Clone() const override;
    void UpdateScrollMax() override;

    Point cursor;
    int drag_start = -1;
    bool readonly = false;

    using fn_edit = std::function<void(RichEdit* edit, const std::string& text)>;
    fn_edit on_edit;
    RichText::Style current_style;
};

using RichEditPtr = std::shared_ptr<RichEdit>;

struct MarkdownStyle {
    RichText::Style text;
    RichText::Style bold_text;
    RichText::Style italic_text;
    RichText::Style code;
    RichText::Style horizontal_rule;
    std::string horizontal_rule_char = u8"─";
    RichText::Style quote;
    std::string quote_prefix = u8"│ ";

    RichText::Style table_header;
    RichText::Style table_cell;
    RichText::Style table_border_style;
    RichText::Style table_separator;

    RichText::Style heading[6];
    RichText::Style heading_rule[6];
    bool heading_rule_enabled[6] = { true, true, true, false, false, false };
    std::string heading_rule_char[6] = { u8"═", u8"─", u8"─", u8"·", u8"·", u8"·" };

    struct Replacement {
        std::string source;
        std::string text;
        RichText::Style style;
    };

    std::vector<Replacement> replacements;

    static std::string table_border[];
    static MarkdownStyle DEFAULT;
};

void Markdown(RichEdit* edit, const std::string& md, const MarkdownStyle& style = MarkdownStyle::DEFAULT);

struct Syntax {
    struct Rule {
        std::string text;
        RichText::Style style;
    };

    struct Delimiter {
        std::string start;
        std::string end;
        RichText::Style style;
        bool escape = true;
    };

    std::vector<Rule> keywords;
    std::vector<Delimiter> literals;

    std::string line_comment_start;
    std::vector<std::string> line_comment_starts;
    std::string block_comment_start;
    std::string block_comment_end;
    bool supports_preprocessor = false;
    bool ini_mode = false;

    RichText::Style normal;
    RichText::Style comment;
    RichText::Style keyword;
    RichText::Style section;
    RichText::Style key;
    RichText::Style value;
    RichText::Style constant;
    RichText::Style string_literal;
    RichText::Style number;
    RichText::Style preprocessor;

    static Syntax CPP;
    static Syntax JS;
    static Syntax TS;
    static Syntax Go;
    static Syntax Rust;
    static Syntax Python;
    static Syntax JSON;
    static Syntax INI;
};

void SyntaxText(RichEdit* edit, const std::string& text, Syntax syntax = Syntax::CPP);
void SyntaxText(RichEdit* edit, const std::string& text, const std::string& filename);

struct Mgr : Win
{
    Mgr();
    bool Parse(std::string content);
    bool Update(Terminal& terminal);
    void Paint(DrawBuffer& drawbuf) override;
    void Popup(WinPtr ob);
    void ClosePopup();
    WinPtr NextNotify();
    void SetNotify(WinPtr ob);
    void Navigator(const TUI::Event &ev);

    bool is_dirty = true;
    WinPtr notify_ = nullptr;     //input focus
    WinPtr hover_ = nullptr;
    WinPtr hover_slider_ = nullptr;
    WinPtr popup_ = nullptr;
    Point cursor;
    std::vector<WinPtr> paint_list;

    static WinPtr CreateByID(std::string csid, Mgr *mgr);
};

using MgrPtr = std::shared_ptr<Mgr>;

ButtonPtr GetButton(WinPtr ob, Win::fn_click click, const char* find = nullptr);
CheckPtr GetCheck(WinPtr ob, bool value, Check::fn_check check, const char* find = nullptr);
EditPtr GetEdit(WinPtr ob, const std::string& text, Edit::fn_edit edit, const char* find = nullptr);


NAMESPACE_END
