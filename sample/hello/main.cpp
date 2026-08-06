#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <filesystem>
#include <functional>

#include "zltui.h"

int main()
{
    TUI::Terminal terminal;

    bool quit = false;

    TUI::Color col;
    TUI::Point pos;
    int offset = 0;

// AI Agent UI Layout:
// ┌───────────────────────────────────────────────┐
// │ 🤖 AI Agent — gpt-4o | ████████░░ 82%        │ ← Title Bar (Dock_Top)
// ├──────────────┬────────────────────────────────┤
// │ Tools Panel  │ Chat / Output Area             │ ← Split: Left|Right
// │ ✅ Read file │ [Agent]: Analyzing code...     │
// │ ⏳ Edit file │ ┌──────────────┐               │
// │ 🔧 Spawn ag. │ │  Code Block   │               │
// │ 📊 Diags     │ └──────────────┘               │
// │              │ [Agent]: Changes applied.      │
// ├──────────────┴────────────────────────────────┤
// │ > Type your message...                        │ ← Input (Dock_Down)
// └───────────────────────────────────────────────┘

    TUI::Mgr mgr;
    mgr.Parse(u8R"(
# ── Title Bar ──────────────────────────────
Object Label
{
    Name titleBar
    Rect 0 0 80 0
    Dock right 0 0 100 100
    Text 🤖 AI Agent — qwen3.6 | ████████░░ 82%
    Bold true
    FgColor BrightCyan
}

# ── Tools Panel (Left) ─────────────────────
Object Slider
{
    Name filesPanel
    Rect 0 1 30 30
    DrawBorder true
    BorderStyle Single
    Title Files
    Dock down 0 0 25 100
    DockOffset 0 0 0 -5
    FgColor BrightCyan
    Arrange Content true
}

# ── Chat / Output Area (Right) ─────────────
Object Slider
{
    Name chatArea
    Rect 31 1 100 100
    DrawBorder true
    BorderStyle Single
    Title Output
    Dock Right|Down 25 2 100 100
    DockOffset 0 0 0 -5
    BgColor RGB(30,30,40)

    Object Label
    {
        Name agentMsg1
        Rect 0 1 76 2
        Text [Agent]: Analyzing codebase...
        FgColor BrightCyan
    }

    Object Edit
    {
        Name codeText
        Rect 1 4 48 11
        DrawBorder true
        BorderStyle none
        Dock right 0 0 100 100
        DockOffset 0 0 -1 0
        Text # this is foo\nvoid foo() {\n    bar();\n}
        FgColor BrightGreen
        BgColor RGB(20,20,30)
    }

    Object Label
    {
        Name agentMsg2
        Rect 0 15 76 16
        Text [Agent]: Changes applied. Ready for next task.
        FgColor BrightCyan
    }
}

# ── Input Bar (Bottom) ─────────────────────
Object Label
{
    Name prompt
    Rect 0 0 1 0
    Dock top|down 0 100 100 100
    DockOffset 0 -4 0 -4
    Text >
}
Object Edit
{
    Name inputBar
    Rect 1 0 100 5
    Dock top|right|down 0 100 100 100
    DockOffset 0 -4 0 0
    ScrollY true
    Text Type your message...\nThis is next line\nABC
    FgColor BrightYellow
}
    )");

    TUI::Slider* filesPanel = mgr.GetUI<TUI::Slider>("filesPanel");
    TUI::Slider* chatArea = mgr.GetUI<TUI::Slider>("chatArea");
    TUI::Edit* inputBar = mgr.GetUI<TUI::Edit>("inputBar");

    std::filesystem::path current_path = ".";

    std::function<void()> populate_files_panel = [&]() {
        // Clear existing children
        filesPanel->child.clear();
        filesPanel->scroll_value.y = 0;
        filesPanel->mgr->notify_ = nullptr;

        int file_y = 0;

        // Add ".." button if not at root
        if (current_path.has_parent_path()) {

            TUI::Button* btn = new TUI::Button(&mgr);
            btn->name = "..";
            btn->local = { 1, file_y, filesPanel->local.width() - 2, file_y };
            btn->setText(u8"📁 ..");
            btn->dock_.mode = TUI::Dock_Right;
            btn->dock_.dock = { 0, 0, 100, 100 };
            btn->text_algn = TUI::Align_Start;

            btn->on_click = [&current_path, &populate_files_panel]() {
                current_path = current_path.parent_path();
                populate_files_panel();
            };
            filesPanel->AddChild(TUI::WinPtr(btn));
            file_y ++;
        }

        for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
            bool is_dir = entry.is_directory();
            std::string icon = is_dir ? u8"📁" : u8"📄";
            std::string filename = entry.path().filename().string();
            std::filesystem::path entry_path = entry.path();

            TUI::Button* btn = new TUI::Button(&mgr);
            btn->name = filename;
            btn->local = { 1, file_y, filesPanel->local.width() - 2, file_y };
            btn->setText(icon + " " + filename);
            btn->dock_.mode = TUI::Dock_Right;
            btn->dock_.dock = { 0, 0, 100, 100 };
            btn->text_algn = TUI::Align_Start;

            //btn->bg_color = file_y % 2 == 1;
            btn->on_click = [&current_path, &populate_files_panel, inputBar, is_dir, entry_path]() mutable {
                if (is_dir) {
                    current_path = entry_path;
                    populate_files_panel();
                }
                else {
                    inputBar->setText(inputBar->text + entry_path.string());
                }
            };
            filesPanel->AddChild(TUI::WinPtr(btn));
            file_y++;
        }
        mgr.is_dirty = true;
    };

    populate_files_panel();

    inputBar->on_key = [inputBar, chatArea, &mgr, &quit](const TUI::Event &ev) -> bool {
        if (!ev.ctrl && !ev.shift && ev.vkey == VK_RETURN) {
            TUI::Label* lb = new TUI::Label(&mgr);
            int last_y = 0;
            if(chatArea->child.size()>0) {
                auto ch = chatArea->child.back();
                last_y = std::max(last_y, ch->local.y2 + 1);
            }
            lb->local = { 0,last_y,chatArea->local.width() - 1, last_y + 5 };
            lb->autosize_ = TUI::Autosize_TextHeight;
            lb->setText(inputBar->text);
            if (inputBar->text == "/q") {
                quit = true;
            }
            inputBar->setText("");
            chatArea->AddChild(TUI::WinPtr(lb));
            chatArea->scroll_value.y = std::max(0, lb->local.y2 + 1 - chatArea->clip.height());
            mgr.is_dirty = true;
            return true;
        }
        return false;
        };
    mgr.notify_ = inputBar;

    while (!quit) {
        if (mgr.Update(terminal)) {
            auto& buf = terminal.GetDrawBuffer();
            buf.clear();
            mgr.Paint(buf);
            terminal.Render();
        }
        Sleep(10);
    }
    return 0;
}
