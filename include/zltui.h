#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#endif

#include <thread>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

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

struct Selection {
    int start = -1;
    int end = -1;

    bool has_selection() const { return start >= 0 && end >= 0; }
    bool is_selected(int pos) const { return start >= 0 && end >= 0 && pos >= start && pos <= end; }
    void unselect() { start = -1; end = -1; }
    void valid() { if (start > end) std::swap(start, end); }
};

struct Text {
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

    void setText(const std::string& _text, int wrap);
    std::string get_selected() const;
    int char_at(int x, int y) const;
    Point pos_of(int idx) const;                // Helper: get display position from char index
    size_t byte_offset_of(int idx) const;       // Helper: get byte offset of char index in the string
    int cur_idx_of(const Point& cursor) const;  // Helper: find current char index from cursor position
    void select_word(int x, int y);             // select word at clicked position
    void reparse();                             // Helper: reparse text without clearing selection
    int delete_selected(int idx);
    int enter(int idx);
    int backspace(int idx);
    int del(int idx);
    int insert(int idx, const std::string text);
    Point home(int idx);
    Point end(int idx);
    Point left(int idx);
    Point right(int idx);
    Point up(int idx);
    Point down(int idx);
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
    inline bool operator==(const Cell& o) const {
        return size == o.size && bold == o.bold && italic == o.italic && underline == o.underline &&
            fg_color == o.fg_color && bg_color == o.bg_color && content == o.content;
    }
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

    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool is_vt = false;
    bool is_paste_bracket = false;

    std::string paste_text;
    std::string seq;

    void set_key(uint32_t _key, uint32_t _vkey) { type = EventType_Key; key = _key; vkey = _vkey; }
    bool any_button_down() const { return type == EventType_Mouse && button >= 1 && button <= 3; }
    bool any_first_down() const { return first_down[0] || first_down[1] || first_down[2]; }
    void reset() { type = EventType_None; seq.clear(); paste_text.clear(); key = 0; vkey = 0; is_vt = false; is_paste_bracket = false; }
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
    virtual Win* GetNotify(const Point& pt);
    virtual Win* GetSlider(const Point& pt);
    virtual Win* GetUI(const std::string &name);
    virtual void Copy(const Win* ob);
    virtual Win* Clone() const;

    template<class T>
    T* GetUI(const char *name) {
        return dynamic_cast<T*>(GetUI(std::string(name)));
    }
    virtual void AddChild(WinPtr obj);
    virtual void Click() { if (on_click) on_click(); }
    virtual bool IsSlider() const { return false; }
    virtual void Event(const Event& ev) {}
    virtual Point GetClipPos() const;
    virtual Point GetTextSize() const { return { 0,0 }; }

    std::string name = "";
    std::string title = "";
    bool is_visible = true;
    bool is_notifiable = true;
    bool is_notify = false;
    bool is_down = false;
    bool draw_border = false;
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
    std::function<void()> on_click;

    static Color COLOR_BG;
    static Color COLOR_HOVER;
    static Color COLOR_DOWN;
    static Color COLOR_BTN;
    static Color COLOR_TRACK;
    static Color COLOR_THUMB;
    static Color COLOR_SELECTED;
};

struct Label : Win, Text
{
    Label(Mgr* mgr) :Win(mgr) {}

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void Paint(DrawBuffer& drawbuf) override;
    virtual void PaintText(DrawBuffer& drawbuf);
    Point GetTextSize() const override;
    void setText(const std::string& _text);
    void Copy(const Win* ob) override;
    Win* Clone() const override;

    Align_ text_algn = Align_Start;
};

struct Button : Label
{
    Button(Mgr* mgr);
    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void PaintBorder(DrawBuffer& drawbuf) override;
    void Copy(const Win* ob) override;
    Win* Clone() const override;

    Color bg_color_hover = COLOR_HOVER;
    Color bg_color_down = COLOR_DOWN;
};

struct Check : Button
{
    Check(Mgr* mgr);
    void PaintText(DrawBuffer& drawbuf) override;
    void Click() override;
    void Copy(const Win* ob) override;
    Win* Clone() const override;

    bool checked = false;
    std::function<void(bool)> on_check;
};

struct Slider : Win
{
    Slider(Mgr* mgr) :Win(mgr) {}

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void CalRect(Win* parent) override;
    void Paint(DrawBuffer& drawbuf) override;
    bool IsSlider() const override { return (is_scroll_x || is_scroll_y); }
    void Event(const TUI::Event& ev) override;
    Point GetClipPos() const override;
    virtual void PaintScrollBar(DrawBuffer& drawbuf);
    void Copy(const Win* ob) override;
    Win* Clone() const override;

    bool is_scroll_x = false;
    bool is_scroll_y = true;
    Point scroll_value = { 0,0 };
    Point scroll_max = { 0,0 };
    Point content_length = { 0,0 };
    Color color_track = COLOR_TRACK;
    Color color_thumb = COLOR_THUMB;
};

struct Edit : Slider, Text
{
    Edit(Mgr* mgr);

    bool ParseCmd(const std::string& cmd, EditLine& el) override;
    void Paint(DrawBuffer& drawbuf) override;
    void PaintText(DrawBuffer& drawbuf);
    void setText(const std::string& _text);
    void Event(const TUI::Event& ev) override;
    void Copy(const Win* ob) override;
    Win* Clone() const override;

    Point cursor;
    int drag_start = -1;  // character index where drag selection started
    bool readonly = false;

    std::function<bool(const TUI::Event &evt)> on_key;
};

struct Mgr : Win
{
    Mgr() :Win(this) { draw_border = false; }
    WinPtr Create(std::string csid);
    bool Parse(std::string content);
    bool Update(Terminal& terminal);
    void Paint(DrawBuffer& drawbuf) override;

    bool is_dirty = true;
    Win* notify_ = nullptr;     //input focus
    Win* hover_ = nullptr;
    Win* hover_slider_ = nullptr;
    Point cursor;
};

NAMESPACE_END
