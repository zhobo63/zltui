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

    // Foreground colors
    AnsiColor_Fg_Black,
    AnsiColor_Fg_Red,
    AnsiColor_Fg_Green,
    AnsiColor_Fg_Yellow,
    AnsiColor_Fg_Blue,
    AnsiColor_Fg_Magenta,
    AnsiColor_Fg_Cyan,
    AnsiColor_Fg_White,

    // Bright foreground colors
    AnsiColor_Bright_Black,
    AnsiColor_Bright_Red,
    AnsiColor_Bright_Green,
    AnsiColor_Bright_Yellow,
    AnsiColor_Bright_Blue,
    AnsiColor_Bright_Magenta,
    AnsiColor_Bright_Cyan,
    AnsiColor_Bright_White,

    // Background colors
    AnsiColor_Bg_Black,
    AnsiColor_Bg_Red,
    AnsiColor_Bg_Green,
    AnsiColor_Bg_Yellow,
    AnsiColor_Bg_Blue,
    AnsiColor_Bg_Magenta,
    AnsiColor_Bg_Cyan,
    AnsiColor_Bg_White,

    // Bright background colors
    AnsiColor_BrightBg_Black,
    AnsiColor_BrightBg_Red,
    AnsiColor_BrightBg_Green,
    AnsiColor_BrightBg_Yellow,
    AnsiColor_BrightBg_Blue,
    AnsiColor_BrightBg_Magenta,
    AnsiColor_BrightBg_Cyan,
    AnsiColor_BrightBg_White,
    AnsiColor_Max,
};

struct Point {
    int x = 0, y = 0;
};

struct Rect {
    int x = 0, y = 0;
    int x2 = 0, y2 = 0;

    int width() const { return x2 - x; }
    int height() const { return y2 - y; }
};

struct Color {
    uint8_t r = 255, g = 255, b = 255;
    uint8_t ansi = AnsiColor_None;

    Color(uint8_t r, uint8_t g, uint8_t b) :r(r), g(g), b(b), ansi(AnsiColor_None) {}
    Color(AnsiColor_ ansi) :ansi(ansi) {}
    Color() {}

    std::string toAnsi() const;
    inline bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && ansi == o.ansi; }
    inline bool operator!=(const Color& o) const { return !operator==(o); }

    static const Color TRACK;
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

    Color fg_color = { AnsiColor_Fg_White };
    Color bg_color = { AnsiColor_Bg_Black };
    bool bold = false;
    bool italic = false;
    bool underline = false;

    void setText(const std::string& _text);
};

struct Cell {
    std::string content = " ";
    int size = 1;
    Color fg_color = { AnsiColor_Fg_White };
    Color bg_color = { AnsiColor_Bg_Black };
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

struct DrawBuffer {
    std::vector<Cell> cells_;

    int width_ = 0;
    int height_ = 0;

    void resize(int w, int h);
    void clear();
    void Text(const std::string& text, const Point& pos, const Color& color = AnsiColor_Fg_White, bool bold = false, bool italic = false, bool underline = false);
    void Border(const Rect& r, const Color& bgcolor, BorderStyle_ style = BorderStyle_Round, const Color& color = AnsiColor_Bright_White);
    void ScrollBar(const Point& pos, int length, int offset, int content_length, bool vertical, const Color& track_color = AnsiColor_BrightBg_Black, const Color& thumb_color = AnsiColor_BrightBg_White);
};

std::string CursorMove(int x, int y);

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
    static void event_thread();
};

NAMESPACE_END
