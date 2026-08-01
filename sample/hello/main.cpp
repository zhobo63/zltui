#define _CRT_SECURE_NO_WARNINGS
#include <iostream>

#include "zltui.h"

int main()
{
    TUI::Terminal terminal;

    bool quit = false;

    TUI::Color col;
    TUI::Point pos;
    int offset = 0;

    TUI::Mgr mgr;
    //mgr.Parse(R"(

    //)");
    auto size = terminal.GetSize();
    TUI::WinPtr pane1 = mgr.Create("Win");
    pane1->local.set(0, 0, 40, size.y - 1);
    pane1->draw_border = true;
    pane1->fg_color = TUI::Color(0, 250, 250);
    mgr.child.push_back(pane1);

    TUI::WinPtr pane2 = mgr.Create("Win");
    pane2->local.set(41, 0, size.x - 1, size.y - 1);
    pane2->draw_border = true;
    pane2->fg_color = TUI::Color(250, 0, 250);
    mgr.child.push_back(pane2);

    
    while (!quit) {
        auto size = terminal.GetSize();
        //pos.y++;
        //if (pos.y >= size.y) {
        //    pos.y = 0;
        //}

        //col.ansi++;
        //if (col.ansi > TUI::AnsiColor_Bright_White) {
        //    col.ansi = TUI::AnsiColor_Black;
        //}

        auto& buf = terminal.GetDrawBuffer();
        buf.clear();

        //buf.Border({ 1,0,10,5 }, TUI::Color(46,46,46));
        //buf.Border({ 11,1,40,10 }, TUI::AnsiColor_Magenta, TUI::BorderStyle_Single);
        //buf.Border({ 41,1,60,10 }, TUI::AnsiColor_Magenta, TUI::BorderStyle_Double);
        //buf.Border({ 61,1,80,10 }, TUI::AnsiColor_Bright_Magenta, TUI::BorderStyle_None);
        //offset = (offset + 1) % 100;
        //buf.ScrollBar({ size.x - 1, 0 }, size.y - 1, offset, 100, true);
        //buf.ScrollBar({ 0, size.y - 1 }, size.x - 1, offset, 200, false);

        //buf.Text("hello", pos, col);

        //buf.Text("hello", { 12,1 }, TUI::AnsiColor_White, true);

        mgr.Paint(buf);

        terminal.Render();
        Sleep(10);
    }
    return 0;
}
