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
        auto ch = Char::from_code(cp);
        text_width += ch.char_width;
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

Char Char::from_code(uint32_t cp)
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
        if (cp == '\n') {
            position.push_back({ cx, cy });
            chars.push_back(Char::from_code(cp));
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
    if (idx >= 0 && idx < static_cast<int>(chars.size()))
        return position[idx];
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
        auto& pos = position[i];
        if (pos.y == cursor.y)
            idx = i;
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
    Text::setText(text, wrap_width);
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
int Text::insert(int idx, const std::string text)
{
    return idx;
}
Point Text::left(int idx)
{
    if (idx > 0) {
        idx--;
    }
    return pos_of(idx);
}
Point Text::right(int idx)
{
    if (idx < position.size()) {
        idx++;
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
        if (clip.inside(Point{ cur_x, cur_y }) && cur_x + char_width <= clip.x2) {
            auto& cell = cells_[px + cur_x];
            cell.fg_color = color;
            cell.size = char_width;
            cell.bold = bold;
            cell.italic = italic;
            cell.underline = underline;
            cell.content = std::string((const char*)p, (size_t)n);
            if (cell.size > 1) {
                cells_[px + cur_x + 1].content = "";
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

        if (clip.inside(Point{ cur_x, cur_y }) && cur_x + ch.char_width <= clip.x2) {
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
                if (is_sel) {
                    next_cell.bg_color = text.color_selected;
                }
            }
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
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw top-right corner
        if (clip.inside(Point{ r.x2, r.y })) {
            auto& cell = cells_[r.y * width_ + r.x2];
            cell.content = tr;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw bottom-left corner
        if (clip.inside(Point{ r.x, r.y2 })) {
            auto& cell = cells_[r.y2 * width_ + r.x];
            cell.content = bl;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }
        // Draw bottom-right corner
        if (clip.inside(Point{ r.x2, r.y2 })) {
            auto& cell = cells_[r.y2 * width_ + r.x2];
            cell.content = br;
            cell.fg_color = color;
            cell.bg_color = bgcolor;
        }

        // Draw top and bottom horizontal lines (excluding corners)
        for (int x = std::max(r.x + 1, clip.x); x < r.x2 && x <= clip.x2; x++) {
            if (r.y >= clip.y && r.y <= clip.y2) {
                auto& cell = cells_[r.y * width_ + x];
                cell.content = h_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
            if (r.y2 >= clip.y && r.y2 <= clip.y2) {
                auto& cell = cells_[r.y2 * width_ + x];
                cell.content = h_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
        }

        // Draw left and right vertical lines (excluding corners)
        for (int y = std::max(r.y + 1, clip.y); y < r.y2 && y <= clip.y2; y++) {
            if (r.x >= clip.x && r.x <= clip.x2) {
                auto& cell = cells_[y * width_ + r.x];
                cell.content = v_line;
                cell.fg_color = color;
                cell.bg_color = bgcolor;
            }
            if (r.x2 >= clip.x && r.x2 <= clip.x2) {
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
    for (int y = std::max(fill_y_start, clip.y); y <= fill_y_end && y <= clip.y2; y++) {
        for (int x = std::max(fill_x_start, clip.x); x <= fill_x_end && x <= clip.x2; x++) {
            auto& cell = cells_[y * width_ + x];
            cell.content = " ";
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
            if (is_thumb) {
                cell.content = u8"\u2588";  // █ Full Block
                cell.fg_color = thumb_color;
            } else {
                cell.content = u8"\u2588";
                cell.fg_color = track_color;
            }
        }
        else {
            int cx = pos.x + i;
            if (!clip.inside(Point{cx, pos.y}))
                continue;
            auto& cell = cells_[pos.y * width_ + cx];
            cell.size = 1;
            if (is_thumb) {
                cell.content = u8"\u2584";  // half Block
                cell.fg_color = thumb_color;
            } else {
                cell.content = u8"\u2584";
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
/// Terminal
/// </summary>

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

            //if (cur_cell == pre_cell)
            //    continue;
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
            if (cur_cell.size == 1 && cur_cell.content[0] == '\n') {
                out += " ";
            }
            else {
                out += cur_cell.content;
            }
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

void CopyClipboard(const std::string& text)
{
    HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hglb) {
        char* p = static_cast<char*>(GlobalLock(hglb));
        memcpy(p, text.data(), text.size());
        p[text.size()] = '\0';
        GlobalUnlock(hglb);
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            SetClipboardData(CF_TEXT, hglb);
            CloseClipboard();
        }
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

static uint32_t map_key_event(const KEY_EVENT_RECORD& key)
{
    if (key.uChar.UnicodeChar != 0) {
        uint16_t ch = key.uChar.UnicodeChar;

        // High surrogate — buffer it for the next event
        if (ch >= 0xD800 && ch <= 0xDBFF) {
            s_pending_hi = ch;
            return 0; // don't emit yet
        }

        // Low surrogate — combine with pending high surrogate
        if (ch >= 0xDC00 && ch <= 0xDFFF && s_pending_hi != 0) {
            uint32_t cp = combine_surrogate(s_pending_hi, ch);
            s_pending_hi = 0;
            return cp;
        }

        // Lone low surrogate — discard stale high surrogate
        if (ch >= 0xDC00 && ch <= 0xDFFF) {
            s_pending_hi = 0;
            return 0;
        }

        // Normal BMP character — flush any pending surrogate
        s_pending_hi = 0;
        return static_cast<uint32_t>(ch);
    }

    switch (key.wVirtualKeyCode) {
    //case VK_ESCAPE:  return 0x1B;
    //case VK_RETURN:  return '\n';
    //case VK_BACK:    return '\b';
    case VK_TAB:     return '\t';
    case VK_SPACE:   return ' ';
    default:         return 0; // navigation/function keys — use vkey instead
    }
}

// Per-thread state for VT sequence parsing.
static thread_local bool        s_in_bracketed_paste = false;
static thread_local std::string s_paste_buffer;
// Small buffer used to detect the end-of-paste escape sequence character-by-character.
static thread_local std::string s_escape_detect;  // max length ~6

// Parse a complete CSI mouse event: ESC [ < Cb ; Cx ; Cy M/m
// Returns true if the sequence was consumed and ev is populated.
static bool parse_sgr_mouse(Event &ev)
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

    // Button code: 0=left, 1=middle, 2=right, 3=move,
    //              4=scroll up, 5=scroll down, 6=h-scroll left, 7=h-scroll right
    switch (cb & 3) {
    case 0: ev.button = is_release ? 0 : 1; break;
    case 1: ev.button = is_release ? 0 : 2; break;
    case 2: ev.button = is_release ? 0 : 3; break;
    case 3: ev.button = 0;            break; // motion
    }

    // Scroll buttons (bit 2 set)
    if ((cb & 4) != 0) {
        switch (cb & 7) {
        case 4: ev.button = 4; break; // scroll up
        case 5: ev.button = 5; break; // scroll down
        case 6: ev.button = 6; break; // h-scroll left
        case 7: ev.button = 7; break; // h-scroll right
        }
    }

    return true;
}

// Check whether the escape_detect buffer matches a prefix of "\x1b[201~".
// Returns true if it's still a possible match, false if it has diverged.
static bool is_paste_end_prefix(const std::string& buf)
{
    static const char end_marker[] = "\x1b[201~";
    for (size_t i = 0; i < buf.size(); ++i) {
        if (buf[i] != end_marker[i])
            return false;
    }
    return true;
}

// Emit a VT-parsed event: mark is_vt and store the raw sequence.
static void emit_vt_event(Event &ev, EventType_ type, const std::string& seq)
{
    ev.type = type;
    ev.is_vt = true;
    ev.seq = seq;
    ev.key = 0;
}

// Emit a VT-parsed key event: set vkey and clear the sequence buffer.
static bool emit_vt_key(Event &ev)
{
    ev.type = EventType_Key;
    ev.is_vt = false;  // not needed — caller will treat as normal key
    ev.seq.clear();
    ev.key = 0;
    return true;  // fully parsed, ready to push
}

bool parse_vt_sequence(INPUT_RECORD &rec, Event &ev){
    auto& key = rec.Event.KeyEvent;
    uint16_t ch = key.uChar.UnicodeChar;

    // Only process printable characters and ESC from VT input.
    // Control keys (arrows, function keys) are handled by the normal path.
    if (ch == 0 && key.wVirtualKeyCode != VK_ESCAPE)
        return false;

    char byte = static_cast<char>(ch);

    // Inside bracketed paste — accumulate raw characters until end marker.
    if (s_in_bracketed_paste) {
        s_escape_detect += byte;

        // If we see ESC, start watching for the end marker "\x1b[201~".
        // Otherwise, just append to paste buffer and reset escape detection.
        if (byte == '\x1b') {
            // Start of potential end marker — don't add to paste yet.
        } else if (!s_escape_detect.empty() && s_escape_detect[0] == '\x1b') {
            // We're in the middle of a potential escape sequence.
            if (is_paste_end_prefix(s_escape_detect)) {
                // Still matching — keep watching. When complete, emit paste event.
                if (s_escape_detect.size() == 6) {
                    ev.paste_text = s_paste_buffer;
                    emit_vt_event(ev, EventType_Paste, "\x1b[201~");
                    s_in_bracketed_paste = false;
                    s_paste_buffer.clear();
                    s_escape_detect.clear();
                    return true;
                }
            } else {
                // Not the end marker — flush escape_detect into paste buffer.
                s_paste_buffer += s_escape_detect;
                s_escape_detect.clear();
            }
        } else {
            // Normal character during paste.
            s_paste_buffer += byte;
        }

        return true;  // consumed, still buffering
    }

    // Normal VT sequence buffering.
    ev.key = ch;
    ev.seq += byte;

    // If we just received ESC, wait for more characters — don't parse yet.
    if (byte == '\x1b') {
        ev.is_vt = true;
        return true;  // consumed, but incomplete
    }
    if (ev.is_vt && ev.seq.length() < 3)
        return true;
    // Try to parse the accumulated buffer as a complete sequence.
    size_t len = ev.seq.length();
    if (len >= 6 && ev.seq.substr(0,6) == "\x1b[200~") {
        s_in_bracketed_paste = true;
    }
    else if (len >= 6 && ev.seq.substr(0, 6) == "\x1b[201~") {
        // End-of-paste received outside paste mode — consume silently.
        s_in_bracketed_paste = false;
        return true;  // consumed
    }
    else if (len >3 && ev.seq[0]=='\x1b' && ev.seq[1] == '[' && ev.seq[2] == '<') {
        if (ch == 'M' || ch == 'm') {
            if (parse_sgr_mouse(ev)) {
                ev.type = EventType_Mouse;
            }
        }
    }
/*
 * CSI Key sequences:
 *
 *   Arrow keys:       \x1b[A (Up)  \x1b[B (Down)  \x1b[C (Right)  \x1b[D (Left)
 *   Home / End:       \x1b[H / \x1b[F    or    \x1b[1~ / \x1b[4~
 *   Page Up/Down:     \x1b[5~ / \x1b[6~
 *   Delete / Insert:  \x1b[3~ / \x1b[2~
 *
 *   F1–F4 (SS3):      \x1bOP / \x1bOQ / \x1bOR / \x1bOS
 *   F5–F12:           \x1b[15~ .. \x1b[24~
 *
 *   Windows VT Input modifier format:
 *     \x1b[<Cb;Modifier;Key M/m
 *     Modifier bits: 1=Shift, 2=Ctrl, 4=Alt (combined)
 */
    else if (len == 3 && ev.seq[0] == '\x1b' && ev.seq[1] == '[') {
        // CSI + single letter — arrow keys, Home/End
        char term = ev.seq[2];
        switch (term) {
            case 'A': ev.vkey = VK_UP;    break;
            case 'B': ev.vkey = VK_DOWN;  break;
            case 'C': ev.vkey = VK_RIGHT; break;
            case 'D': ev.vkey = VK_LEFT;  break;
            case 'H': ev.vkey = VK_HOME;  break;
            case 'F': ev.vkey = VK_END;   break;
            default:  goto not_complete;  // unknown CSI letter — keep buffering
        }
        return emit_vt_key(ev);
    }
    else if (len == 3 && ev.seq[0] == '\x1b' && ev.seq[1] == 'O') {
        // SS3 — F1–F4
        char term = ev.seq[2];
        switch (term) {
            case 'P': ev.vkey = VK_F1;  break;
            case 'Q': ev.vkey = VK_F2;  break;
            case 'R': ev.vkey = VK_F3;  break;
            case 'S': ev.vkey = VK_F4;  break;
            default:  goto not_complete;
        }
        return emit_vt_key(ev);
    }
    else if (len >= 5 && ev.seq[0] == '\x1b' && ev.seq[1] == '[' && ev.seq.back() == '~') {
        // CSI n~ — Home/End/PgUp/PgDown/Del/Ins/F5-F12
        std::string num = ev.seq.substr(2, len - 4);
        int n = 0;
        for (char c : num) { if (c >= '0' && c <= '9') n = n * 10 + (c - '0'); }
        switch (n) {
            case 1: ev.vkey = VK_HOME;     break;
            case 2: ev.vkey = VK_INSERT;   break;
            case 3: ev.vkey = VK_DELETE;   break;
            case 4: ev.vkey = VK_END;      break;
            case 5: ev.vkey = VK_PRIOR;    break; // Page Up
            case 6: ev.vkey = VK_NEXT;     break; // Page Down
            case 11: ev.vkey = VK_F1;      break;
            case 12: ev.vkey = VK_F2;      break;
            case 13: ev.vkey = VK_F3;      break;
            case 14: ev.vkey = VK_F4;      break;
            case 15: ev.vkey = VK_F5;      break;
            case 17: ev.vkey = VK_F6;      break;
            case 18: ev.vkey = VK_F7;      break;
            case 19: ev.vkey = VK_F8;      break;
            case 20: ev.vkey = VK_F9;      break;
            case 21: ev.vkey = VK_F10;     break;
            case 23: ev.vkey = VK_F11;     break;
            case 24: ev.vkey = VK_F12;     break;
            default: goto not_complete;
        }
        return emit_vt_key(ev);
    }

not_complete:
    // Buffer is growing but not yet a recognized sequence.
    // If it gets too long, discard and treat as normal input.
    if (ev.seq.size() > 16) {
        OutputDebugStringA(ev.seq.c_str());
        return false;
    }

    return ev.is_vt;  // consumed, waiting for more
}

void Terminal::event_thread()
{
    INPUT_RECORD rec;
    DWORD count;
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    DWORD prev_btn_state = 0;

    Event ev;
    while (s_running.load()) {
        ReadConsoleInputW(hIn, &rec, 1, &count);
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            // Try to parse VT/ANSI escape sequences first.
            bool vt_parsed = parse_vt_sequence(rec, ev);

            if (!vt_parsed) {
                ev.type = EventType_Key;
                ev.vkey = rec.Event.KeyEvent.wVirtualKeyCode;
                ev.key = map_key_event(rec.Event.KeyEvent);
                ev.shift = (rec.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) != 0;
                ev.ctrl = (rec.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
                ev.alt = (rec.Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
                if (ev.key == 13) {
                    ev.vkey = VK_RETURN;
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
    //TODO
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
        if (eqi(s, "Top_Pane"))   return Dock_Top_Pane;
        if (eqi(s, "Left_Pane"))  return Dock_Left_Pane;
        if (eqi(s, "Right_Pane")) return Dock_Right_Pane;
        if (eqi(s, "Down_Pane"))  return Dock_Down_Pane;
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
    else if (eqi(cmd, "Title")) {
        title = el.tok();
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
        local.y = local.y2 - lw;
    }
    if (dock_.mode & Dock_Down_Pane) {
        local.x = local.x2 - lh;
    }
    switch (autosize_) {
    case Autosize_None:
        break;
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
        ch->Paint(drawbuf);
    }
    drawbuf.PopClip();
}

void Win::PaintBorder(DrawBuffer& drawbuf)
{
    if (draw_border) {
        drawbuf.Border(screen, bg_color, border_style, fg_color);
        if (!title.empty()) {
            drawbuf.Text(title, { screen.x + 1, screen.y }, fg_color);
        }
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
    if (!screen.inside(pt))
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

void Label::setText(const std::string& _text)
{
    Text::setText(_text, local.width());
    mgr->is_dirty = true;
}

/// <summary>
/// Button
/// </summary>

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

bool Slider::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "ScrollX")) {
        is_scroll_x = el.tok_bool();
    }
    else if (eqi(cmd, "ScrollY")) {
        is_scroll_y = el.tok_bool();
    }
    else if (eqi(cmd, "TrackColor")) {
        color_track = Color::Parse(el.tok());
    }
    else if (eqi(cmd, "ThumbColor")) {
        color_thumb = Color::Parse(el.tok());
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
        for (auto ch : child) {
            content_length.y = std::max(content_length.y, ch->local.y2 + 1);
        }
        if (content_length.y < clip.height()) {
            content_length.y = clip.height();
        }
        scroll_max.y = content_length.y - clip.height();
    }
    if (is_scroll_x) {
        clip.y2--;
        for (auto ch : child) {
            content_length.x = std::max(content_length.x, ch->local.x2 + 1);
        }
        if (content_length.x < clip.width()) {
            content_length.x = clip.width();
        }
        scroll_max.x = content_length.x - clip.width();
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

void Slider::Event(const TUI::Event& ev)
{
    if (mgr->hover_slider_ != this)
        return;
    Point old_scroll_value = scroll_value;
    bool any_click = ev.any_button_down();

    if (any_click) {
        Point pt = { ev.x, ev.y };

        if (is_scroll_y) {
            Rect scrollbar = { clip.x2 + 1, clip.y, clip.x2 + 1, clip.y2 };
            // Vertical scrollbar at x=clip.x2+1, y=[clip.y .. clip.y+length-1]
            if (scrollbar.inside(pt)) {
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
            Rect scrollbar = { clip.x, clip.y2 + 1, clip.x2, clip.y2 + 1 };
            // Horizontal scrollbar at y=clip.y2+1, x=[clip.x .. clip.x+length-1]
            if (scrollbar.inside(pt)) {
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

    switch (ev.button) {
    case 4:
        if (is_scroll_y) {
            if (scroll_value.y > 0) {
                scroll_value.y--;
            }
        }
        else if (is_scroll_x) {
            if (scroll_value.x > 0) {
                scroll_value.x--;
            }
        }
        break;
    case 5:
        if (is_scroll_y) {
            if (scroll_value.y < scroll_max.y) {
                scroll_value.y++;
            }
        }
        else if(is_scroll_x) {
            if (scroll_value.x < scroll_max.x) {
                scroll_value.x++;
            }
        }
        break;
    default:
        break;
    }
    if (old_scroll_value != scroll_value) {
        mgr->is_dirty = true;
    }
}

Point Slider::GetClipPos() const
{
    return { clip.x - scroll_value.x, clip.y - scroll_value.y };
}

///
/// Edit
///

Edit::Edit(Mgr* mgr) :Slider(mgr) {
    color_selected = COLOR_SELECTED;
}

bool Edit::ParseCmd(const std::string& cmd, EditLine& el)
{
    bool ret = true;
    if (eqi(cmd, "Text")) {
        setText(ParseText(el.tok_line()));
    }
    else if (Win::ParseCmd(cmd, el)) {
    }
    else {
        ret = false;
    }
    return ret;
}

void Edit::Paint(DrawBuffer& drawbuf)
{
    PaintBorder(drawbuf);
    PaintText(drawbuf);
    PaintChild(drawbuf);
}

void Edit::PaintText(DrawBuffer& drawbuf)
{
    if (!text.empty()) {
        int tx = clip.x;
        int ty = clip.y;
        drawbuf.Text({ tx, ty }, *this, fg_color);
    }
    if (mgr->notify_ == this) {
        int cx = clip.x + cursor.x;
        int cy = clip.y + cursor.y;
        drawbuf.SetBgColor({ cx,cy }, Color(200, 200, 200));
    }
}

void Edit::setText(const std::string& _text)
{
    Text::setText(_text, local.width());
    selected.unselect();
    cursor = { 0,0 };
    mgr->is_dirty = true;
}

void Edit::Event(const TUI::Event& ev)
{
    if (mgr->notify_ != this)
        return;

    if (ev.type == EventType_Mouse) {
        Point pt = { ev.x, ev.y };
        is_down = ev.any_button_down();

        if (!clip.inside(pt)) {
            // If dragging outside clip, finalize drag
            if (drag_start >= 0 && !is_down) {
                drag_start = -1;
            }
            mgr->is_dirty = true;
            return;
        }

        int lx = ev.x - clip.x;
        int ly = ev.y - clip.y;
        int idx = char_at(lx, ly);

        if (idx < 0) {
            mgr->is_dirty = true;
            return;
        }

        // Detect first press of left button — start drag selection
        bool is_first_down = ev.any_first_down();

        // Dragging: extend selection from drag_start to current position
        if (drag_start >= 0 && is_down) {
            selected.start = std::min(drag_start, idx);
            selected.end   = std::max(drag_start, idx);
            Point p = pos_of(idx);
            cursor.set(p.x, p.y);
            mgr->is_dirty = true;
            return;
        }
        if (is_first_down) {
            drag_start = idx;
        }
        // Button released — finalize drag
        if (drag_start >= 0 && !is_down) {
            drag_start = -1;
        }

        if (ev.clicks == 2 && ev.button == 1) {
            // Double-click: select word at clicked position
            select_word(lx, ly);
        } else if(ev.any_button_down()) {
            // Single click: move cursor, clear selection
            selected.unselect();
            cursor = pos_of(idx);
        }
        mgr->cursor = { clip.x + cursor.x, clip.y + cursor.y };
        mgr->is_dirty = true;
    }
    else if (ev.type == EventType_Key) {
        int cur_idx = cur_idx_of(cursor);
        bool handled = true;
        if (on_key && on_key(ev)) {
            return;
        }
        // Printable character — insert at cursor (replace selection first if active)
        if (ev.key >= 32 && ev.key < 0x10FFFF && !readonly) {
            char utf8_buf[4];
            int n = utf8_wctomb(utf8_buf, ev.key);
            std::string ch_str(utf8_buf, n);

            cur_idx = delete_selected(cur_idx);
            size_t off = byte_offset_of(cur_idx);
            text.insert(off, ch_str);
            reparse();
            selected.unselect();

            // Move cursor after inserted character
            if (cur_idx < static_cast<int>(chars.size())) {
                Point p = position[cur_idx];
                cursor.set(p.x + chars[cur_idx].char_width, p.y);
            } else {
                auto ch = Char::from_code(ev.key);
                cursor.x += ch.char_width;
            }
        }
        // Special keys via ev.key
        else if (ev.vkey == VK_BACK) { // Backspace
            if (!readonly) {
                cur_idx = backspace(cur_idx);
                cursor = pos_of(cur_idx);
            }
        }
        else if (ev.vkey == VK_RETURN) { // Enter — insert newline
            if (!readonly) {
                enter(cur_idx);
                cursor.set(0, cursor.y + 1);
            }
        }
        else if (ev.vkey == VK_DELETE) { // Delete — remove char at cursor
            if (!readonly) {
                cur_idx = del(cur_idx);
                cursor = pos_of(cur_idx);
            }
        }
        else if (ev.vkey == VK_LEFT || ev.vkey == VK_RIGHT
            || ev.vkey == VK_UP || ev.vkey == VK_DOWN
            || ev.vkey == VK_HOME || ev.vkey == VK_END) {
            // Shift + arrow: extend selection instead of just moving cursor
            if (ev.shift) {
                Point new_cursor;
                if (ev.vkey == VK_LEFT)  new_cursor = left(cur_idx);
                else if (ev.vkey == VK_RIGHT) new_cursor = right(cur_idx);
                else if (ev.vkey == VK_UP)    new_cursor = up(cur_idx);
                else if (ev.vkey == VK_DOWN)  new_cursor = down(cur_idx);
                else if (ev.vkey == VK_HOME)  new_cursor = home(cur_idx);
                else                          new_cursor = end(cur_idx);

                cursor = new_cursor;
                int new_idx = cur_idx_of(cursor);
                // Initialize anchor on first Shift+arrow press
                if (!selected.has_selection()) {
                    selected.start = cur_idx;
                    selected.end = cur_idx;
                }
                // Extend selection — keep start <= end regardless of direction
                selected.start = std::min(selected.start, new_idx);
                selected.end = std::max(selected.end, new_idx);
            }
            else {
                // No Shift: clear selection and move normally
                selected.unselect();
                if (ev.vkey == VK_LEFT)  cursor = left(cur_idx);
                else if (ev.vkey == VK_RIGHT) cursor = right(cur_idx);
                else if (ev.vkey == VK_UP)    cursor = up(cur_idx);
                else if (ev.vkey == VK_DOWN)  cursor = down(cur_idx);
                else if (ev.vkey == VK_HOME)  cursor = home(cur_idx);
                else                          cursor = end(cur_idx);
            }
        }
        else if (ev.ctrl && ev.key == 3) {  // Ctrl C
            // Copy selection to clipboard
            std::string sel = get_selected();
            if (!sel.empty()) {
                CopyClipboard(sel);
            }
        }
        else {
            handled = false;
        }

        if (handled) {
            mgr->cursor = { clip.x + cursor.x, clip.y + cursor.y };
            mgr->is_dirty = true;
        }
    }
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
    else if (eqi(csid, "Label")) {
        ob = new Label(this);
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
        ob = new Edit(this);
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
        if (ev.type == EventType_Key) {
            if (notify_) {
                notify_->Event(ev);
            }
            is_dirty = true;
            continue;
        }
        if (ev.type == EventType_Mouse) {
            Point pt = { ev.x, ev.y };
            bool any_down = ev.button >= 1 && ev.button <= 3;
            int btn_idx = -1;
            if (any_down) {
                btn_idx = ev.button - 1; // button 1->0, 2->1, 3->2
            }
            bool first_down = any_down && btn_idx >= 0 && ev.first_down[btn_idx];
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
    if (is_dirty) {
        std::cout << CursorMove(cursor.x, cursor.y);
    }
    return is_dirty;
}
void Mgr::Paint(DrawBuffer& drawbuf)
{
    Win::Paint(drawbuf);
    is_dirty = false;
}

NAMESPACE_END
