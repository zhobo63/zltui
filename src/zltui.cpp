#include "zltui.h"
#include <iostream>

#undef max
#undef min

NAMESPACE_BEGIN(TUI)

/// Compute the display width of a single UTF-8 character (codepoint).
/// CJK and other wide characters return 2; ASCII returns 1.
static int utf8_char_width(uint32_t cp) {
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) return 0; // control chars
    if (cp < 0x1100) return 1;
    // CJK, Hangul, etc. — wide characters
    if ((cp >= 0x1100 && cp <= 0x115F) ||   // Hangul Jamo
        (cp >= 0x2E80 && cp <= 0xA4CF) ||   // CJK radicals, Kangxi, etc.
        (cp >= 0xAC00 && cp <= 0xD7A3) ||   // Hangul syllables
        (cp >= 0xF900 && cp <= 0xFAFF) ||   // CJK compatibility
        (cp >= 0xFE10 && cp <= 0xFE6F) ||   // Vertical forms, small forms
        (cp >= 0xFF00 && cp <= 0xFF60) ||   // Fullwidth ASCII variants
        (cp >= 0xFFE0 && cp <= 0xFFE6)) {
        return 2;
    }
    return 1;
}


static int utf8_mbtowc(uint32_t& cp, const uint8_t* s, int len)
{
    uint32_t c=(uint32_t)s[0];
    if ((c & 0x80) == 0) {
        cp=c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        if (len <= 1) return 0;
        cp = (c & 0x1F) << 6;
        cp |= (s[1] & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        if (len <= 2) return 0;
        cp = (c & 0x0F) << 12;
        cp |= (s[1] & 0x3F) << 6;
        cp |= (s[2] & 0x3F);
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        if (len <= 3) return 0;
        cp = (c & 0x07) << 18;
        cp |= (s[1] & 0x3F) << 12;
        cp |= (s[2] & 0x3F) << 6;
        cp |= (s[3] & 0x3F);
        return 4;
    }else {
        cp=c;
        return 1;
    }
}

std::string CursorMove(int x, int y)
{
    return "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
}

const Color Color::TRACK(46, 46, 46);

std::string Color::toAnsi() const
{
    if (ansi == AnsiColor_None) {
        return "\033[38;2;" + std::to_string(r) + ";" +
               std::to_string(g) + ";" +
               std::to_string(b) + "m";
    }

    switch (ansi) {
        case AnsiColor_Fg_Black:     return ANSI_FG_BLACK;
        case AnsiColor_Fg_Red:       return ANSI_FG_RED;
        case AnsiColor_Fg_Green:     return ANSI_FG_GREEN;
        case AnsiColor_Fg_Yellow:    return ANSI_FG_YELLOW;
        case AnsiColor_Fg_Blue:      return ANSI_FG_BLUE;
        case AnsiColor_Fg_Magenta:   return ANSI_FG_MAGENTA;
        case AnsiColor_Fg_Cyan:      return ANSI_FG_CYAN;
        case AnsiColor_Fg_White:     return ANSI_FG_WHITE;

        case AnsiColor_Bright_Black:     return ANSI_BRIGHT_BLACK;
        case AnsiColor_Bright_Red:       return ANSI_BRIGHT_RED;
        case AnsiColor_Bright_Green:     return ANSI_BRIGHT_GREEN;
        case AnsiColor_Bright_Yellow:    return ANSI_BRIGHT_YELLOW;
        case AnsiColor_Bright_Blue:      return ANSI_BRIGHT_BLUE;
        case AnsiColor_Bright_Magenta:   return ANSI_BRIGHT_MAGENTA;
        case AnsiColor_Bright_Cyan:      return ANSI_BRIGHT_CYAN;
        case AnsiColor_Bright_White:     return ANSI_BRIGHT_WHITE;

        case AnsiColor_Bg_Black:       return ANSI_BG_BLACK;
        case AnsiColor_Bg_Red:         return ANSI_BG_RED;
        case AnsiColor_Bg_Green:       return ANSI_BG_GREEN;
        case AnsiColor_Bg_Yellow:      return ANSI_BG_YELLOW;
        case AnsiColor_Bg_Blue:        return ANSI_BG_BLUE;
        case AnsiColor_Bg_Magenta:     return ANSI_BG_MAGENTA;
        case AnsiColor_Bg_Cyan:        return ANSI_BG_CYAN;
        case AnsiColor_Bg_White:       return ANSI_BG_WHITE;

        case AnsiColor_BrightBg_Black:    return ANSI_BRIGHT_BG_BLACK;
        case AnsiColor_BrightBg_Red:      return ANSI_BRIGHT_BG_RED;
        case AnsiColor_BrightBg_Green:    return ANSI_BRIGHT_BG_GREEN;
        case AnsiColor_BrightBg_Yellow:   return ANSI_BRIGHT_BG_YELLOW;
        case AnsiColor_BrightBg_Blue:     return ANSI_BRIGHT_BG_BLUE;
        case AnsiColor_BrightBg_Magenta:  return ANSI_BRIGHT_BG_MAGENTA;
        case AnsiColor_BrightBg_Cyan:     return ANSI_BRIGHT_BG_CYAN;
        case AnsiColor_BrightBg_White:    return ANSI_BRIGHT_BG_WHITE;

        default: return "";
    }
}

Char Char::from_code(uint32_t cp)
{
    Char c;
    c.ch = cp;
    c.char_width = utf8_char_width(cp);
    if (cp < 0x80) {
        c.size = 1;
    }
    else if (cp < 0x800) {
        c.size = 2;
    }
    else if (cp < 0x10000) {
        c.size = 3;
    }
    else {
        c.size = 4;
    }
    return c;
}

void Text::setText(const std::string& _text)
{
    text = _text;
    chars.resize(0);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    size_t len = text.size();
    while (len > 0) {
        uint32_t cp = 0;
        int n = utf8_mbtowc(cp, p, static_cast<int>(len));
        if (n <= 0) break;
        chars.push_back(Char::from_code(cp));
        p += n; len -= n;
    }
}

void DrawBuffer::resize(int w, int h)
{
    width_ = w;
    height_ = h;
    cells_.resize(w * h);
    clear();
}

void DrawBuffer::clear()
{
    for (auto& cell : cells_) {
        cell = {};
    }
}

void DrawBuffer::Text(const std::string& text, const Point& pos, const Color& color, bool bold, bool italic, bool underline)
{
    // skip if position is already out of bounds
    if (pos.x < 0 || pos.y < 0 || pos.y >= height_) return;

    int px = pos.y * width_ + pos.x;
    int cw = 0;

    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    size_t len = text.size();
    while (len > 0) {
        // boundary check: stop if we've gone past the row end
        if (pos.x + cw >= width_) break;

        uint32_t cp = 0;
        int n = utf8_mbtowc(cp, p, static_cast<int>(len));
        if (n <= 0) break;

        int char_width = utf8_char_width(cp);

        // skip wide characters that would overflow the row edge
        if (pos.x + cw + char_width > width_) break;

        auto& cell = cells_[px + cw];
        cell.fg_color = color;
        cell.size = char_width;
        cell.bold = bold;
        cell.italic = italic;
        cell.underline = underline;
        cell.content = std::string((const char*)p, (size_t)n);
        if (cell.size > 1) {
            cells_[px + cw + 1].content = "";
        }
        cw += cell.size;
        p += n; len -= n;
    }
}

void DrawBuffer::Border(const Rect& r, const Color& bgcolor, BorderStyle_ style, const Color& color)
{
    std::string h_line, v_line, tl, tr, bl, br;
    switch (style) {
    case BorderStyle_None:

        break;
    case BorderStyle_Single:
        h_line = u8"\u2500";
        v_line = u8"\u2502";
        tl = u8"\u250C";
        tr = u8"\u2510";
        bl = u8"\u2514";
        br = u8"\u2518";
        break;
    case BorderStyle_Double:
        h_line = u8"\u2550";
        v_line = u8"\u2551";
        tl = u8"\u2554";
        tr = u8"\u2557";
        bl = u8"\u255A";
        br = u8"\u255D";
        break;
    case BorderStyle_Round:
        h_line = u8"\u2500";
        v_line = u8"\u2502";
        tl = u8"\u256D";
        tr = u8"\u256E";
        bl = u8"\u2570";
        br = u8"\u256F";
        break;
    }
    if (style != BorderStyle_None) {
        // Draw top-left corner
        if (r.x >= 0 && r.y >= 0) {
            auto& cell = cells_[r.y * width_ + r.x];
            cell.content = tl;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw top-right corner
        if (r.x2 >= 0 && r.y >= 0) {
            auto& cell = cells_[r.y * width_ + r.x2];
            cell.content = tr;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw bottom-left corner
        if (r.x >= 0 && r.y2 >= 0) {
            auto& cell = cells_[r.y2 * width_ + r.x];
            cell.content = bl;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw bottom-right corner
        if (r.x2 >= 0 && r.y2 >= 0) {
            auto& cell = cells_[r.y2 * width_ + r.x2];
            cell.content = br;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }

        // Draw top and bottom horizontal lines (excluding corners)
        for (int x = r.x + 1; x < r.x2; x++) {
            if (x >= 0 && r.y >= 0) {
                auto& cell = cells_[r.y * width_ + x];
                cell.content = h_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
            if (x >= 0 && r.y2 >= 0) {
                auto& cell = cells_[r.y2 * width_ + x];
                cell.content = h_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
        }

        // Draw left and right vertical lines (excluding corners)
        for (int y = r.y + 1; y < r.y2; y++) {
            if (r.x >= 0 && y >= 0) {
                auto& cell = cells_[y * width_ + r.x];
                cell.content = v_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
            if (r.x2 >= 0 && y >= 0) {
                auto& cell = cells_[y * width_ + r.x2];
                cell.content = v_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
        }
    }
    // Fill with background: entire rect for None, interior only otherwise
    int fill_y_start = (style == BorderStyle_None) ? r.y : r.y + 1;
    int fill_x_start = (style == BorderStyle_None) ? r.x : r.x + 1;
    int fill_y_end   = (style == BorderStyle_None) ? r.y2 : r.y2 - 1;
    int fill_x_end   = (style == BorderStyle_None) ? r.x2 : r.x2 - 1;
    for (int y = fill_y_start; y <= fill_y_end; y++) {
        for (int x = fill_x_start; x <= fill_x_end; x++) {
            if (x >= 0 && y >= 0) {
                auto& cell = cells_[y * width_ + x];
                cell.content = " ";
                cell.bg_color = bgcolor;
            }
        }
    }
}

void DrawBuffer::ScrollBar(const Point& pos, int length, int offset, int content_length, bool vertical, const Color& track_color, const Color& thumb_color)
{
    int max_scroll = content_length - length;
    int thumb_size = std::max(1, length * length / content_length);
    int thumb_pos = (int)((float)offset / max_scroll * (length - thumb_size));

    for (int i = 0; i < length; ++i) {
        bool is_thumb = (i >= thumb_pos && i < thumb_pos + thumb_size);
        if (vertical) {
            auto& cell = cells_[(pos.y + i) * width_ + pos.x];
            cell.content = " ";
            cell.bg_color = is_thumb ? thumb_color : track_color;
        }
        else {
            auto& cell = cells_[pos.y * width_ + pos.x + i];
            cell.content = " ";
            cell.bg_color = is_thumb ? thumb_color : track_color;
        }
    }
}

/// <summary>
/// Terminal
/// </summary>

std::atomic<bool> Terminal::s_running = false;
std::thread* Terminal::s_event_thread = nullptr;
std::mutex   Terminal::s_event_mutex;

Terminal::Terminal()
{
    EnableRawMode();
    auto size = GetSize();
    drawbuffers[0].resize(size.x, size.y);
    drawbuffers[1].resize(size.x, size.y);
}

void Terminal::Render()
{
    auto size = GetSize();
    int prev_drawbuffer = (current_drawbuffer + 1) % 2;
    auto& cur_buf = drawbuffers[current_drawbuffer];
    auto& pre_buf = drawbuffers[prev_drawbuffer];

    if (cur_buf.width_ != size.x || cur_buf.height_ != size.y) {
        cur_buf.resize(size.x, size.y);
        pre_buf.resize(size.x, size.y);
    }

    Color cur_fg;
    Color cur_bg;
    bool cur_bold = false;
    bool cur_italic = false;
    bool cur_underline = false;
    int cur_x = -1;
    int cur_y = -1;
    int yw = 0;

    std::string out;
    out.reserve(size.x * size.y * 30);

    for (int y = 0; y < cur_buf.height_; ++y) {
        for (int x = 0; x < cur_buf.width_; ++x) {
            auto& cur_cell = cur_buf.cells_[yw + x];
            auto& pre_cell = pre_buf.cells_[yw + x];

            if (cur_cell == pre_cell)
                continue;
            if (cur_x != x || cur_y != y) {
                out += CursorMove(x, y);
            }

            if (cur_cell.fg_color != cur_fg) {
                cur_fg = cur_cell.fg_color;
                out += cur_fg.toAnsi();
            }
            if (cur_cell.bg_color != cur_bg) {
                cur_bg = cur_cell.bg_color;
                out += cur_bg.toAnsi();
            }
            if (cur_cell.bold != cur_bold) {
                cur_bold = cur_cell.bold;
                out += cur_bold ? ANSI_BOLD : "\033[22m";
            }
            if (cur_cell.italic != cur_italic) {
                cur_italic = cur_cell.italic;
                out += cur_italic ? ANSI_ITALIC : "\033[23m";
            }
            if (cur_cell.underline != cur_underline) {
                cur_underline = cur_cell.underline;
                out += cur_underline ? ANSI_UNDER : "\033[24m";
            }
            out += cur_cell.content;

            cur_x += cur_cell.size;
            cur_y = y;

            if (cur_cell.size > 1) {
                x += cur_cell.size - 1;
            }
        }
        yw += size.x;
    }
    out += ANSI_RESET;

    current_drawbuffer = prev_drawbuffer;

    std::cout << out;
}

#ifdef _WIN32

void Terminal::EnableRawMode()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    originalOutMode_ = dwMode;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    originalOutCP_ = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    // Verify VT output support
    bool vt_out = false;
    DWORD outModeCheck = 0;
    GetConsoleMode(hOut, &outModeCheck);
    if (outModeCheck & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
        vt_out = true;
    }

    // Fallback: check ConEmu environment variables
    if (!vt_out) {
        if (std::getenv("ConEmuBuild") || std::getenv("ConEmuANSI")) {
            vt_out = true;
        }
    }

    if (vt_out) {
        vtSupported_ = true;
        // Request Mouse Reporting (1003 = Any Event/Motion, 1006 = SGR)
        // Request Bracketed Paste Mode (2004)
        std::cout << "\033[?1003h\033[?1006h\033[?2004h";
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hIn, &dwMode);
    originalInMode_ = dwMode;

    // Configure input mode
    dwMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_QUICK_EDIT_MODE |
                ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);
    dwMode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;

    if (SetConsoleMode(
            hIn, dwMode | ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_MOUSE_INPUT)) {
        vtInputSupported_ = true;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_MOUSE_INPUT;
    } else {
        // Fallback to legacy mouse input
        dwMode |= ENABLE_MOUSE_INPUT;
        vtInputSupported_ = false;
    }
    SetConsoleMode(hIn, dwMode);

    // Handle Ctrl+C cleanup
    SetConsoleCtrlHandler(
        [](DWORD fdwCtrlType) -> BOOL {
            switch (fdwCtrlType) {
                case CTRL_C_EVENT:
                case CTRL_CLOSE_EVENT:
                {
                    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                    std::cout << SHOW_CURSOR;
                    std::cout << "\033[?1049l";  // Disable alternate buffer
                }
                    return FALSE;  // Let default handler call ExitProcess
                default:
                    return FALSE;
            }
        },
        TRUE);

    originalInCP_ = GetConsoleCP();
    SetConsoleCP(CP_UTF8);

    std::cout << "\033[?1049h" << HIDE_CURSOR;

    if (!s_event_thread) {
        s_running.store(true);
        s_event_thread = new std::thread([] {
            event_thread();
            });
    }
}

void Terminal::DisableRawMode()
{
    if (s_event_thread) {
        s_running.store(false);
        s_event_thread->join();
        delete s_event_thread;
        s_event_thread = nullptr;
    }
    std::cout << SHOW_CURSOR;
    std::cout << "\033[?1049l";  // Disable alternate screen buffer

    if (vtSupported_) {
        std::cout << "\033[?1003l\033[?1006l\033[?2004l";
    }
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleMode(hOut, originalOutMode_);
    SetConsoleOutputCP(originalOutCP_);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, originalInMode_ | ENABLE_EXTENDED_FLAGS);
    SetConsoleCP(originalInCP_);
}

Point Terminal::GetSize()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return { csbi.srWindow.Right - csbi.srWindow.Left + 1,
            csbi.srWindow.Bottom - csbi.srWindow.Top + 1 };
}

void Terminal::event_thread()
{
    INPUT_RECORD rec;
    DWORD count;
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    while (s_running.load()) {
        ReadConsoleInputW(hIn, &rec, 1, &count);
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
        }
        else if (rec.EventType == MOUSE_EVENT) {
        
        }
    }
}

#else

void Terminal::EnableRawMode()
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    originalTermios_ = raw;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |
                     ISIG);  // Disable ISIG to allow Ctrl+Z as raw input
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
#ifdef IUTF8
    raw.c_iflag |= IUTF8;
#endif
    raw.c_cflag |= (CS8);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    std::cout << "\033[?1003h\033[?1006h\033[?2004h";  // Enable mouse reporting (all
                                                       // motion) + SGR + Bracketed
                                                       // Paste
}

void Terminal::DisableRawMode()
{
    std::cout << SHOW_CURSOR;          // Show cursor
    std::cout << "\033[?1049l";  // Disable alternate screen buffer

    std::cout << "\033[?1003l\033[?1006l\033[?2004l";  // Disable mouse and bracketed
                                                       // paste
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios_);
}

Point Terminal::GetSize()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return { w.ws_col, w.ws_row };
}

void Terminal::event_thread()
{

}

#endif

NAMESPACE_END
