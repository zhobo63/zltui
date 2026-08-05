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
    Rect 0 0 80 1
    Dock right 0 0 100 100
    Text 🤖 AI Agent — qwen3.6 | ████████░░ 82%
    Bold true
    FgColor BrightCyan
}

# ── Tools Panel (Left) ─────────────────────
Object Slider
{
    Name toolsPanel
    Rect 0 2 30 100
    DrawBorder true
    BorderStyle Single
    Title Tools
    Dock down 0 0 25 100
    DockOffset 0 0 0 -5
    FgColor BrightCyan
    Arrange Content true

    Object Label
    {
        Name toolRead
        Rect 0 1 23 2
        Text ✅ Read file
        FgColor BrightGreen
    }
    Object Label
    {
        Name toolEdit
        Rect 0 4 23 5
        Text ⏳ Edit file\n    Applying changes...
        FgColor Yellow
    }
    Object Label
    {
        Name toolSpawn
        Rect 0 7 23 8
        Text 🔧 Spawn agent
        FgColor BrightCyan
    }
    Object Label
    {
        Name toolDiag
        Rect 0 10 23 11
        Text 📊 Diagnostics
        FgColor White
    }
    Object Button
    {
        Name toolDeploy
        Rect 0 14 23 15
        Text 🚀 Deploy
        FgColor BrightMagenta
    }
    Object Check
    {
        Name toolAutoSave
        Rect 0 18 23 19
        Text 💾 Auto-save
        Checked true
        FgColor Green
    }
    Object Check
    {
        Name toolDarkMode
        Rect 0 22 23 23
        Text 🌙 Dark mode
        Checked false
        FgColor BrightBlue
    }
    Object Label
    {
        Name toolSearch
        Rect 0 26 23 27
        Text 🔍 Search files
        FgColor Yellow
    }
    Object Button
    {
        Name toolTerminal
        Rect 0 30 23 31
        Text 💻 Terminal
        FgColor BrightWhite
    }
    Object Label
    {
        Name toolGit
        Rect 0 34 23 35
        Text 📦 Git status
        FgColor Green
    }
    Object Check
    {
        Name toolNotifications
        Rect 0 38 23 39
        Text 🔔 Notifications
        Checked true
        FgColor BrightYellow
    }
    Object Label
    {
        Name toolSettings
        Rect 0 42 23 43
        Text ⚙️ Settings
        FgColor White
    }
}

# ── Chat / Output Area (Right) ─────────────
Object Slider
{
    Name chatArea
    Rect 31 2 100 100
    DrawBorder true
    BorderStyle Single
    Title Output
    Dock Right|Down 25 2 100 100
    DockOffset 0 0 0 -5
    Vertical true
    BgColor RGB(30,30,40)

    Object Label
    {
        Name agentMsg1
        Rect 0 1 76 2
        Text [Agent]: Analyzing codebase...
        FgColor BrightCyan
    }
    Object Slider
    {
        Name codeBlock
        Rect 0 4 50 12
        DrawBorder true
        BorderStyle none
        BgColor RGB(20,20,30)

        Object Label
        {
            Name codeText
            Rect 1 1 48 10
            Text void foo() {\n    bar();\n}
            FgColor BrightGreen
        }
    }
    Object Label
    {
        Name agentMsg2
        Rect 0 15 76 16
        Text [Agent]: Changes applied. Ready for next task.
        FgColor White
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
    Text Type your message...\nThis is next line\nABC
    FgColor BrightYellow
}
    )");

    TUI::Slider* toolsPanel = mgr.GetUI<TUI::Slider>("toolsPanel");

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
