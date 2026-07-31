#define _CRT_SECURE_NO_WARNINGS
#include <iostream>

#include "zltui.h"

int main()
{
    TUI::Terminal terminal;

    bool quit = false;

    TUI::Color col;
    TUI::Point pos;
    
    while (!quit) {
        auto size = terminal.GetSize();
        pos.y++;
        if (pos.y >= size.y) {
            pos.y = 0;
        }

        col.ansi++;
        if (col.ansi > TUI::AnsiColor_Bright_White) {
            col.ansi = TUI::AnsiColor_Fg_Black;
        }

        auto& buf = terminal.GetDrawBuffer();
        buf.clear();

        buf.Border({ 1,0,5,5 }, TUI::AnsiColor_Bg_Green);
        buf.Border({ 10,1,40,10 }, TUI::AnsiColor_Bg_Magenta, TUI::BorderStyle_Single);
        buf.Border({ 41,1,60,10 }, TUI::AnsiColor_Bg_Magenta, TUI::BorderStyle_Double);
        buf.Border({ 61,1,80,10 }, TUI::AnsiColor_BrightBg_Magenta, TUI::BorderStyle_None);
        buf.ScrollBar({ size.x - 1, 0 }, size.y - 1, 50, 100, true);

        buf.Text("hello", pos, col);

        buf.Text("hello", { 10,10 }, TUI::AnsiColor_Fg_White, true);

        terminal.Render();
        Sleep(10);
    }
    return 0;
}
