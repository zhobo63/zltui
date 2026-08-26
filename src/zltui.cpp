#include "zltui.h"
#include <algorithm>
#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <condition_variable>
#include <climits>
#include <cctype>
#include <initializer_list>

#ifndef _WIN32
#include <cerrno>
#include <poll.h>
#include <sys/ioctl.h>
#endif

#define USE_REMOTE_LOG 1
#if USE_REMOTE_LOG
#define REMOTE_LOG_IMPLEMENT
#include <remote_log.h>
#else
#define LOG
#endif

#undef max
#undef min

NAMESPACE_BEGIN(TUI)

/// Compute the display width of a single UTF-8 character (codepoint).
/// CJK and other wide characters return 2; ASCII returns 1.
static int utf8_char_width(uint32_t cp) {
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) return 0; // control chars
    if (cp < 0x1100) return 1;
    // Variation selectors modify the preceding character and occupy no cell.
    if ((cp >= 0xFE00 && cp <= 0xFE0F) ||
        (cp >= 0xE0100 && cp <= 0xE01EF)) {
        return 0;
    }

    // CJK, Hangul, fullwidth forms, and other East Asian wide characters.
    if ((cp >= 0x1100 && cp <= 0x115F) ||   // Hangul Jamo
        (cp >= 0x231A && cp <= 0x231B) ||   // Watch, hourglass
        (cp >= 0x2329 && cp <= 0x232A) ||   // Angle brackets
        (cp >= 0x23E9 && cp <= 0x23EC) ||   // Fast-forward / reverse
        (cp == 0x23F0) || (cp == 0x23F3) ||
        (cp >= 0x25FD && cp <= 0x25FE) ||
        (cp >= 0x2600 && cp <= 0x26FF) ||   // Misc symbols
        (cp >= 0x2700 && cp <= 0x27BF) ||   // Dingbats
        (cp >= 0x2B00 && cp <= 0x2BFF) ||   // Misc symbols and arrows
        (cp >= 0x2E80 && cp <= 0xA4CF) ||   // CJK radicals, Kangxi, Yi
        (cp >= 0xA960 && cp <= 0xA97F) ||   // Hangul Jamo Extended-A
        (cp >= 0xAC00 && cp <= 0xD7A3) ||   // Hangul syllables
        (cp >= 0xD7B0 && cp <= 0xD7FF) ||   // Hangul Jamo Extended-B
        (cp >= 0xF900 && cp <= 0xFAFF) ||   // CJK compatibility
        (cp >= 0xFE10 && cp <= 0xFE6F) ||   // Vertical and small forms
        (cp >= 0xFF01 && cp <= 0xFF60) ||   // Fullwidth ASCII variants
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||   // Fullwidth currency forms
        (cp >= 0x20000 && cp <= 0x3FFFD)) { // CJK Extensions
        return 2;
    }

    // Emoji and pictographs are conventionally double-width in terminals.
    if ((cp >= 0x1F004 && cp <= 0x1F0CF) ||
        (cp >= 0x1F18E && cp <= 0x1F19A) ||
        (cp >= 0x1F1E6 && cp <= 0x1F1FF) ||
        (cp >= 0x1F300 && cp <= 0x1FAFF)) {
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

static int utf8_mbtowc(std::vector<wchar_t>& wtext, const std::string& text)
{
    wtext.clear();
    if (text.size() > static_cast<size_t>(INT_MAX))
        return -1;

    wtext.reserve(text.size());
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    int len = static_cast<int>(text.size());

    while (len > 0) {
        uint32_t cp = 0;
        int n = utf8_mbtowc(cp, p, len);
        if (n <= 0) {
            cp = 0xFFFD;
            n = 1;
        }

        if (cp <= 0xFFFF) {
            // UTF-16 cannot contain unpaired surrogate code points.
            if (cp >= 0xD800 && cp <= 0xDFFF)
                cp = 0xFFFD;
            wtext.push_back(static_cast<wchar_t>(cp));
        }
        else if (cp <= 0x10FFFF) {
            cp -= 0x10000;
            wtext.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            wtext.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        }
        else {
            wtext.push_back(static_cast<wchar_t>(0xFFFD));
        }

        p += n;
        len -= n;
    }

    if (wtext.size() > static_cast<size_t>(INT_MAX))
        return -1;
    return static_cast<int>(wtext.size());
}

static int utf8_wctomb(char buf[4], uint32_t cp)
{
    // reconstruct the UTF-8 bytes from the char
    int l = 0;
    if (cp < 0x80) {
        buf[0] = static_cast<char>(cp);
        l = 1;
    }
    else if (cp < 0x800) {
        buf[0] = static_cast<char>(0xC0 | (cp >> 6));
        buf[1] = static_cast<char>(0x80 | (cp & 0x3F));
        l = 2;
    }
    else if (cp < 0x10000) {
        buf[0] = static_cast<char>(0xE0 | (cp >> 12));
        buf[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = static_cast<char>(0x80 | (cp & 0x3F));
        l = 3;
    }
    else {
        buf[0] = static_cast<char>(0xF0 | (cp >> 18));
        buf[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = static_cast<char>(0x80 | (cp & 0x3F));
        l = 4;
    }
    return l;
}

static int utf8_width(const std::string& text) {
    int text_width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    size_t len = text.size();
    while (len > 0) {
        uint32_t cp = 0;
        int n = utf8_mbtowc(cp, p, static_cast<int>(len));
        if (n <= 0) break;
        text_width += utf8_char_width(cp);
        p += n; len -= n;
    }
    return text_width;
}

static bool eqi(const std::string& a, const char* b)
{
    for (size_t i = 0; a[i] && b[i]; ++i)
        if (std::tolower(a[i]) != std::tolower(b[i]))
            return false;
    return a.size() == strlen(b);
}

static void send_enter() {
#ifdef _WIN32
    // Inject an Enter key into the console input buffer so that read_key()
    // returns immediately, allowing the thread to check s_running and exit.
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD rec{};
    rec.EventType = KEY_EVENT;
    rec.Event.KeyEvent.bKeyDown = TRUE;
    rec.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    rec.Event.KeyEvent.uChar.AsciiChar = '\r';
    DWORD written;
    WriteConsoleInputW(hIn, &rec, 1, &written);
#else
    // Push a carriage return ('\r') into the tty input buffer so that
    // kbhit()/getch() or read_key() returns immediately.
#ifdef TIOCSTI
    char c = '\r';
    ioctl(STDIN_FILENO, TIOCSTI, &c);
#endif
#endif
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
        {"Unused",        AnsiColor_Unused},
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
            el.current = 0;
            uint8_t r = static_cast<uint8_t>(el.tok_int(","));
            uint8_t g = static_cast<uint8_t>(el.tok_int(","));
            uint8_t b = static_cast<uint8_t>(el.tok_int(","));
            return Color(r, g, b);
        }
    }
    // "#RRGGBB" or "#RGB"
    if (!param.empty() && param[0] == '#') {
        size_t len = param.size();
        auto hex_val = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
            return 0;
        };
        if (len == 7) {
            // #RRGGBB
            uint8_t r = (hex_val(param[1]) << 4) | hex_val(param[2]);
            uint8_t g = (hex_val(param[3]) << 4) | hex_val(param[4]);
            uint8_t b = (hex_val(param[5]) << 4) | hex_val(param[6]);
            return Color(r, g, b);
        }
        if (len == 4) {
            // #RGB -> #RRGGBB
            uint8_t r = hex_val(param[1]) * 17;
            uint8_t g = hex_val(param[2]) * 17;
            uint8_t b = hex_val(param[3]) * 17;
            return Color(r, g, b);
        }
    }

    return Color();
}

Text::Char Text::Char::from_code(uint32_t cp)
{
    Char c;
    c.ch = cp;
    c.char_width = utf8_char_width(cp);
    c.size = utf8_wctomb(c.utf8, cp);
    return c;
}

/// <summary>
/// Text
/// </summary>

void Text::setText(const std::string& _text, int wrap)
{
    wrap_width = wrap;
    text = _text;
    text_width = 0;
    chars.resize(0);
    position.resize(0);

    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    size_t len = text.size();
    int cx = 0;
    int cy = 0;

    while (len > 0) {
        uint32_t cp = 0;
        int n = utf8_mbtowc(cp, p, static_cast<int>(len));
        if (n <= 0) break;

        // newline: start a new row
        if (cp == '\n' || cp == '\r') {
            position.push_back({ cx, cy });
            chars.push_back(Char::from_code('\n'));
            text_width = std::max(text_width, cx);
            cx = 0;
            cy++;
            p += n; len -= n;
            continue;
        }

        auto ch = Char::from_code(cp);

        // wrap: if this char would exceed the limit and we're not at row start, break
        if (wrap > 0 && cx + ch.char_width > wrap && cx > 0) {
            text_width = std::max(text_width, cx);
            cx = 0;
            cy++;
        }

        position.push_back({cx, cy});
        cx += ch.char_width;
        chars.push_back(ch);
        p += n; len -= n;
    }
    position.push_back({ cx, cy });

    text_width = std::max(text_width, cx);
    text_height = cy;
}

std::string Text::get_selected() const
{
    if (!selected.has_selection())
        return "";
    size_t off = byte_offset_of(selected.start);
    size_t len = byte_offset_of(selected.end) - off;
    return text.substr(off, len);
}

int Text::char_at(int x, int y) const
{
    int best_idx = -1;
    for (size_t i = 0; i < chars.size(); i++) {
        if (chars[i].char_width == 0)
            continue;
        if (position[i].y == y && x >= position[i].x) {
            if (x < position[i].x + chars[i].char_width)
                best_idx = static_cast<int>(i);
            else if (chars[i].ch == '\n')
                best_idx = static_cast<int>(i);
            else
                best_idx = static_cast<int>(i) + 1;
        }
    }
    // Handle empty lines or positions past all chars on a line
    if (best_idx < 0) {
        for (size_t i = 0; i < chars.size(); i++) {
            if (position[i].y == y) {
                best_idx = static_cast<int>(i);
                break;
            }
            if (position[i].y > y) {
                best_idx = static_cast<int>(i);
                break;
            }
        }
    }
    if (best_idx < 0)
        best_idx = static_cast<int>(chars.size());
    return std::max(0, std::min(best_idx, static_cast<int>(chars.size())));
}

Point Text::pos_of(int idx) const
{
    if (idx >= 0 && idx < static_cast<int>(chars.size())) {
        if (chars[idx].char_width == 0 && idx + 1 < position.size()) {
            return position[idx + 1];
        }
        return position[idx];
    }
    // Past end — compute position after last char on its line
    if (!chars.empty() && idx > 0) {
        int prev = idx - 1;
        while (prev >= 0 && prev < static_cast<int>(chars.size())) {
            Point p = position[prev];
            p.x += chars[prev].char_width;
            return p;
        }
    }
    return {};
}

size_t Text::byte_offset_of(int idx) const
{
    size_t off = 0;
    for (int i = 0; i < idx && i < static_cast<int>(chars.size()); i++)
        off += chars[i].size;
    return off;
}

int Text::cur_idx_of(const Point& cursor) const
{
    int idx = 0;
    for (size_t i = 0; i < position.size(); i++) {
        if (i<chars.size() && chars[i].char_width == 0)
            continue;
        auto& pos = position[i];
        if (pos.y == cursor.y)
            idx = (int)i;
        if (pos.x == cursor.x && pos.y == cursor.y)
            return static_cast<int>(i);
        if (pos.y > cursor.y)
            break;
    }
    return idx;
}

void Text::select_word(int x, int y)
{
    int idx = char_at(x, y);
    int start = idx, end = idx;
    size_t text_len = text.size();
    while (start > 0) {
        size_t off = byte_offset_of(start - 1);
        if (off >= text_len || std::isspace(static_cast<unsigned char>(text[off]))) break;
        start--;
    }
    while (end < static_cast<int>(chars.size())) {
        size_t off = byte_offset_of(end);
        if (off >= text_len || std::isspace(static_cast<unsigned char>(text[off]))) break;
        end++;
    }
    selected.start = start;
    selected.end = end;
}

void Text::reparse()
{
    Selection sel = selected;
    setText(text, wrap_width);
    selected = sel;
}

int Text::delete_selected(int idx)
{
    int s = idx;
    if (selected.start >= 0 && selected.end >= 0) {
        s = std::min(selected.start, selected.end);
        int e = std::max(selected.start, selected.end);
        size_t del_off = byte_offset_of(s);
        size_t del_len = byte_offset_of(e) - del_off;
        text.erase(del_off, del_len);
        reparse();
    }
    return s;
}

int Text::enter(int idx)
{
    if (selected.has_selection()) {
        idx = delete_selected(idx);
        selected.unselect();
    }
    size_t off = byte_offset_of(idx);
    text.insert(off, 1, '\n');
    reparse();
    return idx;
}
int Text::backspace(int idx)
{
    if (selected.has_selection()) {
        idx = delete_selected(idx);
        selected.unselect();
    }
    else if (idx > 0) {
        int prev = idx - 1;
        size_t off = byte_offset_of(prev);
        text.erase(off, chars[prev].size);
        reparse();
        idx--;
    }
    return idx;
}
int Text::del(int idx)
{
    if (selected.has_selection()) {
        idx = delete_selected(idx);
        selected.unselect();
    }
    else if (idx < static_cast<int>(chars.size())) {
        size_t off = byte_offset_of(idx);
        text.erase(off, chars[idx].size);
        reparse();
    }
    return idx;
}
int Text::insert(int idx, const std::string value)
{
    idx = std::max(0, std::min(idx, static_cast<int>(chars.size())));

    Text parsed;
    parsed.setText(value, 0);

    size_t off = byte_offset_of(idx);
    text.insert(off, value);
    reparse();
    return idx + static_cast<int>(parsed.chars.size());
}
Point Text::left(int idx)
{
    if (idx > 0) {
        idx--;
        if (chars[idx].char_width == 0) {
            return left(idx);
        }
    }
    return pos_of(idx);
}
Point Text::right(int idx)
{
    if (idx < position.size()) {
        idx++;
        if (chars[idx].char_width == 0)
            return right(idx);
    }
    return pos_of(idx);
}
Point Text::up(int idx)
{
    Point pos = pos_of(idx);
    int target_y = pos.y - 1;
    if (target_y >= 0) {
        for (size_t i = 0; i < position.size(); i++) {
            if (position[i].y == target_y && position[i].x <= pos.x)
                idx = static_cast<int>(i);
        }
        pos = pos_of(idx >= 0 ? idx : 0);
    }
    return pos;
}
Point Text::down(int idx)
{
    Point pos = pos_of(idx);
    int target_y = pos.y + 1;
    for (size_t i = 0; i < position.size(); i++) {
        if (position[i].y == target_y && position[i].x <= pos.x)
            idx = static_cast<int>(i);
    }
    pos = pos_of(idx >= 0 ? idx : static_cast<int>(position.size()));
    return pos;
}
Point Text::home(int idx)
{
    Point pos = pos_of(idx);
    pos.x = 0;
    return pos;
}
Point Text::end(int idx)
{
    Point pos = pos_of(idx);
    int target_y = pos.y;
    for (int i = static_cast<int>(position.size()) - 1; i >= 0; i--) {
        if (position[i].y == target_y) {
            idx = i;
            break;
        }
    }
    pos = pos_of(idx);
    return { pos.x , pos.y };
}

/// <summary>
/// RichText
/// </summary>

namespace {

static RichText::Style default_rich_style()
{
    return RichText::Style{};
}

}

void RichText::setStyle(int start, int end, const Color& fgColor,
    const Color& bgColor, bool bold, bool italic, bool underline)
{
    Style value;
    value.fg_color = fgColor;
    value.bg_color = bgColor;
    value.bold = bold;
    value.italic = italic;
    value.underline = underline;
    setStyle(start, end, value);
}

void RichText::setStyle(int start, int end, const Style& value)
{
    start = std::max(start, 0);
    end = std::min(end, static_cast<int>(styles.size()));
    if (start >= end)
        return;

    for (int i = start; i < end; ++i)
        styles[i] = value;
}

void RichText::setText(const std::string& _text, int wrap)
{
    // setText replaces the document, so all characters start with the
    // default style. Editing methods preserve styles separately below.
    Text::setText(_text, wrap);
    styles.assign(chars.size(), default_rich_style());
}

void RichText::appendText(const std::string& _text, const Style& value)
{
    if (_text.empty())
        return;

    if (styles.size() != chars.size())
        styles.resize(chars.size(), default_rich_style());

    // position has one sentinel entry describing the position after the last
    // character. Remove it while appending, then recreate it below.
    Point cursor = { 0, 0 };
    if (!position.empty()) {
        cursor = position.back();
        position.pop_back();
    }

    text += _text;

    const uint8_t* p = reinterpret_cast<const uint8_t*>(_text.data());
    size_t len = _text.size();
    while (len > 0) {
        uint32_t cp = 0;
        int n = utf8_mbtowc(cp, p, static_cast<int>(len));
        if (n <= 0)
            break;

        if (cp == '\n' || cp == '\r') {
            position.push_back(cursor);
            chars.push_back(Char::from_code('\n'));
            styles.push_back(value);
            text_width = std::max(text_width, cursor.x);
            cursor.x = 0;
            ++cursor.y;
        }
        else {
            Char ch = Char::from_code(cp);
            if (wrap_width > 0 && cursor.x + ch.char_width > wrap_width &&
                cursor.x > 0) {
                text_width = std::max(text_width, cursor.x);
                cursor.x = 0;
                ++cursor.y;
            }

            position.push_back(cursor);
            chars.push_back(ch);
            styles.push_back(value);
            cursor.x += ch.char_width;
        }

        p += n;
        len -= n;
    }

    position.push_back(cursor);
    text_width = std::max(text_width, cursor.x);
    text_height = cursor.y;
}

int RichText::delete_selected(int idx)
{
    if (!selected.has_selection())
        return idx;

    int start = std::min(selected.start, selected.end);
    int end = std::max(selected.start, selected.end);
    start = std::max(start, 0);
    end = std::min(end, static_cast<int>(chars.size()));

    if (start < end) {
        size_t byte_start = byte_offset_of(start);
        size_t byte_end = byte_offset_of(end);
        text.erase(byte_start, byte_end - byte_start);
        styles.erase(styles.begin() + start, styles.begin() + end);
        Text::setText(text, wrap_width);
        styles.resize(chars.size(), default_rich_style());
    }

    return start;
}

int RichText::enter(int idx)
{
    if (selected.has_selection()) {
        idx = delete_selected(idx);
        selected.unselect();
    }

    idx = std::max(0, std::min(idx, static_cast<int>(chars.size())));
    size_t byte_offset = byte_offset_of(idx);
    text.insert(byte_offset, 1, '\n');
    styles.insert(styles.begin() + idx, default_rich_style());
    Text::setText(text, wrap_width);
    styles.resize(chars.size(), default_rich_style());
    return idx;
}

int RichText::backspace(int idx)
{
    if (selected.has_selection()) {
        idx = delete_selected(idx);
        selected.unselect();
    }
    else if (idx > 0 && idx <= static_cast<int>(chars.size())) {
        int remove_at = idx - 1;
        size_t byte_offset = byte_offset_of(remove_at);
        text.erase(byte_offset, chars[remove_at].size);
        styles.erase(styles.begin() + remove_at);
        Text::setText(text, wrap_width);
        styles.resize(chars.size(), default_rich_style());
        --idx;
    }
    return idx;
}

int RichText::del(int idx)
{
    if (selected.has_selection()) {
        idx = delete_selected(idx);
        selected.unselect();
    }
    else if (idx >= 0 && idx < static_cast<int>(chars.size())) {
        size_t byte_offset = byte_offset_of(idx);
        text.erase(byte_offset, chars[idx].size);
        styles.erase(styles.begin() + idx);
        Text::setText(text, wrap_width);
        styles.resize(chars.size(), default_rich_style());
    }
    return idx;
}

int RichText::insert(int idx, const std::string value)
{
    if (selected.has_selection()) {
        idx = delete_selected(idx);
        selected.unselect();
    }

    idx = std::max(0, std::min(idx, static_cast<int>(chars.size())));

    Text parsed;
    parsed.setText(value, 0);
    size_t byte_offset = byte_offset_of(idx);
    text.insert(byte_offset, value);
    styles.insert(styles.begin() + idx, parsed.chars.size(), default_rich_style());

    Text::setText(text, wrap_width);
    styles.resize(chars.size(), default_rich_style());
    return idx + static_cast<int>(parsed.chars.size());
}

RichText::Style RichText::RichTextStyle(const Color& fg, const Color& bg, bool bold, bool italic, bool underline)
{
    return {
        fg, bg, bold, italic, underline
    };
}

/// <summary>
/// Cell
/// </summary>


void Cell::reset()
{
    content = " ";
    size = 1;
    fg_color = AnsiColor_White;
    bg_color = AnsiColor_Black;
    bold = false;
    italic = false;
    underline = false;
}

bool Cell::operator==(const Cell& o) const
{
    return size == o.size && bold == o.bold && italic == o.italic && underline == o.underline &&
        fg_color == o.fg_color && bg_color == o.bg_color && content == o.content;
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
    if (clips_.size() > 0) {
        clips_.push_back(clips_.back().intersect(clip));
    }
    else {
        clips_.push_back(clip);
    }
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
        cell.reset();
    }
}


void DrawBuffer::Text(const std::string& text, const Point& pos, const Color& color, bool bold, bool italic, bool underline)
{
    int cur_x = pos.x;
    int cur_y = pos.y;

    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    size_t len = text.size();
    Rect clip = { 0,0,width_ - 1, height_ - 1 };

    if (!clips_.empty()) {
        clip = clip.intersect(clips_.back());
    }

    int px = cur_y * width_;
    while (len > 0 && cur_y <= clip.y2) {
        uint32_t cp = 0;
        int n = utf8_mbtowc(cp, p, static_cast<int>(len));
        if (n <= 0) break;

        // newline: move to next row
        if (cp == '\n') {
            cur_y++;
            cur_x = pos.x;
            p += n; len -= n;
            px = cur_y * width_;
            continue;
        }

        int char_width = utf8_char_width(cp);
        if (clip.inside(Point{ cur_x, cur_y }) && cur_x + char_width - 1 <= clip.x2) {
            auto& cell = cells_[px + cur_x];
            cell.fg_color = color;
            cell.size = char_width;
            cell.bold = bold;
            cell.italic = italic;
            cell.underline = underline;
            cell.content = std::string((const char*)p, (size_t)n);
            if (cell.size > 1) {
                auto& next_cell = cells_[cur_y * width_ + cur_x + 1];
                next_cell.content = "";
            }
        }
        cur_x += char_width;
        p += n; len -= n;
    }
}

void DrawBuffer::Text(const Point& pos, const TUI::Text& text, const Color& color)
{
    Rect clip = { 0,0,width_ - 1, height_ - 1 };

    if (!clips_.empty()) {
        clip = clip.intersect(clips_.back());
    }

    for (size_t i = 0; i < text.chars.size(); i++) {
        const auto& ch = text.chars[i];
        if (ch.ch == '\n')
            continue;
        const auto& pt = text.position[i];
        int cur_x = pos.x + pt.x;
        int cur_y = pos.y + pt.y;

        if (cur_y > clip.y2) break;

        if (clip.inside(Point{ cur_x, cur_y }) && cur_x + ch.char_width - 1 <= clip.x2) {
            auto& cell = cells_[cur_y * width_ + cur_x];
            cell.fg_color = color;
            cell.size = ch.char_width;
            cell.bold = text.bold;
            cell.italic = text.italic;
            cell.underline = text.underline;
            cell.content = std::string((const char*)&ch.utf8, ch.size);
            bool is_sel = text.selected.is_selected((int)i);
            if (is_sel) {
                cell.bg_color = text.color_selected;
            }
            if (cell.size > 1) {
                auto& next_cell = cells_[cur_y * width_ + cur_x + 1];
                next_cell.content = "";
            }
        }
    }
}

void DrawBuffer::Text(const Point& pos, const TUI::RichText& text)
{
    Rect clip = { 0, 0, width_ - 1, height_ - 1 };
    if (!clips_.empty())
        clip = clip.intersect(clips_.back());

    for (size_t i = 0; i < text.chars.size(); ++i) {
        const auto& ch = text.chars[i];
        if (ch.ch == '\n')
            continue;
        //if (ch.char_width == 0)
        //    continue;

        const auto& pt = text.position[i];
        const auto& style = i < text.styles.size() ? text.styles[i] : RichText::Style{};
        int cur_x = pos.x + pt.x;
        int cur_y = pos.y + pt.y;

        if (cur_y > clip.y2)
            break;
        if (!clip.inside(Point{ cur_x, cur_y }) ||
            cur_x + ch.char_width - 1 > clip.x2)
            continue;

        auto& cell = cells_[cur_y * width_ + cur_x];
        bool is_selected = text.selected.is_selected(static_cast<int>(i));

        cell.fg_color = style.fg_color;
        cell.size = ch.char_width;
        cell.bold = style.bold;
        cell.italic = style.italic;
        cell.underline = style.underline;
        cell.content = std::string(ch.utf8, ch.size);

        if (is_selected)
            cell.bg_color = text.color_selected;
        else if (style.bg_color.ansi != AnsiColor_Unused)
            cell.bg_color = style.bg_color;

        if (cell.size > 1) {
            auto& next_cell = cells_[cur_y * width_ + cur_x + 1];
            next_cell.content = "";
            next_cell.size = 0;
            if (i + 1 < text.chars.size()) {
                const auto& next_ch = text.chars[i + 1];
                if (next_ch.char_width == 0 && next_ch.ch >= 0xFE00 && next_ch.ch <= 0xFE0F) {
                    next_cell.content = std::string(next_ch.utf8, next_ch.size);
                }
            }
        }
        else if (cell.size == 0) {
            cell.size = 0;
        }
    }
}

void DrawBuffer::Border(const Rect& r, const Color& bgcolor, BorderStyle_ style, const Color& color)
{
    // Apply clip region
    Rect clip = { 0,0,width_ - 1, height_ - 1 };
    if (!clips_.empty()) {
        clip = clip.intersect(clips_.back());
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
        if (clip.inside(Point {r.x, r.y})) {
            auto& cell = cells_[r.y * width_ + r.x];
            cell.content = tl;
            cell.size = 1;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw top-right corner
        if (clip.inside(Point{ r.x2, r.y })) {
            auto& cell = cells_[r.y * width_ + r.x2];
            cell.content = tr;
            cell.size = 1;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw bottom-left corner
        if (clip.inside(Point{ r.x, r.y2 })) {
            auto& cell = cells_[r.y2 * width_ + r.x];
            cell.content = bl;
            cell.size = 1;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw bottom-right corner
        if (clip.inside(Point{ r.x2, r.y2 })) {
            auto& cell = cells_[r.y2 * width_ + r.x2];
            cell.content = br;
            cell.size = 1;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }

        // Draw top and bottom horizontal lines (excluding corners)
        for (int x = std::max(r.x + 1, clip.x); x < r.x2 && x <= clip.x2; x++) {
            if (r.y >= clip.y && r.y <= clip.y2) {
                auto& cell = cells_[r.y * width_ + x];
                cell.content = h_line;
                cell.size = 1;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
            if (r.y2 >= clip.y && r.y2 <= clip.y2) {
                auto& cell = cells_[r.y2 * width_ + x];
                cell.content = h_line;
                cell.size = 1;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
        }

        // Draw left and right vertical lines (excluding corners)
        for (int y = std::max(r.y + 1, clip.y); y < r.y2 && y <= clip.y2; y++) {
            if (r.x >= clip.x && r.x <= clip.x2) {
                auto& cell = cells_[y * width_ + r.x];
                cell.content = v_line;
                cell.size = 1;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
            if (r.x2 >= clip.x && r.x2 <= clip.x2) {
                auto& cell = cells_[y * width_ + r.x2];
                cell.content = v_line;
                cell.size = 1;
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
    for (int y = std::max(fill_y_start, clip.y); y <= fill_y_end && y <= clip.y2; y++) {
        for (int x = std::max(fill_x_start, clip.x); x <= fill_x_end && x <= clip.x2; x++) {
            auto& cell = cells_[y * width_ + x];
            cell.content = " ";
            cell.size = 1;
            cell.bg_color = bgcolor;
        }
    }
}

void DrawBuffer::ScrollBar(const Point& pos, int length, int offset, int content_length, bool vertical, const Color& track_color, const Color& thumb_color)
{
    // Apply clip region
    Rect clip = { 0,0,width_ - 1, height_ - 1 };
    if (!clips_.empty()) {
        clip = clip.intersect(clips_.back());
    }

    int max_scroll = content_length - length;
    int thumb_size = std::max(1, length * length / content_length);
    int thumb_pos = (int)((float)offset / max_scroll * (length - thumb_size));

    for (int i = 0; i < length; ++i) {
        bool is_thumb = (i >= thumb_pos && i < thumb_pos + thumb_size);
        if (vertical) {
            int cy = pos.y + i;
            if (!clip.inside(Point{ pos.x, cy }))
                continue;
            auto& cell = cells_[cy * width_ + pos.x];
            cell.size = 1;
            cell.content = u8"█";
            if (is_thumb) {
                cell.fg_color = thumb_color;
            } else {
                cell.fg_color = track_color;
            }
        }
        else {
            int cx = pos.x + i;
            if (!clip.inside(Point{cx, pos.y}))
                continue;
            auto& cell = cells_[pos.y * width_ + cx];
            cell.size = 1;
            cell.content = u8"▄";
            if (is_thumb) {
                cell.fg_color = thumb_color;
            } else {
                cell.fg_color = track_color;
            }
        }
    }
}

void DrawBuffer::SetColor(const Point& pos, const Color& fgColor, const Color& bgColor)
{
    Rect clip = { 0,0,width_ - 1, height_ - 1 };
    if (!clips_.empty()) {
        clip = clip.intersect(clips_.back());
    }
    if (!clip.inside(pos))
        return;
    auto& cell = cells_[pos.y * width_ + pos.x];
    cell.fg_color = fgColor;
    cell.bg_color = bgColor;
}

void DrawBuffer::SetBgColor(const Point& pos, const Color& bgColor)
{
    Rect clip = { 0,0,width_ - 1, height_ - 1 };
    if (!clips_.empty()) {
        clip = clip.intersect(clips_.back());
    }
    if (!clip.inside(pos))
        return;
    auto& cell = cells_[pos.y * width_ + pos.x];
    cell.bg_color = bgColor;
}

void DrawBuffer::FillBgColor(const Rect& _r, const Color& bgColor)
{
    Rect clip = { 0,0,width_ - 1, height_ - 1 };
    if (!clips_.empty()) {
        clip = clip.intersect(clips_.back());
    }
    Rect r = _r.intersect(clip);
    int yx = r.y * width_;
    for (int y = r.y; y <= r.y2; ++y) {
        for (int x = r.x; x <= r.x2; ++x) {
            auto& cell = cells_[yx + x];
            cell.bg_color = bgColor;
        }
        yx += width_;
    }
}

/// <summary>
/// Event
/// </summary>


void Event::set_key(uint32_t _key, uint32_t _vkey) {
    type = EventType_Key;
    key = _key;
    vkey = _vkey;
}
bool Event::any_button_down() const {
    return type == EventType_Mouse && button >= 1 && button <= 3;
}
bool Event::any_first_down() const {
    return first_down[0] || first_down[1] || first_down[2];
}
void Event::reset() {
    type = EventType_None;
    key = 0;
    vkey = 0;
    button = 0;
    x = 0;
    y = 0;
    clicks = 0;
    first_down[0] = first_down[1] = first_down[2] = false;
    mouse_motion = false;
    shift = false;
    ctrl = false;
    alt = false;
    is_vt = false;
    is_paste_bracket = false;
    seq.clear();
    paste_text.clear();
}

// Parse modifier from a CSI sequence parameter.
// Windows VT Input encodes modifiers as the second semicolon-separated value:
//   1=none, 2=Shift, 3=Alt, 4=Shift+Alt, 5=Ctrl, 6=Ctrl+Shift,
//   7=Ctrl+Alt, 8=Ctrl+Shift+Alt
static void parse_csi_modifier(Event& ev, const std::string& seq)
{
    // Find the first semicolon after "ESC["
    size_t semi = seq.find(';', 2);
    if (semi == std::string::npos) return;

    // Read the modifier number up to the next non-digit
    int mod = 0;
    size_t p = semi + 1;
    while (p < seq.size() && seq[p] >= '0' && seq[p] <= '9')
        mod = mod * 10 + (seq[p++] - '0');

    if (mod >= 2) {
        ev.shift = ((mod & 2) != 0);
        ev.alt = ((mod & 4) != 0);
        ev.ctrl = ((mod & 8) != 0);
    }
}

// Combine a UTF-16 surrogate pair into a single codepoint.
// Returns the combined codepoint, or 0 if `ch` is not a high surrogate.
static uint32_t combine_surrogate(uint16_t hi, uint16_t lo)
{
    return 0x10000 + (static_cast<uint32_t>(hi - 0xD800) << 10) + (lo - 0xDC00);
}

// Per-thread state for buffering high surrogates.
static thread_local uint16_t s_pending_hi = 0;

// Decode a UTF-16 code unit into a Unicode codepoint, handling surrogate pairs.
// Returns the codepoint on success, or 0 if:
//   - ch is a high surrogate (buffered for next call)
//   - ch is a lone low surrogate (discarded)
static uint32_t decode_utf16(uint16_t ch)
{
    // High surrogate — buffer it
    if (ch >= 0xD800 && ch <= 0xDBFF) {
        s_pending_hi = ch;
        return 0;
    }

    // Low surrogate — combine with pending high surrogate
    if (ch >= 0xDC00 && ch <= 0xDFFF) {
        if (s_pending_hi != 0) {
            uint32_t cp = combine_surrogate(s_pending_hi, ch);
            s_pending_hi = 0;
            return cp;
        }
        // Lone low surrogate — discard
        return 0;
    }

    // Normal BMP character — flush any pending surrogate
    s_pending_hi = 0;
    return static_cast<uint32_t>(ch);
}

static void parse_ss3_key(Event& ev)
{
    if (ev.seq.size() < 3)
        return;

    switch (ev.seq[2]) {
    case 'P': ev.set_key(0, VK_F1); break;
    case 'Q': ev.set_key(0, VK_F2); break;
    case 'R': ev.set_key(0, VK_F3); break;
    case 'S': ev.set_key(0, VK_F4); break;
    default: break;
    }
    parse_csi_modifier(ev, ev.seq);
}

static bool parse_bracketed_paste(Event& ev, uint32_t ch)
{
    uint32_t cp = decode_utf16(static_cast<uint16_t>(ch));

    if (!cp)
        return true;

    char utf8[4];
    int n = utf8_wctomb(utf8, cp);
    ev.seq += std::string(utf8, n);

    if (ev.seq.size() >= 6 && ev.seq.substr(ev.seq.length() - 6) == "\x1b[201~") {
        ev.type = EventType_Paste;
        ev.paste_text = ev.seq.substr(6, ev.seq.length() - 12);
        ev.is_paste_bracket = false;
    }
    return true;
}

// Parse a complete CSI mouse event: ESC [ < Cb ; Cx ; Cy M/m
// Returns true if the sequence was consumed and ev is populated.
static bool parse_sgr_mouse(Event& ev)
{
    const std::string& seq = ev.seq;
    // Format: "\x1b[<Cb;Cx;CyM"  (button release uses 'm')

    size_t p = 3;
    auto next_semi = [&]() -> int {
        int val = 0;
        while (p < seq.size() && seq[p] == ';') { ++p; break; }
        while (p < seq.size() && seq[p] >= '0' && seq[p] <= '9')
            val = val * 10 + (seq[p++] - '0');
        return val;
        };

    int cb = next_semi(); // button code
    int cx = next_semi(); // column (1-based)
    int cy = next_semi(); // row    (1-based)

    if (p >= seq.size()) return false;
    bool is_release = (seq[p] == 'm');

    ev.type = EventType_Mouse;
    ev.x = cx - 1; // convert to 0-based
    ev.y = cy - 1;
    ev.mouse_motion = (cb & 32) != 0;
    ev.shift = (cb & 4) != 0;
    ev.alt = (cb & 8) != 0;
    ev.ctrl = (cb & 16) != 0;

    // Windows VT Input uses a hybrid encoding:
    //   Button codes (CSI-style): 0=left, 1=middle, 2=right, 3=move
    //   Scroll codes (SGR-style): 64=up, 65=down, 66=h-left, 67=h-right
    if (cb >= 64 && cb <= 67) {
        switch (cb) {
        case 64: ev.button = 4; break; // scroll up
        case 65: ev.button = 5; break; // scroll down
        case 66: ev.button = 6; break; // h-scroll left
        case 67: ev.button = 7; break; // h-scroll right
        }
    }
    else {
        switch (cb & 3) {
        case 0: ev.button = is_release ? 0 : 1; break;
        case 1: ev.button = is_release ? 0 : 2; break;
        case 2: ev.button = is_release ? 0 : 3; break;
        case 3: ev.button = 0;            break; // motion
        }
    }

    return true;
}

bool Event::parse_sequence(uint32_t ch)
{
    if (is_paste_bracket) {
        return parse_bracketed_paste(*this, ch);
    }

    if (!is_vt) {
        if (ch != '\x1b')
            return false;
        is_vt = true;
        seq.clear();
    }

    seq.push_back(static_cast<char>(ch));

    // ESC followed by anything other than '[' or 'O' is an Alt+Key
    // sequence, not a VT control sequence.
    //if (seq.size() == 2 && ch != '[' && ch != 'O') {
    //    type = EventType_Key;
    //    key = ch;
    //    alt = true;
    //    return true;
    //}

    if (seq.size() == 1)
        return true;

    if (seq.size() >= 6 && seq.compare(0, 6, "\x1b[200~") == 0) {
        is_paste_bracket = true;
        return true;
    }

    if (seq.size() >= 3 && seq[0] == '\x1b' && seq[1] == '[' && seq[2] == '<') {
        if (seq.back() == 'M' || seq.back() == 'm') {
            parse_sgr(ch);
        }
        return true;
    }

    if (seq.size() >= 3 && seq[0] == '\x1b' && seq[1] == '[') {
        parse_csi(ch);
        return true;
    }

    if (seq.size() >= 3 && seq[0] == '\x1b' && seq[1] == 'O') {
        parse_ss3_key(*this);
        return true;
    }

    if (seq.size() > 16) {
        LOG(::Color::RED, "parse_sequence %s", seq.c_str());
        reset();
        return false;
    }

    return true;
}

void Event::parse_csi(uint32_t ch)
{
    (void)ch;
    if (seq.size() < 3 || seq[0] != '\x1b' || seq[1] != '[')
        return;

    if (seq.back() == '~') {
        std::string num = seq.substr(2, seq.length() - 3);
        int n = 0;
        for (char c : num) {
            if (c >= '0' && c <= '9') {
                n = n * 10 + (c - '0');
            }
        }
        switch (n) {
        case 1:  set_key(0, VK_HOME);   break;
        case 2:  set_key(0, VK_INSERT); break;
        case 3:  set_key(0, VK_DELETE); break;
        case 4:  set_key(0, VK_END);    break;
        case 5:  set_key(0, VK_PRIOR);  break;
        case 6:  set_key(0, VK_NEXT);   break;
        case 11: set_key(0, VK_F1);     break;
        case 12: set_key(0, VK_F2);     break;
        case 13: set_key(0, VK_F3);     break;
        case 14: set_key(0, VK_F4);     break;
        case 15: set_key(0, VK_F5);     break;
        case 17: set_key(0, VK_F6);     break;
        case 18: set_key(0, VK_F7);     break;
        case 19: set_key(0, VK_F8);     break;
        case 20: set_key(0, VK_F9);     break;
        case 21: set_key(0, VK_F10);    break;
        case 23: set_key(0, VK_F11);    break;
        case 24: set_key(0, VK_F12);    break;
        default: break;
        }
        parse_csi_modifier(*this, seq);
        return;
    }

    char term = seq.back();
    switch (term) {
    case 'A': set_key(0, VK_UP);    break;
    case 'B': set_key(0, VK_DOWN);  break;
    case 'C': set_key(0, VK_RIGHT); break;
    case 'D': set_key(0, VK_LEFT);  break;
    case 'H': set_key(0, VK_HOME);  break;
    case 'F': set_key(0, VK_END);   break;
    case 'Z': set_key('\t', VK_TAB); shift = true; break;
    default: break;
    }
    parse_csi_modifier(*this, seq);
}
void Event::parse_sgr(uint32_t ch)
{
    (void)ch;
    if (seq.size() < 3 || seq[0] != '\x1b' || seq[1] != '[' || seq[2] != '<')
        return;

    if (seq.back() != 'M' && seq.back() != 'm')
        return;

    if (parse_sgr_mouse(*this)) {
        type = EventType_Mouse;
    }
}


/// <summary>
/// Terminal
/// </summary>

static bool is_terminal_control(const std::string& content)
{
    if (content.empty())
        return true;

    uint32_t cp = 0;
    const auto* bytes = reinterpret_cast<const uint8_t*>(content.data());
    int n = utf8_mbtowc(cp, bytes, static_cast<int>(content.size()));
    if (n <= 0)
        return true;

    // C0 controls, DEL, and C1 controls must not be emitted as terminal text.
    return cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F);
}

std::atomic<bool> Terminal::s_running = false;
std::thread* Terminal::s_event_thread = nullptr;
std::mutex   Terminal::s_event_mutex;
std::vector<Event> Terminal::s_events;

Terminal::Terminal()
{
    EnableRawMode();
    Resize();
}

void Terminal::Resize()
{
    auto size = GetSize();
    drawbuffers[0].resize(size.x, size.y);
    drawbuffers[1].resize(size.x, size.y);
    //std::cout << ANSI_CLEAR_SCREEN;
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

    Color cur_fg = AnsiColor_White;
    Color cur_bg = AnsiColor_Black;
    bool cur_bold = false;
    bool cur_italic = false;
    bool cur_underline = false;
    int cur_x = -1;
    int cur_y = -1;
    int yw = 0;

    std::string out;
    out.reserve(size.x * size.y * 30);

    for (int y = 0; y < cur_buf.height_; ++y) {
        bool same_line = true;
        for (int x = 0; x < cur_buf.width_; ++x) {
            auto& cur_cell = cur_buf.cells_[yw + x];
            auto& pre_cell = pre_buf.cells_[yw + x];
            if (cur_cell == pre_cell) {
                continue;
            }
            same_line = false;
            break;
        }
        if (same_line) {
            yw += size.x;
            continue;
        }
        for (int x = 0; x < cur_buf.width_; ++x) {
            auto& cur_cell = cur_buf.cells_[yw + x];
            if (cur_y != y) {
                out += CursorMove(x, y);
            }
            if (cur_cell.size > 0) {
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
            }
            if (cur_cell.content.empty()) {
            }
            else if (is_terminal_control(cur_cell.content)) {
                out += " ";
            }
            else {
                out += cur_cell.content;
            }
            cur_x += cur_cell.size;
            cur_y = y;

            if (cur_cell.size > 1) {
                cur_cell.size = cur_cell.size;
                // x += cur_cell.size - 1;
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

    if (SetConsoleMode(
        hIn, dwMode | ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_MOUSE_INPUT)) {
        vtSupported_ = true;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_MOUSE_INPUT;
    }
    else {
        // Fallback to legacy mouse input
        dwMode |= ENABLE_MOUSE_INPUT;
        vtSupported_ = false;
    }
    SetConsoleMode(hIn, dwMode);
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
        send_enter();
        s_event_thread->join();
        delete s_event_thread;
        s_event_thread = nullptr;
    }
    std::cout << SHOW_CURSOR << "\033[?1049l";  // Disable alternate screen buffer

    if (vtSupported_) {
        // 1003l 停用「延伸滑鼠報表」模式
        // 1006l 停用「SGR 滑鼠模式」
        // 2004l 停用「括號貼上」模式
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

void CopyClipboard(const std::string& text)
{
    // Convert the library's UTF-8 text to UTF-16 for CF_UNICODETEXT.
    std::vector<wchar_t> utf16;
    if (utf8_mbtowc(utf16, text) < 0)
        return;

    HGLOBAL hglb = GlobalAlloc(
        GMEM_MOVEABLE | GMEM_ZEROINIT,
        (utf16.size() + 1) * sizeof(wchar_t));
    if (!hglb)
        return;

    wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hglb));
    if (!dst) {
        GlobalFree(hglb);
        return;
    }

    if (!utf16.empty())
        memcpy(dst, utf16.data(), utf16.size() * sizeof(wchar_t));
    dst[utf16.size()] = L'\0';
    GlobalUnlock(hglb);

    if (!OpenClipboard(nullptr)) {
        GlobalFree(hglb);
        return;
    }

    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, hglb))
        GlobalFree(hglb);
    CloseClipboard();
}

static uint32_t map_key_event(const KEY_EVENT_RECORD& key, Event &ev)
{
    if (key.uChar.UnicodeChar != 0) {
        return decode_utf16(key.uChar.UnicodeChar);
    }

    switch (key.wVirtualKeyCode) {
    case VK_ESCAPE:  return 0x1B;
    //case VK_RETURN:  return '\n';
    //case VK_BACK:    return '\b';
    case VK_TAB:     return '\t';
    case VK_SPACE:   return ' ';
    default:         return 0; // navigation/function keys — use vkey instead
    }
}

void Terminal::event_thread()
{
    INPUT_RECORD rec;
    DWORD count;
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    DWORD prev_btn_state = 0;

    Event ev;
    auto flush_escape = [&]() {
        if (!ev.is_vt || ev.seq != "\x1b")
            return;

        ev.reset();
        ev.type = EventType_Key;
        ev.key = '\x1b';
        ev.vkey = VK_ESCAPE;
        {
            std::lock_guard<std::mutex> lock(s_event_mutex);
            s_events.push_back(ev);
        }
        ev.reset();
    };

    while (s_running.load()) {
        // ESC is also the prefix of every VT sequence. Give a lone ESC a
        // short window to receive its possible sequence continuation.
        DWORD wait_ms = (ev.is_vt && ev.seq == "\x1b") ? 50 : INFINITE;
        DWORD wait_result = WaitForSingleObject(hIn, wait_ms);
        if (wait_result == WAIT_TIMEOUT) {
            flush_escape();
            continue;
        }
        if (wait_result != WAIT_OBJECT_0)
            break;

        if (!ReadConsoleInputW(hIn, &rec, 1, &count) || count == 0)
            continue;

        if (ev.is_vt && ev.seq == "\x1b" && rec.EventType != KEY_EVENT) {
            flush_escape();
        }

        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            const auto& key_event = rec.Event.KeyEvent;
            bool vt_parsed = ev.parse_sequence(key_event.uChar.UnicodeChar);

            if (!vt_parsed) {
                ev.type = EventType_Key;
                ev.vkey = key_event.wVirtualKeyCode;
                ev.key = map_key_event(rec.Event.KeyEvent, ev);
                auto& cks = key_event.dwControlKeyState;
                ev.shift = (cks & SHIFT_PRESSED) != 0;
                ev.ctrl = (cks & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
                ev.alt = (cks & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
                if (ev.key == 0) {
                    ev.ctrl = true;
                    ev.key = ' ';
                }
                else if (ev.key == 8) {
                    ev.ctrl = true;
                    ev.vkey = VK_BACK;
                }
                else if (ev.key == 9) { //Tab
                }
                else if (ev.key == 10) {
                    ev.vkey = VK_RETURN;
                    ev.ctrl = true;
                }
                else if (ev.key == 13) {
                    ev.vkey = VK_RETURN;
                }
                else if (ev.key == 27) {
                    ev.vkey = VK_ESCAPE;
                }
                else if (ev.key == 127) {
                    ev.vkey = VK_BACK;
                }
                else if (ev.key >= 1 && ev.key <= 26) {
                    ev.ctrl = true; // Ctrl+A..Z (except Enter)
                }
            }
            // Push the event if it carries meaningful data.
            if (ev.type == EventType_Mouse) {
                ev.first_down[0] = ev.button == 1 && (prev_btn_state & FROM_LEFT_1ST_BUTTON_PRESSED) == 0;
                ev.first_down[1] = ev.button == 2 && (prev_btn_state & RIGHTMOST_BUTTON_PRESSED) == 0;
                ev.first_down[2] = ev.button == 3 && (prev_btn_state & FROM_LEFT_2ND_BUTTON_PRESSED) == 0;
                prev_btn_state = 0;
                if (ev.button == 1) prev_btn_state |= FROM_LEFT_1ST_BUTTON_PRESSED;
                if (ev.button == 2) prev_btn_state |= RIGHTMOST_BUTTON_PRESSED;
                if (ev.button == 3) prev_btn_state |= FROM_LEFT_2ND_BUTTON_PRESSED;
                std::lock_guard<std::mutex> lock(s_event_mutex);
                s_events.push_back(ev);
                ev.reset();
            }
            else if (ev.type == EventType_Paste) {
                std::lock_guard<std::mutex> lock(s_event_mutex);
                s_events.push_back(ev);
                ev.reset();
            }
            else if (ev.type == EventType_Key && (ev.key != 0 || ev.vkey != 0)) {
                std::lock_guard<std::mutex> lock(s_event_mutex);
                const WORD repeat = rec.Event.KeyEvent.wRepeatCount ? rec.Event.KeyEvent.wRepeatCount : 1;
                for (WORD i = 0; i < repeat; ++i) {
                    s_events.push_back(ev);
                }
                ev.reset();
            }
        }
        else if (rec.EventType == MOUSE_EVENT) {
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
                if (btnState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                    ev.button = 1;
                    ev.first_down[0] = (prev_btn_state & FROM_LEFT_1ST_BUTTON_PRESSED) == 0;
                } else if (btnState & RIGHTMOST_BUTTON_PRESSED) {
                    ev.button = 2;
                    ev.first_down[1] = (prev_btn_state & RIGHTMOST_BUTTON_PRESSED) == 0;
                } else if (btnState & FROM_LEFT_2ND_BUTTON_PRESSED) {
                    ev.button = 3;
                    ev.first_down[2] = (prev_btn_state & FROM_LEFT_2ND_BUTTON_PRESSED) == 0;
                }

                ev.clicks = (flags == DOUBLE_CLICK) ? 2 : 1;
            }

            prev_btn_state = btnState;

            std::lock_guard<std::mutex> lock(s_event_mutex);
            s_events.push_back(ev);
            ev.reset();
        }
    }
}

#else

void CopyClipboard(const std::string& text)
{
    (void)text;
}

struct termios originalTermios_;

// Decode one UTF-8 byte stream while preserving partial characters between
// read() calls.  VT control sequences are ASCII, while ordinary terminal
// input may contain arbitrary UTF-8 text.
struct Utf8InputDecoder {
    uint32_t codepoint = 0;
    uint32_t minimum = 0;
    int remaining = 0;

    template <typename Emit>
    void feed(unsigned char byte, Emit emit)
    {
        if (remaining == 0) {
            if (byte < 0x80) {
                emit(byte);
            }
            else if ((byte & 0xE0) == 0xC0) {
                codepoint = byte & 0x1F;
                minimum = 0x80;
                remaining = 1;
            }
            else if ((byte & 0xF0) == 0xE0) {
                codepoint = byte & 0x0F;
                minimum = 0x800;
                remaining = 2;
            }
            else if ((byte & 0xF8) == 0xF0) {
                codepoint = byte & 0x07;
                minimum = 0x10000;
                remaining = 3;
            }
            else {
                emit(0xFFFD);
            }
            return;
        }

        if ((byte & 0xC0) != 0x80) {
            // Invalid continuation: discard the incomplete character and
            // process this byte again as the beginning of a new character.
            codepoint = 0;
            minimum = 0;
            remaining = 0;
            emit(0xFFFD);
            feed(byte, emit);
            return;
        }

        codepoint = (codepoint << 6) | (byte & 0x3F);
        if (--remaining == 0) {
            uint32_t cp = codepoint;
            bool valid = cp >= minimum && cp <= 0x10FFFF &&
                         !(cp >= 0xD800 && cp <= 0xDFFF);
            emit(valid ? cp : 0xFFFD);
            codepoint = 0;
            minimum = 0;
        }
    }
};

static void emit_linux_input(Event& ev, uint32_t cp)
{
    std::lock_guard<std::mutex> lock(Terminal::s_event_mutex);
    static int held_button = 0;

    // ESC is ambiguous: it may be Escape itself, or the prefix of an Alt
    // key/VT sequence.  The event thread resolves a lone ESC after a short
    // timeout; if the next character is already available, treat it as Alt.
    if (ev.is_vt && ev.seq == "\x1b" && cp != '[' && cp != 'O') {
        ev.reset();
        ev.type = EventType_Key;
        ev.key = cp;
        ev.alt = true;
        Terminal::s_events.push_back(ev);
        ev.reset();
        return;
    }

    // parse_sequence() consumes ESC-prefixed VT sequences.  Once a complete
    // sequence has populated ev, publish it and start the next event.
    if (ev.parse_sequence(cp)) {
        if (ev.type != EventType_None) {
            if (ev.type == EventType_Mouse && ev.mouse_motion) {
                // SGR bit 5 means motion.  Preserve the currently held
                // button so Mgr can continue drag operations, while motion
                // with no button remains a hover event.
                ev.button = held_button;
                Terminal::s_events.push_back(ev);
                ev.reset();
                return;
            }

            if (ev.type == EventType_Mouse) {
                if (ev.button >= 1 && ev.button <= 3) {
                    ev.first_down[ev.button - 1] = held_button == 0;
                    held_button = ev.button;
                    ev.clicks = 1;
                }
                else if (ev.button == 0 && !ev.mouse_motion) {
                    held_button = 0;
                }
            }
            Terminal::s_events.push_back(ev);
            ev.reset();
        }
        return;
    }

    ev.type = EventType_Key;
    if (cp == 0) {
        ev.key = ' ';
        ev.ctrl = true;
    }
    else if (cp == 8 || cp == 127) {
        ev.key = cp;
        ev.vkey = VK_BACK;
    }
    else if (cp == '\t') {
        ev.key = '\t';
        ev.vkey = VK_TAB;
    }
    else if (cp == '\n' || cp == '\r') {
        ev.key = cp;
        ev.vkey = VK_RETURN;
    }
    else if (cp >= 1 && cp <= 26) {
        ev.key = static_cast<uint32_t>('a' + cp - 1);
        ev.ctrl = true;
    }
    else {
        ev.key = cp;
    }

    Terminal::s_events.push_back(ev);
    ev.reset();
}

static void flush_linux_escape(Event& ev)
{
    std::lock_guard<std::mutex> lock(Terminal::s_event_mutex);
    if (!ev.is_vt || ev.seq != "\x1b")
        return;

    ev.reset();
    ev.type = EventType_Key;
    ev.key = '\x1b';
    ev.vkey = VK_ESCAPE;
    Terminal::s_events.push_back(ev);
    ev.reset();
}

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
                                                       // motion) + SGR + Bracketed Paste
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
    std::cout << SHOW_CURSOR;          // Show cursor
    std::cout << "\033[?1049l";  // Disable alternate screen buffer
    std::cout << "\033[?1003l\033[?1006l\033[?2004l";  // Disable mouse and bracketed paste
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
    pollfd pfd{};
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    Event ev;
    Utf8InputDecoder decoder;
    auto escape_started = std::chrono::steady_clock::time_point{};

    while (s_running.load()) {
        int ready = poll(&pfd, 1, 100);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready == 0)
        {
            if (ev.is_vt && ev.seq == "\x1b") {
                auto now = std::chrono::steady_clock::now();
                if (escape_started.time_since_epoch().count() == 0)
                    escape_started = now;
                else if (now - escape_started >= std::chrono::milliseconds(50)) {
                    flush_linux_escape(ev);
                    escape_started = {};
                }
            }
            continue;
        }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            break;
        if (!(pfd.revents & POLLIN))
            continue;

        unsigned char buffer[4096];
        ssize_t count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            break;
        }
        if (count == 0)
            break;

        for (ssize_t i = 0; i < count; ++i) {
            decoder.feed(buffer[i], [&ev](uint32_t cp) {
                emit_linux_input(ev, cp);
            });
        }

        if (ev.is_vt && ev.seq == "\x1b") {
            if (escape_started.time_since_epoch().count() == 0)
                escape_started = std::chrono::steady_clock::now();
        }
        else {
            escape_started = {};
        }
    }
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
        tok_ = 0;
        if (tok().empty())
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

std::string EditLine::tok_line()
{
    auto line = lines[current];
    return line.substr(tok_ + 1);
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
///
/// </summary>

std::string ParseText(const std::string& t)
{
    std::string out;
    out.reserve(t.size());
    const char* p = t.c_str();
    while (*p) {
        if (*p == '\\' && *(p + 1)) {
            ++p;
            switch (*p) {
            case 'n': out += '\n'; break;
            case 's': out += ' ';  break;
            case 'u':
            {
                uint32_t cp = 0;
                for (int i = 0; i < 4 && *(p + 1); ++i) {
                    char c = *++p;
                    cp <<= 4;
                    if (c >= '0' && c <= '9')      cp |= (c - '0');
                    else if (c >= 'a' && c <= 'f')  cp |= (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F')  cp |= (c - 'A' + 10);
                }
                // encode codepoint as UTF-8
                if (cp < 0x80) {
                    out += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    out += static_cast<char>(0xC0 | (cp >> 6));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    out += static_cast<char>(0xE0 | (cp >> 12));
                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    out += static_cast<char>(0xF0 | (cp >> 18));
                    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                }
                break;
            }
            default: out += '\\'; out += *p; break;
            }
        } else {
            out += *p;
        }
        ++p;
    }
    return out;
}

Align_ ParseAlign(const std::string& tok)
{
    if (eqi(tok, "Start"))  return Align_Start;
    if (eqi(tok, "Center")) return Align_Center;
    if (eqi(tok, "End"))    return Align_End;
    return Align_Start;
}

Dock_ ParseDock(const std::string& tok)
{
    // support combinations: Left|Right, Top|Left|Down, etc.
    auto parse_one = [](const std::string& s) -> Dock_
    {
        if (eqi(s, "None"))       return Dock_None;
        if (eqi(s, "Top"))        return Dock_Top;
        if (eqi(s, "Left"))       return Dock_Left;
        if (eqi(s, "Right"))      return Dock_Right;
        if (eqi(s, "Down"))       return Dock_Down;
        if (eqi(s, "All"))        return Dock_All;
        if (eqi(s, "TopPane"))   return Dock_Top_Pane;
        if (eqi(s, "LeftPane"))  return Dock_Left_Pane;
        if (eqi(s, "RightPane")) return Dock_Right_Pane;
        if (eqi(s, "DownPane"))  return Dock_Down_Pane;
        return Dock_None;
    };

    uint32_t result = Dock_None;
    size_t start = 0;
    while (start < tok.size()) {
        size_t bar = tok.find('|', start);
        std::string part = tok.substr(start, bar == std::string::npos ? bar : bar - start);
        // trim whitespace
        while (!part.empty() && isspace(part.front())) part.erase(part.begin());
        while (!part.empty() && isspace(part.back()))  part.pop_back();
        result |= parse_one(part);
        if (bar == std::string::npos) break;
        start = bar + 1;
    }
    return (Dock_)result;
}

Arrange_ ParseArrange(const std::string& tok)
{
    if (eqi(tok, "None"))     return Arrange_None;
    if (eqi(tok, "Item"))     return Arrange_Item;
    if (eqi(tok, "Content"))  return Arrange_Content;
    return Arrange_None;
}

Autosize_ ParseAutosize(const std::string& tok)
{
    if (eqi(tok, "None"))         return Autosize_None;
    if (eqi(tok, "TextWidth"))    return Autosize_TextWidth;
    if (eqi(tok, "TextHeight"))   return Autosize_TextHeight;
    if (eqi(tok, "TextSize"))     return Autosize_TextSize;
    return Autosize_None;
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
Color Win::COLOR_SELECTED(120, 120, 120);
Color Win::COLOR_CURSOR(AnsiColor_Black);
Color Win::COLOR_CURSOR_BG(120, 180, 255);
Color Win::COLOR_LABEL(150, 150, 150);
Color Win::COLOR_BUTTON(200, 200, 200);
Color Win::COLOR_CHECKED(100, 255, 100);

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
        auto ob = Mgr::CreateByID(el.tok(), mgr);
        if (ob) {
            child.push_back(ob);
            ob->Parse(el);
        }
    }
    else if (eqi(cmd, "Clone")) {
        auto ob = GetUI(el.tok());
        if (ob) {
            ob = ob->Clone();
            child.push_back(ob);
            ob->Parse(el);
        }
    }
    else if (eqi(cmd, "Param")) {
        auto ob = GetUI(el.tok());
        if (ob) {
            ob->Parse(el);
        }
    }
    else if (eqi(cmd, "Name")) {
        name = el.tok();
    }
    else if (eqi(cmd, "Title")) {
        title = ParseText(el.tok_line());
    }
    else if (eqi(cmd, "Rect")) {
        local.x = el.tok_int();
        local.y = el.tok_int();
        local.x2 = el.tok_int();
        local.y2 = el.tok_int();
    }
    else if (eqi(cmd, "Visible")) {
        is_visible = el.tok_bool();
    }
    else if (eqi(cmd, "Notify")) {
        is_notifiable = el.tok_bool();
    }
    else if (eqi(cmd, "DrawBorder")) {
        draw_border = el.tok_bool();
    }
    else if (eqi(cmd, "BorderStyle")) {
        border_style = ParseBorderStyle(el.tok());
    }
    else if (eqi(cmd, "fgColor")) {
        fg_color = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "bgColor")) {
        bg_color = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "Dock")) {
        dock_.mode = ParseDock(el.tok());
        dock_.dock.x = el.tok_int();
        dock_.dock.y = el.tok_int();
        dock_.dock.x2 = el.tok_int();
        dock_.dock.y2 = el.tok_int();
    }
    else if (eqi(cmd, "DockOffset")) {
        dock_.offset.x = el.tok_int();
        dock_.offset.y = el.tok_int();
        dock_.offset.x2 = el.tok_int();
        dock_.offset.y2 = el.tok_int();
    }
    else if (eqi(cmd, "Arrange")) {
        arrange_.mode = ParseArrange(el.tok());
        arrange_.is_vertical = el.tok_bool();
        if (arrange_.mode == Arrange_Item) {
            arrange_.items = el.tok_int();
            arrange_.item_size.x = el.tok_int();
            arrange_.item_size.y = el.tok_int();
        }
    }
    else if (eqi(cmd, "Autosize")) {
        autosize_ = ParseAutosize(el.tok());
    }
    else if (eqi(cmd, "COLOR_BG")) {
        COLOR_BG = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "COLOR_HOVER")) {
        COLOR_HOVER = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "COLOR_DOWN")) {
        COLOR_DOWN = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "COLOR_BTN")) {
        COLOR_BTN = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "COLOR_TRACK")) {
        COLOR_TRACK = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "COLOR_THUMB")) {
        COLOR_THUMB = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "COLOR_SELECTED")) {
        COLOR_SELECTED = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "COLOR_CURSOR")) {
        COLOR_CURSOR = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "COLOR_CURSOR_BG")) {
        COLOR_CURSOR_BG = Color::Parse(el.tok());
    }
    else {
        ret = false;
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
    int lw = local.width();
    int lh = local.height();
    if (dock_.mode & Dock_Left) {
        local.x = (pw - 1) * dock_.dock.x / 100 + dock_.offset.x;
    }
    if (dock_.mode & Dock_Top) {
        local.y = (ph - 1) * dock_.dock.y / 100 + dock_.offset.y;
    }
    if (dock_.mode & Dock_Right) {
        local.x2 = (pw - 1) * dock_.dock.x2 / 100 + dock_.offset.x2;
    }
    if (dock_.mode & Dock_Down) {
        local.y2 = (ph - 1) * dock_.dock.y2 / 100 + dock_.offset.y2;
    }
    if (dock_.mode & Dock_Right_Pane) {
        local.x2 = (pw - 1) * dock_.dock.x2 / 100 + dock_.offset.x2;
        local.x = local.x2 - (lw - 1);
    }
    if (dock_.mode & Dock_Down_Pane) {
        local.y2 = (ph - 1) * dock_.dock.y2 / 100 + dock_.offset.y2;
        local.y = local.y2 - (lh - 1);
    }

    auto textSize = GetTextSize();

    switch (autosize_) {
    case Autosize_None:
        break;
    case Autosize_TextWidth:
        local.x2 = local.x + textSize.x;
        break;
    case Autosize_TextHeight:
        local.y2 = local.y + textSize.y;
        break;
    case Autosize_TextSize:
        local.x2 = local.x + textSize.x;
        local.y2 = local.y + textSize.y;
        break;
    }
    if (local.width() != lw || local.height() != lh) {
        OnSize();
        mgr->is_dirty = true;
    }

    screen = local.move(pt.x, pt.y);
    if (draw_border && border_style != BorderStyle_None) {
        clip = screen.expand(-1, -1);
    }
    else {
        clip = screen;
    }

    if (arrange_.mode == Arrange_Item) {
        /*
        Items mode:
        每個子元素使用 `item_width`，按方向排列。
        子元素放置於欄位中間
        Vertical:
        if items = 3
        item1 item2 item3
        item4 item5 item6

        Item_size mode:
        每個子元素使用固定的 `item_size`，按方向排列。
        子元素放置欄位左邊

        Vertical:
        |    width    |
        item1 item2   <- 超過寬度換到下行
        item3

        */
        if (arrange_.is_vertical) {
            int lw = local.width();
            int item_width = 0;
            if (arrange_.items > 0) {
                item_width = local.width() / arrange_.items;
            }
            else {
                item_width = arrange_.item_size.x;
            }
            int cx = 0;
            int cy = 0;
            int i = 0;
            int maxh = 0;

            for (auto ob : child) {
                if (!ob->is_visible)
                    continue;
                ob->dock_.mode = Dock_None;
                if (arrange_.items > 0) {
                    int cw = ob->local.width();
                    int ch = ob->local.height();
                    ob->local.x = cx + ((item_width - cw) >> 1);
                    ob->local.y = cy;
                    ob->local.x2 = ob->local.x + cw;
                    ob->local.y2 = cy + ch;
                    maxh = std::max(maxh, ch);
                    i++;
                    if (i >= arrange_.items) {
                        i = 0;
                        cx = 0;
                        cy += maxh;
                        maxh = 0;
                    }
                }
                else {
                    ob->local.x = cx;
                    ob->local.y = cy;
                    ob->local.x2 = cx + arrange_.item_size.x;
                    ob->local.y2 = cy + arrange_.item_size.y;

                    cx += arrange_.item_size.x;
                    if (cx >= lw) {
                        cx = 0;
                        cy += arrange_.item_size.y;
                    }
                }
            }
        }
        else {
            // Horizontal: items flow top-to-bottom, wrap to next column
            int lh = local.height();
            if (arrange_.items > 0) {
                int item_height = local.height() / arrange_.items;
                int cy = 0;
                int cx = 0;
                int i = 0;
                int maxw = 0;

                for (auto ob : child) {
                    if (!ob->is_visible)
                        continue;
                    ob->dock_.mode = Dock_None;
                    int cw = ob->local.width();
                    int ch = ob->local.height();
                    ob->local.x = cx;
                    ob->local.y = cy + ((item_height - ch) >> 1);
                    ob->local.x2 = cx + cw;
                    ob->local.y2 = cy + ch;
                    maxw = std::max(maxw, cw);
                    i++;
                    if (i >= arrange_.items) {
                        i = 0;
                        cy = 0;
                        cx += maxw;
                        maxw = 0;
                    }
                }
            }
            else {
                int cx = 0;
                int cy = 0;

                for (auto ob : child) {
                    if (!ob->is_visible)
                        continue;
                    ob->dock_.mode = Dock_None;
                    ob->local.x = cx;
                    ob->local.y = cy;
                    ob->local.x2 = cx + arrange_.item_size.x;
                    ob->local.y2 = cy + arrange_.item_size.y;

                    cy += arrange_.item_size.y;
                    if (cy >= lh) {
                        cy = 0;
                        cx += arrange_.item_size.x;
                    }
                }
            }
        }
    }
    else if (arrange_.mode == Arrange_Content) {
        /*
        Content mode:
        子元素寬度不是固定 如果下個元素超過 就換到下行

        Vertical:
        |    width    |
        btn1 check2   <- 超過寬度換到下行
        arrange1 btn3 <- 超過寬度換到下行
        chk1 chk2 chk3
        */
        if (arrange_.is_vertical) {
            int lw = local.width();
            int cx = 0;
            int cy = 0;
            int maxh = 0;

            for (auto ob : child) {
                if (!ob->is_visible)
                    continue;
                int cw = ob->local.width();
                int ch = ob->local.height();

                // wrap if this item doesn't fit on the current row (except first in row)
                if (cx + cw > lw && cx > 0) {
                    cx = 0;
                    cy += maxh;
                    maxh = 0;
                }

                ob->local.x = cx;
                ob->local.y = cy;
                ob->local.x2 = cx + cw - 1;
                ob->local.y2 = cy + ch - 1;

                cx += cw;
                maxh = std::max(maxh, ch);
            }
        }
        else {
            // Horizontal: items flow top-to-bottom, wrap to next column when exceeding height
            int lh = local.height();
            int cy = 0;
            int cx = 0;
            int maxw = 0;

            for (auto ob : child) {
                if (!ob->is_visible)
                    continue;
                int cw = ob->local.width();
                int ch = ob->local.height();

                if (cy + ch > lh && cy > 0) {
                    cy = 0;
                    cx += maxw;
                    maxw = 0;
                }

                ob->local.x = cx;
                ob->local.y = cy;
                ob->local.x2 = cx + cw - 1;
                ob->local.y2 = cy + ch - 1;

                cy += ch;
                maxw = std::max(maxw, cw);
            }
        }
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
        mgr->paint_list.push_back(ch);
        ch->Paint(drawbuf);
    }
    drawbuf.PopClip();
}

void Win::PaintBorder(DrawBuffer& drawbuf)
{
    if (draw_border) {
        drawbuf.Border(screen, bg_color, border_style, fg_color);
        if (!title.empty()) {
            drawbuf.Text(" " + title + " ", { screen.x + 1, screen.y }, fg_color);
        }
    }
}

WinPtr Win::GetUI(const std::string& _name)
{
    for (auto ch : child) {
        if (ch->name == _name)
            return ch;
        auto found = ch->GetUI(_name);
        if (found)
            return found;
    }
    return nullptr;
}

WinPtr Win::GetNotify(const Point& pt)
{
    for (int i = (int)child.size() - 1; i >= 0; i--) {
        auto ch = child[i];
        if (!ch->is_visible)
            continue;
        if (!ch->clip.inside(pt))
            continue;
        WinPtr n = ch->GetNotify(pt);
        if (n) { return n; }
        if (ch->is_notifiable)
            return ch;
    }
    return WinPtr(nullptr);
}

WinPtr Win::GetSlider(const Point& pt)
{
    if (!is_visible)
        return nullptr;
    if (!screen.inside(pt))
        return nullptr;
    for (int i = (int)child.size() - 1; i >= 0; i--) {
        auto ch = child[i];
        if (!ch->is_visible)
            continue;
        if (!ch->screen.inside(pt))
            continue;
        auto n = ch->GetSlider(pt);
        if (n) { return n; }
        if (ch->IsSlider() && ch->is_notifiable)
            return ch;
    }
    return nullptr;
}

void Win::AddChild(WinPtr obj)
{
    child.push_back(obj);
    mgr->is_dirty = true;
}

void Win::RemoveChild(const WinPtr& obj)
{
    auto it = std::find(child.begin(), child.end(), obj);
    if (it != child.end()) {
        child.erase(it);
        mgr->is_dirty = true;
    }
}

void Win::Copy(const Win* ob)
{
    name = ob->name;
    title = ob->title;
    is_visible = ob->is_visible;
    is_notifiable = ob->is_notifiable;
    draw_border = ob->draw_border;
    border_style = ob->border_style;
    bg_color = ob->bg_color;
    fg_color = ob->fg_color;
    dock_ = ob->dock_;
    arrange_ = ob->arrange_;
    autosize_ = ob->autosize_;
    local = ob->local;

    for (auto ch : ob->child) {
        if (!ch->is_cloneable)
            continue;
        AddChild(WinPtr(ch->Clone()));
    }
}

WinPtr Win::Clone() const
{
    Win *ob = new Win(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}

void Win::SetVisible(bool visible)
{
    is_visible = visible;
    mgr->is_dirty = true;
}

/// <summary>
/// Label
/// </summary>

Label::Label(Mgr* mgr) :Win(mgr) {
    is_notifiable = false;
    fg_color = COLOR_LABEL;
}

bool Label::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "TextAlign")) {
        text_algn = ParseAlign(el.tok());
    }
    else if (eqi(cmd, "Text")) {
        setText(ParseText(el.tok_line()));
    }
    else if (eqi(cmd, "Bold")) {
        bold = el.tok_bool();
    }
    else if (eqi(cmd, "Italic")) {
        italic = el.tok_bool();
    }
    else if (eqi(cmd, "Underline")) {
        underline = el.tok_bool();
    }
    else if (Win::ParseCmd(cmd, el)) {
    }
    else {
        ret = false;
    }
    return ret;
}

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
        int ty = clip.y;
        switch (text_algn) {
        case Align_Center:
            tx = clip.x + ((clip.width() - text_width) >> 1);
            ty = clip.y + ((clip.height() - text_height) >> 1);
            break;
        case Align_End:
            tx = clip.x2 - text_width;
            break;
        }
        drawbuf.Text({ tx, ty }, *this, fg_color);
    }
}

Point Label::GetTextSize() const
{
    Text tx;
    tx.setText(text, 0);
    return { tx.text_width, tx.text_height };
}

void Label::setText(const std::string& _text)
{
    Text::setText(_text, local.width());
    mgr->is_dirty = true;
    if (autosize_ != Autosize_None) {
        auto textSize = GetTextSize();
        switch (autosize_) {
        case Autosize_TextWidth:
            if (local.x2 != local.x + textSize.x) {
                local.x2 = local.x + textSize.x;
                Text::setText(_text, local.width());
            }
            break;
        case Autosize_TextHeight:
            local.y2 = local.y + textSize.y;
            break;
        case Autosize_TextSize:
            local.y2 = local.y + textSize.y;
            if (local.x2 != local.x + textSize.x) {
                local.x2 = local.x + textSize.x;
                Text::setText(_text, local.width());
            }
            break;
        }
    }
}

void Label::Copy(const Win* ob)
{
    Win::Copy(ob);
    const Label* o = dynamic_cast<const Label*>(ob);
    text_algn = o->text_algn;
    setText(o->text);
}
WinPtr Label::Clone() const
{
    Label* ob = new Label(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}

/// <summary>
/// Button
/// </summary>

Button::Button(Mgr* mgr) :Label(mgr) {
    text_algn = Align_Center;
    border_style = BorderStyle_None;
    draw_border = true;
    bg_color = COLOR_BTN;
    fg_color = COLOR_BUTTON;
    is_notifiable = true;
}

bool Button::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "ColorHover")) {
        bg_color_hover = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "ColorDown")) {
        bg_color_down = Color::Parse(el.tok());
    }
    else if (Label::ParseCmd(cmd, el)) {
    }
    else {
        ret = false;
    }
    return ret;
}

void Button::PaintBorder(DrawBuffer& drawbuf)
{
    Color bg = bg_color;
    if (is_down) {
        bg = bg_color_down;
    }
    else if (mgr->hover_.get() == this || mgr->notify_.get() == this) {
        bg = bg_color_hover;
    }
    if (draw_border) {
        drawbuf.Border(screen, bg, border_style, fg_color);
    }
}

void Button::Copy(const Win* ob)
{
    Label::Copy(ob);
    const Button* o = dynamic_cast<const Button*>(ob);
    bg_color_hover = o->bg_color_hover;
    bg_color_down = o->bg_color_down;
}
WinPtr Button::Clone() const
{
    Button* ob = new Button(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}

/// <summary>
/// Check
/// </summary>

Check::Check(Mgr* mgr) : Button(mgr) {
    bg_color = COLOR_BG;
    text_algn = Align_Start;
}

bool Check::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "ColorChecked")) {
        fg_color_checked = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "TextChecked")) {
        check_text.checked = ParseText(el.tok_line());
    }
    else if (eqi(cmd, "TextUnchecked")) {
        check_text.unchecked = ParseText(el.tok_line());
    }
    else if (Button::ParseCmd(cmd, el)) {
    }
    else {
        ret = false;
    }
    return ret;
}

void Check::PaintText(DrawBuffer& drawbuf)
{
    std::string checkmark = (checked) ? u8"✅" : u8"🔳";
    //std::string checkmark = (checked) ? "[x]" : "[ ]";
    int mark_start = utf8_width(checkmark) + 1;
    drawbuf.Text(checkmark, { clip.x, clip.y }, AnsiColor_Bright_White);
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
        Color fgcolor = checked ? fg_color_checked : fg_color;
        drawbuf.Text(text, { tx, clip.y }, fgcolor, bold, italic, underline);
    }
}

void Check::SetChecked(bool _checked)
{
    if (checked == _checked)
        return;
    checked = _checked;
    if(!check_text.checked.empty() && !check_text.unchecked.empty())
        setText(checked ? check_text.checked : check_text.unchecked);
    if (on_check) {
        on_check(checked);
    }
}

void Check::Click()
{
    SetChecked(!checked);
}

void Check::Copy(const Win* ob)
{
    Button::Copy(ob);
    const Check* o = dynamic_cast<const Check*>(ob);
    checked = o->checked;
    fg_color_checked = o->fg_color_checked;
    check_text = o->check_text;
}
WinPtr Check::Clone() const
{
    Check* ob = new Check(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}

/// <summary>
/// Combo
/// </summary>

Combo::Combo(Mgr* mgr) :Button(mgr)
{
    text_algn = Align_Start;
}

bool Combo::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "Item")) {
        items.push_back(ParseText(el.tok_line()));
    }
    else if (Button::ParseCmd(cmd, el)) {
    }
    else {
        ret = false;
    }
    return ret;
}

void Combo::PaintText(DrawBuffer& drawbuf)
{
    Button::PaintText(drawbuf);
    drawbuf.Text(u8"▼", { clip.x2, clip.y }, AnsiColor_Bright_White);
}

void Combo::SetValue(int _value)
{
    if (value == _value)
        return;
    value = _value;
    setText(value >= 0 && value < items.size() ? items[value] : "");
    if (on_selected) {
        on_selected(value);
    }
}

void Combo::Click()
{
    if (items.size() == 0)
        return;
    if (!menu) {
        menu = WinPtr(new Slider(mgr));
        menu->name = "COMBO_POPUP";
        menu->is_cloneable = false;
        menu->draw_border = true;
    }
    int item_count = 10;
    if (items.size() < item_count) {
        item_count = items.size();
    }
    menu->child.clear();
    if (screen.y + item_count + 2 > mgr->local.height()) {
        menu->local = { screen.x, screen.y - item_count - 2, screen.x2, screen.y - 1 };
    }
    else {
        menu->local = { screen.x, screen.y + 1, screen.x2, screen.y + item_count + 2 };
    }
    int w = screen.width() - 3;
    ButtonPtr first_btn = nullptr;
    for (int i = 0; i < items.size(); i++) {
        ButtonPtr btn = menu->Create<Button>("COMBO_MENU_ITEM_" + std::to_string(i), {0, i, w, i});
        btn->setText(items[i]);
        btn->on_click = [&, i]() {
            SetValue(i);
            mgr->ClosePopup();
            };
        if (!first_btn) {
            first_btn = btn;
        }
    }
    mgr->Popup(menu);
    mgr->SetNotify(first_btn);
    mgr->hover_slider_ = menu;
}

void Combo::Copy(const Win* ob)
{
    Button::Copy(ob);
    const Combo* o = dynamic_cast<const Combo*>(ob);
    items = o->items;
}
WinPtr Combo::Clone() const
{
    Combo* ob = new Combo(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}


/// <summary>
/// Slider
/// </summary>

Slider::Slider(Mgr* mgr) :Win(mgr)
{
    is_notifiable = true;
}

bool Slider::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "ScrollX")) {
        is_scroll_x = el.tok_bool();
    }
    else if (eqi(cmd, "ScrollY")) {
        is_scroll_y = el.tok_bool();
    }
    else if (eqi(cmd, "ColorTrack")) {
        color_track = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "ColorThumb")) {
        color_thumb = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "ScrollSpeed")) {
        scroll_speed = el.tok_int();
    }
    else if (Win::ParseCmd(cmd, el)) {
    }
    else {
        ret = false;
    }
    return ret;
}

void Slider::CalRect(Win* parent)
{
    Win::CalRect(parent);
    content_length = { 0,0 };
    if (is_scroll_y) {
        clip.x2--;
    }
    if (is_scroll_x) {
        clip.y2--;
    }
    if (is_scroll_y) {
        if (child.size() > 0) {
            auto ch = child.back();
            content_length.y = std::max(content_length.y, ch->local.y2 + 1);
        }
        if (content_length.y < clip.height()) {
            content_length.y = clip.height();
        }
        scroll_max.y = std::max(0, content_length.y - clip.height());
    }
    if (is_scroll_x) {
        if (child.size() > 0) {
            auto ch = child.back();
            content_length.x = std::max(content_length.x, ch->local.x2 + 1);
        }
        if (content_length.x < clip.width()) {
            content_length.x = clip.width();
        }
        scroll_max.x = std::max(0, content_length.x - clip.width());
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
    if (is_scroll_y) {
        drawbuf.ScrollBar({ clip.x2 + 1, clip.y }, clip.height(), scroll_value.y, content_length.y, true,
            color_track, color_thumb);
    }
    if (is_scroll_x) {
        drawbuf.ScrollBar({ clip.x, clip.y2 + 1 }, clip.width(), scroll_value.x, content_length.x, false,
            color_track, color_thumb);
    }
}

bool Slider::Event(const TUI::Event& ev)
{
    if (mgr->hover_slider_.get() != this)
        return false;
    Point pt = { ev.x, ev.y };
    Point old_scroll_value = scroll_value;
    bool any_click = ev.any_button_down();
    bool is_hover_x = false;
    bool is_hover_y = false;
    if (is_scroll_y) {
        Rect scrollbar = { clip.x2 + 1, clip.y, clip.x2 + 1, clip.y2 };
        is_hover_y = scrollbar.inside(pt);
    }
    if (is_scroll_x) {
        Rect scrollbar = { clip.x, clip.y2 + 1, clip.x2, clip.y2 + 1 };
        is_hover_x = scrollbar.inside(pt);
    }

    if (any_click) {
        if (is_scroll_y) {
            if (is_hover_y) {
                int click_offset = ev.y - clip.y;
                int max_scroll = content_length.y - clip.height();
                int thumb_size = std::max(1, clip.height() * clip.height() / content_length.y);
                int thumb_pos = (int)((float)scroll_value.y / max_scroll * (clip.height() - thumb_size));

                if (click_offset < thumb_pos) {
                    // Click above thumb — scroll up by one page
                    scroll_value.y = std::max(0, scroll_value.y - clip.height());
                }
                else if (click_offset >= thumb_pos + thumb_size) {
                    // Click below thumb — scroll down by one page
                    scroll_value.y = std::min(scroll_max.y, scroll_value.y + clip.height());
                }
            }
        }
        if (is_scroll_x) {
            if (is_hover_x) {
                int click_offset = ev.x - clip.x;
                int max_scroll = content_length.x - clip.width();
                int thumb_size = std::max(1, clip.width() * clip.width() / content_length.x);
                int thumb_pos = (int)((float)scroll_value.x / max_scroll * (clip.width() - thumb_size));
                if (click_offset < thumb_pos) {
                    scroll_value.x = std::max(0, scroll_value.x - clip.width());
                } else if (click_offset >= thumb_pos + thumb_size) {
                    scroll_value.x = std::min(scroll_max.x, scroll_value.x + clip.width());
                }
            }
        }
    }

    switch (ev.vkey) {
    case VK_PRIOR:
        if (is_scroll_y)
            scroll_value.y = std::max(0, scroll_value.y - clip.height());
        break;
    case VK_NEXT:
        if (is_scroll_y)
            scroll_value.y = std::min(scroll_max.y, scroll_value.y + clip.height());
        break;
    default:
        break;
    }

    switch (ev.button) {
    case 4:
        if (is_scroll_y && is_scroll_x) {
            if (is_hover_x) {
                if (scroll_value.x > 0) {
                    scroll_value.x -= scroll_speed;
                }
            }
            else {
                if (scroll_value.y > 0) {
                    scroll_value.y -= scroll_speed;
                }
            }
        }
        else if (is_scroll_y) {
            if (scroll_value.y > 0) {
                scroll_value.y -= scroll_speed;
            }
        }
        else if (is_scroll_x) {
            if (scroll_value.x > 0) {
                scroll_value.x -= scroll_speed;
            }
        }
        break;
    case 5:
        if (is_scroll_y && is_scroll_x) {
            if (is_hover_x) {
                if (scroll_value.x < scroll_max.x) {
                    scroll_value.x += scroll_speed;
                }
            }
            else {
                if (scroll_value.y < scroll_max.y) {
                    scroll_value.y += scroll_speed;
                }
            }
        }
        else if (is_scroll_y) {
            if (scroll_value.y < scroll_max.y) {
                scroll_value.y += scroll_speed;
            }
        }
        else if(is_scroll_x) {
            if (scroll_value.x < scroll_max.x) {
                scroll_value.x += scroll_speed;
            }
        }
        break;
    default:
        break;
    }
    if (old_scroll_value != scroll_value) {
        if (scroll_value.x < 0) scroll_value.x = 0;
        else if (scroll_value.x > scroll_max.x) scroll_value.x = scroll_max.x;
        if (scroll_value.y < 0) scroll_value.y = 0;
        else if (scroll_value.y > scroll_max.y) scroll_value.y = scroll_max.y;
        mgr->is_dirty = true;
        return true;
    }
    return false;
}

Point Slider::GetClipPos() const
{
    return { clip.x - scroll_value.x, clip.y - scroll_value.y };
}

int Slider::ScrollTo(int percent)
{
    int value = 0;
    if (is_scroll_x) {
        scroll_value.x = scroll_max.x * percent / 100;
        value = scroll_value.x;
    }
    if (is_scroll_y) {
        scroll_value.y = scroll_max.y * percent / 100;
        value = scroll_value.y;
    }
    return value;
}

void Slider::Copy(const Win* ob)
{
    Win::Copy(ob);
    const Slider* o = dynamic_cast<const Slider*>(ob);
    is_scroll_x = o->is_scroll_x;
    is_scroll_y = o->is_scroll_y;
    scroll_value = o->scroll_value;
    scroll_max = o->scroll_max;
    content_length = o->content_length;
    color_track = o->color_track;
    color_thumb = o->color_thumb;
    scroll_speed = o->scroll_speed;
}
WinPtr Slider::Clone() const
{
    Slider* ob = new Slider(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}

void Slider::AddChild(WinPtr obj)
{
    Win::AddChild(obj);
    if (is_scroll_x) {
        scroll_max.x = std::max(scroll_max.x, obj->local.x2 + 1 - clip.width());
    }
    if (is_scroll_y) {
        scroll_max.y = std::max(scroll_max.y, obj->local.y2 + 1 - clip.height());
    }
}

///
/// Edit
///

static bool TextEvent(
    Slider& slider,
    Text& text,
    Point& cursor,
    int& drag_start,
    bool& readonly,
    const TUI::Event& ev)
{
    bool change = false;
    Mgr* mgr = slider.mgr;
    if (!mgr || mgr->notify_.get() != &slider)
        return change;

    if (ev.type == EventType_Mouse) {
        Point pt = { ev.x, ev.y };
        slider.is_down = ev.any_button_down();

        if (!slider.clip.inside(pt)) {
            if (drag_start >= 0 && !slider.is_down)
                drag_start = -1;
            mgr->is_dirty = true;
            return change;
        }

        int lx = ev.x - slider.clip.x + slider.scroll_value.x;
        int ly = ev.y - slider.clip.y + slider.scroll_value.y;
        int idx = text.char_at(lx, ly);
        bool is_first_down = ev.any_first_down();

        if (drag_start >= 0 && slider.is_down) {
            text.selected.start = std::min(drag_start, idx);
            text.selected.end = std::max(drag_start, idx);
            cursor = text.pos_of(idx);
            mgr->is_dirty = true;
            return change;
        }

        if (is_first_down)
            drag_start = idx;

        if (drag_start >= 0 && !slider.is_down)
            drag_start = -1;

        if (ev.clicks == 2 && ev.button == 1) {
            text.select_word(lx, ly);
        }
        else if (ev.any_button_down()) {
            text.selected.unselect();
            cursor = text.pos_of(idx);
        }

        mgr->cursor = {
            slider.clip.x + cursor.x - slider.scroll_value.x,
            slider.clip.y + cursor.y - slider.scroll_value.y
        };
        mgr->is_dirty = true;
        return change;
    }

    if (ev.type == EventType_Paste) {
        int idx = text.cur_idx_of(cursor);
        idx = text.delete_selected(idx);
        idx = text.insert(idx, ev.paste_text);
        text.selected.unselect();
        cursor = text.pos_of(idx);
        mgr->is_dirty = true;
        return true;
    }

    if (ev.type != EventType_Key)
        return change;

    int idx = text.cur_idx_of(cursor);
    bool handled = true;

    if (ev.vkey == VK_BACK) {
        if (!readonly) {
            idx = text.backspace(idx);
            cursor = text.pos_of(idx);
            change = true;
        }
    }
    else if (ev.vkey == VK_RETURN) {
        if (!readonly) {
            Point newline_pos = text.pos_of(idx);
            text.enter(idx);
            cursor = { 0, newline_pos.y + 1 };
            change = true;
        }
    }
    else if (ev.vkey == VK_DELETE) {
        if (!readonly) {
            idx = text.del(idx);
            cursor = text.pos_of(idx);
            change = true;
        }
    }
    else if (ev.vkey == VK_LEFT || ev.vkey == VK_RIGHT ||
             ev.vkey == VK_UP || ev.vkey == VK_DOWN ||
             ev.vkey == VK_HOME || ev.vkey == VK_END) {
        Point new_cursor;
        if (ev.vkey == VK_LEFT)      new_cursor = text.left(idx);
        else if (ev.vkey == VK_RIGHT) new_cursor = text.right(idx);
        else if (ev.vkey == VK_UP)   new_cursor = text.up(idx);
        else if (ev.vkey == VK_DOWN) new_cursor = text.down(idx);
        else if (ev.vkey == VK_HOME) new_cursor = text.home(idx);
        else                         new_cursor = text.end(idx);

        int new_idx = text.cur_idx_of(new_cursor);
        if (ev.shift) {
            if (!text.selected.has_selection()) {
                text.selected.start = idx;
                text.selected.end = idx;
            }
            text.selected.start = std::min(text.selected.start, new_idx);
            text.selected.end = std::max(text.selected.end, new_idx);
        }
        else {
            text.selected.unselect();
        }
        cursor = new_cursor;
    }
    else if (ev.ctrl && ev.key == 3) {
        std::string selected = text.get_selected();
        if (!selected.empty())
            CopyClipboard(selected);
    }
    else if (ev.ctrl && ev.key == 1) {
        text.selected.start = 0;
        text.selected.end = static_cast<int>(text.chars.size());
    }
    else if (ev.key >= 32 && ev.key < 0x10FFFF && !readonly) {
        char utf8_buf[4];
        int n = utf8_wctomb(utf8_buf, ev.key);
        if (n > 0) {
            std::string value(utf8_buf, n);
            idx = text.delete_selected(idx);
            idx = text.insert(idx, value);
            text.selected.unselect();
            cursor = text.pos_of(idx);
            change = true;
        }
    }
    else {
        handled = false;
    }

    if (handled) {
        mgr->cursor = {
            slider.clip.x + cursor.x - slider.scroll_value.x,
            slider.clip.y + cursor.y - slider.scroll_value.y
        };
        mgr->is_dirty = true;
    }
    return change;
}

// Keep the active text cursor inside the widget viewport.  Edit and RichEdit
// share the same cursor coordinates and scrolling rules.
static void KeepCursorVisible(Slider& slider, const Point& cursor)
{
    Point old_scroll = slider.scroll_value;
    const int visible_width = std::max(1, slider.clip.width());
    const int visible_height = std::max(1, slider.clip.height());

    if (slider.is_scroll_x) {
        if (cursor.x < slider.scroll_value.x)
            slider.scroll_value.x = cursor.x;
        else if (cursor.x >= slider.scroll_value.x + visible_width)
            slider.scroll_value.x = cursor.x - visible_width + 1;
        slider.scroll_value.x = std::clamp(slider.scroll_value.x, 0, slider.scroll_max.x);
    }
    if (slider.is_scroll_y) {
        if (cursor.y < slider.scroll_value.y)
            slider.scroll_value.y = cursor.y;
        else if (cursor.y >= slider.scroll_value.y + visible_height)
            slider.scroll_value.y = cursor.y - visible_height + 1;
        slider.scroll_value.y = std::clamp(slider.scroll_value.y, 0, slider.scroll_max.y);
    }

    if (slider.mgr) {
        if (old_scroll != slider.scroll_value)
            slider.mgr->is_dirty = true;
        slider.mgr->cursor = {
            slider.clip.x + cursor.x - slider.scroll_value.x,
            slider.clip.y + cursor.y - slider.scroll_value.y
        };
    }
}

Edit::Edit(Mgr* mgr) :Slider(mgr) {
    color_selected = COLOR_SELECTED;
    is_notifiable = true;
}

bool Edit::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "Text")) {
        setText(ParseText(el.tok_line()));
    }
    else if (Slider::ParseCmd(cmd, el)) {
    }
    else {
        ret = false;
    }
    return ret;
}
Point Edit::GetTextSize() const
{
    Text natural;
    natural.setText(text, 0);
    return { natural.text_width, natural.text_height };
}

void Edit::CalRect(Win* parent)
{
    Slider::CalRect(parent);
    if (is_scroll_y) {
        content_length.y = std::max(content_length.y, text_height + 1);
        scroll_max.y = std::max(0, content_length.y - clip.height());
    }
    if (is_scroll_x) {
        content_length.x = std::max(content_length.x, text_width + 1);
        scroll_max.x = std::max(0, content_length.x - clip.width());
    }
}

void Edit::Paint(DrawBuffer& drawbuf)
{
    PaintBorder(drawbuf);
    PaintText(drawbuf);
    PaintScrollBar(drawbuf);
    PaintChild(drawbuf);
}

void Edit::PaintText(DrawBuffer& drawbuf)
{
    drawbuf.PushClip(clip);
    if (!text.empty()) {
        int tx = clip.x - scroll_value.x;
        int ty = clip.y - scroll_value.y;
        drawbuf.Text({ tx, ty }, *this, fg_color);
    }
    if (mgr->notify_.get() == this) {
        int cx = clip.x + cursor.x - scroll_value.x;
        int cy = clip.y + cursor.y - scroll_value.y;
        drawbuf.SetColor({ cx,cy }, COLOR_CURSOR, COLOR_CURSOR_BG);
    }
    drawbuf.PopClip();
}

void Edit::setText(const std::string& _text)
{
    if (text == _text)
        return;
    Text::setText(_text, local.width());
    selected.unselect();
    cursor = { 0,0 };
    KeepCursorVisible(*this, cursor);
    mgr->is_dirty = true;
    if (on_edit) {
        on_edit(this, _text);
    }
}

void Edit::OnSize()
{
    Text::setText(text, local.width());
}

bool Edit::Event(const TUI::Event& ev)
{
    bool r = Slider::Event(ev);
    auto old_cursor = cursor;
    if (on_key && on_key(ev))
        return true;

    if (TextEvent(*this, *this, cursor, drag_start, readonly, ev)) {
        if (on_edit) {
            on_edit(this, text);
        }
        r = true;
    }
    if (old_cursor != cursor)
        KeepCursorVisible(*this, cursor);
    return r;
}

void Edit::Copy(const Win* ob)
{
    Slider::Copy(ob);
    const Edit* other = dynamic_cast<const Edit*>(ob);
    text = other->text;
    chars = other->chars;
    position = other->position;
    bold = other->bold;
    italic = other->italic;
    underline = other->underline;
    text_width = other->text_width;
    text_height = other->text_height;
    wrap_width = other->wrap_width;
    selected = other->selected;
    color_selected = other->color_selected;
    cursor = other->cursor;
    readonly = other->readonly;
}
WinPtr Edit::Clone() const
{
    Edit* ob = new Edit(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}

/// <summary>
/// LabelEdit
/// </summary>

LabelEdit::LabelEdit(Mgr* mgr) : Edit(mgr)
{
    is_scroll_y = false;
    draw_border = true;
    border_style = BorderStyle_None;
    bg_color_hover = COLOR_HOVER;
}

bool LabelEdit::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "Label")) {
        label = ParseText(el.tok_line());
    }
    else if (eqi(cmd, "LabelWidth")) {
        label_width = el.tok_int();
    }
    else if (eqi(cmd, "ColorHover")) {
        bg_color_hover = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "ColorLabel")) {
        fg_color_label = Color::Parse(el.tok());
    }
    else if (Edit::ParseCmd(cmd, el)) {
    }
    else {
        ret = false;
    }
    return ret;
}
void LabelEdit::CalRect(Win* parent)
{
    Edit::CalRect(parent);
    clip.x += label_width;
}
void LabelEdit::PaintText(DrawBuffer& drawbuf)
{
    {
        int tx = clip.x - label_width;
        int ty = clip.y;
        drawbuf.Text(label, { tx, ty }, fg_color_label);
    }
    drawbuf.PushClip(clip);
    if (!text.empty()) {
        int tx = clip.x - scroll_value.x;
        int ty = clip.y - scroll_value.y;
        drawbuf.Text({ tx, ty }, *this, fg_color);
    }
    if (mgr->notify_.get() == this) {
        int cx = clip.x + cursor.x - scroll_value.x;
        int cy = clip.y + cursor.y - scroll_value.y;
        drawbuf.SetColor({ cx,cy }, COLOR_CURSOR, COLOR_CURSOR_BG);
    }
    drawbuf.PopClip();
}
void LabelEdit::PaintBorder(DrawBuffer& drawbuf)
{
    Color bg = bg_color;
    if (mgr->hover_.get() == this) {
        bg = bg_color_hover;
    }
    if (draw_border) {
        drawbuf.Border({ screen.x + label_width, screen.y, screen.x2, screen.y2 }, bg, border_style, fg_color);
    }
}

void LabelEdit::Copy(const Win* ob)
{
    Edit::Copy(ob);
    const LabelEdit* other = dynamic_cast<const LabelEdit*>(ob);
    label = other->label;
    label_width = other->label_width;
}

WinPtr LabelEdit::Clone() const
{
    LabelEdit* ob = new LabelEdit(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}

void LabelEdit::Input(const std::string& _label, std::string& value, fn_edit onedit)
{
    label = _label;
    setText(value);
    on_edit = [onedit, &value](Edit* edit, const std::string& text) {
        value = text;
        if (onedit) {
            onedit(edit, text);
        }
        };
}
void LabelEdit::Input(const std::string& _label, uint32_t& value, uint32_t step, fn_edit onedit)
{
    label = _label;
    setText(std::to_string(value));
    on_edit = [onedit, &value](Edit* edit, const std::string& _text) {
        try {
            value = std::stoi(_text);
        }
        catch (...) {}
        if (onedit) {
            onedit(edit, _text);
        }
        };
    int w = local.width() - label_width;
    ButtonPtr sub = Create<TUI::Button>(name + "-", { w - 6, 0, w - 4, 0 });
    sub->is_cloneable = false;
    sub->setText("-");
    sub->on_click = [&, step]() {
        if (value >= step) {
            value -= step;
            setText(std::to_string(value));
        }
        };
    ButtonPtr add = Create<TUI::Button>(name + "+", { w - 3, 0, w - 1, 0 });
    add->is_cloneable = false;
    add->setText("+");
    add->on_click = [&, step]() {
        value += step;
        setText(std::to_string(value));
        };
}
void LabelEdit::Button(const std::string& _label, const std::string& value, fn_click onclick)
{
    label = _label;
    ButtonPtr btn = Create<TUI::Button>(name + "<BUTTON>", { 0, 0, local.width() - label_width - 1, 0 });
    btn->setText(value);
    btn->is_cloneable = false;
    btn->on_click = onclick;
    is_notifiable = false;
}
void LabelEdit::Check(const std::string& _label, bool& value, const Check::CheckText& check_text, Check::fn_check oncheck)
{
    label = _label;
    CheckPtr chk = Create<TUI::Check>(name + "<CHECK>", { 0, 0, local.width() - label_width - 1, 0 });
    chk->check_text = check_text;
    chk->is_cloneable = false;
    chk->SetChecked(value);
    chk->on_check = [&value, oncheck, chk](bool checked) {
        value = checked;
        if (oncheck) {
            oncheck(value);
        }
        };
    is_notifiable = false;
}

void LabelEdit::Combo(const std::string& _label, int& value, const std::vector<std::string>& items, Combo::fn_selected onselect)
{
    label = _label;
    ComboPtr cbo = Create<TUI::Combo>(name + "COMBO", {0, 0, local.width() - label_width - 1, 0});
    cbo->items = items;
    cbo->SetValue(value);
    cbo->is_cloneable = false;
    cbo->on_selected = [&value, onselect](int _value) {
        value = _value;
        if (onselect) {
            onselect(_value);
        }
        };
    is_notifiable = false;
}

/// <summary>
/// RichEdit
/// </summary>

RichEdit::RichEdit(Mgr* mgr)
    : Slider(mgr)
{
    color_selected = COLOR_SELECTED;
    is_notifiable = true;
}

bool RichEdit::ParseCmd(const std::string& cmd, EditLine& el)
{
    if (eqi(cmd, "Text")) {
        setText(ParseText(el.tok_line()));
        return true;
    }
    else if (eqi(cmd, "Style")) {
        // Style <fgcolor> [<bgcolor>] [bold] [italic] [underline]
        std::istringstream args(el.tok_line());
        std::string fg;
        if (!(args >> fg))
            return false;

        RichText::Style style;
        style.fg_color = Color::Parse(fg);

        auto is_flag = [](const std::string& value) {
            return eqi(value, "bold") ||
                   eqi(value, "italic") ||
                   eqi(value, "underline");
        };

        std::string token;
        if (args >> token) {
            // A non-flag token after the foreground color is the optional
            // background color. "Unused" explicitly means no background fill.
            if (!is_flag(token)) {
                if (!eqi(token, "Unused"))
                    style.bg_color = Color::Parse(token);
            }
            else {
                // The first token is already a style flag.
                if (eqi(token, "bold")) style.bold = true;
                else if (eqi(token, "italic")) style.italic = true;
                else if (eqi(token, "underline")) style.underline = true;
            }
        }

        while (args >> token) {
            if (eqi(token, "bold"))
                style.bold = true;
            else if (eqi(token, "italic"))
                style.italic = true;
            else if (eqi(token, "underline"))
                style.underline = true;
            else
                return false;
        }

        current_style = style;
        return true;
    }
    else if (eqi(cmd, "AppendText")) {
        appendText(ParseText(el.tok_line()), current_style);
        return true;
    }
    return Slider::ParseCmd(cmd, el);
}

Point RichEdit::GetTextSize() const
{
    Text natural;
    natural.setText(text, 0);
    return { natural.text_width, natural.text_height };
}

void RichEdit::CalRect(Win* parent)
{
    Slider::CalRect(parent);

    if (is_scroll_y) {
        content_length.y = std::max(content_length.y, text_height + 1);
        scroll_max.y = std::max(0, content_length.y - clip.height());
    }
    if (is_scroll_x) {
        content_length.x = std::max(content_length.x, text_width + 1);
        scroll_max.x = std::max(0, content_length.x - clip.width());
    }
}

void RichEdit::Paint(DrawBuffer& drawbuf)
{
    PaintBorder(drawbuf);
    PaintText(drawbuf);
    PaintScrollBar(drawbuf);
    PaintChild(drawbuf);
}

void RichEdit::PaintText(DrawBuffer& drawbuf)
{
    drawbuf.PushClip(clip);

    if (!text.empty()) {
        int tx = clip.x - scroll_value.x;
        int ty = clip.y - scroll_value.y;
        drawbuf.Text({ tx, ty }, *this);
    }

    if (mgr && mgr->notify_.get() == this) {
        int cx = clip.x + cursor.x - scroll_value.x;
        int cy = clip.y + cursor.y - scroll_value.y;
        drawbuf.SetColor({ cx, cy }, COLOR_CURSOR, COLOR_CURSOR_BG);
    }

    drawbuf.PopClip();
}

void RichEdit::setText(const std::string& _text)
{
    if (text == _text)
        return;
    RichText::setText(_text, local.width());
    selected.unselect();
    cursor = { 0, 0 };
    if (mgr)
        mgr->is_dirty = true;
    if (on_edit) {
        on_edit(this, _text);
    }
}

void RichEdit::appendText(const std::string& _text, const Style& style)
{
    RichText::appendText(_text, style);

    if (is_scroll_y) {
        content_length.y = std::max(content_length.y, text_height + 1);
        scroll_max.y = std::max(0, content_length.y - clip.height());
    }
    if (is_scroll_x) {
        content_length.x = std::max(content_length.x, text_width + 1);
        scroll_max.x = std::max(0, content_length.x - clip.width());
    }

    if (mgr)
        mgr->is_dirty = true;
}

void RichEdit::OnSize()
{
    Text::setText(text, local.width());
}

bool RichEdit::Event(const TUI::Event& ev)
{
    bool r = Slider::Event(ev);
    auto old_cursor = cursor;
    if (on_key && on_key(ev)) {
        return true;
    }
    if (TextEvent(*this, *this, cursor, drag_start, readonly, ev)) {
        if (on_edit) {
            on_edit(this, text);
        }
        r = true;
    }
    if (old_cursor != cursor)
        KeepCursorVisible(*this, cursor);
    return r;
}

void RichEdit::Copy(const Win* ob)
{
    Slider::Copy(ob);
    const RichEdit* other = dynamic_cast<const RichEdit*>(ob);
    if (!other)
        return;

    text = other->text;
    chars = other->chars;
    position = other->position;
    styles = other->styles;
    bold = other->bold;
    italic = other->italic;
    underline = other->underline;
    text_width = other->text_width;
    text_height = other->text_height;
    wrap_width = other->wrap_width;
    selected = other->selected;
    color_selected = other->color_selected;
    cursor = other->cursor;
    readonly = other->readonly;
    current_style = other->current_style;
}

WinPtr RichEdit::Clone() const
{
    RichEdit* ob = new RichEdit(mgr);
    ob->Copy(this);
    return WinPtr(ob);
}

/// <summary>
/// Markdown
/// </summary>

enum TableBorderIndex {
    TableVertical,
    TableHorizontal,
    TableCornerTL,
    TableTopJoin,
    TableCornerTR,
    TableMiddleLeft,
    TableCross,
    TableMiddleRight,
    TableCornerBL,
    TableBottomJoin,
    TableCornerBR,
    TableBorderCount
};

std::string MarkdownStyle::table_border[TableBorderCount] = {
        u8"│", u8"─", u8"┌", u8"┬", u8"┐", u8"├", u8"┼", u8"┤", u8"└", u8"┴", u8"┘"
};

MarkdownStyle MarkdownStyle::DEFAULT = [] {
    MarkdownStyle result;

    result.text = RichText::RichTextStyle(Color(150,150,150));
    result.bold_text = RichText::RichTextStyle(Color(255,255,255), AnsiColor_Unused, true);
    result.italic_text = RichText::RichTextStyle(Color(220, 90, 90), AnsiColor_Unused, false, true);
    result.code = RichText::RichTextStyle(AnsiColor_Bright_Green, AnsiColor_Black);
    result.horizontal_rule = RichText::RichTextStyle(Color(100, 100, 100), AnsiColor_Unused, true);
    result.quote = RichText::RichTextStyle(AnsiColor_Bright_Blue, AnsiColor_Unused, false, true);

    result.heading[0] = RichText::RichTextStyle(Color(140, 190, 255), AnsiColor_Unused, true);
    result.heading[1] = RichText::RichTextStyle(Color(140, 190, 255), AnsiColor_Unused, true);
    result.heading[2] = RichText::RichTextStyle(Color(95, 200, 150), AnsiColor_Unused, true);
    result.heading[3] = RichText::RichTextStyle(Color(95, 200, 150), AnsiColor_Unused, true, false, true);
    result.heading[4] = RichText::RichTextStyle(Color(95, 200, 150), AnsiColor_Unused, true, true, true);
    result.heading[5] = RichText::RichTextStyle(Color(95, 200, 150), AnsiColor_Unused, true, true);

    result.table_header = RichText::RichTextStyle(AnsiColor_Bright_White, AnsiColor_Unused, true);
    result.table_cell = result.text;
    result.table_border_style.fg_color = Color(100, 100, 100);
    result.table_separator.fg_color = Color(100, 100, 100);
    result.table_separator.bold = true;

    for (int i = 0; i < 6; ++i)
        result.heading_rule[i] = result.heading[i];

    //·•●⬤
    MarkdownStyle::Replacement bullet = { "- " , u8"● " , RichText::RichTextStyle(AnsiColor_Bright_White) };
    MarkdownStyle::Replacement check = { "[x]" , u8"✅" , RichText::RichTextStyle(AnsiColor_Bright_White) };
    MarkdownStyle::Replacement uncheck = { "[ ]" , u8"🔳" , RichText::RichTextStyle(AnsiColor_Bright_White) };
    result.replacements.push_back(bullet);
    result.replacements.push_back(check);
    result.replacements.push_back(uncheck);

    return result;
}();

void Markdown(RichEdit* edit, const std::string& md, const MarkdownStyle& markdown_style)
{
    if (!edit)
        return;

    //edit->setText("");

    auto append_inline = [&](const std::string& input, RichText::Style base_style) {
        std::string plain;
        RichText::Style run_style = base_style;

        auto flush = [&]() {
            if (!plain.empty()) {
                edit->appendText(plain, run_style);
                plain.clear();
            }
        };

        auto is_marker = [](const std::string& marker) {
            return marker == "**" || marker == "__" ||
                   marker == "*" || marker == "_";
        };

        auto has_closing_marker = [&](size_t start, const std::string& marker) {
            return input.find(marker, start) != std::string::npos;
        };

        struct TagState {
            std::string name;
            RichText::Style previous;
        };
        std::vector<TagState> tag_stack;
        std::vector<RichText::Style> bold_styles;
        std::vector<RichText::Style> italic_styles;

        for (size_t i = 0; i < input.size();) {
            if (input[i] == '<') {
                size_t close = input.find('>', i + 1);
                if (close != std::string::npos) {
                    std::string tag = input.substr(i + 1, close - i - 1);
                    bool closing = !tag.empty() && tag[0] == '/';
                    std::string name = closing ? tag.substr(1) : tag;

                    if (closing) {
                        if (!tag_stack.empty() &&
                            eqi(tag_stack.back().name, name.c_str())) {
                            flush();
                            run_style = tag_stack.back().previous;
                            tag_stack.pop_back();
                            i = close + 1;
                            continue;
                        }
                    }
                    else {
                        size_t equal = tag.find('=');
                        if (equal != std::string::npos) {
                            std::string key = tag.substr(0, equal);
                            std::string value = tag.substr(equal + 1);
                            if (eqi(key, "fg") || eqi(key, "bg")) {
                                flush();
                                tag_stack.push_back({ key, run_style });
                                if (eqi(key, "fg"))
                                    run_style.fg_color = Color::Parse(value);
                                else
                                    run_style.bg_color = Color::Parse(value);
                                i = close + 1;
                                continue;
                            }
                        }
                    }
                }
            }

            // Apply the longest matching replacement first. This allows a
            // replacement such as an emoji sequence to contain multiple code points.
            const MarkdownStyle::Replacement* replacement = nullptr;
            for (const auto& candidate : markdown_style.replacements) {
                if (candidate.source.empty() ||
                    input.compare(i, candidate.source.size(), candidate.source) != 0)
                    continue;
                if (!replacement || candidate.source.size() > replacement->source.size())
                    replacement = &candidate;
            }

            if (replacement) {
                flush();
                edit->appendText(replacement->text, replacement->style);
                i += replacement->source.size();
                continue;
            }

            std::string marker;
            if (i + 1 < input.size()) {
                std::string two = input.substr(i, 2);
                if (is_marker(two))
                    marker = two;
            }
            if (marker.empty() && (input[i] == '*' || input[i] == '_'))
                marker.assign(1, input[i]);

            if (marker.empty()) {
                plain += input[i++];
                continue;
            }

            bool is_bold = marker.size() == 2;
            bool active = is_bold ? run_style.bold : run_style.italic;
            bool has_closing = active ||
                has_closing_marker(i + marker.size(), marker);

            // Keep unmatched Markdown punctuation as ordinary text.
            if (!has_closing) {
                plain += marker;
                i += marker.size();
                continue;
            }

            flush();
            if (is_bold) {
                if (bold_styles.empty()) {
                    bold_styles.push_back(run_style);
                    run_style = markdown_style.bold_text;
                }
                else {
                    run_style = bold_styles.back();
                    bold_styles.pop_back();
                }
            }
            else {
                if (italic_styles.empty()) {
                    italic_styles.push_back(run_style);
                    run_style = markdown_style.italic_text;
                }
                else {
                    run_style = italic_styles.back();
                    italic_styles.pop_back();
                }
            }
            i += marker.size();
        }

        flush();
    };

    auto split_table_row = [](const std::string& line) {
        std::vector<std::string> cells;
        size_t start = 0;
        while (start < line.size() && line[start] == ' ')
            ++start;
        if (start < line.size() && line[start] == '|')
            ++start;

        while (start <= line.size()) {
            size_t end = line.find('|', start);
            if (end == std::string::npos)
                end = line.size();

            std::string cell = line.substr(start, end - start);
            size_t left = cell.find_first_not_of(" \t");
            size_t right = cell.find_last_not_of(" \t");
            if (left == std::string::npos)
                cell.clear();
            else
                cell = cell.substr(left, right - left + 1);
            cells.push_back(cell);

            if (end == line.size())
                break;
            start = end + 1;
        }

        size_t last = line.find_last_not_of(" \t");
        if (last != std::string::npos && line[last] == '|' &&
            !cells.empty() && cells.back().empty()) {
            cells.pop_back();
        }
        return cells;
    };

    auto is_table_separator = [](const std::vector<std::string>& cells) {
        if (cells.empty())
            return false;
        for (const auto& cell : cells) {
            if (cell.size() < 3)
                return false;
            for (char ch : cell) {
                if (ch != '-' && ch != ':')
                    return false;
            }
        }
        return true;
    };

    auto repeat = [](const std::string& value, int count) {
        std::string result;
        for (int i = 0; i < count; ++i)
            result += value;
        return result;
    };

    auto table_display_width = [&](const std::string& value) {
        std::string visible;
        for (size_t i = 0; i < value.size();) {
            if (i + 1 < value.size() &&
                ((value[i] == '*' && value[i + 1] == '*') ||
                 (value[i] == '_' && value[i + 1] == '_'))) {
                i += 2;
                continue;
            }
            if (value[i] == '*' || value[i] == '_') {
                ++i;
                continue;
            }

            const MarkdownStyle::Replacement* replacement = nullptr;
            for (const auto& candidate : markdown_style.replacements) {
                if (!candidate.source.empty() &&
                    value.compare(i, candidate.source.size(), candidate.source) == 0 &&
                    (!replacement || candidate.source.size() > replacement->source.size())) {
                    replacement = &candidate;
                }
            }
            if (replacement) {
                visible += replacement->text;
                i += replacement->source.size();
            }
            else {
                visible += value[i++];
            }
        }
        return utf8_width(visible);
    };

    std::vector<std::string> lines;
    size_t line_start = 0;
    while (line_start <= md.size()) {
        size_t line_end = md.find('\n', line_start);
        if (line_end == std::string::npos)
            line_end = md.size();
        lines.push_back(md.substr(line_start, line_end - line_start));
        if (line_end == md.size())
            break;
        line_start = line_end + 1;
    }

    auto is_fence = [](const std::string& line) {
        size_t first = line.find_first_not_of(' ');
        return first != std::string::npos &&
               line.compare(first, 3, "```") == 0;
    };

    auto syntax_for_fence = [](const std::string& line) -> const Syntax& {
        size_t first = line.find_first_not_of(' ');
        if (first == std::string::npos)
            return Syntax::CPP;

        std::string language = line.substr(first + 3);
        const size_t start = language.find_first_not_of(" \t");
        if (start == std::string::npos)
            return Syntax::CPP;
        const size_t end = language.find_last_not_of(" \t");
        language = language.substr(start, end - start + 1);
        std::transform(language.begin(), language.end(), language.begin(),
            [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });

        if (language == "cpp" || language == "c++" ||
            language == "cxx" || language == "cc" || language == "c")
            return Syntax::CPP;
        if (language == "js" || language == "javascript")
            return Syntax::JS;
        if (language == "ts" || language == "typescript")
            return Syntax::TS;
        if (language == "go")
            return Syntax::Go;
        if (language == "rust" || language == "rs")
            return Syntax::Rust;
        if (language == "python" || language == "py")
            return Syntax::Python;
        if (language == "json")
            return Syntax::JSON;
        if (language == "ini")
            return Syntax::INI;
        return Syntax::CPP;
    };

    auto horizontal_rule_width = [](const std::string& line) {
        size_t first = line.find_first_not_of(" \t");
        size_t last = line.find_last_not_of(" \t");
        if (first == std::string::npos || last - first + 1 < 3)
            return 0;

        char marker = line[first];
        if (marker != '-' && marker != '*' && marker != '_')
            return 0;
        for (size_t i = first; i <= last; ++i) {
            if (line[i] != marker)
                return 0;
        }
        return static_cast<int>(last - first + 1);
    };

    for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const std::string& line = lines[line_index];

        if (is_fence(line)) {
            size_t code_index = line_index + 1;
            bool first_code_line = true;
            std::string code;
            while (code_index < lines.size() && !is_fence(lines[code_index])) {
                if (!first_code_line)
                    code += '\n';
                code += lines[code_index];
                first_code_line = false;
                ++code_index;
            }
            if (!code.empty())
                SyntaxText(edit, code, syntax_for_fence(line));

            // Consume the closing fence when present. An unclosed fence
            // consumes the remainder of the Markdown document.
            if (code_index < lines.size())
                line_index = code_index;
            else
                line_index = lines.size() - 1;
        }
        else {
        int marker_width = horizontal_rule_width(line);
        if (marker_width > 0) {
            int rule_width = edit->local.width();
            if (rule_width <= 0)
                rule_width = marker_width;
            append_inline(repeat(markdown_style.horizontal_rule_char, rule_width),
                markdown_style.horizontal_rule);
        }
        else {
        std::vector<std::string> header_cells = split_table_row(line);
        bool is_table = header_cells.size() >= 2 &&
            line.find('|') != std::string::npos &&
            line_index + 1 < lines.size() &&
            is_table_separator(split_table_row(lines[line_index + 1]));

        if (is_table) {
            std::vector<std::vector<std::string>> rows;
            rows.push_back(header_cells);
            line_index += 2; // skip header and separator

            while (line_index < lines.size() &&
                   lines[line_index].find('|') != std::string::npos) {
                rows.push_back(split_table_row(lines[line_index]));
                ++line_index;
            }
            --line_index;

            size_t column_count = 0;
            for (const auto& row : rows)
                column_count = std::max(column_count, row.size());

            std::vector<int> column_width(column_count, 1);
            for (const auto& row : rows) {
                for (size_t column = 0; column < row.size(); ++column)
                    column_width[column] = std::max(
                        column_width[column], table_display_width(row[column]));
            }

            auto append_border = [&](const std::string& left,
                                     const std::string& cross,
                                     const std::string& right,
                                     const RichText::Style& border_style) {
                std::string border = left;
                for (size_t column = 0; column < column_count; ++column) {
                    border += repeat(MarkdownStyle::table_border[TableHorizontal],
                        column_width[column] + 2);
                    border += column + 1 == column_count ? right : cross;
                }
                append_inline(border, border_style);
            };

            auto append_row = [&](const std::vector<std::string>& row,
                                  const RichText::Style& row_style) {
                append_inline(MarkdownStyle::table_border[TableVertical],
                    markdown_style.table_border_style);
                for (size_t column = 0; column < column_count; ++column) {
                    std::string cell = column < row.size() ? row[column] : "";
                    int padding = column_width[column] - table_display_width(cell);
                    append_inline(" " + cell + std::string(padding + 1, ' '),
                        row_style);
                    append_inline(MarkdownStyle::table_border[TableVertical],
                        markdown_style.table_border_style);
                }
            };

            append_border(MarkdownStyle::table_border[TableCornerTL],
                MarkdownStyle::table_border[TableTopJoin],
                MarkdownStyle::table_border[TableCornerTR],
                markdown_style.table_border_style);
            edit->appendText("\n", markdown_style.text);
            append_row(rows[0], markdown_style.table_header);

            if (rows.size() > 1) {
                edit->appendText("\n", markdown_style.text);
                append_border(MarkdownStyle::table_border[TableMiddleLeft],
                    MarkdownStyle::table_border[TableCross],
                    MarkdownStyle::table_border[TableMiddleRight],
                    markdown_style.table_separator);
                for (size_t row = 1; row < rows.size(); ++row) {
                    edit->appendText("\n", markdown_style.text);
                    append_row(rows[row], markdown_style.table_cell);
                }
            }

            edit->appendText("\n", markdown_style.text);
            append_border(MarkdownStyle::table_border[TableCornerBL],
                MarkdownStyle::table_border[TableBottomJoin],
                MarkdownStyle::table_border[TableCornerBR],
                markdown_style.table_border_style);
        }
        else {
            size_t first = line.find_first_not_of(' ');
            int level = 0;
            if (first != std::string::npos && line[first] == '>') {
                size_t p = first;
                int quote_level = 0;
                while (p < line.size() && line[p] == '>') {
                    ++p;
                    ++quote_level;
                    while (p < line.size() && line[p] == ' ')
                        ++p;
                }

                for (int i = 0; i < quote_level; ++i)
                    append_inline(markdown_style.quote_prefix,
                        markdown_style.quote);
                append_inline(line.substr(p), markdown_style.quote);
            }
            else if (first != std::string::npos) {
                size_t p = first;
                while (p < line.size() && line[p] == '#' && level < 6) {
                    ++p;
                    ++level;
                }
                if (level > 0 && (p == line.size() || line[p] != ' '))
                    level = 0;
                if (level > 0)
                    ++p;

                if (level > 0) {
                    std::string heading_text = line.substr(p);
                    append_inline(" " + heading_text + " ",
                        markdown_style.heading[level - 1]);
                    if (markdown_style.heading_rule_enabled[level - 1]) {
                        edit->appendText("\n", markdown_style.text);
                        int rule_width = utf8_width(" " + heading_text + " ");
                        std::string rule;
                        const std::string& rule_char =
                            markdown_style.heading_rule_char[level - 1];
                        for (int i = 0; i < rule_width; ++i)
                            rule += rule_char;
                        append_inline(rule,
                            markdown_style.heading_rule[level - 1]);
                    }
                }
                else {
                    append_inline(line, markdown_style.text);
                }
            }
            else if (!line.empty()) {
                append_inline(line, markdown_style.text);
            }
        }
        }
        }

        if (line_index + 1 < lines.size())
            edit->appendText("\n", markdown_style.text);
    }
    Point old_scroll = edit->scroll_value;
    if (edit->is_scroll_x)
        edit->scroll_value.x = std::clamp(edit->scroll_value.x,
            0, edit->scroll_max.x);
    if (edit->is_scroll_y)
        edit->scroll_value.y = std::clamp(edit->scroll_value.y,
            0, edit->scroll_max.y);

    edit->mgr->is_dirty = true;
}

/// <summary>
/// Syntax
/// </summary>

namespace {

static Syntax MakeSyntaxBase()
{
    Syntax result;
    result.normal = RichText::RichTextStyle(Color(210, 210, 210));
    result.comment = RichText::RichTextStyle(Color(110, 130, 110),
        AnsiColor_Unused, false, true);
    result.string_literal = RichText::RichTextStyle(Color(220, 170, 100));
    result.number = RichText::RichTextStyle(Color(190, 150, 230));
    result.keyword = RichText::RichTextStyle(Color(130, 180, 255),
        AnsiColor_Unused, true);
    result.section = RichText::RichTextStyle(Color(120, 190, 255),
        AnsiColor_Unused, true);
    result.key = RichText::RichTextStyle(Color(100, 210, 190));
    result.value = RichText::RichTextStyle(Color(220, 190, 120));
    result.constant = RichText::RichTextStyle(Color(190, 150, 230),
        AnsiColor_Unused, true);
    result.preprocessor = RichText::RichTextStyle(Color(220, 130, 150),
        AnsiColor_Unused, true);
    return result;
}

static void AddKeywords(Syntax& syntax,
                        std::initializer_list<const char*> keywords)
{
    for (const char* keyword : keywords)
        syntax.keywords.push_back({ keyword, syntax.keyword });
}

static void SortKeywords(Syntax& syntax)
{
    std::sort(syntax.keywords.begin(), syntax.keywords.end(),
        [](const Syntax::Rule& lhs, const Syntax::Rule& rhs) {
            return lhs.text < rhs.text;
        });
}

static void AddLineComment(Syntax& syntax, const char* start)
{
    syntax.line_comment_starts.push_back(start);
    if (syntax.line_comment_start.empty())
        syntax.line_comment_start = start;
}

static void AddSlashComments(Syntax& syntax)
{
    syntax.line_comment_start = "//";
    AddLineComment(syntax, "//");
    syntax.block_comment_start = "/*";
    syntax.block_comment_end = "*/";
}

static void AddQuotedLiterals(Syntax& syntax, bool backtick = false,
                              bool backtick_escape = true)
{
    syntax.literals.push_back({ "\"", "\"", syntax.string_literal });
    syntax.literals.push_back({ "'", "'", syntax.string_literal });
    if (backtick)
        syntax.literals.push_back({ "`", "`", syntax.string_literal,
            backtick_escape });
}

static void AddDoubleQuotedLiteral(Syntax& syntax)
{
    syntax.literals.push_back({ "\"", "\"", syntax.string_literal });
}

static void AddTripleQuotedLiterals(Syntax& syntax)
{
    syntax.literals.push_back({ "\"\"\"", "\"\"\"",
        syntax.string_literal });
    syntax.literals.push_back({ "'''", "'''", syntax.string_literal });
}

}

Syntax Syntax::CPP = [] {
    Syntax result = MakeSyntaxBase();
    AddSlashComments(result);
    AddQuotedLiterals(result);
    result.supports_preprocessor = true;

    const char* keyword_list[] = {
        "alignas", "alignof", "and", "asm", "auto",
        "bool", "break",
        "case", "catch", "char", "class", "const",
        "constexpr", "consteval", "constinit", "const_cast",
        "continue",
        "decltype", "default", "delete", "do", "double",
        "dynamic_cast",
        "else", "enum", "explicit", "export", "extern",
        "false", "float", "for", "friend",
        "if", "inline", "int",
        "long",
        "mutable",
        "namespace", "new", "noexcept", "not", "nullptr",
        "operator", "or", "override",
        "private", "protected", "public",
        "register", "reinterpret_cast", "requires", "return",
        "short", "signed", "sizeof", "static", "static_assert",
        "static_cast", "struct", "switch",
        "template", "this", "thread_local", "throw", "true",
        "try", "typedef", "typeid", "typename",
        "union", "unsigned", "using",
        "virtual", "void", "volatile",
        "wchar_t", "while",
        "xor", "co_await", "co_return", "co_yield", "concept"
    };

    for (const char* keyword : keyword_list)
        result.keywords.push_back({ keyword, result.keyword });

    SortKeywords(result);
    return result;
}();

Syntax Syntax::JS = [] {
    Syntax result = MakeSyntaxBase();
    AddSlashComments(result);
    AddQuotedLiterals(result, true, true);
    AddKeywords(result, {
        "as", "async", "await", "break", "case", "catch", "class",
        "const", "continue", "debugger", "default", "delete", "do",
        "else", "export", "extends", "false", "finally", "for",
        "from", "function", "if", "import", "in", "instanceof",
        "let", "new", "null", "of", "return", "static", "super",
        "switch", "this", "throw", "true", "try", "typeof", "var",
        "void", "while", "with", "yield"
    });
    SortKeywords(result);
    return result;
}();

Syntax Syntax::TS = [] {
    Syntax result = Syntax::JS;
    AddKeywords(result, {
        "any", "boolean", "declare", "enum", "implements", "interface",
        "keyof", "module", "namespace", "never", "number", "object",
        "private", "protected", "public", "readonly", "require",
        "string", "symbol", "type", "undefined", "unknown", "satisfies"
    });
    SortKeywords(result);
    return result;
}();

Syntax Syntax::Go = [] {
    Syntax result = MakeSyntaxBase();
    AddSlashComments(result);
    AddQuotedLiterals(result, true, false);
    AddKeywords(result, {
        "break", "case", "chan", "const", "continue", "default",
        "defer", "else", "fallthrough", "for", "func", "go", "goto",
        "if", "import", "interface", "map", "package", "range",
        "return", "select", "struct", "switch", "type", "var"
    });
    SortKeywords(result);
    return result;
}();

Syntax Syntax::Rust = [] {
    Syntax result = MakeSyntaxBase();
    AddSlashComments(result);
    AddQuotedLiterals(result);
    AddKeywords(result, {
        "as", "async", "await", "break", "const", "continue", "crate",
        "dyn", "else", "enum", "extern", "false", "fn", "for", "if",
        "impl", "in", "let", "loop", "match", "mod", "move", "mut",
        "pub", "ref", "return", "self", "Self", "static", "struct",
        "super", "trait", "true", "type", "unsafe", "use", "where",
        "while", "abstract", "become", "box", "do", "final", "macro",
        "override", "priv", "typeof", "unsized", "virtual", "yield"
    });
    SortKeywords(result);
    return result;
}();

Syntax Syntax::Python = [] {
    Syntax result = MakeSyntaxBase();
    result.line_comment_start = "#";
    AddLineComment(result, "#");
    AddQuotedLiterals(result);
    AddTripleQuotedLiterals(result);
    AddKeywords(result, {
        "and", "as", "assert", "async", "await", "break", "case",
        "class", "continue", "def", "del", "elif", "else", "except",
        "False", "finally", "for", "from", "global", "if", "import",
        "in", "is", "lambda", "match", "None", "nonlocal", "not",
        "or", "pass", "raise", "return", "True", "try", "while",
        "with", "yield"
    });
    SortKeywords(result);
    return result;
}();

Syntax Syntax::JSON = [] {
    Syntax result = MakeSyntaxBase();
    AddDoubleQuotedLiteral(result);
    AddKeywords(result, { "false", "null", "true" });
    SortKeywords(result);
    return result;
}();

Syntax Syntax::INI = [] {
    Syntax result = MakeSyntaxBase();
    result.ini_mode = true;
    result.line_comment_start = ";";
    AddLineComment(result, ";");
    AddLineComment(result, "#");
    AddQuotedLiterals(result);
    for (const char* constant : { "true", "false", "yes", "no", "on", "off" })
        result.keywords.push_back({ constant, result.constant });
    SortKeywords(result);
    return result;
}();


void SyntaxText(RichEdit* edit, const std::string& text, Syntax syntax)
{
    if (!edit || text.empty())
        return;

    std::sort(syntax.keywords.begin(), syntax.keywords.end(),
        [](const Syntax::Rule& lhs, const Syntax::Rule& rhs) {
            return lhs.text < rhs.text;
        });

    const auto is_identifier_char = [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    };

    const auto starts_with = [&text](size_t pos, const std::string& value) {
        return !value.empty() && value.size() <= text.size() &&
            pos <= text.size() - value.size() &&
            text.compare(pos, value.size(), value) == 0;
    };

    struct Run {
        std::string text;
        RichText::Style style;
    };

    std::vector<Run> runs;
    runs.reserve(text.size() / 8 + 1);

    const auto append = [&runs](const std::string& value,
                                const RichText::Style& style) {
        if (!value.empty())
            runs.push_back({ value, style });
    };

    const auto find_keyword = [&syntax](const std::string& word)
        -> const Syntax::Rule* {
        const auto it = std::lower_bound(syntax.keywords.begin(),
            syntax.keywords.end(), word,
            [](const Syntax::Rule& rule, const std::string& value) {
                return rule.text < value;
            });
        if (it != syntax.keywords.end() && it->text == word)
            return &*it;
        return nullptr;
    };

    size_t pos = 0;
    size_t plain_start = 0;
    bool line_start = true;
    bool ini_value = false;
    const auto plain_style = [&]() -> const RichText::Style& {
        return ini_value ? syntax.value : syntax.normal;
    };

    while (pos < text.size()) {
        if (text[pos] == '\n') {
            if (ini_value) {
                append(text.substr(plain_start, pos - plain_start),
                       syntax.value);
                plain_start = pos;
                ini_value = false;
            }
            ++pos;
            line_start = true;
            continue;
        }

        if (line_start && (text[pos] == ' ' || text[pos] == '\t' ||
                           text[pos] == '\r')) {
            ++pos;
            continue;
        }

        if (syntax.ini_mode && line_start) {
            const size_t line_end = text.find('\n', pos) == std::string::npos
                ? text.size() : text.find('\n', pos);
            if (text[pos] == '[') {
                const size_t section_end = text.find(']', pos + 1);
                if (section_end != std::string::npos &&
                    section_end < line_end) {
                    append(text.substr(plain_start, line_end - plain_start),
                           syntax.section);
                    pos = line_end;
                    plain_start = pos;
                    continue;
                }
            }

            size_t separator = pos;
            while (separator < line_end && text[separator] != '=' &&
                   text[separator] != ':')
                ++separator;
            if (separator < line_end && separator > pos) {
                size_t key_end = separator;
                while (key_end > pos &&
                       (text[key_end - 1] == ' ' || text[key_end - 1] == '\t'))
                    --key_end;
                append(text.substr(plain_start, pos - plain_start),
                       syntax.normal);
                append(text.substr(pos, key_end - pos), syntax.key);
                append(text.substr(key_end, separator - key_end + 1),
                       syntax.normal);
                pos = separator + 1;
                plain_start = pos;
                line_start = false;
                ini_value = true;
                continue;
            }
        }

        if (syntax.supports_preprocessor && line_start && text[pos] == '#') {
            append(text.substr(plain_start, pos - plain_start), plain_style());
            size_t end = pos + 1;
            while (end < text.size() && (text[end] == ' ' || text[end] == '\t'))
                ++end;
            while (end < text.size() && is_identifier_char(static_cast<unsigned char>(text[end])))
                ++end;
            append(text.substr(pos, end - pos), syntax.preprocessor);
            pos = end;
            plain_start = pos;
            line_start = false;
            continue;
        }

        const std::string* line_comment = nullptr;
        if (starts_with(pos, syntax.line_comment_start))
            line_comment = &syntax.line_comment_start;
        for (const std::string& candidate : syntax.line_comment_starts) {
            if (starts_with(pos, candidate) &&
                (!line_comment || candidate.size() > line_comment->size()))
                line_comment = &candidate;
        }

        if (line_comment) {
            append(text.substr(plain_start, pos - plain_start), plain_style());
            size_t end = text.find('\n', pos);
            if (end == std::string::npos)
                end = text.size();
            append(text.substr(pos, end - pos), syntax.comment);
            pos = end;
            plain_start = pos;
            line_start = true;
            continue;
        }

        if (starts_with(pos, syntax.block_comment_start)) {
            append(text.substr(plain_start, pos - plain_start), plain_style());
            size_t end = text.find(syntax.block_comment_end,
                                   pos + syntax.block_comment_start.size());
            if (end == std::string::npos)
                end = text.size();
            else
                end += syntax.block_comment_end.size();
            append(text.substr(pos, end - pos), syntax.comment);
            line_start = text.find_last_of('\n', end - 1) == end - 1;
            pos = end;
            plain_start = pos;
            continue;
        }

        const Syntax::Delimiter* literal = nullptr;
        for (const Syntax::Delimiter& candidate : syntax.literals) {
            if (starts_with(pos, candidate.start) &&
                (!literal || candidate.start.size() > literal->start.size()))
                literal = &candidate;
        }

        if (literal) {
            append(text.substr(plain_start, pos - plain_start), plain_style());
            const size_t literal_start = pos;
            pos += literal->start.size();
            while (pos < text.size()) {
                if (literal->escape && text[pos] == '\\') {
                    pos += std::min<size_t>(2, text.size() - pos);
                }
                else if (starts_with(pos, literal->end)) {
                    pos += literal->end.size();
                    break;
                }
                else {
                    ++pos;
                }
            }
            append(text.substr(literal_start, pos - literal_start), literal->style);
            plain_start = pos;
            line_start = false;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(text[pos]))) {
            append(text.substr(plain_start, pos - plain_start), plain_style());
            size_t end = pos + 1;
            while (end < text.size() && (std::isalnum(
                       static_cast<unsigned char>(text[end])) ||
                   text[end] == '.' || text[end] == '_'))
                ++end;
            append(text.substr(pos, end - pos), syntax.number);
            pos = end;
            plain_start = pos;
            line_start = false;
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(text[pos])) ||
            text[pos] == '_') {
            size_t end = pos + 1;
            while (end < text.size() && is_identifier_char(static_cast<unsigned char>(text[end])))
                ++end;
            const Syntax::Rule* keyword = find_keyword(
                text.substr(pos, end - pos));
            if (keyword) {
                append(text.substr(plain_start, pos - plain_start), plain_style());
                append(text.substr(pos, end - pos), keyword->style);
                plain_start = end;
            }
            pos = end;
            line_start = false;
            continue;
        }

        line_start = false;
        ++pos;
    }

    if (ini_value)
        append(text.substr(plain_start), syntax.value);
    else
        append(text.substr(plain_start), syntax.normal);

    // Append the complete text once, then apply token styles in one pass.
    std::string rendered;
    rendered.reserve(text.size());
    for (const Run& run : runs)
        rendered += run.text;

    const size_t first_char = edit->chars.size();
    edit->appendText(rendered, syntax.normal);

    size_t char_index = first_char;
    for (const Run& run : runs) {
        const size_t start = char_index;
        size_t byte_count = 0;
        while (char_index < edit->chars.size() &&
               byte_count < run.text.size()) {
            byte_count += static_cast<size_t>(edit->chars[char_index].size);
            ++char_index;
        }
        edit->setStyle(static_cast<int>(start),
            static_cast<int>(char_index), run.style);
    }
}

void SyntaxText(RichEdit* edit, const std::string& text, const std::string& filename)
{
    std::string name = filename;
    const size_t separator = name.find_last_of("/\\");
    if (separator != std::string::npos)
        name.erase(0, separator + 1);

    const size_t dot = name.find_last_of('.');
    std::string extension;
    if (dot != std::string::npos && dot + 1 < name.size())
        extension = name.substr(dot + 1);

    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    const Syntax* syntax = &Syntax::CPP;
    if (extension == "js" || extension == "mjs" || extension == "cjs")
        syntax = &Syntax::JS;
    else if (extension == "ts" || extension == "tsx" ||
             extension == "mts" || extension == "cts")
        syntax = &Syntax::TS;
    else if (extension == "go")
        syntax = &Syntax::Go;
    else if (extension == "rs")
        syntax = &Syntax::Rust;
    else if (extension == "py" || extension == "pyw")
        syntax = &Syntax::Python;
    else if (extension == "json" || extension == "jsonc")
        syntax = &Syntax::JSON;
    else if (extension == "ini" || extension == "cfg" ||
             extension == "conf")
        syntax = &Syntax::INI;

    SyntaxText(edit, text, *syntax);
}

/// <summary>
/// Mgr
/// </summary>

Mgr::Mgr() :Win(this)
{
    paint_list.reserve(256);
    draw_border = false;
}

WinPtr Mgr::CreateByID(std::string csid, Mgr* mgr)
{
    Win* ob = nullptr;
    if (eqi(csid, "Win")) {
        ob = new Win(mgr);
    }
    else if (eqi(csid, "Label")) {
        ob = new Label(mgr);
    }
    else if (eqi(csid, "Button")) {
        ob = new Button(mgr);
    }
    else if (eqi(csid, "Check")) {
        ob = new Check(mgr);
    }
    else if (eqi(csid, "Combo")) {
        ob = new Combo(mgr);
    }
    else if (eqi(csid, "Slider")) {
        ob = new Slider(mgr);
    }
    else if (eqi(csid, "Edit")) {
        ob = new Edit(mgr);
    }
    else if (eqi(csid, "LabelEdit")) {
        ob = new LabelEdit(mgr);
    }
    else if (eqi(csid, "RichEdit")) {
        ob = new RichEdit(mgr);
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
    if (local.x2 != size.x - 1 || local.y2 != size.y - 1) {
        local.x2 = size.x - 1;
        local.y2 = size.y - 1;
        screen = local;
        clip = local;
        terminal.Resize();
        is_dirty = true;
    }

    std::vector<TUI::Event> events;
    Terminal::GetEvent(events);

    for (auto& ev : events) {
        if (ev.type == EventType_Key || ev.type == EventType_Paste) {
            if (on_key && on_key(ev)) {
                continue;
            }
            if (notify_ && notify_->Event(ev)) {
                continue;
            }
            Navigator(ev);
            is_dirty = true;
            continue;
        }
        if (ev.type == EventType_Mouse) {
            Point pt = { ev.x, ev.y };
            bool any_down = ev.any_button_down();
            bool first_down = any_down && ev.any_first_down();
            auto notify = GetNotify(pt);
            if (notify) {
                if (notify != notify_) {
                    if (first_down) {
                        notify_ = notify;

                        if (popup_) { ClosePopup(); }
                    }
                    is_dirty = true;
                }
                else if (first_down) {
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
            hover_slider_ = WinPtr(GetSlider(pt));
            if (hover_slider_ && hover_slider_ != notify_) {
                hover_slider_->Event(ev);
            }
        }
    }
    std::cout << CursorMove(cursor.x, cursor.y);
    return is_dirty;
}
void Mgr::Paint(DrawBuffer& drawbuf)
{
    is_dirty = false;
    paint_list.resize(0);
    Win::Paint(drawbuf);
}

void Mgr::Popup(WinPtr ob)
{
    ClosePopup();
    ob->is_visible = true;
    AddChild(ob);
    popup_ = ob;
    is_dirty = true;
}
void Mgr::ClosePopup()
{
    if (popup_) {
        RemoveChild(popup_);
        popup_ = nullptr;
        is_dirty = true;
    }
}

WinPtr Mgr::NextNotify()
{
    if (paint_list.empty())
        return nullptr;

    // Paint order is also the order used for keyboard focus navigation.  A
    // widget may disappear between frames, so start from the beginning when
    // the current focus is not in the latest paint list.
    size_t start = 0;
    if (notify_) {
        auto current = std::find(paint_list.begin(), paint_list.end(), notify_);
        if (current != paint_list.end()) {
            start = static_cast<size_t>(std::distance(paint_list.begin(), current) + 1) % paint_list.size();
        }
    }

    // Wrap around so Tab navigation never gets stuck at the end of the list.
    for (size_t offset = 0; offset < paint_list.size(); ++offset) {
        const auto& candidate = paint_list[(start + offset) % paint_list.size()];
        if (candidate && candidate->is_visible && candidate->is_notifiable)
            return candidate;
    }
    return nullptr;
}

void Mgr::SetNotify(WinPtr ob)
{
    if (!ob || ob == notify_)
        return;
    notify_ = ob;
    hover_slider_ = GetSlider({ ob->clip.x, ob->clip.y });
    is_dirty = true;
}

void Mgr::Navigator(const TUI::Event& ev)
{
    if (ev.ctrl || ev.shift)
        return;

    if (ev.key == VK_TAB) {
        SetNotify(NextNotify());
    }
    else if (ev.key == VK_RETURN) {
        if (notify_)
            notify_->Click();
    }
    else if ((ev.vkey == VK_UP || ev.vkey == VK_DOWN) && hover_slider_ && notify_) {
        // Arrow navigation is local to the slider under the mouse.  This keeps
        // a form or list usable without moving focus to another panel.
        auto& children = hover_slider_->child;
        auto current = std::find(children.begin(), children.end(), notify_);
        if (current == children.end() || children.empty())
            return;

        const bool down = ev.vkey == VK_DOWN;
        const size_t count = children.size();
        const size_t current_index = static_cast<size_t>(std::distance(children.begin(), current));
        for (size_t offset = 1; offset <= count; ++offset) {
            const size_t index = down
                ? (current_index + offset) % count
                : (current_index + count - (offset % count)) % count;
            const auto& candidate = children[index];
            if (candidate && candidate->is_visible && candidate->is_notifiable) {
                SetNotify(candidate);
                return;
            }
        }
    }
}

/// <summary>
/// 輔助
/// </summary>

ButtonPtr GetButton(WinPtr ob, std::function<void()> click, const char* find)
{
    ButtonPtr btn = std::dynamic_pointer_cast<Button>(ob);
    if (find) {
        btn = ob->GetUI<Button>(find);
    }
    if (!btn)
        return btn;
    btn->on_click = click;
    return btn;
}

CheckPtr GetCheck(WinPtr ob, bool value, Check::fn_check check, const char* find)
{
    CheckPtr chk = std::dynamic_pointer_cast<Check>(ob);
    if (find) {
        chk = ob->GetUI<Check>(find);
    }
    if (!chk) return chk;
    chk->checked = value;
    chk->on_check = check;
    return chk;
}

EditPtr GetEdit(WinPtr ob, const std::string& text, Edit::fn_edit func, const char* find)
{
    EditPtr ed = std::dynamic_pointer_cast<Edit>(ob);
    if (find) {
        ed = ob->GetUI<Edit>(find);
    }
    if (!ed)
        return ed;
    ed->setText(text);
    ed->on_edit = func;
    return ed;
}

LabelEditPtr GetLabelEdit(WinPtr ob, const std::string& text, Edit::fn_edit func, const char* find)
{
    LabelEditPtr ed = std::dynamic_pointer_cast<LabelEdit>(ob);
    if (find) {
        ed = ob->GetUI<LabelEdit>(find);
    }
    if (!ed)
        return ed;
    ed->setText(text);
    ed->on_edit = func;
    return ed;
}

NAMESPACE_END
