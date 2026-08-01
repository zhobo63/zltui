#pragma once

#ifdef _WIN32
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
};

struct Rect {
    int x = 0, y = 0;
    int x2 = 0, y2 = 0;

    int width() const { return x2 - x; }
    int height() const { return y2 - y; }
    void set(int _x, int _y, int _x2, int _y2) { x = _x; y = _y; x2 = _x2; y2 = _y2; }
    Rect move(int ox, int oy) { return { x + ox, y + oy, x2 + ox, y2 + oy }; }
    Rect expand(int ox, int oy) { return { x - ox, y - oy, x2 + ox, y2 + oy }; }
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
    static const Color TRACK;
    static const Color THUMB;
};

struct Char {
    union {
        uint8_t code[4];
        uint32_t ch;
    };
    int size;
    int char_width;

    static Char from_code(uint32_t cp);
};

struct Text {
    std::string text;
    std::vector<Char> chars;

    Color fg_color = { AnsiColor_White };
    bool bold = false;
    bool italic = false;
    bool underline = false;

    void setText(const std::string& _text);
};

struct Cell {
    std::string content = " ";
    int size = 1;
    Color fg_color = { AnsiColor_White };
    Color bg_color = { AnsiColor_Black };
    bool bold = false;       
    bool italic = false;     
    bool underline = false;  

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
    void Text(const Point& pos, const TUI::Text& text);
    void Border(const Rect& r, const Color& bgcolor, BorderStyle_ style = BorderStyle_Round, const Color& color = AnsiColor_Bright_White);
    void ScrollBar(const Point& pos, int length, int offset, int content_length, bool vertical, const Color& track_color = Color::TRACK, const Color& thumb_color = Color::THUMB);
};

std::string CursorMove(int x, int y);

enum EventType_
{
    EventType_None,
    EventType_Key,
    EventType_Mouse,
    EventType_Paste,
};

struct Event
{
    EventType_ type = EventType_None;
    uint32_t key = 0;
    int button = 0;
    int x = 0;
    int y = 0;
    int clicks = 0;
    std::string paste_text;

    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

class Terminal
{
public:
    Terminal();
    ~Terminal() { DisableRawMode(); }

    void EnableRawMode();
    void DisableRawMode();
    Point GetSize();

    DrawBuffer& GetDrawBuffer() { return drawbuffers[current_drawbuffer]; }
    void Render();
private:
    int current_drawbuffer = 0;
    DrawBuffer drawbuffers[2];
#ifdef _WIN32
    DWORD originalOutMode_ = 0;
    DWORD originalInMode_ = 0;
    UINT originalOutCP_ = 0;
    UINT originalInCP_ = 0;
    bool vtSupported_ = false;
    bool vtInputSupported_ = false;
#else
    struct termios originalTermios_;
#endif

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
    int tok_int(std::string delims = " \t");
    bool tok_bool(std::string delims = " \t");
};

struct Mgr;
struct Win;
using WinPtr = std::shared_ptr<Win>;

struct Win
{
    Win(Mgr* mgr) : mgr(mgr) {}

    virtual bool Parse(EditLine& el);
    virtual bool ParseCmd(const std::string& cmd, EditLine& el);
    virtual void CalRect(Win* parent);
    virtual void Paint(DrawBuffer& drawbuf);
    virtual void PaintChild(DrawBuffer& drawbuf);

    std::string name = "";
    bool is_visible = true;
    bool draw_border = false;
    BorderStyle_ border_style = BorderStyle_Round;
    Color bg_color = Color(30, 30, 30);
    Color fg_color = AnsiColor_White;

    Rect local;
    Rect screen;
    Rect clip;

    std::vector<WinPtr> child;

    Mgr* mgr = nullptr;
};

struct Button : Win
{
    Button(Mgr* mgr) :Win(mgr) {}

    Color bg_color_hover = Color(50, 50, 50);
    Color bg_color_down = Color(70, 70, 70);
};

struct Slider : Win
{
    Slider(Mgr* mgr) :Win(mgr) {}

    bool is_vertical = true;
    int scroll_value = 0;
};

struct Edit : Slider
{
    Edit(Mgr* mgr) :Slider(mgr) {}
};

struct Mgr : Win
{
    Mgr() :Win(this) { draw_border = false; }
    WinPtr Create(std::string csid);
    bool Parse(std::string content);
    void Paint(DrawBuffer& drawbuf) override;
};

NAMESPACE_END
