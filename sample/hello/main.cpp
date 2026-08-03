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
    mgr.Parse(u8R"(
Object Slider 
{
    Name pane1
    DrawBorder true
    FgColor RGB(0,250,250)
    Rect 0 0 40 29
    Object Label
    {
        Name label1
        Rect 0 0 39 1
        Text u8"這是🔥🔳 label\n隨著螢幕大小自動縮放「內部零件」或「裡面的一部分」"
    }
    Object Button
    {
        Name btn1
        Rect 0 2 12 2
        Text Button 1
        FgColor BrightGreen
    }
    Object Button
    {
        Name btn2
        Rect 14 2 28 2
        Text Button 2
        FgColor BrightGreen
    }
    Object Check
    {
        Name chk1
        Rect 0 3 12 3
        Text Check
    }
}
    )");
    auto size = terminal.GetSize();
    //TUI::WinPtr pane1 = mgr.Create("Slider");
    //pane1->name = "pane1";
    //pane1->local.set(0, 0, 40, size.y - 1);
    //pane1->draw_border = true;
    //pane1->fg_color = TUI::Color(0, 250, 250);
    //mgr.AddChild(pane1);

    //std::shared_ptr<TUI::Label> label = std::shared_ptr<TUI::Label>(new TUI::Label(&mgr));
    //label->name = "label1";
    //label->local.set(0, 0, 39, 1);
    //label->setText(u8"這是🔥🔳 label 隨著螢幕大小自動縮放「內部零件」或「裡面的一部分」");
    //pane1->AddChild(label);

    //std::shared_ptr<TUI::Button> btn = std::shared_ptr<TUI::Button>(new TUI::Button(&mgr));
    //btn->name = "btn1";
    //btn->local.set(0, 2, 12, 2);
    //btn->setText("Button 1");
    //btn->fg_color = TUI::AnsiColor_Bright_Green;
    //pane1->AddChild(btn);

    //btn->name = "btn2";
    //btn = std::shared_ptr<TUI::Button>(new TUI::Button(&mgr));
    //btn->local.set(14, 2, 28, 2);
    //btn->setText("Button 2");
    //btn->fg_color = TUI::AnsiColor_Bright_Green;
    //pane1->AddChild(btn);

    //std::shared_ptr<TUI::Check> chk = std::shared_ptr<TUI::Check>(new TUI::Check(&mgr));
    //chk->name = "chk1";
    //chk->local.set(0, 3, 12, 3);
    //chk->setText("Check");
    //pane1->AddChild(chk);

    TUI::WinPtr pane2 = mgr.Create("Slider");
    pane2->name = "pane2";
    pane2->local.set(41, 0, size.x - 1, size.y - 1);
    pane2->draw_border = true;
    pane2->fg_color = TUI::Color(250, 0, 250);
    mgr.AddChild(pane2);

    
    while (!quit) {
        auto size = terminal.GetSize();

        if (mgr.Update(terminal)) {
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
        }
        Sleep(10);
    }
    return 0;
}
