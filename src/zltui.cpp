#include "zltui.h"
#include <iostream>
#include <fstream>
#include <sstream>

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
        (cp >= 0x2600 && cp <= 0x26FF) ||   // Misc symbols (☀ ☮ ♠)
        (cp >= 0x2700 && cp <= 0x27BF) ||   // Dingbats
        (cp >= 0x2E80 && cp <= 0xA4CF) ||   // CJK radicals, Kangxi, etc.
        (cp >= 0xAC00 && cp <= 0xD7A3) ||   // Hangul syllables
        (cp >= 0xF900 && cp <= 0xFAFF) ||   // CJK compatibility
        (cp >= 0xFE00 && cp <= 0xFE0F) ||   // Variation Selectors
        (cp >= 0xFE10 && cp <= 0xFE6F) ||   // Vertical forms, small forms
        (cp >= 0xFF00 && cp <= 0xFF60) ||   // Fullwidth ASCII variants
        (cp >= 0xFFE0 && cp <= 0xFFE6)) {
        return 2;
    }
    // Emoji — typically double-width in terminals
    if ((cp >= 0x1F300 && cp <= 0x1F9FF) ||   // Misc Symbols & Pictographs, Emoticons
        (cp >= 0x1FA00 && cp <= 0x1FA6F) ||   // Chess symbols
        (cp >= 0x1FA70 && cp <= 0x1FAFF)) {   // Extended-A (chess, dominoes, etc.)
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

static bool eqi(const std::string& a, const char* b)
{
    for (size_t i = 0; a[i] && b[i]; ++i)
        if (std::tolower(a[i]) != std::tolower(b[i]))
            return false;
    return a.size() == strlen(b);
}

std::string CursorMove(int x, int y)
{
    return "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
}

std::string Color::toAnsi(bool fg) const
{
    if (ansi == AnsiColor_None) {
        return "\033[" + std::string(fg ? "38" : "48") + ";2;" + std::to_string(r) + ";" +
            std::to_string(g) + ";" +
            std::to_string(b) + "m";
    }

    switch (ansi) {
    case AnsiColor_Black:     return fg ? ANSI_FG_BLACK : ANSI_BG_BLACK;
    case AnsiColor_Red:       return fg ? ANSI_FG_RED : ANSI_BG_RED;
    case AnsiColor_Green:     return fg ? ANSI_FG_GREEN : ANSI_BG_GREEN;
    case AnsiColor_Yellow:    return fg ? ANSI_FG_YELLOW : ANSI_BG_YELLOW;
    case AnsiColor_Blue:      return fg ? ANSI_FG_BLUE : ANSI_BG_BLUE;
    case AnsiColor_Magenta:   return fg ? ANSI_FG_MAGENTA : ANSI_BG_MAGENTA;
    case AnsiColor_Cyan:      return fg ? ANSI_FG_CYAN : ANSI_BG_CYAN;
    case AnsiColor_White:     return fg ? ANSI_FG_WHITE : ANSI_BG_WHITE;

    case AnsiColor_Bright_Black:     return fg ? ANSI_BRIGHT_BLACK : ANSI_BRIGHT_BG_BLACK;
    case AnsiColor_Bright_Red:       return fg ? ANSI_BRIGHT_RED : ANSI_BRIGHT_BG_RED;
    case AnsiColor_Bright_Green:     return fg ? ANSI_BRIGHT_GREEN : ANSI_BRIGHT_BG_GREEN;
    case AnsiColor_Bright_Yellow:    return fg ? ANSI_BRIGHT_YELLOW : ANSI_BRIGHT_BG_YELLOW;
    case AnsiColor_Bright_Blue:      return fg ? ANSI_BRIGHT_BLUE : ANSI_BRIGHT_BG_BLUE;
    case AnsiColor_Bright_Magenta:   return fg ? ANSI_BRIGHT_MAGENTA : ANSI_BRIGHT_BG_MAGENTA;
    case AnsiColor_Bright_Cyan:      return fg ? ANSI_BRIGHT_CYAN : ANSI_BRIGHT_BG_CYAN;
    case AnsiColor_Bright_White:     return fg ? ANSI_BRIGHT_WHITE : ANSI_BRIGHT_BG_WHITE;

    default: return "";
    }
}

Color Color::Parse(const std::string& param)
{
    static const struct { const char* name; AnsiColor_ val; } names[] = {
        {"Black",         AnsiColor_Black},
        {"Red",           AnsiColor_Red},
        {"Green",         AnsiColor_Green},
        {"Yellow",        AnsiColor_Yellow},
        {"Blue",          AnsiColor_Blue},
        {"Magenta",       AnsiColor_Magenta},
        {"Cyan",          AnsiColor_Cyan},
        {"White",         AnsiColor_White},
        {"BrightBlack",   AnsiColor_Bright_Black},
        {"BrightRed",     AnsiColor_Bright_Red},
        {"BrightGreen",   AnsiColor_Bright_Green},
        {"BrightYellow",  AnsiColor_Bright_Yellow},
        {"BrightBlue",    AnsiColor_Bright_Blue},
        {"BrightMagenta", AnsiColor_Bright_Magenta},
        {"BrightCyan",    AnsiColor_Bright_Cyan},
        {"BrightWhite",   AnsiColor_Bright_White},
    };

    for (auto& n : names) {
        if (eqi(param, n.name))
            return Color(n.val);
    }

    // "RGB(r, g, b)"
    if (eqi(param.substr(0, 3), "rgb")) {
        size_t lp = param.find('(');
        size_t rp = param.rfind(')');
        if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
            std::string inner = param.substr(lp + 1, rp - lp - 1);
            EditLine el;
            el.lines.push_back(inner);
            uint8_t r = static_cast<uint8_t>(el.tok_int(","));
            uint8_t g = static_cast<uint8_t>(el.tok_int(","));
            uint8_t b = static_cast<uint8_t>(el.tok_int(","));
            return Color(r, g, b);
        }
    }

    return Color();
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
    text_width = 0;
    chars.resize(0);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    size_t len = text.size();
    while (len > 0) {
        uint32_t cp = 0;
        int n = utf8_mbtowc(cp, p, static_cast<int>(len));
        if (n <= 0) break;
        auto ch = Char::from_code(cp);
        text_width += ch.char_width;
        chars.push_back(ch);
        p += n; len -= n;
    }
}

BorderStyle_ ParseBorderStyle(const std::string& param)
{
    if (eqi(param, "None"))   return BorderStyle_None;
    if (eqi(param, "Single")) return BorderStyle_Single;
    if (eqi(param, "Double")) return BorderStyle_Double;
    if (eqi(param, "Round"))  return BorderStyle_Round;
    return BorderStyle_None;
}

void DrawBuffer::PushClip(const Rect& clip)
{
    clips_.push_back(clip);
}
void DrawBuffer::PopClip()
{
    clips_.pop_back();
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
    int cur_x = pos.x;
    int cur_y = pos.y;

    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    size_t len = text.size();

    while (len > 0) {
        // skip if out of bounds
        if (cur_x < 0 || cur_y < 0 || cur_y >= height_) break;

        int right = width_;
        if (!clips_.empty()) {
            const auto& clip = clips_.back();
            if (cur_y < clip.y || cur_y > clip.y2)
                break;
            if (clip.x2 < right)
                right = clip.x2;
        }

        int px = cur_y * width_ + cur_x;
        int cw = 0;

        while (len > 0 && cur_x + cw < right) {
            uint32_t cp = 0;
            int n = utf8_mbtowc(cp, p, static_cast<int>(len));
            if (n <= 0) break;

            // newline: move to next row
            if (cp == '\n') {
                cur_y++;
                cur_x = pos.x;
                p += n; len -= n;
                break;
            }

            int char_width = utf8_char_width(cp);
            if (cur_x + cw + char_width > right) break;

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
}

void DrawBuffer::Text(const Point& pos, const TUI::Text& text, const Color& color)
{
    Text(text.text, pos, color, text.bold, text.italic, text.underline);
}

void DrawBuffer::Border(const Rect& r, const Color& bgcolor, BorderStyle_ style, const Color& color)
{
    // Apply clip region
    int clip_x = 0;
    int clip_y = 0;
    int clip_x2 = width_ - 1;
    int clip_y2 = height_ - 1;
    if (!clips_.empty()) {
        const auto& clip = clips_.back();
        clip_x = clip.x;
        clip_y = clip.y;
        clip_x2 = clip.x2;
        clip_y2 = clip.y2;
    }

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
        if (r.x >= clip_x && r.x <= clip_x2 && r.y >= clip_y && r.y <= clip_y2) {
            auto& cell = cells_[r.y * width_ + r.x];
            cell.content = tl;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw top-right corner
        if (r.x2 >= clip_x && r.x2 <= clip_x2 && r.y >= clip_y && r.y <= clip_y2) {
            auto& cell = cells_[r.y * width_ + r.x2];
            cell.content = tr;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw bottom-left corner
        if (r.x >= clip_x && r.x <= clip_x2 && r.y2 >= clip_y && r.y2 <= clip_y2) {
            auto& cell = cells_[r.y2 * width_ + r.x];
            cell.content = bl;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw bottom-right corner
        if (r.x2 >= clip_x && r.x2 <= clip_x2 && r.y2 >= clip_y && r.y2 <= clip_y2) {
            auto& cell = cells_[r.y2 * width_ + r.x2];
            cell.content = br;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }

        // Draw top and bottom horizontal lines (excluding corners)
        for (int x = std::max(r.x + 1, clip_x); x < r.x2 && x <= clip_x2; x++) {
            if (r.y >= clip_y && r.y <= clip_y2) {
                auto& cell = cells_[r.y * width_ + x];
                cell.content = h_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
            if (r.y2 >= clip_y && r.y2 <= clip_y2) {
                auto& cell = cells_[r.y2 * width_ + x];
                cell.content = h_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
        }

        // Draw left and right vertical lines (excluding corners)
        for (int y = std::max(r.y + 1, clip_y); y < r.y2 && y <= clip_y2; y++) {
            if (r.x >= clip_x && r.x <= clip_x2) {
                auto& cell = cells_[y * width_ + r.x];
                cell.content = v_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
            if (r.x2 >= clip_x && r.x2 <= clip_x2) {
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
    for (int y = std::max(fill_y_start, clip_y); y <= fill_y_end && y <= clip_y2; y++) {
        for (int x = std::max(fill_x_start, clip_x); x <= fill_x_end && x <= clip_x2; x++) {
            auto& cell = cells_[y * width_ + x];
            cell.content = " ";
            cell.bg_color = bgcolor;
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
            cell.content = u8"\u2584";
            cell.fg_color = is_thumb ? thumb_color : track_color;
        }
    }
}

/// <summary>
/// Terminal
/// </summary>

std::atomic<bool> Terminal::s_running = false;
std::thread* Terminal::s_event_thread = nullptr;
std::mutex   Terminal::s_event_mutex;
std::vector<Event> Terminal::s_events;

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
                out += cur_fg.toAnsi(true);
            }
            if (cur_cell.bg_color != cur_bg) {
                cur_bg = cur_cell.bg_color;
                out += cur_bg.toAnsi(false);
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

void Terminal::GetEvent(std::vector<Event>& events)
{
    std::lock_guard<std::mutex> lock(s_event_mutex);
    events.swap(s_events);
}

#ifdef _WIN32

DWORD originalOutMode_ = 0;
DWORD originalInMode_ = 0;
UINT originalOutCP_ = 0;
UINT originalInCP_ = 0;
bool vtSupported_ = false;
bool vtInputSupported_ = false;

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
        // Request Bracketed Paste Mode (2004)
        // Note: mouse events are handled via ENABLE_MOUSE_INPUT + ReadConsoleInputW,
        // so we do NOT enable VT mouse reporting (1003/1006) which would conflict.
        std::cout << "\033[?2004h";
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hIn, &dwMode);
    originalInMode_ = dwMode;

    // Configure input mode — do NOT use ENABLE_VIRTUAL_TERMINAL_INPUT because it
    // converts mouse events into ANSI escape sequences (read as KEY events),
    // which conflicts with ReadConsoleInputW MOUSE_EVENT records.
    dwMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_QUICK_EDIT_MODE |
                ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);
    dwMode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;

    SetConsoleMode(hIn, dwMode);
    vtInputSupported_ = false;

    // Handle Ctrl+C cleanup
    //SetConsoleCtrlHandler(
    //    [](DWORD fdwCtrlType) -> BOOL {
    //        switch (fdwCtrlType) {
    //            case CTRL_C_EVENT:
    //            case CTRL_CLOSE_EVENT:
    //            {
    //                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    //                std::cout << SHOW_CURSOR;
    //                std::cout << "\033[?1049l";  // Disable alternate buffer
    //            }
    //                return FALSE;  // Let default handler call ExitProcess
    //            default:
    //                return FALSE;
    //        }
    //    },
    //    TRUE);

    originalInCP_ = GetConsoleCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

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
            Event ev;
            ev.type = EventType_Mouse;
            ev.x = rec.Event.MouseEvent.dwMousePosition.X;
            ev.y = rec.Event.MouseEvent.dwMousePosition.Y;

            auto flags = rec.Event.MouseEvent.dwEventFlags;
            auto btnState = rec.Event.MouseEvent.dwButtonState;

            // Wheel events: btnState is the delta, not button flags
            if (flags == MOUSE_WHEELED) {
                ev.button = GET_WHEEL_DELTA_WPARAM(btnState) > 0 ? 4 : 5; // scroll up/down
            } else if (flags == MOUSE_HWHEELED) {
                ev.button = GET_WHEEL_DELTA_WPARAM(btnState) > 0 ? 6 : 7; // scroll left/right
            } else {
                // Button events: determine which button and click count
                if (btnState & FROM_LEFT_1ST_BUTTON_PRESSED)
                    ev.button = 1;
                else if (btnState & RIGHTMOST_BUTTON_PRESSED)
                    ev.button = 2;
                else if (btnState & FROM_LEFT_2ND_BUTTON_PRESSED)
                    ev.button = 3;

                ev.clicks = (flags == DOUBLE_CLICK) ? 2 : 1;
            }

            std::lock_guard<std::mutex> lock(s_event_mutex);
            s_events.push_back(ev);
        }
    }
}

#else

struct termios originalTermios_;

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

bool EditLine::read_file(const std::string& path)
{
    std::ifstream infile(path, std::ios::binary);
    if (!infile.is_open()) return false;
    std::ostringstream ss;
    ss << infile.rdbuf();
    std::string content = ss.str();
    infile.close();
    parse(content);
    return true;
}

void EditLine::parse(const std::string& text)
{
    size_t count = 1; // at least one line (the remainder after last \n)
    const char* p = text.c_str();
    while (*p) {
        if (*p == '\n') ++count;
        ++p;
    }

    lines.clear();
    lines.reserve(count);

    // Second pass: extract lines
    p = text.c_str();
    const char* s = p;  // start of current line
    while (*p) {
        if (*p == '\n') {
            size_t len = static_cast<size_t>(p - s);
            // strip trailing \r from \r\n
            if (len > 0 && p[-1] == '\r')
                --len;
            lines.emplace_back(s, len);
            s = p + 1;
        }
        ++p;
    }
    // push remaining content after the last \n
    if (s <= p)
        lines.emplace_back(s, static_cast<size_t>(p - s));
}

std::string EditLine::next_line()
{
    current++;
    for (; current < lines.size(); ++current) {
        if (lines[current].empty())
            continue;
        return lines[current];
    }
    return "";
}

std::string EditLine::next_tok(std::string delims)
{
    auto line = next_line();
    if (line.empty())
        return "";
    tok_ = 0;
    return tok(delims);
}

std::string EditLine::tok(std::string delims)
{
    auto line = lines[current];
    size_t start = line.find_first_not_of(delims, tok_);
    if (start == std::string::npos)
        return "";

    size_t end = line.find_first_of(delims, start);
    if (end == std::string::npos)
        return line.substr(start);
    tok_ = (int)end;
    return line.substr(start, end - start);
}

int EditLine::tok_int(std::string delims)
{
    auto tk = tok(delims);
    if (tk.empty())
        return 0;
    try {
        return std::stoi(tk);
    }
    catch (...)
    {
        return 0;
    }
}

bool EditLine::tok_bool(std::string delims)
{
    auto tk = tok(delims);
    // true: "yes", "true", "1"
    // false: "no", "false", "0"
    if (tk == "yes" || tk == "true" || tk == "1")
        return true;
    return false;
}

/// <summary>
/// Win
/// </summary>

Color Win::COLOR_BG(30, 30, 30);
Color Win::COLOR_HOVER(70, 70, 70);
Color Win::COLOR_DOWN(90, 90, 90);
Color Win::COLOR_BTN(50, 50, 50);
Color Win::COLOR_TRACK(44, 44, 44);
Color Win::COLOR_THUMB(159, 159, 159);

bool Win::Parse(EditLine& el)
{
    bool isComment = false;
    std::string cmd = el.next_tok();
    while (!cmd.empty()) {
        if (cmd[0] == '#') {

        }
        else if (cmd.length() >= 2 && (cmd[0] == '/' && cmd[1] == '/')) {
        }
        else if (cmd.length() >= 2 && (cmd[0] == '/' && cmd[1] == '*')) {
            isComment = true;
        }
        else if (cmd.length() >= 2 && (cmd[0] == '*' && cmd[1] == '/')) {
            isComment = false;
        }
        else if (isComment) {

        }
        else if (cmd[0] == '}') {
            break;
        }
        else if (ParseCmd(cmd, el)) {

        }
        else {

        }
        cmd = el.next_tok();
    }
    mgr->is_dirty = true;
    return true;
}

bool Win::ParseCmd(const std::string &cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "{")) {
    }
    else if (eqi(cmd, "Object")) {
        auto ob = mgr->Create(el.tok());
        if (ob) {
            child.push_back(ob);
            ob->Parse(el);
        }
    }
    else if (eqi(cmd, "Name")) {
        name = el.tok();
    }
    else if (eqi(cmd, "Rect")) {
        local.set(el.tok_int(), el.tok_int(), el.tok_int(), el.tok_int());
    }
    else if (eqi(cmd, "Visible")) {
        is_visible = el.tok_bool();
    }
    else if (eqi(cmd, "DrawBorder")) {
        draw_border = el.tok_bool();
    }
    else if (eqi(cmd, "fgColor")) {
        fg_color = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "bgColor")) {
        bg_color = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "BorderStyle")) {
        border_style = ParseBorderStyle(el.tok());
    }
    return ret;
}

Point Win::GetClipPos() const
{
    return { clip.x, clip.y };
}

void Win::CalRect(Win* parent)
{
    Point pt = { 0, 0 };
    int pw = local.width();
    int ph = local.height();
    if (parent) {
        pt = parent->GetClipPos();
        pw = parent->clip.width();
        ph = parent->clip.height();
    }
    screen = local.move(pt.x, pt.y);
    if (draw_border && border_style != BorderStyle_None) {
        clip = screen.expand(-1, -1);
    }
    else {
        clip = screen;
    }

}

void Win::Paint(DrawBuffer& drawbuf)
{
    PaintBorder(drawbuf);
    PaintChild(drawbuf);
}

void Win::PaintChild(DrawBuffer& drawbuf)
{
    drawbuf.PushClip(clip);
    for (auto ch : child) {
        if (!ch->is_visible)
            continue;
        ch->CalRect(this);
        if (!clip.collide(ch->screen))
            continue;
        ch->Paint(drawbuf);
    }
    drawbuf.PopClip();
}

void Win::PaintBorder(DrawBuffer& drawbuf)
{
    if (draw_border) {
        drawbuf.Border(screen, bg_color, border_style, fg_color);
    }
}

Win* Win::GetUI(const std::string& _name)
{
    if (name == _name)
        return this;
    for (auto ch : child) {
        Win* found = ch->GetUI(_name);
        if (found)
            return found;
    }
    return nullptr;
}

Win* Win::GetNotify(const Point& pt)
{
    if (!is_visible)
        return nullptr;
    if (!clip.inside(pt))
        return nullptr;
    for (int i = (int)child.size() - 1; i >= 0; i--) {
        auto ch = child[i];
        Win* n = ch->GetNotify(pt);
        if (n) { return n; }
    }
    return (is_notifiable) ? this : nullptr;
}

Win* Win::GetSlider(const Point& pt)
{
    if (!is_visible)
        return nullptr;
    if (!clip.inside(pt))
        return nullptr;
    for (int i = (int)child.size() - 1; i >= 0; i--) {
        auto ch = child[i];
        Win* n = ch->GetSlider(pt);
        if (n) { return n; }
    }
    return (IsSlider() && is_notifiable) ? this : nullptr;
}

void Win::AddChild(WinPtr obj)
{
    child.push_back(obj);
    mgr->is_dirty = true;
}

/// <summary>
/// Label
/// </summary>

void Label::Paint(DrawBuffer& drawbuf)
{
    PaintBorder(drawbuf);
    PaintText(drawbuf);
    PaintChild(drawbuf);
}
void Label::PaintText(DrawBuffer& drawbuf)
{
    if (!text.empty()) {
        int tx = clip.x;
        switch (text_algn) {
        case Align_Center:
            tx = clip.x + ((clip.width() - text_width) >> 1);
            break;
        case Align_End:
            tx = clip.x2 - text_width;
            break;
        }
        drawbuf.Text(text, { tx, clip.y }, fg_color, bold, italic, underline);
    }
}

void Label::setText(const std::string& _text)
{
    Text::setText(_text);
    mgr->is_dirty = true;
}

/// <summary>
/// Button
/// </summary>

Button::Button(Mgr* mgr) :Label(mgr) {
    text_algn = Align_Center;
    border_style = BorderStyle_None;
    draw_border = true;
    bg_color = COLOR_BTN;
}

void Button::PaintBorder(DrawBuffer& drawbuf)
{
    Color bg = bg_color;
    if (is_down) {
        bg = bg_color_down;
    }
    else if (mgr->hover_ == this) {
        bg = bg_color_hover;
    }
    if (draw_border) {
        drawbuf.Border(screen, bg, border_style, fg_color);
    }
}

/// <summary>
/// Check
/// </summary>

Check::Check(Mgr* mgr) : Button(mgr) {
    bg_color = COLOR_BG;
    text_algn = Align_Start;
}


void Check::PaintText(DrawBuffer& drawbuf)
{
    int mark_start = 3;
    drawbuf.Text((checked) ? u8"✅" : u8"🔳", { clip.x, clip.y }, AnsiColor_Bright_White);    
    //drawbuf.Text((checked) ? "[x]" : "[ ]", { clip.x, clip.y }, AnsiColor_White);
    if (!text.empty()) {
        int tx = clip.x + mark_start;
        switch (text_algn) {
        case Align_Center:
            tx = clip.x + mark_start + ((clip.width() - text_width) >> 1);
            break;
        case Align_End:
            tx = clip.x2 - text_width;
            break;
        }
        drawbuf.Text(text, { tx, clip.y }, fg_color, bold, italic, underline);
    }
}

void Check::Click()
{
    checked = !checked;
    if (on_check) {
        on_check(checked);
    }
}

/// <summary>
/// Slider
/// </summary>

void Slider::CalRect(Win* parent)
{
    Win::CalRect(parent);
    if (is_vertical) {
        clip.x2--;
    }
    else {
        clip.y2--;
    }

}

void Slider::Paint(DrawBuffer& drawbuf)
{
    PaintBorder(drawbuf);
    PaintScrollBar(drawbuf);
    PaintChild(drawbuf);
}
void Slider::PaintScrollBar(DrawBuffer& drawbuf)
{
    if (is_vertical) {
        drawbuf.ScrollBar({ clip.x2 + 1, clip.y }, clip.height(), scroll_value, clip.height() + 20, is_vertical,
            track_color, thumb_color);
    }
}

void Slider::Event(const TUI::Event& ev)
{
    switch (ev.button) {
    case 4:
        scroll_value--;
        if (scroll_value < 0) scroll_value = 0;
        break;
    case 5:
        scroll_value++;
        break;
    }
    mgr->is_dirty = true;
}

Point Slider::GetClipPos() const
{
    if (is_vertical)
        return { clip.x, clip.y - scroll_value };
    return { clip.x - scroll_value, clip.y };
}

/// <summary>
/// Mgr
/// </summary>

WinPtr Mgr::Create(std::string csid)
{
    Win* ob = nullptr;
    if (eqi(csid, "Win")) {
        ob = new Win(this);
    }
    else if (eqi(csid, "Button")) {
        ob = new Button(this);
    }
    else if (eqi(csid, "Check")) {
        ob = new Check(this);
    }
    else if (eqi(csid, "Slider")) {
        ob = new Slider(this);
    }
    else if (eqi(csid, "Edit")) {
        ob = new Slider(this);
    }
    return WinPtr(ob);
}

bool Mgr::Parse(std::string content)
{
    EditLine el;
    el.parse(content);
    return Win::Parse(el);
}

bool Mgr::Update(Terminal& terminal)
{
    auto size = terminal.GetSize();
    if (size.x != local.x2 - 1 || size.y != local.y2 - 1) {
        local.x2 = size.x - 1;
        local.y2 = size.y - 1;
        screen = local;
        clip = local;
        is_dirty = true;
    }

    std::vector<TUI::Event> events;
    Terminal::GetEvent(events);

    for (auto& ev : events) {
        if (ev.type == EventType_Mouse) {
            Point pt = { ev.x, ev.y };
            bool any_down = ev.button >= 1 && ev.button <= 3;
            bool first_down = any_down && !is_prev_down;
            is_prev_down = any_down;
            Win* notify = GetNotify(pt);
            if (notify) {
                if (notify != notify_) {
                    if (first_down) {
                        if (notify_) {
                            notify_->is_notify = false;
                        }
                        notify_ = notify;
                        notify_->is_notify = true;
                    }
                    is_dirty = true;
                }
                else if (first_down) {
                    notify->is_notify = true;
                    is_dirty = true;
                }
                if (notify->is_down != any_down) {
                    notify->is_down = any_down;
                    is_dirty = true;
                }
                if (first_down) {
                    notify->Click();
                }
            }
            if (hover_ != notify) {
                if (hover_) {
                    hover_->is_down = false;
                }
                if(!any_down)
                    hover_ = notify;
                is_dirty = true;
            }
            if (notify_) {
                notify_->Event(ev);
            }
            hover_slider_ = GetSlider(pt);
            if (hover_slider_ && hover_slider_ != notify_) {
                hover_slider_->Event(ev);
            }
        }
    }
    return is_dirty;
}
void Mgr::Paint(DrawBuffer& drawbuf)
{
    Win::Paint(drawbuf);
    is_dirty = false;    
}

NAMESPACE_END
