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
        buf.bg_color.ansi = TUI::AnsiColor_Bg_Black;
        buf.Text("hello", pos, col);

        buf.bg_color.ansi = TUI::AnsiColor_Bg_Blue;
        buf.Text("hello", { 10,10 }, { TUI::AnsiColor_Fg_White });

        terminal.Render();
        Sleep(100);
    }
    return 0;
}
