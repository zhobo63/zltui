#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <windows.h>

#include "zltui.h"

namespace {

using namespace TUI;

const Color kInk(220, 230, 238);
const Color kMuted(125, 150, 165);
const Color kTeal(76, 201, 190);
const Color kBlue(110, 170, 255);
const Color kAmber(235, 190, 95);
const Color kGreen(115, 205, 155);
const Color kRed(235, 105, 115);
const Color kHeader(18, 28, 40);
const Color kSidebar(23, 34, 47);
const Color kSurface(29, 42, 55);
const Color kInput(22, 35, 43);

void SetVisible(const std::vector<SliderPtr>& panels, const SliderPtr& active, Mgr& mgr)
{
    for (const auto& panel : panels)
        panel->is_visible = panel == active;
    mgr.is_dirty = true;
}

LabelPtr AddLabel(const SliderPtr& parent, Mgr& mgr, const std::string& name,
                  const Rect& rect, const std::string& text, Color color = AnsiColor_White)
{
    auto label = parent->Create<Label>(name, rect);
    label->setText(text);
    label->fg_color = color;
    return label;
}

} // namespace

const char *dsl=u8R"(
Object Label
{
    Name header
    Rect 0 0 100 2
    Text  ZLTUI COMPONENT GALLERY   |   interactive terminal UI examples
    Bold true
    FgColor RGB(76,201,190)
    BgColor RGB(18, 28, 40)
}
Object Slider
{
    Name navigation
    Rect 0 1 22 26
    DrawBorder true
    BorderStyle Single
    Title Navigation
    FgColor RGB(235,190,95)
    BgColor RGB(23, 34, 47)
    Dock down 23 0 100 100
    DockOffset 0 0 0 -4
}
Object Slider
{
    Name content
    Rect 23 1 100 26
    DrawBorder true
    BorderStyle Single
    Title Showcase
    Dock Right|down 23 0 100 100
    DockOffset 0 0 0 -4
    FgColor RGB(220,230,238)
    Visible false
}
Object Edit
{
    Name commandLine
    Rect 0 26 100 28
    DrawBorder true
    BorderStyle round
    Title Command (Enter to run, /q to quit)
    Dock Right|DownPane 0 0 100 100
    DockOffset 0 0 0 -1
    FgColor RGB(115,205,155)
    BgColor RGB(22,35,43)
    ScrollY false
}
Object Label
{
    Name footer
    Rect 0 31 100 31
    Text  Tab/arrow keys navigate   |   Enter activates   |   Ctrl+C exits
    FgColor RGB(125,150,165)
    Dock downpane 0 0 100 100
    //Visible false
}
)";

int main()
{
    Terminal terminal;
    Mgr mgr;
    bool quit = false;

    // The DSL is intentionally used for the shell layout.  The controls inside
    // each page are created from C++ so their event wiring is easy to follow.
    mgr.Parse(dsl);

    auto navigation = mgr.GetUI<Slider>("navigation");
    auto content = mgr.GetUI<Slider>("content");
    auto commandLine = mgr.GetUI<Edit>("commandLine");
    mgr.GetUI<Label>("header")->bg_color = kHeader;
    navigation->bg_color = kSidebar;
    content->bg_color = kSurface;
    commandLine->bg_color = kInput;

    // Four independent pages make it obvious which feature is being shown.
    auto overview = std::dynamic_pointer_cast<TUI::Slider>(content->Clone());
    auto richText = std::dynamic_pointer_cast<TUI::Slider>(content->Clone());
    auto controls = std::dynamic_pointer_cast<TUI::Slider>(content->Clone());
    auto files = std::dynamic_pointer_cast<TUI::Slider>(content->Clone());

    for (const auto& panel : { overview, richText, controls, files }) {
        mgr.AddChild(panel);
    }
    overview->title = "Overview / layout and widgets";
    richText->title = "Rich text / Markdown renderer";
    controls->title = "Controls / form interaction";
    files->title = "File browser / application scenario";

    const std::vector<SliderPtr> pages = { overview, richText, controls, files };
    SetVisible(pages, overview, mgr);

    AddLabel(overview, mgr, "intro", { 2, 1, 70, 2 },
             "A small tour of the building blocks provided by zltui.", kTeal);
    AddLabel(overview, mgr, "layout", { 2, 4, 70, 5 },
             u8"• Win tree + Dock: compose nested terminal layouts\n• Slider: clipping and scrolling\n• Label / Button / Check / Combo: common controls\n• Edit / RichEdit: input and styled text", kInk);
    AddLabel(overview, mgr, "usage", { 2, 8, 70, 10 },
             "Use the navigation buttons to inspect each scenario.\n"
             "All pages share the same Mgr, event loop, and draw buffer.", kAmber);
    AddLabel(overview, mgr, "status", { 2, 13, 70, 15 },
             "Architecture: declarative shell + programmatic behavior\n"
             "Rendering: ANSI colors, borders, UTF-8 width handling", kGreen);

    auto rich = RichEditPtr(new RichEdit(&mgr));
    rich->local = { 2, 1, 72, 20 };
    rich->draw_border = false;
    auto normal = RichText::RichTextStyle(kInk);
    auto green = RichText::RichTextStyle(kGreen, AnsiColor_Unused, true);
    auto yellow = RichText::RichTextStyle(kAmber, AnsiColor_Unused, true);
    rich->appendText("Per-character styles: ", normal);
    rich->appendText("success ", green);
    rich->appendText("warning\n", yellow);
    rich->appendText(u8"UTF-8 / CJK width test:\n", normal);
    rich->appendText(u8"中文：你好，世界！全形標點：，。！？\n", green);
    rich->appendText(u8"日本語：こんにちは、世界！カタカナ：テスト\n", yellow);
    rich->appendText(u8"한국어：안녕하세요, 세계! 한글 테스트\n", normal);
    rich->appendText(u8"Mixed: CJK 中文 日本語 한국어 · ASCII · 🔥\n\n", normal);
    const std::string markdown = u8R"(
# Markdown preview / Markdown 預覽

- **中文**：粗體與 *斜體* 文字
- **日本語**：見出しと *強調* のテスト
- **한국어**：굵은 글씨와 *기울임* 테스트
- <fg=Green>Color tags / 顏色標籤 / 色タグ / 색상 태그</fg>

> RichEdit combines scrolling with UTF-8 text.
> 中日韓文字應正確對齊：中文、日本語、한국어。

| Language | Sample | Status |
|---|---|---|
| 中文 | 你好世界 | **ready** |
| 日本語 | こんにちは | **ready** |
| 한국어 | 안녕하세요 | **ready** |

```cpp
TUI::Markdown(edit, markdown);
```
)";
    Markdown(rich.get(), markdown);
    richText->AddChild(WinPtr(rich));

    AddLabel(controls, mgr, "nativeTitle", { 2, 1, 70, 1 },
             "Native controls", kTeal);
    auto nativeLabel = controls->Create<Label>("nativeLabel", { 2, 3, 20, 3 });
    nativeLabel->setText("Label: read-only text");
    nativeLabel->fg_color = kInk;
    auto nativeButton = controls->Create<Button>("nativeButton", { 26, 3, 45, 3 });
    nativeButton->setText("Button: click me");
    nativeButton->fg_color = kInk;
    nativeButton->bg_color = kSurface;
    nativeButton->bg_color_hover = Color(45, 70, 78);
    auto nativeStatus = AddLabel(controls, mgr, "nativeStatus", { 2, 10, 70, 10 },
                                 "Native control events appear here.", kMuted);
    nativeButton->on_click = [nativeStatus, &mgr]() {
        nativeStatus->setText("Button clicked: event delivered to the application.");
        mgr.is_dirty = true;
    };

    bool nativeChecked = true;
    auto nativeCheck = controls->Create<Check>("nativeCheck", { 2, 5, 20, 5 });
    nativeCheck->setText("Check: enabled");
    nativeCheck->SetChecked(nativeChecked);
    nativeCheck->on_check = [nativeStatus, &mgr](bool checked) {
        nativeStatus->setText(std::string("Check changed: ") + (checked ? "ON" : "OFF"));
        mgr.is_dirty = true;
    };

    auto nativeCombo = controls->Create<Combo>("nativeCombo", { 26, 5, 48, 5 });
    nativeCombo->setText("Combo");
    nativeCombo->items = { "Ocean", "Forest", "Mono" };
    nativeCombo->SetValue(0);
    nativeCombo->on_selected = [nativeStatus, &mgr](int value) {
        nativeStatus->setText("Combo selected index: " + std::to_string(value));
        mgr.is_dirty = true;
    };

    auto nativeEdit = controls->Create<Edit>("nativeEdit", { 2, 7, 48, 8 });
    nativeEdit->setText("Edit: type here");
    nativeEdit->draw_border = false;
    nativeEdit->border_style = BorderStyle_Single;
    nativeEdit->fg_color = kInk;
    nativeEdit->bg_color = kInput;

    AddLabel(controls, mgr, "formTitle", { 2, 12, 70, 12 },
             "LabelEdit composite controls", kTeal);
    std::string name = "Ada Lovelace";
    uint32_t refresh = 30;
    bool enabled = true;
    int theme = 0;
    auto nameEdit = controls->Create<LabelEdit>("name", { 2, 14, 50, 14 });
    nameEdit->Input("User name", name);
    auto refreshEdit = controls->Create<LabelEdit>("refresh", { 2, 15, 50, 15 });
    refreshEdit->Input("Refresh sec", refresh, 5);
    auto action = controls->Create<LabelEdit>("action", { 2, 16, 50, 16 });
    auto status = AddLabel(controls, mgr, "controlStatus", { 2, 19, 70, 19 },
                           "Interact with the composite fields below.", kTeal);
    action->Button("Action", "Apply", [&, status]() {
        status->setText("Applied: " + name + " / refresh " + std::to_string(refresh) + "s");
        mgr.is_dirty = true;
    });
    auto enabledCheck = controls->Create<LabelEdit>("enabled", { 2, 17, 50, 17 });
    enabledCheck->Check("Enabled", enabled, { "ON", "OFF" });
    auto themeCombo = controls->Create<LabelEdit>("theme", { 2, 18, 50, 18 });
    themeCombo->Combo("Theme", theme, { "Ocean", "Forest", "Mono" });
    AddLabel(controls, mgr, "hint", { 2, 20, 70, 21 },
             "LabelEdit wraps Label + Edit + Button / Check / Combo.", kAmber);

    std::filesystem::path currentPath = std::filesystem::current_path();
    std::function<void()> populateFiles;
    populateFiles = [&]() {
        files->child.clear();
        files->scroll_value.y = 0;
        AddLabel(files, mgr, "path", { 2, 1, 70, 1 },
                 "Path: " + currentPath.string(), kTeal);
        int y = 3;
        auto addEntry = [&](const std::string& text, const std::filesystem::path& path,
                            bool directory) {
            auto button = files->Create<Button>(path.filename().string(), { 2, y, 70, y });
            button->setText(text);
            button->text_algn = Align_Start;
            button->on_click = [&, path, directory]() {
                if (directory) {
                    currentPath = path;
                    populateFiles();
                } else {
                    commandLine->setText(commandLine->text + path.string());
                    mgr.notify_ = commandLine;
                }
            };
            ++y;
        };
        if (currentPath.has_parent_path())
            addEntry(u8"📁 ..", currentPath.parent_path(), true);
        try {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                addEntry((entry.is_directory() ? u8"📁 " : u8"📄 ") +
                         entry.path().filename().string(), entry.path(), entry.is_directory());
                if (y > 20) break;
            }
        } catch (const std::filesystem::filesystem_error& error) {
            AddLabel(files, mgr, "error", { 2, y, 70, y + 1 },
                     std::string("Cannot read directory: ") + error.code().message(),
                     kRed);
        }
        mgr.is_dirty = true;
    };
    populateFiles();

    const std::vector<std::string> pageNames = { "Overview", "RichText", "Controls", "Files" };
    for (size_t i = 0; i < pageNames.size(); ++i) {
        auto button = navigation->Create<Button>(pageNames[i], { 1, static_cast<int>(i) + 2, 20, static_cast<int>(i) + 2 });
        button->setText("[ " + std::to_string(i + 1) + " ] " + pageNames[i]);
        button->text_algn = Align_Start;
        button->fg_color = kInk;
        button->bg_color = kSidebar;
        button->bg_color_hover = Color(45, 70, 78);
        button->bg_color_down = Color(58, 92, 96);
        button->on_click = [&, i]() { SetVisible(pages, pages[i], mgr); };
    }
    AddLabel(navigation, mgr, "navHint", { 1, 9, 20, 13 },
             "Keyboard focus\n follows the\n active control.", kMuted);

    auto commandStatus = mgr.GetUI<Label>("footer");
    commandLine->on_key = [&, commandStatus](const Event& ev) -> bool {
        if (!ev.ctrl && !ev.shift && ev.vkey == VK_RETURN) {
            if (commandLine->text == "/q") {
                quit = true;
            } else if (!commandLine->text.empty()) {
                commandStatus->setText("Last command: " + commandLine->text);
            }
            commandLine->setText("");
            mgr.is_dirty = true;
            return true;
        }
        return false;
    };
    mgr.notify_ = commandLine;

    while (!quit) {
        if (mgr.Update(terminal)) {
            auto& buffer = terminal.GetDrawBuffer();
            buffer.clear();
            mgr.Paint(buffer);
            terminal.Render();
        }
        Sleep(10);
    }
    return 0;
}
