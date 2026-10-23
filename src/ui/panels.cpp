#include "ui/panels.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numeric>
#include <sstream>

#include <imgui.h>

#include "bklv/core/json_view.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace designer
{

namespace
{

const Palette& paletteOf(const DesignerApp& app)
{
    return app.palette != nullptr ? *app.palette : darkPalette();
}

std::string typeIcon(const std::string& type)
{
    if (type == "Label") return "text_fields";
    if (type == "Image") return "image";
    if (type == "Button") return "widgets";
    if (type == "page") return "dashboard";
    if (type == "gameCard") return "widgets";
    if (type == "navItem") return "list";
    if (type == "platformChip") return "crop_square";
    if (type == "sidebar") return "view_list";
    if (type == "header") return "tune";
    return "crop_square";
}

std::string typeLabel(const std::string& type)
{
    if (type == "page") return "页面";
    if (type == "gameCard") return "组件";
    if (type == "navItem") return "导航项";
    if (type == "platformChip") return "平台";
    if (type == "Label") return "文本";
    if (type == "Image") return "图片";
    if (type == "Button") return "按钮";
    return "容器";
}

std::string formatValue(const Json& value)
{
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_float())
    {
        const double number = value.get<double>();
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.6g", number);
        return buffer;
    }
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    return value.dump();
}

std::string styleValueText(const Json& value)
{
    return formatValue(value);
}

bool refNameOf(const Json& value, std::string& name, bool& local)
{
    if (!value.is_string()) return false;
    const std::string text = value.get<std::string>();
    if (text.rfind("@theme/", 0) == 0 || text.rfind("@style/", 0) == 0)
    {
        name = text;
        local = false;
        return true;
    }
    if (text.rfind("&color.", 0) == 0 || text.rfind("&number.", 0) == 0)
    {
        name = text;
        local = true;
        return true;
    }
    return false;
}

std::string jsonPointerText(const Json& value)
{
    return value.dump();
}

std::string ratioLabel(int width, int height)
{
    if (width <= 0 || height <= 0) return "—";
    const int divisor = std::gcd(width, height);
    const int w = width / divisor;
    const int h = height / divisor;
    if (w <= 32 && h <= 32) return std::to_string(w) + ":" + std::to_string(h);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f:1", (double)width / (double)height);
    return buffer;
}

std::string formatZoom(float zoom)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d%%", (int)std::round(zoom * 100.0f));
    return buffer;
}

void panelHeader(ImDrawList* drawList, const DesignerApp& app, const Rect& rect, const std::string& title,
    const std::string& iconName)
{
    const Palette& palette = paletteOf(app);
    ui::fill(drawList, rect, palette.header);
    ui::hline(drawList, rect.y + rect.h - 1, rect.x, rect.x + rect.w, palette.border);
    const float iconSize = 15.0f;
    const float textHeight = ui::lineHeight(11.5f);
    const float y = rect.y + (rect.h - textHeight) * 0.5f;
    ui::icon(drawList, rect.x + 12, rect.y + (rect.h - iconSize) * 0.5f - 1, iconName.c_str(), palette.textMuted,
        iconSize);
    ui::text(drawList, rect.x + 34, y, title, palette.text, 11.5f);
}

void iconButton(ImDrawList* drawList, const DesignerApp& app, const Rect& rect, const char* icon, bool hovered,
    bool active = false, bool disabled = false)
{
    const Palette& palette = paletteOf(app);
    if (hovered && !disabled)
        ui::fill(drawList, rect, palette.hover, 5.0f);
    if (active)
        ui::fill(drawList, rect, palette.accentSoft, 5.0f);
    ui::iconCentered(drawList, rect, icon,
        disabled ? palette.textFaint : (active ? palette.accent : palette.text), 15.0f);
}

void drawInputBox(ImDrawList* drawList, const DesignerApp& app, const Rect& rect, const std::string& text,
    bool focused, const std::string& placeholder = std::string())
{
    const Palette& palette = paletteOf(app);
    ui::fill(drawList, rect, palette.inputBg, 5.0f);
    ui::stroke(drawList, rect, focused ? palette.accent : palette.border, 5.0f);
    const std::string shown = text.empty() ? placeholder : text;
    ui::textClipped(drawList, rect.x + 8, rect.y + (rect.h - 16.0f) * 0.5f, rect.w - 16,
        shown, text.empty() ? palette.textFaint : palette.text, 12.0f);
}

void drawCaret(ImDrawList* drawList, const DesignerApp& app, const Rect& rect, const std::string& text, int caret)
{
    const Palette& palette = paletteOf(app);
    const float x = rect.x + 8 + ui::textWidth(text.substr(0, (size_t)caret), 12.0f);
    const float height = std::min(rect.h - 10.0f, 18.0f);
    ui::fill(drawList, Rect(x, rect.y + (rect.h - height) * 0.5f, 1.0f, height), palette.accent);
}

void drawInlineValue(ImDrawList* drawList, const DesignerApp& app, const Rect& rect, const std::string& text,
    int caret, ImU32 color, float size, bool centered = false)
{
    const Palette& palette = paletteOf(app);
    const std::string shown = text;
    if (centered)
        ui::textClipped(drawList, rect.x + 8, rect.y + (rect.h - ui::lineHeight(size)) * 0.5f, rect.w - 16, shown,
            color, size);
    else
        ui::textClipped(drawList, rect.x + 8, rect.y + (rect.h - ui::lineHeight(size)) * 0.5f, rect.w - 16, shown,
            color, size);
    const float caretX = rect.x + 8 + ui::textWidth(shown.substr(0, (size_t)caret), size);
    const float caretHeight = std::min(rect.h - 8.0f, ui::lineHeight(size) + 2.0f);
    ui::fill(drawList, Rect(caretX, rect.y + (rect.h - caretHeight) * 0.5f, 1.0f, caretHeight),
        palette.accent);
}

void drawSegment(ImDrawList* drawList, const DesignerApp& app, const Rect& rect, const std::string& text,
    bool active, bool hovered)
{
    const Palette& palette = paletteOf(app);
    if (active)
        ui::fill(drawList, rect, palette.accentSoft, 5.0f);
    else if (hovered)
        ui::fill(drawList, rect, palette.hover, 5.0f);
    ui::label(drawList, rect, text, active ? palette.text : palette.textMuted, 12.0f, 1);
}

std::string indentation(int depth)
{
    return std::string((size_t)std::max(0, depth) * 14, ' ');
}

bklv::View* hitPreviewView(bklv::View* view, const ImVec2& point)
{
    if (view == nullptr || view->getVisibility() != bklv::Visibility::VISIBLE)
        return nullptr;
    const float x = view->getX();
    const float y = view->getY();
    if (point.x < x || point.y < y || point.x >= x + view->getWidth() || point.y >= y + view->getHeight())
        return nullptr;
    if (bklv::Box* box = view->asBox())
    {
        const std::vector<bklv::View*>& children = box->getChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            if (bklv::View* hit = hitPreviewView(*it, point))
                return hit;
    }
    return view;
}

} // namespace

DesignerPanel::DesignerPanel(DesignerApp& app, const std::string& id)
    : app(app)
{
    this->setId(id);
    this->setClipsToBounds(true);
}

void collectVisibleRows(const Node& node, const std::set<std::string>& expanded, std::vector<TreeRow>& rows,
    float originX, float originY, float areaWidth, float scroll, float rowHeight, float& contentHeight)
{
    rows.clear();
    int index = 0;
    std::function<void(const Node&)> visit = [&](const Node& current) {
        TreeRow row;
        row.pointer = current.pointer;
        row.type = current.type;
        row.name = current.name;
        row.id = current.id;
        row.sourceFile = current.sourceFile;
        row.sourcePointer = current.sourcePointer;
        row.depth = current.depth;
        row.hasChildren = current.hasChildren;
        row.expanded = expanded.find(current.pointer) != expanded.end();
        row.contentY = index * rowHeight;
        row.rect = Rect(originX, originY + row.contentY - scroll, areaWidth, rowHeight);
        rows.push_back(row);
        index++;
        if (row.expanded)
            for (const auto& child : current.children)
                visit(*child);
    };
    visit(node);
    contentHeight = index * rowHeight;
}

void drawTreeRow(ImDrawList* drawList, const Palette& palette, const TreeRow& row, bool selected, bool hovered,
    bool activeDragDrop, int dropMode)
{
    if (row.rect.y + row.rect.h < row.rect.y) return;
    const bool imported = !row.sourceFile.empty();
    if (selected && !imported)
        ui::fill(drawList, row.rect, palette.selected, 0.0f);
    else if (hovered)
        ui::fill(drawList, row.rect, palette.hover, 0.0f);

    float x = row.rect.x + 10.0f + row.depth * 14.0f;
    if (row.hasChildren)
    {
        ui::chevron(drawList, x + 4, row.rect.y + row.rect.h * 0.5f, 4.0f, row.expanded, palette.textFaint);
    }
    x += 14.0f;

    const std::string icon = typeIcon(row.type);
    ui::icon(drawList, x, row.rect.y + 5.0f, icon.c_str(),
        (selected && !imported) ? palette.accent : (imported ? palette.teal : palette.textMuted), 13.0f);
    x += 20.0f;

    const float badgeWidth = ui::textWidth(typeLabel(row.type), 9.5f) + 10.0f;
    const float nameWidth = row.rect.w - (x - row.rect.x) - badgeWidth - 18.0f;
    ui::textClipped(drawList, x, row.rect.y + 5.0f, nameWidth,
        row.name + (imported ? "  ↗" : ""),
        (selected && !imported) ? palette.text : (imported ? palette.teal : palette.text), 11.5f);

    Rect badge(row.rect.x + row.rect.w - badgeWidth - 8.0f, row.rect.y + 6.0f, badgeWidth, 14.0f);
    ui::fill(drawList, badge, selected ? uiAlpha(palette.accent, 0.22f) : palette.panelAlt, 4.0f);
    ui::label(drawList, badge, typeLabel(row.type), selected ? palette.accent : palette.textFaint, 9.0f, 1);

    if (activeDragDrop)
    {
        if (dropMode == 0)
            ui::fill(drawList, Rect(row.rect.x + 10, row.rect.y, row.rect.w - 20, 2), palette.accent);
        else if (dropMode == 2)
            ui::fill(drawList, Rect(row.rect.x + 10, row.rect.y + row.rect.h - 2, row.rect.w - 20, 2), palette.accent);
        else
        {
            ui::fill(drawList, row.rect, uiAlpha(palette.accent, 0.12f));
            ui::stroke(drawList, row.rect, palette.accent, 0.0f, 1.0f);
        }
    }
}

Rect popupBounds(const PopupState& popup, ImVec2 viewport)
{
    const float itemHeight = 30.0f;
    const float padding = 6.0f;
    float height = padding * 2.0f;
    for (const MenuItem& item : popup.items)
        height += item.separator ? 9.0f : itemHeight;
    float x = popup.alignRight ? popup.anchor.x + popup.anchor.w - popup.width : popup.anchor.x;
    float y = popup.anchor.y + popup.anchor.h;
    if (x < 8) x = 8;
    if (x + popup.width > viewport.x - 8) x = viewport.x - popup.width - 8;
    if (y + height > viewport.y - 8) y = popup.anchor.y - height;
    if (y < 8) y = 8;
    return Rect(x, y, popup.width, height);
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

MenuBarPanel::MenuBarPanel(DesignerApp& app)
    : DesignerPanel(app, "menubar")
{
}

Rect MenuBarPanel::menuRect(int index) const
{
    return Rect(150.0f + index * 50.0f, 0.0f, 46.0f, 46.0f);
}

Rect MenuBarPanel::iconButtonRect(int index) const
{
    const float width = this->getWidth();
    const float base = width - 150.0f;
    return Rect(base + index * 34.0f, 9.0f, 30.0f, 28.0f);
}

void MenuBarPanel::draw(ImDrawList* drawList, float x, float y, float width, float height,
    bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    ui::fill(drawList, Rect(x, y, width, height), palette.header);
    ui::hline(drawList, y + height - 1, x, x + width, palette.border);

    // Brand mark
    Rect logo(x + 14, y + 10, 26, 26);
    ui::fill(drawList, logo, palette.accent, 7.0f);
    ui::iconCentered(drawList, logo, "widgets", IM_COL32_WHITE, 17.0f);
    ui::textVC(drawList, x + 48, y, height, "UI 设计器", palette.text, 13.0f);

    static const char* labels[4] = { "文件", "编辑", "视图", "设置" };
    for (int i = 0; i < 4; ++i)
    {
        Rect rect = this->menuRect(i);
        const bool hover = this->app.hovering(rect);
        if (this->app.menuIndex == i || hover)
            ui::fill(drawList, Rect(rect.x + 3, rect.y + 11, rect.w - 6, 24), palette.hover, 5.0f);
        ui::label(drawList, rect, labels[i], this->app.menuIndex == i ? palette.text : palette.textMuted, 12.0f, 1);
    }

    // Live state
    const bool renderError = !this->app.previewError.empty();
    Rect live(width - 246, y + 13, 96, 20);
    ui::fill(drawList, live, palette.panelAlt, 10.0f);
    ui::dot(drawList, live.x + 11, live.y + 10, 3.2f, renderError ? palette.red : palette.green);
    ui::textVC(drawList, live.x + 20, live.y, live.h, renderError ? "渲染异常" : "实时预览", palette.textMuted,
        10.5f);

    const Document* document = this->app.doc();
    const bool canUndo = document != nullptr && !document->undoStack.empty();
    const bool canRedo = document != nullptr && !document->redoStack.empty();
    Rect undo = this->iconButtonRect(0);
    Rect redo = this->iconButtonRect(1);
    Rect run = this->iconButtonRect(2);
    iconButton(drawList, this->app, undo, "undo", this->app.hovering(undo), false, !canUndo);
    iconButton(drawList, this->app, redo, "redo", this->app.hovering(redo), false, !canRedo);
    iconButton(drawList, this->app, run, this->app.canvasPreviewMode ? "fullscreen_exit" : "play_arrow",
        this->app.hovering(run), this->app.canvasPreviewMode);
    Rect theme = this->iconButtonRect(3);
    iconButton(drawList, this->app, theme, "palette", this->app.hovering(theme),
        this->app.lightTheme);
}

void MenuBarPanel::handleInput(UiInput& input)
{
    const Rect bar(0, 0, this->getWidth(), this->getHeight());
    if (!bar.contains(input.mouse)) return;

    static const char* labels[4] = { "文件", "编辑", "视图", "设置" };
    for (int i = 0; i < 4; ++i)
    {
        Rect rect = this->menuRect(i);
        if (rect.contains(input.mouse))
        {
            this->app.tooltip = labels[i];
            if (input.pressed)
            {
                if (this->app.menuIndex == i && this->app.menuPopup.open)
                    this->app.menuPopup.open = false;
                else
                    this->app.openMenu(i, rect);
                return;
            }
        }
    }

    Rect undo = this->iconButtonRect(0);
    Rect redo = this->iconButtonRect(1);
    Rect run = this->iconButtonRect(2);
    Rect theme = this->iconButtonRect(3);
    if (undo.contains(input.mouse))
    {
        this->app.tooltip = "撤销  ⌘Z";
        if (input.pressed) this->app.undo();
        return;
    }
    if (redo.contains(input.mouse))
    {
        this->app.tooltip = "重做  ⇧⌘Z";
        if (input.pressed) this->app.redo();
        return;
    }
    if (run.contains(input.mouse))
    {
        this->app.tooltip = this->app.canvasPreviewMode ? "退出预览" : "运行预览";
        if (input.pressed) this->app.runAction("toggle_preview");
        return;
    }
    if (theme.contains(input.mouse))
    {
        this->app.tooltip = this->app.lightTheme ? "切换到深色主题" : "切换到浅色主题";
        if (input.pressed) this->app.runAction(this->app.lightTheme ? "theme_dark" : "theme_light");
        return;
    }
    Rect live(this->getWidth() - 246, 13, 96, 20);
    if (live.contains(input.mouse))
        this->app.tooltip = this->app.previewError.empty() ? "画布正在使用 bklv 实时渲染" : "渲染异常，见画布提示";
}

// ---------------------------------------------------------------------------
// Project tree
// ---------------------------------------------------------------------------

ProjectPanel::ProjectPanel(DesignerApp& app)
    : DesignerPanel(app, "project")
{
}

void ProjectPanel::buildRows()
{
    this->rows.clear();
    const float listTop = this->getY() + 68.0f;
    const float width = this->getWidth();
    const Palette& palette = paletteOf(this->app);

    FileRow root;
    root.relative = std::string();
    root.name = this->app.project.root.empty() ? "未打开工程" : this->app.project.root.filename().string();
    root.isDirectory = true;
    root.depth = 0;
    root.expanded = true;
    root.rect = Rect(this->getX(), listTop - this->app.projectScroll, width, 26);
    this->rows.push_back(root);

    int index = 1;
    if (this->app.fileFilter.empty())
    {
        for (const FileEntry& entry : this->app.files)
        {
            bool visible = true;
            std::string prefix;
            const std::vector<std::string> parts = [&] {
                std::vector<std::string> result;
                std::stringstream stream(entry.relative);
                std::string part;
                while (std::getline(stream, part, '/')) result.push_back(part);
                return result;
            }();
            for (size_t i = 0; i + 1 < parts.size(); ++i)
            {
                prefix = prefix.empty() ? parts[i] : prefix + "/" + parts[i];
                if (this->app.expandedDirs.find(prefix) == this->app.expandedDirs.end())
                {
                    visible = false;
                    break;
                }
            }
            if (!visible) continue;

            FileRow row;
            row.relative = entry.relative;
            row.name = entry.name;
            row.isDirectory = entry.isDirectory;
            row.depth = entry.depth + 1;
            row.expanded = row.isDirectory && this->app.expandedDirs.find(entry.relative) != this->app.expandedDirs.end();
            row.rect = Rect(this->getX(), listTop + index * 26 - this->app.projectScroll, width, 26);
            this->rows.push_back(row);
            index++;
        }
    }
    else
    {
        std::string needle = this->app.fileFilter;
        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        for (const FileEntry& entry : this->app.files)
        {
            if (entry.isDirectory) continue;
            std::string haystack = entry.relative;
            std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });
            if (haystack.find(needle) == std::string::npos) continue;
            FileRow row;
            row.relative = entry.relative;
            row.name = entry.relative;
            row.isDirectory = false;
            row.depth = 0;
            row.rect = Rect(this->getX(), listTop + index * 26 - this->app.projectScroll, width, 26);
            this->rows.push_back(row);
            index++;
        }
    }

    (void)palette;
    this->listArea = Rect(this->getX(), listTop, width, this->getHeight() - 68.0f);
}

void ProjectPanel::draw(ImDrawList* drawList, float x, float y, float width, float height, bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    ui::fill(drawList, Rect(x, y, width, height), palette.panel);

    Rect header(x, y, width, 34);
    panelHeader(drawList, this->app, header, "项目树", "folder_open");

    this->addRect = Rect(x + width - 64, y + 7, 20, 20);
    this->refreshRect = Rect(x + width - 36, y + 7, 20, 20);
    iconButton(drawList, this->app, this->addRect, "add", this->app.hovering(addRect));
    iconButton(drawList, this->app, this->refreshRect, "refresh",
        this->app.hovering(refreshRect));

    this->searchRect = Rect(x + 10, y + 39, width - 20, 24);
    drawInputBox(drawList, this->app, this->searchRect, this->app.edit.active && this->app.edit.key == "search"
        ? this->app.edit.buffer : this->app.fileFilter, this->app.edit.active && this->app.edit.key == "search",
        "搜索文件…");
    ui::icon(drawList, this->searchRect.x + this->searchRect.w - 22, this->searchRect.y + 5, "search",
        palette.textFaint, 13.0f);

    this->buildRows();

    const Rect area = this->listArea;
    drawList->PushClipRect(area.min(), area.max(), true);
    for (const FileRow& row : this->rows)
    {
        if (row.rect.y + row.rect.h < area.y || row.rect.y > area.y + area.h) continue;
        const bool active = !row.isDirectory && this->app.hasDoc() && this->app.doc()->relative == row.relative;
        const bool hover = this->app.hovering(row.rect);
        if (active)
            ui::fill(drawList, row.rect, palette.selected);
        else if (hover)
            ui::fill(drawList, row.rect, palette.hover);

        float left = row.rect.x + 10 + row.depth * 14.0f;
        if (row.relative.empty())
        {
            ui::icon(drawList, left, row.rect.y + 6, "folder_open", palette.accent, 14.0f);
            ui::textVC(drawList, left + 20, row.rect.y, row.rect.h, row.name, palette.text, 11.5f);
            continue;
        }
        if (row.isDirectory)
        {
            ui::chevron(drawList, left + 4, row.rect.y + row.rect.h * 0.5f, 4.0f, row.expanded, palette.textFaint);
            left += 14;
            ui::icon(drawList, left, row.rect.y + 6, "folder_open", palette.textMuted, 14.0f);
            left += 20;
            ui::textClipped(drawList, left, row.rect.y + 6, row.rect.w - (left - row.rect.x) - 12, row.name,
                palette.textMuted, 11.5f);
        }
        else
        {
            std::string icon = "image";
            if (row.name.size() > 5 && row.name.substr(row.name.size() - 5) == ".json")
            {
                if (row.relative.rfind("view/", 0) == 0) icon = "description";
                else if (row.relative.rfind("tokens/", 0) == 0) icon = "tune";
                else if (row.relative == "theme.json") icon = "info";
                else icon = "palette";
            }
            ui::icon(drawList, left + 14, row.rect.y + 6, icon.c_str(),
                active ? palette.accent : palette.textFaint, 13.0f);
            ui::textClipped(drawList, left + 34, row.rect.y + 6, row.rect.w - (left - row.rect.x) - 44, row.name,
                active ? palette.text : palette.text, 11.5f);
            if (active)
                ui::fill(drawList, Rect(row.rect.x, row.rect.y + 6, 2, row.rect.h - 12), palette.accent);
        }
    }
    ui::scrollbar(drawList, Rect(area.x + area.w - 7, area.y, 7, area.h), this->app.projectScroll,
        std::max(area.h, (float)this->rows.size() * 26.0f), area.h, palette.scrollbar);
    drawList->PopClipRect();
}

void ProjectPanel::handleInput(UiInput& input)
{
    const Rect self(this->getX(), this->getY(), this->getWidth(), this->getHeight());
    if (!self.contains(input.mouse)) return;

    this->buildRows();
    if (this->addRect.contains(input.mouse))
    {
        this->app.tooltip = "新建页面 / 组件";
        if (input.pressed)
        {
            std::vector<MenuItem> items;
            MenuItem page; page.label = "新建页面"; page.action = "new_page"; page.icon = "description";
            MenuItem component; component.label = "新建组件"; component.action = "new_component"; component.icon = "widgets";
            items.push_back(page);
            items.push_back(component);
            this->app.openPopup("project:add", this->addRect, items, 170.0f);
        }
        return;
    }
    if (this->refreshRect.contains(input.mouse))
    {
        this->app.tooltip = "刷新工程文件";
        if (input.pressed)
        {
            this->app.refreshFiles();
            this->app.addToast("工程文件已刷新");
        }
        return;
    }
    if (this->searchRect.contains(input.mouse))
    {
        if (input.pressed && !(this->app.edit.active && this->app.edit.key == "search"))
            this->app.beginEdit("search", this->app.fileFilter, this->searchRect);
        return;
    }
    if (!this->listArea.contains(input.mouse)) return;

    if (input.wheel != 0.0f)
    {
        const float content = (float)this->rows.size() * 26.0f;
        this->app.projectScroll = std::clamp(this->app.projectScroll - input.wheel * 34.0f, 0.0f,
            std::max(0.0f, content - this->listArea.h));
        return;
    }

    for (const FileRow& row : this->rows)
    {
        if (!row.rect.contains(input.mouse)) continue;
        if (input.pressed)
        {
            if (row.relative.empty())
            {
                // project root: no-op beyond selection
            }
            else if (row.isDirectory)
            {
                if (this->app.expandedDirs.find(row.relative) != this->app.expandedDirs.end())
                    this->app.expandedDirs.erase(row.relative);
                else
                    this->app.expandedDirs.insert(row.relative);
            }
            else
            {
                if (row.name.size() > 5 && row.name.substr(row.name.size() - 5) == ".json")
                    this->app.openDocument(row.relative, true);
                else
                    this->app.addToast("资源文件：" + row.relative);
            }
        }
        if (input.rightPressed && !row.isDirectory && !row.relative.empty())
        {
            std::vector<MenuItem> items;
            MenuItem open; open.label = "打开"; open.action = "open_file"; open.value = row.relative; open.icon = "folder_open";
            MenuItem rename; rename.label = "重命名"; rename.action = "file_rename_ctx"; rename.value = row.relative; rename.icon = "edit";
            MenuItem remove; remove.label = "删除"; remove.action = "file_delete_ctx"; remove.value = row.relative;
            remove.icon = "delete"; remove.danger = true;
            items.push_back(open);
            items.push_back(rename);
            MenuItem separator; separator.separator = true;
            items.push_back(separator);
            items.push_back(remove);
            this->app.openPopup("project:file", Rect(input.mouse.x, input.mouse.y, 0, 0), items, 170.0f);
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// Global variables
// ---------------------------------------------------------------------------

GlobalVarsPanel::GlobalVarsPanel(DesignerApp& app)
    : DesignerPanel(app, "globals")
{
}

void GlobalVarsPanel::buildRows()
{
    this->rows.clear();
    if (!this->app.hasDoc()) return;
    Document* document = this->app.doc();
    if (document == nullptr) return;
    const TokenType filter = this->app.tabTokenType == TokenType::NUMBER ? TokenType::NUMBER
                                                                        : TokenType::COLOR;
    const float top = this->getY() + 94.0f;
    const float width = this->getWidth();
    float y = 0.0f;

    if (this->app.varScope == VarScope::LOCAL)
    {
        const Json* vars = document->vars(std::string());
        const char* group = filter == TokenType::COLOR ? "colors" : "numbers";
        if (vars != nullptr && vars->contains(group) && (*vars)[group].is_object())
        {
            std::vector<std::string> names;
            for (auto it = (*vars)[group].begin(); it != (*vars)[group].end(); ++it)
                names.push_back(it.key());
            std::sort(names.begin(), names.end());
            for (const std::string& name : names)
            {
                const Json raw = (*vars)[group][name];
                TokenRow row;
                row.local = true;
                row.type = filter;
                row.tokenPath = (filter == TokenType::COLOR ? "&color." : "&number.") + name;
                row.label = name;
                const Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                    std::string(), raw);
                row.valueText = formatValue(resolved);
                if (filter == TokenType::COLOR)
                {
                    const std::string text = resolved.is_string() ? resolved.get<std::string>() : std::string();
                    parseColor(text, row.swatch);
                }
                row.rect = Rect(this->getX(), top + y - this->app.tokenScroll, width, 26);
                this->rows.push_back(row);
                y += 26.0f;
            }
        }
    }
    else
    {
        std::vector<const Token*> tokens;
        for (const Token& token : this->app.project.tokens)
            if (token.type == filter) tokens.push_back(&token);
        std::sort(tokens.begin(), tokens.end(), [](const Token* a, const Token* b) { return a->path < b->path; });
        for (const Token* token : tokens)
        {
            TokenRow row;
            row.local = false;
            row.type = filter;
            row.tokenPath = filter == TokenType::COLOR ? ("@theme/" + token->path) : ("@style/" + token->path);
            row.label = token->path;
            row.valueText = formatValue(token->value);
            if (filter == TokenType::COLOR && token->value.is_string())
                parseColor(token->value.get<std::string>(), row.swatch);
            row.rect = Rect(this->getX(), top + y - this->app.tokenScroll, width, 26);
            this->rows.push_back(row);
            y += 26.0f;
        }
    }
    this->contentHeight = y;
    this->listArea = Rect(this->getX(), top, width,
        std::max(0.0f, this->getHeight() - 94.0f - 64.0f));
    if (!this->rows.empty())
    {
        bool selected = false;
        for (const TokenRow& row : this->rows)
            if (row.tokenPath == this->app.selectedToken) { selected = true; break; }
        if (!selected) this->app.selectToken(this->rows.front().tokenPath);
    }
}

void GlobalVarsPanel::draw(ImDrawList* drawList, float x, float y, float width, float height,
    bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    Document* document = this->app.doc();
    ui::fill(drawList, Rect(x, y, width, height), palette.panel);
    panelHeader(drawList, this->app, Rect(x, y, width, 34), "变量", "data_object");

    const bool local = this->app.varScope == VarScope::LOCAL;
    if (local)
    {
        this->addRect = Rect(x + width - 94, y + 7, 20, 20);
        this->renameRect = Rect(x + width - 66, y + 7, 20, 20);
        this->deleteRect = Rect(x + width - 38, y + 7, 20, 20);
        iconButton(drawList, this->app, this->addRect, "add", this->app.hovering(this->addRect));
        iconButton(drawList, this->app, this->renameRect, "edit", this->app.hovering(this->renameRect), false,
            this->app.selectedToken.empty());
        iconButton(drawList, this->app, this->deleteRect, "delete", this->app.hovering(this->deleteRect), false,
            this->app.selectedToken.empty());
    }
    else
    {
        this->addRect = Rect();
        this->renameRect = Rect();
        this->deleteRect = Rect();
        ui::fill(drawList, Rect(x + width - 100, y + 1, 96, 32), palette.header);
    }

    static const char* scopes[2] = { "局部变量", "全局变量" };
    const float scopeWidth = (width - 20.0f) / 2.0f;
    for (int i = 0; i < 2; ++i)
    {
        Rect rect(x + 10.0f + i * scopeWidth, y + 38.0f, scopeWidth, 24.0f);
        const bool active = (this->app.varScope == VarScope::LOCAL) == (i == 0);
        if (active)
        {
            ui::fill(drawList, rect, palette.accentSoft, 4.0f);
            ui::fill(drawList, Rect(rect.x + 8, rect.y + rect.h - 2, rect.w - 16, 2), palette.accent);
        }
        else if (this->app.hovering(rect))
            ui::fill(drawList, rect, palette.hover, 4.0f);
        ui::label(drawList, rect, scopes[i], active ? palette.text : palette.textMuted, 10.5f, 1);
        this->tabRects[i] = rect;
    }

    static const char* types[2] = { "颜色", "数值" };
    for (int i = 0; i < 2; ++i)
    {
        Rect rect(x + 10.0f + i * 58.0f, y + 66.0f, 54.0f, 22.0f);
        const bool active = (this->app.tabTokenType == TokenType::NUMBER) == (i == 1);
        if (active)
        {
            ui::fill(drawList, rect, palette.panelRaised, 4.0f);
            ui::stroke(drawList, rect, palette.border, 4.0f);
        }
        else if (this->app.hovering(rect))
            ui::fill(drawList, rect, palette.hover, 4.0f);
        ui::label(drawList, rect, types[i], active ? palette.text : palette.textMuted, 10.0f, 1);
        this->typeRects[i] = rect;
    }

    this->buildRows();
    const Rect area = this->listArea;
    drawList->PushClipRect(area.min(), area.max(), true);
    if (this->rows.empty())
    {
        ui::textClipped(drawList, x + 16, area.y + 14, width - 32,
            local ? "当前 JSON 没有这种类型的 vars 局部变量" : "主题 tokens 中没有这种类型的变量",
            palette.textFaint, 10.5f);
    }
    for (const TokenRow& row : this->rows)
    {
        if (row.rect.y + row.rect.h < area.y || row.rect.y > area.y + area.h) continue;
        const bool hover = this->app.hovering(row.rect);
        if (row.tokenPath == this->app.selectedToken)
            ui::fill(drawList, row.rect, palette.selected);
        else if (hover)
            ui::fill(drawList, row.rect, palette.hover);

        float left = row.rect.x + 10.0f;
        if (row.type == TokenType::COLOR)
        {
            Rect swatch(left, row.rect.y + 7, 12, 12);
            ui::fill(drawList, swatch, row.swatch != 0 ? row.swatch : palette.panelRaised, 3.0f);
            ui::stroke(drawList, swatch, palette.border, 3.0f);
            left += 20.0f;
        }
        else
        {
            ui::icon(drawList, left + 1, row.rect.y + 6, "tune", palette.textFaint, 13.0f);
            left += 19.0f;
        }
        const std::string valueText = row.valueText;
        const float valueWidth = std::min(96.0f, ui::textWidth(valueText, 9.5f) + 10.0f);
        ui::textClipped(drawList, left, row.rect.y + 6, row.rect.w - (left - row.rect.x) - valueWidth - 14,
            row.label, row.tokenPath == this->app.selectedToken ? palette.text : palette.textMuted, 10.5f);
        ui::textRight(drawList, row.rect.x + row.rect.w - 10, row.rect.y + 6, valueText, palette.textFaint, 9.5f);
    }
    ui::scrollbar(drawList, Rect(area.x + area.w - 7, area.y, 7, area.h), this->app.tokenScroll,
        std::max(area.h, this->contentHeight), area.h, palette.scrollbar);
    drawList->PopClipRect();

    this->detailsArea = Rect(x, y + height - 64, width, 64);
    ui::fill(drawList, this->detailsArea, palette.panelAlt);
    ui::hline(drawList, this->detailsArea.y, x, x + width, palette.border);
    std::string title;
    std::string value;
    std::string source;
    ImU32 swatch = 0;
    if (!this->app.selectedToken.empty())
    {
        const std::string& ref = this->app.selectedToken;
        if (ref.rfind("&", 0) == 0)
        {
            const bool color = ref.rfind("&color.", 0) == 0;
            const std::string name = ref.substr(color ? 7 : 8);
            const Json* raw = document != nullptr
                ? findLocalVar(document->root, color ? "color" : "number", name)
                : nullptr;
            title = ref;
            if (raw != nullptr)
            {
                const Json resolved = document != nullptr
                    ? PreviewBuilder::resolveStyleValue(this->app.project, document->root, std::string(), *raw)
                    : *raw;
                value = formatValue(resolved);
                if (color && resolved.is_string()) parseColor(resolved.get<std::string>(), swatch);
            }
            source = document != nullptr ? document->relative : std::string();
        }
        else
        {
            const std::string path = ref.rfind("@theme/", 0) == 0 ? ref.substr(7) : ref.substr(7);
            const Token* token = this->app.project.getToken(path);
            title = ref;
            if (token != nullptr)
            {
                value = formatValue(token->value);
                if (token->type == TokenType::COLOR && token->value.is_string())
                    parseColor(token->value.get<std::string>(), swatch);
                source = token->file;
            }
        }
        if (swatch != 0)
        {
            ui::fill(drawList, Rect(x + 12, y + height - 46, 18, 18), swatch, 4.0f);
            ui::stroke(drawList, Rect(x + 12, y + height - 46, 18, 18), palette.border, 4.0f);
        }
        ui::textClipped(drawList, x + 38, y + height - 48, width - 50, title, palette.text, 10.5f);
        ui::textClipped(drawList, x + 12, y + height - 26, width - 24,
            value + "  ·  " + source + "  ·  " +
                std::to_string(this->app.tokenRefCount(this->app.selectedToken)) + " 处引用",
            palette.textFaint, 9.0f);
    }
    else
    {
        ui::textVC(drawList, x + 12, y + height - 42, 18,
            local ? "选择一个局部变量查看引用" : "选择一个全局变量拖到属性行", palette.textFaint, 10.0f);
    }
}

void GlobalVarsPanel::handleInput(UiInput& input)
{
    const Rect self(this->getX(), this->getY(), this->getWidth(), this->getHeight());
    if (!self.contains(input.mouse)) return;

    if (this->app.varScope == VarScope::LOCAL)
    {
        if (this->addRect.contains(input.mouse))
        {
            this->app.tooltip = "新建局部变量";
            if (input.pressed) this->app.runAction("local_var_add");
            return;
        }
        if (this->renameRect.contains(input.mouse))
        {
            this->app.tooltip = "重命名局部变量";
            if (input.pressed) this->app.runAction("local_var_rename");
            return;
        }
        if (this->deleteRect.contains(input.mouse))
        {
            this->app.tooltip = "删除局部变量";
            if (input.pressed) this->app.runAction("local_var_delete");
            return;
        }
    }

    for (int i = 0; i < 2; ++i)
    {
        if (this->tabRects[i].contains(input.mouse) && input.pressed)
        {
            this->app.varScope = i == 0 ? VarScope::LOCAL : VarScope::GLOBAL;
            this->app.tokenScroll = 0.0f;
            this->app.selectedToken.clear();
            return;
        }
        if (this->typeRects[i].contains(input.mouse) && input.pressed)
        {
            this->app.tabTokenType = i == 0 ? TokenType::COLOR : TokenType::NUMBER;
            this->app.tokenScroll = 0.0f;
            return;
        }
    }

    this->buildRows();
    if (input.wheel != 0.0f && this->listArea.contains(input.mouse))
    {
        this->app.tokenScroll = std::clamp(this->app.tokenScroll - input.wheel * 34.0f, 0.0f,
            std::max(0.0f, this->contentHeight - this->listArea.h));
        return;
    }

    for (const TokenRow& row : this->rows)
    {
        if (!row.rect.contains(input.mouse)) continue;
        if (input.pressed)
        {
            this->app.selectToken(row.tokenPath);
            if (input.doubleClicked)
            {
                Rect valueRect(row.rect.x + row.rect.w - 110, row.rect.y + 3, 100, 20);
                if (row.type == TokenType::COLOR)
                {
                    ImU32 color = row.swatch;
                    const std::string id = row.local ? ("color:local:" + row.label) : ("color:token:" + row.tokenPath.substr(7));
                    this->app.openColorPopup(id, row.rect, color);
                }
                else
                {
                    this->app.beginEdit(row.local ? ("localval:" + row.label) : ("varval:" + row.label),
                        row.valueText, valueRect, true);
                }
            }
            else
                this->app.beginVariableDrop(row.tokenPath, row.type, row.rect);
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// Node tree (left)
// ---------------------------------------------------------------------------

NodeTreePanel::NodeTreePanel(DesignerApp& app)
    : DesignerPanel(app, "nodetree")
{
}

void NodeTreePanel::buildRows()
{
    const float top = this->getY() + 62.0f;
    this->listArea = Rect(this->getX(), top, this->getWidth(), std::max(0.0f, this->getHeight() - 62.0f));
    if (!this->app.hasDoc())
    {
        this->rows.clear();
        this->contentHeight = 0.0f;
        return;
    }
    collectVisibleRows(this->app.doc()->tree, this->app.expandedNodes, this->rows,
        this->getX(), this->listArea.y, this->getWidth() - 8, this->app.nodeTreeScroll, 25.0f, this->contentHeight);
    if (this->app.scrollNodeIntoView)
    {
        this->app.scrollNodeIntoView = false;
        for (const TreeRow& row : this->rows)
        {
            if (row.pointer != this->app.selectedNode) continue;
            if (row.contentY < this->app.nodeTreeScroll)
                this->app.nodeTreeScroll = row.contentY;
            else if (row.contentY + 25.0f > this->app.nodeTreeScroll + this->listArea.h)
                this->app.nodeTreeScroll = row.contentY + 25.0f - this->listArea.h;
            this->app.nodeTreeScroll = std::clamp(this->app.nodeTreeScroll, 0.0f,
                std::max(0.0f, this->contentHeight - this->listArea.h));
            break;
        }
    }
}

void NodeTreePanel::syncCodeFromDocument()
{
    Document* document = this->app.doc();
    if (document == nullptr)
    {
        this->app.code.text.clear();
        this->app.code.original.clear();
        return;
    }
    Json source;
    if (this->app.code.showDocument)
    {
        source = document->root;
    }
    else
    {
        const Json* node = document->node(this->app.selectedNode);
        if (node != nullptr) source = *node;
    }
    this->app.code.text = source.is_null() ? std::string() : prettyJson(source);
    this->app.code.original = this->app.code.text;
    this->app.code.caret = (int)this->app.code.text.size();
    this->app.code.error.clear();
}

void NodeTreePanel::drawCodeView(ImDrawList* drawList, const Rect& area)
{
    const Palette& palette = paletteOf(this->app);
    Document* document = this->app.doc();

    this->codeToolbar = Rect(area.x, area.y, area.w, 30);
    ui::fill(drawList, this->codeToolbar, palette.panelAlt);
    ui::hline(drawList, area.y + 29, area.x, area.x + area.w, palette.borderSoft);

    if (document == nullptr)
    {
        ui::label(drawList, area, "没有打开的文档", palette.textFaint, 10.5f, 1);
        return;
    }

    const float gap = 4.0f;
    const float buttonWidth = (area.w - 12.0f - gap * 4.0f) / 5.0f;
    float bx = area.x + 6.0f;
    const float by = area.y + 4.0f;
    Rect* rects[5] = { &this->codeNodeTab, &this->codeDocTab, &this->codeEditButton,
        &this->codeApplyButton, &this->codeRevertButton };
    static const char* labels[5] = { "节点", "文档", "编辑", "应用", "还原" };
    const bool activeFlags[5] = { !this->app.code.showDocument, this->app.code.showDocument,
        this->app.code.active, this->app.code.active, false };
    for (int i = 0; i < 5; ++i)
    {
        *rects[i] = Rect(bx, by, buttonWidth, 22.0f);
        const bool hover = this->app.hovering(*rects[i]);
        ImU32 fillColor = palette.panel;
        ImU32 textColor = palette.textMuted;
        if (activeFlags[i])
        {
            fillColor = palette.accentSoft;
            textColor = palette.text;
        }
        else if (hover)
        {
            fillColor = palette.hover;
        }
        ui::fill(drawList, *rects[i], fillColor, 4.0f);
        if (i == 2 && this->app.code.active)
            ui::stroke(drawList, *rects[i], palette.accent, 4.0f);
        ui::label(drawList, *rects[i], labels[i], textColor, 9.5f, 1);
        bx += buttonWidth + gap;
    }

    if (!this->app.code.active)
        this->syncCodeFromDocument();

    const float errorHeight = this->app.code.error.empty() ? 0.0f : 20.0f;
    this->codeArea = Rect(area.x + 4.0f, area.y + 34.0f, area.w - 8.0f,
        std::max(40.0f, area.h - 38.0f - errorHeight));
    this->app.code.area = this->codeArea;
    const float lineHeight = 15.0f;
    const float gutterWidth = 30.0f;
    const float fontSize = 10.0f;
    ui::fill(drawList, this->codeArea, palette.inputBg, 4.0f);
    ui::stroke(drawList, this->codeArea, palette.border, 4.0f);

    const std::vector<std::string> lines = [&] {
        std::vector<std::string> result;
        std::stringstream stream(this->app.code.text);
        std::string line;
        while (std::getline(stream, line)) result.push_back(line);
        if (result.empty()) result.push_back("");
        return result;
    }();

    const float maxScroll = std::max(0.0f, (float)lines.size() * lineHeight + 8.0f - this->codeArea.h);
    this->app.code.scroll = std::clamp(this->app.code.scroll, 0.0f, maxScroll);
    drawList->PushClipRect(ImVec2(this->codeArea.x + 1, this->codeArea.y + 1),
        ImVec2(this->codeArea.x + this->codeArea.w - 1, this->codeArea.y + this->codeArea.h - 1), true);
    ui::fill(drawList, Rect(this->codeArea.x, this->codeArea.y, gutterWidth, this->codeArea.h), palette.panelAlt);
    ui::vline(drawList, this->codeArea.x + gutterWidth, this->codeArea.y,
        this->codeArea.y + this->codeArea.h, palette.border);

    int caretLine = 0;
    int caretColumn = 0;
    if (this->app.code.active)
    {
        const size_t index = (size_t)std::clamp(this->app.code.caret, 0, (int)this->app.code.text.size());
        for (size_t i = 0; i < index && i < this->app.code.text.size(); ++i)
        {
            if (this->app.code.text[i] == '\n')
            {
                caretLine++;
                caretColumn = 0;
            }
            else
                caretColumn++;
        }
    }

    const int firstLine = std::max(0, (int)(this->app.code.scroll / lineHeight));
    for (int i = firstLine; i < (int)lines.size(); ++i)
    {
        const float lineY = this->codeArea.y + 4 + i * lineHeight - this->app.code.scroll;
        if (lineY > this->codeArea.y + this->codeArea.h) break;
        if (lineY + lineHeight < this->codeArea.y) continue;
        ui::textRight(drawList, this->codeArea.x + gutterWidth - 6, lineY + 1, std::to_string(i + 1),
            palette.textFaint, 8.5f);

        const std::string& line = lines[(size_t)i];
        float cursorX = this->codeArea.x + gutterWidth + 6;
        size_t index = 0;
        while (index < line.size())
        {
            const char ch = line[index];
            if (ch == '"')
            {
                size_t end = index + 1;
                while (end < line.size())
                {
                    if (line[end] == '\\') { end += 2; continue; }
                    if (line[end] == '"') { end++; break; }
                    end++;
                }
                const std::string part = line.substr(index, end - index);
                bool key = false;
                size_t look = end;
                while (look < line.size() && std::isspace((unsigned char)line[look])) look++;
                if (look < line.size() && line[look] == ':') key = true;
                ui::text(drawList, cursorX, lineY, part, key ? palette.accent : palette.teal, fontSize);
                cursorX += ui::textWidth(part, fontSize);
                index = end;
                continue;
            }
            if (std::isdigit((unsigned char)ch) || (ch == '-' && index + 1 < line.size()))
            {
                size_t end = index;
                while (end < line.size() && (std::isdigit((unsigned char)line[end]) || line[end] == '.' ||
                                                line[end] == '-' || line[end] == 'e' || line[end] == 'E'))
                    end++;
                const std::string part = line.substr(index, end - index);
                ui::text(drawList, cursorX, lineY, part, palette.yellow, fontSize);
                cursorX += ui::textWidth(part, fontSize);
                index = end;
                continue;
            }
            if (line.compare(index, 4, "true") == 0 || line.compare(index, 5, "false") == 0 ||
                line.compare(index, 4, "null") == 0)
            {
                const size_t length = line.compare(index, 4, "true") == 0 || line.compare(index, 4, "null") == 0 ? 4 : 5;
                const std::string part = line.substr(index, length);
                ui::text(drawList, cursorX, lineY, part, palette.yellow, fontSize);
                cursorX += ui::textWidth(part, fontSize);
                index += length;
                continue;
            }
            size_t length = 1;
            const unsigned char lead = (unsigned char)ch;
            if ((lead & 0xE0) == 0xC0) length = 2;
            else if ((lead & 0xF0) == 0xE0) length = 3;
            else if ((lead & 0xF8) == 0xF0) length = 4;
            if (index + length > line.size()) length = 1;
            const std::string part = line.substr(index, length);
            ui::text(drawList, cursorX, lineY, part, palette.text, fontSize);
            cursorX += ui::textWidth(part, fontSize);
            index += length;
        }

        if (this->app.code.active && i == caretLine)
        {
            const float caretX = this->codeArea.x + gutterWidth + 6 +
                ui::textWidth(line.substr(0, (size_t)std::min((int)line.size(), caretColumn)), fontSize);
            ui::fill(drawList, Rect(caretX, lineY, 1.0f, lineHeight - 2), palette.accent);
        }
    }
    drawList->PopClipRect();
    ui::scrollbar(drawList, Rect(this->codeArea.x + this->codeArea.w - 7, this->codeArea.y, 7, this->codeArea.h),
        this->app.code.scroll, std::max(this->codeArea.h, (float)lines.size() * lineHeight + 8.0f),
        this->codeArea.h, palette.scrollbar);

    if (!this->app.code.error.empty())
    {
        Rect errorRect(area.x + 4, area.y + area.h - 20, area.w - 8, 18);
        ui::fill(drawList, errorRect, uiAlpha(palette.red, 0.16f), 4.0f);
        ui::textClipped(drawList, errorRect.x + 6, errorRect.y + 2, errorRect.w - 12, this->app.code.error,
            palette.red, 9.0f);
    }
}

void NodeTreePanel::draw(ImDrawList* drawList, float x, float y, float width, float height,
    bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    ui::fill(drawList, Rect(x, y, width, height), palette.panel);

    Rect header(x, y, width, 34);
    panelHeader(drawList, this->app, header, "结构", this->tab == 0 ? "layers" : "description");

    const bool appearanceDocument = this->app.hasDoc() &&
        !this->app.doc()->root.contains("type") &&
        (this->app.doc()->root.contains("layers") || this->app.doc()->root.contains("box") ||
            this->app.doc()->root.contains("import"));
    const bool structuralDocument = !this->app.hasDoc() ||
        this->app.doc()->root.contains("type") || appearanceDocument;
    if (this->tab == 0 && structuralDocument)
    {
        this->addRect = Rect(x + width - 94, y + 7, 20, 20);
        this->duplicateRect = Rect(x + width - 66, y + 7, 20, 20);
        this->deleteRect = Rect(x + width - 38, y + 7, 20, 20);
        iconButton(drawList, this->app, this->addRect, "add", this->app.hovering(addRect));
        iconButton(drawList, this->app, this->duplicateRect, "content_copy",
            this->app.hovering(duplicateRect), false,
            !this->app.hasDoc() || this->app.selectedNode.empty());
        iconButton(drawList, this->app, this->deleteRect, "delete",
            this->app.hovering(deleteRect), false,
            !this->app.hasDoc() || this->app.selectedNode.empty());
    }

    static const char* tabLabels[2] = { "节点树", "代码预览" };
    const float tabWidth = (width - 16.0f) / 2.0f;
    for (int i = 0; i < 2; ++i)
    {
        Rect rect(x + 8.0f + i * tabWidth, y + 34.0f, tabWidth, 26.0f);
        this->tabRects[i] = rect;
        const bool active = this->tab == i;
        if (active)
        {
            ui::fill(drawList, Rect(rect.x + 14, rect.y + rect.h - 2, rect.w - 28, 2), palette.accent);
        }
        else if (this->app.hovering(rect))
        {
            ui::fill(drawList, rect, palette.hover, 4.0f);
        }
        ui::label(drawList, rect, tabLabels[i], active ? palette.text : palette.textMuted, 10.5f, 1);
    }
    ui::hline(drawList, y + 60, x, x + width, palette.borderSoft);

    if (this->tab == 1)
    {
        this->drawCodeView(drawList, Rect(x, y + 61, width, std::max(0.0f, height - 61)));
        return;
    }

    if (!this->app.hasDoc())
    {
        ui::iconCentered(drawList, Rect(x, y + 92, width, 24), "layers", palette.textFaint, 20.0f);
        ui::textClipped(drawList, x + 20, y + 128, width - 40, "打开一个页面或组件后显示节点层级",
            palette.textFaint, 10.5f);
        return;
    }

    const Json& root = this->app.doc()->root;
    if (root.contains("include"))
    {
        ui::iconCentered(drawList, Rect(x, y + 92, width, 24), "description", palette.textFaint, 20.0f);
        ui::textClipped(drawList, x + 20, y + 128, width - 40, "include 已在画布展开",
            palette.textFaint, 10.5f);
        ui::textClipped(drawList, x + 20, y + 148, width - 40, "节点树暂不展开 include",
            palette.textFaint, 9.5f);
        return;
    }
    if (!root.contains("type") && !root.contains("layers") && !root.contains("box") &&
        !root.contains("import"))
    {
        ui::iconCentered(drawList, Rect(x, y + 92, width, 24), "description", palette.textFaint, 20.0f);
        ui::textClipped(drawList, x + 20, y + 128, width - 40, "该 JSON 没有节点树",
            palette.textFaint, 10.5f);
        ui::textClipped(drawList, x + 20, y + 148, width - 40, "主题/变量文件请使用代码页",
            palette.textFaint, 9.5f);
        return;
    }

    this->buildRows();
    drawList->PushClipRect(this->listArea.min(), this->listArea.max(), true);
    for (const TreeRow& row : this->rows)
    {
        if (row.rect.y + row.rect.h < this->listArea.y || row.rect.y > this->listArea.y + this->listArea.h)
            continue;
        const bool imported = !row.sourceFile.empty();
        const bool hover = this->app.hovering(row.rect);
        const bool drop = !imported && this->app.nodeDrag.active &&
            this->app.nodeDrag.dropTarget == row.pointer;
        drawTreeRow(drawList, palette, row,
            !imported && row.pointer == this->app.selectedNode, hover, drop,
            this->app.nodeDrag.dropMode);
    }
    ui::scrollbar(drawList, Rect(this->listArea.x + this->listArea.w - 7, this->listArea.y, 7, this->listArea.h),
        this->app.nodeTreeScroll, std::max(this->listArea.h, this->contentHeight), this->listArea.h,
        palette.scrollbar);
    drawList->PopClipRect();
}

void NodeTreePanel::handleInput(UiInput& input)
{
    const Rect self(this->getX(), this->getY(), this->getWidth(), this->getHeight());
    if (!self.contains(input.mouse)) return;

    for (int i = 0; i < 2; ++i)
    {
        if (this->tabRects[i].contains(input.mouse) && input.pressed)
        {
            if (this->tab != i)
            {
                this->tab = i;
                if (i == 1)
                {
                    this->app.code.active = false;
                    this->app.code.error.clear();
                    this->app.code.scroll = 0.0f;
                    this->syncCodeFromDocument();
                }
                else
                {
                    if (this->app.code.active)
                    {
                        this->app.code.active = false;
                        this->app.code.error.clear();
                    }
                }
            }
            return;
        }
    }

    if (this->tab == 1)
    {
        this->handleCodeInput(input, Rect(this->getX(), this->getY() + 61, this->getWidth(),
            std::max(0.0f, this->getHeight() - 61)));
        return;
    }

    if (this->addRect.contains(input.mouse))
    {
        this->app.tooltip = "添加子节点";
        if (input.pressed)
        {
            std::vector<MenuItem> items;
            Document* document = this->app.doc();
            const bool appearance = document != nullptr && !document->root.contains("type") &&
                (document->root.contains("layers") || document->root.contains("box") ||
                    document->root.contains("import"));
            if (appearance)
            {
                const auto add = [&](const char* label, const char* type, const char* icon) {
                    MenuItem item;
                    item.label = label;
                    item.action = "appearance_add";
                    item.value = type;
                    item.icon = icon;
                    items.push_back(item);
                };
                add("矩形层", "rect", "crop_square");
                add("文本层", "text", "text_fields");
                add("图片层", "image", "image");
                add("阴影层", "shadow", "layers");
                add("玻璃层", "glass", "auto_awesome");
                add("自定义层", "custom", "build");
                MenuItem separator;
                separator.separator = true;
                items.push_back(separator);
                MenuItem import;
                import.label = "导入外观…";
                import.action = "appearance_import_pick";
                import.icon = "file_download";
                items.push_back(import);
                this->app.openPopup("addnode", this->addRect, items, 190.0f);
            }
            else
            {
                MenuItem box; box.label = "容器 Box"; box.action = "node_add_box"; box.icon = "crop_square";
                MenuItem label; label.label = "文本 Label"; label.action = "node_add_label"; label.icon = "text_fields";
                MenuItem image; image.label = "图片 Image"; image.action = "node_add_image"; image.icon = "image";
                MenuItem button; button.label = "按钮 Button"; button.action = "node_add_button"; button.icon = "widgets";
                items.push_back(box);
                items.push_back(label);
                items.push_back(image);
                items.push_back(button);
                this->app.openPopup("addnode", this->addRect, items, 170.0f);
            }
        }
        return;
    }
    if (this->duplicateRect.contains(input.mouse))
    {
        this->app.tooltip = "复制节点";
        if (input.pressed) this->app.duplicateSelected();
        return;
    }
    if (this->deleteRect.contains(input.mouse))
    {
        this->app.tooltip = "删除节点";
        if (input.pressed) this->app.deleteSelected();
        return;
    }

    this->buildRows();
    if (input.wheel != 0.0f && this->listArea.contains(input.mouse))
    {
        this->app.nodeTreeScroll = std::clamp(this->app.nodeTreeScroll - input.wheel * 34.0f, 0.0f,
            std::max(0.0f, this->contentHeight - this->listArea.h));
        return;
    }

    for (const TreeRow& row : this->rows)
    {
        if (!row.rect.contains(input.mouse)) continue;
        const bool imported = !row.sourceFile.empty();
        if (input.pressed)
        {
            const float chevronX = row.rect.x + 10 + row.depth * 14.0f;
            if (row.hasChildren && input.mouse.x < chevronX + 14)
            {
                if (this->app.expandedNodes.find(row.pointer) != this->app.expandedNodes.end())
                    this->app.expandedNodes.erase(row.pointer);
                else
                    this->app.expandedNodes.insert(row.pointer);
            }
            else if (imported)
            {
                this->app.tooltip = "打开导入源文件";
                this->app.openImportedNode(row.sourceFile, row.sourcePointer);
            }
            else
            {
                this->app.selectNode(row.pointer, false);
                this->app.nodeDrag.pressed = true;
                this->app.nodeDrag.active = false;
                this->app.nodeDrag.start = input.mouse;
                this->app.nodeDrag.pointer = row.pointer;
                this->app.nodeDrag.dropTarget.clear();
                this->app.nodeDrag.dropMode = 0;
            }
        }
        if (this->app.nodeDrag.active && !imported)
        {
            this->app.nodeDrag.dropTarget = row.pointer;
            const float local = (input.mouse.y - row.rect.y) / row.rect.h;
            if (row.hasChildren && local > 0.28f && local < 0.72f)
                this->app.nodeDrag.dropMode = 1;
            else
                this->app.nodeDrag.dropMode = local < 0.5f ? 0 : 2;
        }
        return;
    }
    if (input.released && this->app.nodeDrag.active)
    {
        this->app.nodeDrag.dropTarget.clear();
    }
}

void NodeTreePanel::handleCodeInput(UiInput& input, const Rect& area)
{
    (void)area;
    Document* document = this->app.doc();
    if (document == nullptr) return;

    if (this->codeNodeTab.contains(input.mouse))
    {
        this->app.tooltip = "查看当前节点";
        if (input.pressed)
        {
            this->app.code.showDocument = false;
            this->app.code.active = false;
            this->app.code.error.clear();
            this->syncCodeFromDocument();
        }
        return;
    }
    if (this->codeDocTab.contains(input.mouse))
    {
        this->app.tooltip = "查看整篇文档";
        if (input.pressed)
        {
            this->app.code.showDocument = true;
            this->app.code.active = false;
            this->app.code.error.clear();
            this->syncCodeFromDocument();
        }
        return;
    }
    if (this->codeEditButton.contains(input.mouse))
    {
        this->app.tooltip = this->app.code.active ? "完成编辑" : "编辑 JSON";
        if (input.pressed)
        {
            if (!this->app.code.active)
            {
                if (this->app.edit.active)
                    this->app.commitEdit(true);
                this->syncCodeFromDocument();
            }
            this->app.code.active = !this->app.code.active;
            this->app.code.error.clear();
            this->app.code.caret = (int)this->app.code.text.size();
        }
        return;
    }
    if (this->codeApplyButton.contains(input.mouse))
    {
        this->app.tooltip = "应用修改";
        if (input.pressed) this->applyJsonCode();
        return;
    }
    if (this->codeRevertButton.contains(input.mouse))
    {
        this->app.tooltip = "还原修改";
        if (input.pressed)
        {
            this->app.code.text = this->app.code.original;
            this->app.code.caret = (int)this->app.code.text.size();
            this->app.code.error.clear();
        }
        return;
    }

    if (input.wheel != 0.0f && this->codeArea.contains(input.mouse))
    {
        const float lineHeight = 15.0f;
        const float lines = (float)std::count(this->app.code.text.begin(), this->app.code.text.end(), '\n') + 1.0f;
        this->app.code.scroll = std::clamp(this->app.code.scroll - input.wheel * 44.0f, 0.0f,
            std::max(0.0f, lines * lineHeight + 8.0f - this->codeArea.h));
        return;
    }

    if (input.pressed && this->codeArea.contains(input.mouse))
    {
        if (!this->app.code.active) return;
        const float lineHeight = 15.0f;
        const float localY = input.mouse.y - (this->codeArea.y + 4) + this->app.code.scroll;
        const int lineIndex = std::max(0, (int)(localY / lineHeight));
        const std::string& text = this->app.code.text;
        int start = 0;
        int current = 0;
        for (size_t i = 0; i <= text.size(); ++i)
        {
            if (i == text.size() || text[i] == '\n')
            {
                if (current == lineIndex)
                {
                    const std::string content = text.substr((size_t)start, i - (size_t)start);
                    const float targetX = input.mouse.x - (this->codeArea.x + 30 + 6);
                    int best = 0;
                    float bestDistance = 1e9f;
                    for (int c = 0; c <= (int)content.size(); ++c)
                    {
                        const float width = ui::textWidth(content.substr(0, (size_t)c), 10.0f);
                        const float distance = std::fabs(width - targetX);
                        if (distance < bestDistance)
                        {
                            bestDistance = distance;
                            best = c;
                        }
                    }
                    this->app.code.caret = start + best;
                    return;
                }
                current++;
                start = (int)i + 1;
            }
        }
        return;
    }

    if (this->app.code.active)
        this->handleCodeKeys(input);
}

void NodeTreePanel::handleCodeKeys(UiInput& input)
{
    (void)input;
    ImGuiIO& io = ImGui::GetIO();
    std::string& text = this->app.code.text;
    int& caret = this->app.code.caret;
    const bool command = io.KeySuper || io.KeyCtrl;

    if (command && ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        const char* clipboard = ImGui::GetClipboardText();
        if (clipboard != nullptr)
        {
            const std::string paste(clipboard);
            text.insert((size_t)caret, paste);
            caret += (int)paste.size();
        }
    }
    if (command && ImGui::IsKeyPressed(ImGuiKey_C, false))
        ImGui::SetClipboardText(text.c_str());
    if (command && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
    {
        this->applyJsonCode();
        return;
    }

    for (ImWchar character : io.InputQueueCharacters)
    {
        if (character < 32) continue;
        std::string utf8;
        if (character < 0x80) utf8.push_back((char)character);
        else if (character < 0x800)
        {
            utf8.push_back((char)(0xC0 | (character >> 6)));
            utf8.push_back((char)(0x80 | (character & 0x3F)));
        }
        else
        {
            utf8.push_back((char)(0xE0 | (character >> 12)));
            utf8.push_back((char)(0x80 | ((character >> 6) & 0x3F)));
            utf8.push_back((char)(0x80 | (character & 0x3F)));
        }
        text.insert((size_t)caret, utf8);
        caret += (int)utf8.size();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true) && caret > 0)
    {
        text.erase((size_t)caret - 1, 1);
        caret--;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, true) && caret < (int)text.size())
        text.erase((size_t)caret, 1);
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false))
    {
        text.insert((size_t)caret, "\n");
        caret++;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Tab, false))
    {
        text.insert((size_t)caret, "  ");
        caret += 2;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true) && caret > 0) caret--;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) && caret < (int)text.size()) caret++;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
    {
        int lineStart = caret;
        while (lineStart > 0 && text[(size_t)lineStart - 1] != '\n') lineStart--;
        if (lineStart > 0)
        {
            int prevStart = lineStart - 1;
            while (prevStart > 0 && text[(size_t)prevStart - 1] != '\n') prevStart--;
            const int column = caret - lineStart;
            int target = prevStart + column;
            while (target < lineStart - 1 && target < (int)text.size() && text[(size_t)target] != '\n') target++;
            caret = std::min(target, lineStart - 1);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
    {
        int lineStart = caret;
        while (lineStart > 0 && text[(size_t)lineStart - 1] != '\n') lineStart--;
        int lineEnd = caret;
        while (lineEnd < (int)text.size() && text[(size_t)lineEnd] != '\n') lineEnd++;
        if (lineEnd < (int)text.size())
        {
            const int column = caret - lineStart;
            int nextStart = lineEnd + 1;
            int nextEnd = nextStart;
            while (nextEnd < (int)text.size() && text[(size_t)nextEnd] != '\n') nextEnd++;
            caret = std::min(nextStart + column, nextEnd);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
    {
        while (caret > 0 && text[(size_t)caret - 1] != '\n') caret--;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_End, false))
    {
        while (caret < (int)text.size() && text[(size_t)caret] != '\n') caret++;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        this->app.code.active = false;
}

void NodeTreePanel::setTab(int value)
{
    this->tab = value == 1 ? 1 : 0;
    if (this->tab == 1)
    {
        this->app.code.active = false;
        this->app.code.error.clear();
        this->app.code.scroll = 0.0f;
        this->syncCodeFromDocument();
    }
    else if (this->app.code.active)
    {
        this->app.code.active = false;
        this->app.code.error.clear();
    }
}

void NodeTreePanel::applyJsonCode()
{
    Document* document = this->app.doc();
    if (document == nullptr || !this->app.code.active) return;
    Json parsed;
    try
    {
        parsed = Json::parse(this->app.code.text);
    }
    catch (const std::exception& error)
    {
        this->app.code.error = std::string("JSON 错误：") + error.what();
        return;
    }
    if (!parsed.is_object())
    {
        this->app.code.error = "文档根节点必须是 JSON 对象";
        return;
    }
    document->pushUndo();
    if (this->app.code.showDocument)
    {
        document->root = parsed;
    }
    else
    {
        Json* node = document->node(this->app.selectedNode);
        if (node == nullptr)
        {
            this->app.code.error = "选中节点已不存在";
            return;
        }
        *node = parsed;
    }
    document->dirty = true;
    document->rebuild();
    if (document->node(this->app.selectedNode) == nullptr)
        this->app.selectedNode.clear();
    if (this->app.selectedNode.empty())
    {
        this->app.expandedNodes.insert(std::string());
    }
    this->app.markDocumentChanged();
    this->app.code.active = false;
    this->app.code.dirty = false;
    this->app.code.error.clear();
    this->syncCodeFromDocument();
    this->app.addToast("JSON 已应用");
}

// ---------------------------------------------------------------------------
// Document tabs
// ---------------------------------------------------------------------------

DocTabsPanel::DocTabsPanel(DesignerApp& app)
    : DesignerPanel(app, "tabs")
{
}

void DocTabsPanel::draw(ImDrawList* drawList, float x, float y, float width, float height,
    bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    ui::fill(drawList, Rect(x, y, width, height), palette.header);
    ui::hline(drawList, y + height - 1, x, x + width, palette.border);

    this->tabs.clear();
    float cursor = x;
    for (size_t i = 0; i < this->app.documents.size(); ++i)
    {
        const Document& document = *this->app.documents[i];
        const float tabWidth = std::min(190.0f, std::max(112.0f, ui::textWidth(document.title, 11.5f) + 62.0f));
        Rect tab(cursor, y, tabWidth, height);
        this->tabs.push_back({ tab, Rect(tab.x + tab.w - 24, y + 8, 18, 18), (int)i });
        const bool active = (int)i == this->app.activeDoc;
        if (active)
        {
            ui::fill(drawList, tab, palette.panel);
            ui::fill(drawList, Rect(tab.x, tab.y, tab.w, 2), palette.accent);
        }
        else if (this->app.hovering(tab))
        {
            ui::fill(drawList, tab, palette.panelAlt);
        }
        ui::vline(drawList, tab.x + tab.w, tab.y + 6, tab.y + tab.h - 6, palette.borderSoft);
        ui::icon(drawList, tab.x + 10, tab.y + 9, "description", active ? palette.accent : palette.textFaint, 14.0f);
        const float nameWidth = tab.w - 48;
        ui::textClipped(drawList, tab.x + 30, tab.y + 9, nameWidth, document.title,
            active ? palette.text : palette.textMuted, 11.5f);
        if (document.dirty)
            ui::dot(drawList, tab.x + tab.w - 32, tab.y + height * 0.5f, 3.0f, palette.yellow);
        const Rect close = this->tabs.back().closeRect;
        if (this->app.hovering(close))
            ui::fill(drawList, close, palette.hover, 4.0f);
        ui::iconCentered(drawList, close, "close", this->app.hovering(close) ? palette.text : palette.textFaint,
            11.0f);
        cursor += tabWidth;
        if (cursor > x + width - 40) break;
    }

    if (this->app.documents.empty())
        ui::textVC(drawList, x + 14, y, height, "未打开文档", palette.textFaint, 11.0f);
}

void DocTabsPanel::handleInput(UiInput& input)
{
    if (!Rect(this->getX(), this->getY(), this->getWidth(), this->getHeight()).contains(input.mouse)) return;
    for (const TabRect& tab : this->tabs)
    {
        if (input.pressed && tab.closeRect.contains(input.mouse))
        {
            this->app.closeDocument(tab.index);
            return;
        }
        if (input.pressed && tab.rect.contains(input.mouse))
        {
            this->app.switchDocument(tab.index);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Canvas toolbar
// ---------------------------------------------------------------------------

CanvasToolbarPanel::CanvasToolbarPanel(DesignerApp& app)
    : DesignerPanel(app, "toolbar")
{
}

void CanvasToolbarPanel::draw(ImDrawList* drawList, float x, float y, float width, float height,
    bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    ui::fill(drawList, Rect(x, y, width, height), palette.panelAlt);
    ui::hline(drawList, y + height - 1, x, x + width, palette.border);

    float cursor = x + 12;
    this->modeDesign = Rect(cursor, y + 7, 46, 24);
    this->modePreview = Rect(cursor + 48, y + 7, 46, 24);
    drawSegment(drawList, this->app, this->modeDesign, "设计", !this->app.canvasPreviewMode,
        this->app.hovering(modeDesign));
    drawSegment(drawList, this->app, this->modePreview, "预览", this->app.canvasPreviewMode,
        this->app.hovering(modePreview));

    cursor += 106;
    ui::vline(drawList, cursor, y + 9, y + height - 9, palette.border);
    cursor += 10;

    this->ratioRect = Rect(cursor, y + 7, 52, 24);
    ui::fill(drawList, this->ratioRect, this->app.hovering(ratioRect) ? palette.hover : palette.inputBg,
        5.0f);
    ui::textVC(drawList, this->ratioRect.x + 8, this->ratioRect.y, this->ratioRect.h,
        ratioLabel(this->app.canvasResolutionW, this->app.canvasResolutionH), palette.textMuted, 11.0f);
    ui::chevron(drawList, this->ratioRect.x + 40, this->ratioRect.y + 12, 3.5f, true, palette.textFaint);

    cursor += 60;
    this->resolutionRect = Rect(cursor, y + 7, 108, 24);
    ui::fill(drawList, this->resolutionRect,
        this->app.hovering(resolutionRect) ? palette.hover : palette.inputBg, 5.0f);
    ui::textVC(drawList, this->resolutionRect.x + 8, this->resolutionRect.y, this->resolutionRect.h,
        std::to_string(this->app.canvasResolutionW) + " × " + std::to_string(this->app.canvasResolutionH),
        palette.text, 11.0f);
    ui::chevron(drawList, this->resolutionRect.x + 96, this->resolutionRect.y + 12, 3.5f, true, palette.textFaint);

    cursor += 120;
    ui::vline(drawList, cursor, y + 9, y + height - 9, palette.border);
    cursor += 8;
    this->zoomOut = Rect(cursor, y + 7, 24, 24);
    this->zoomValueRect = Rect(cursor + 26, y + 7, 54, 24);
    this->zoomIn = Rect(cursor + 82, y + 7, 24, 24);
    iconButton(drawList, this->app, this->zoomOut, "zoom_out", this->app.hovering(zoomOut));
    ui::fill(drawList, this->zoomValueRect, this->app.hovering(zoomValueRect) ? palette.hover : palette.inputBg,
        5.0f);
    ui::label(drawList, this->zoomValueRect, formatZoom(this->app.canvasZoom), palette.text, 11.0f, 1);
    iconButton(drawList, this->app, this->zoomIn, "zoom_in", this->app.hovering(zoomIn));

    cursor += 112;
    this->fitRect = Rect(cursor, y + 7, 24, 24);
    this->gridRect = Rect(cursor + 30, y + 7, 24, 24);
    iconButton(drawList, this->app, this->fitRect, "center_focus_strong",
        this->app.hovering(fitRect));
    iconButton(drawList, this->app, this->gridRect, "grid_on", this->app.hovering(gridRect),
        this->app.showGrid);

    if (width > 860.0f)
    {
        const std::string info = this->app.hasDoc() ? (this->app.doc()->relative + "  ·  bklv 真实渲染")
                                                    : "bklv 真实渲染";
        ui::textRight(drawList, x + width - 14, y + 12, info, palette.textFaint, 10.5f);
    }
}

void CanvasToolbarPanel::handleInput(UiInput& input)
{
    if (!Rect(this->getX(), this->getY(), this->getWidth(), this->getHeight()).contains(input.mouse)) return;

    auto openResolution = [this] {
        std::vector<MenuItem> items;
        const std::pair<const char*, const char*> presets[] = {
            { "1280 × 720", "1280x720" }, { "1920 × 1080", "1920x1080" }, { "1440 × 900", "1440x900" },
            { "1024 × 768", "1024x768" }, { "390 × 844", "390x844" }
        };
        for (const auto& preset : presets)
        {
            MenuItem item;
            item.label = preset.first;
            item.action = "canvas_resolution";
            item.value = preset.second;
            item.checked = this->app.canvasResolutionW == std::stoi(std::string(preset.second).substr(0, std::string(preset.second).find('x')));
            items.push_back(item);
        }
        this->app.openPopup("resolution", this->resolutionRect, items, 160.0f);
    };

    if (this->modeDesign.contains(input.mouse))
    {
        this->app.tooltip = "设计模式";
        if (input.pressed && this->app.canvasPreviewMode) this->app.runAction("toggle_preview");
        return;
    }
    if (this->modePreview.contains(input.mouse))
    {
        this->app.tooltip = "预览模式（隐藏选中框）";
        if (input.pressed && !this->app.canvasPreviewMode) this->app.runAction("toggle_preview");
        return;
    }
    if (this->ratioRect.contains(input.mouse))
    {
        this->app.tooltip = "画布比例";
        if (input.pressed)
        {
            std::vector<MenuItem> items;
            const char* ratios[] = { "16:9", "16:10", "4:3", "9:16" };
            for (const char* ratio : ratios)
            {
                MenuItem item;
                item.label = ratio;
                item.action = "canvas_ratio";
                item.value = ratio;
                items.push_back(item);
            }
            this->app.openPopup("ratio", this->ratioRect, items, 120.0f);
        }
        return;
    }
    if (this->resolutionRect.contains(input.mouse))
    {
        this->app.tooltip = "目标分辨率";
        if (input.pressed) openResolution();
        return;
    }
    if (this->zoomOut.contains(input.mouse))
    {
        this->app.tooltip = "缩小";
        if (input.pressed) this->app.runAction("zoom_out");
        return;
    }
    if (this->zoomIn.contains(input.mouse))
    {
        this->app.tooltip = "放大";
        if (input.pressed) this->app.runAction("zoom_in");
        return;
    }
    if (this->zoomValueRect.contains(input.mouse))
    {
        this->app.tooltip = "重置为 100%";
        if (input.pressed) this->app.runAction("zoom_100");
        return;
    }
    if (this->fitRect.contains(input.mouse))
    {
        this->app.tooltip = "适配画布  ⌘0";
        if (input.pressed) this->app.fitCanvas();
        return;
    }
    if (this->gridRect.contains(input.mouse))
    {
        this->app.tooltip = "显示网格";
        if (input.pressed) this->app.runAction("toggle_grid");
        return;
    }
}

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------

CanvasPanel::CanvasPanel(DesignerApp& app)
    : bklv::Box(bklv::Axis::COLUMN)
    , app(app)
{
    this->setJustifyContent(bklv::JustifyContent::CENTER);
    this->setAlignItems(bklv::AlignItems::CENTER);
    this->setClipsToBounds(true);
    this->setId("canvas");
}

float CanvasPanel::fitZoom() const
{
    const float width = this->getWidth();
    const float height = this->getHeight();
    if (width < 40.0f || height < 40.0f)
        return this->app.canvasZoom;

    float designW = (float)this->app.canvasResolutionW;
    float designH = (float)this->app.canvasResolutionH;
    bool component = false;
    bool appearance = false;
    Document* document = this->app.doc();
    if (document != nullptr && document->relative.rfind("view/components/", 0) == 0)
    {
        component = true;
        const Json* root = document->node(std::string());
        if (root != nullptr && root->is_object())
        {
            if (root->contains("width") && (*root)["width"].is_number())
                designW = std::max(16.0f, (*root)["width"].get<float>());
            if (root->contains("height") && (*root)["height"].is_number())
                designH = std::max(16.0f, (*root)["height"].get<float>());
        }
    }
    else if (document != nullptr && !document->root.contains("type") &&
        (document->root.contains("layers") || document->root.contains("box") ||
            document->root.contains("import")))
    {
        appearance = true;
        const Json* root = document->node(std::string());
        if (root != nullptr && root->is_object() && root->contains("box") &&
            (*root)["box"].is_object())
        {
            const Json& box = (*root)["box"];
            if (box.contains("width") && box["width"].is_number())
                designW = std::max(16.0f, box["width"].get<float>());
            if (box.contains("height") && box["height"].is_number())
                designH = std::max(16.0f, box["height"].get<float>());
        }
        if (!root->contains("box") || !(*root)["box"].contains("width")) designW = 320.0f;
        if (!root->contains("box") || !(*root)["box"].contains("height")) designH = 160.0f;
    }
    const float fitW = (width - 36.0f) / std::max(1.0f, designW);
    const float fitH = (height - 30.0f) / std::max(1.0f, designH);
    return std::clamp(std::min(fitW, fitH), 0.12f,
        component ? 3.0f : 2.0f);
}

void CanvasPanel::clearPreview()
{
    this->app.previewError.clear();
    this->app.previewWarning.clear();
    if (this->previewRootView != nullptr)
    {
        this->removeView(this->previewRootView, true);
        this->previewRootView = nullptr;
    }
    this->preview = PreviewResult();
    this->builtDoc.clear();
    this->builtZoom = -1.0f;
    this->builtRevision = -1;
}

void CanvasPanel::rebuildPreview()
{
    const float panelW = this->getWidth();
    const float panelH = this->getHeight();
    if (panelW < 20.0f || panelH < 20.0f) return;

    if (this->app.canvasFit || panelW != this->lastPanelW || panelH != this->lastPanelH)
    {
        const float fit = this->fitZoom();
        this->lastPanelW = panelW;
        this->lastPanelH = panelH;
        if (std::fabs(fit - this->app.canvasZoom) > 0.001f)
        {
            this->app.canvasZoom = fit;
            this->app.previewDirty = true;
        }
    }

    Document* document = this->app.doc();
    if (document == nullptr)
    {
        if (this->previewRootView != nullptr) this->clearPreview();
        this->app.previewError.clear();
        this->app.previewWarning.clear();
        return;
    }

    const bool viewLike = document->root.contains("type") || document->root.contains("include");
    const bool appearance = !viewLike &&
        (document->root.contains("layers") || document->root.contains("box") ||
            document->root.contains("import"));
    if (!viewLike && !appearance)
    {
        this->clearPreview();
        this->app.previewError.clear();
        this->app.previewWarning = "该 JSON 不是视图/外观文档，画布显示代码预览。";
        this->builtDoc = document->relative;
        this->builtZoom = this->app.canvasZoom;
        this->builtW = this->app.canvasResolutionW;
        this->builtH = this->app.canvasResolutionH;
        this->builtRevision = this->app.documentRevision;
        this->app.previewDirty = false;
        return;
    }

    const bool needsBuild = this->app.previewDirty || this->previewRootView == nullptr ||
        this->builtDoc != document->relative || std::fabs(this->builtZoom - this->app.canvasZoom) > 0.001f ||
        this->builtW != this->app.canvasResolutionW || this->builtH != this->app.canvasResolutionH ||
        this->builtRevision != this->app.documentRevision;

    if (needsBuild)
    {
        this->clearPreview();
        const bool component = document->relative.rfind("view/components/", 0) == 0;
        const float resolutionScale = (component || appearance) ? 1.0f
            : ((float)this->app.canvasResolutionW / std::max(1, this->app.project.canvasWidth));
        PreviewResult result = PreviewBuilder::build(this->app.project, document->root,
            this->app.canvasZoom * resolutionScale);
        this->preview = std::move(result);
        if (this->preview.root != nullptr)
        {
            this->previewRootView = this->preview.root;
            bklv::View* root = this->previewRootView;
            root->setGrow(0.0f);
            root->setTranslation(this->app.canvasPanX, this->app.canvasPanY);
            this->addView(root);
            this->builtPanX = this->app.canvasPanX;
            this->builtPanY = this->app.canvasPanY;
            this->app.previewError = this->preview.error;
            this->app.previewWarning.clear();
            for (const std::string& warning : this->preview.warnings)
            {
                if (!this->app.previewWarning.empty()) this->app.previewWarning += "  ·  ";
                this->app.previewWarning += warning;
            }
        }
        else
        {
            this->previewRootView = nullptr;
            this->app.previewError = this->preview.error.empty() ? "无法渲染当前文档" : this->preview.error;
            this->app.previewWarning.clear();
            this->app.addToast(this->app.previewError, true);
        }
        this->builtDoc = document->relative;
        this->builtZoom = this->app.canvasZoom;
        this->builtW = this->app.canvasResolutionW;
        this->builtH = this->app.canvasResolutionH;
        this->builtRevision = this->app.documentRevision;
        this->app.previewDirty = false;
    }

    if (this->previewRootView != nullptr &&
        (std::fabs(this->builtPanX - this->app.canvasPanX) > 0.01f ||
            std::fabs(this->builtPanY - this->app.canvasPanY) > 0.01f))
    {
        this->builtPanX = this->app.canvasPanX;
        this->builtPanY = this->app.canvasPanY;
        this->previewRootView->setTranslation(this->app.canvasPanX, this->app.canvasPanY);
    }
}

void CanvasPanel::draw(ImDrawList* drawList, float x, float y, float width, float height, bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    this->canvasArea = Rect(x, y, width, height);
    ui::fill(drawList, this->canvasArea, palette.canvas);

    if (this->app.showGrid)
    {
        const float step = 28.0f;
        for (float gx = x + step; gx < x + width; gx += step)
            drawList->AddLine(ImVec2(gx, y), ImVec2(gx, y + height), uiAlpha(palette.canvasGrid, 0.5f));
        for (float gy = y + step; gy < y + height; gy += step)
            drawList->AddLine(ImVec2(x, gy), ImVec2(x + width, gy), uiAlpha(palette.canvasGrid, 0.5f));
    }

    if (!this->app.hasDoc())
    {
        if (!this->app.hasProject())
        {
            const float cardWidth = std::min(width - 80.0f, 540.0f);
            const float cardHeight = 258.0f;
            Rect card(x + (width - cardWidth) * 0.5f, y + (height - cardHeight) * 0.5f, cardWidth, cardHeight);
            ui::fill(drawList, card, palette.panelAlt, 12.0f);
            ui::stroke(drawList, card, palette.border, 12.0f);
            ui::iconCentered(drawList, Rect(card.x, card.y + 26, card.w, 34), "widgets", palette.accent, 30.0f);
            ui::label(drawList, Rect(card.x, card.y + 72, card.w, 24), "打开 bklv 工程", palette.text, 18.0f, 1);
            ui::label(drawList, Rect(card.x + 30, card.y + 102, card.w - 60, 40),
                "选择一个包含 theme.json 的主题工程目录；设计器会读取 tokens/ 与 view JSON。",
                palette.textMuted, 11.0f, 1);

            const float buttonWidth = 150.0f;
            const float gap = 12.0f;
            const float buttonsY = card.y + 164.0f;
            const float startX = card.x + (card.w - buttonWidth * 2 - gap) * 0.5f;
            this->welcomeOpenRect = Rect(startX, buttonsY, buttonWidth, 36);
            this->welcomeCopyRect = Rect(startX + buttonWidth + gap, buttonsY, buttonWidth, 36);
            const bool openHover = this->app.hovering(this->welcomeOpenRect);
            const bool copyHover = this->app.hovering(this->welcomeCopyRect);
            ui::fill(drawList, this->welcomeOpenRect, openHover ? palette.accentHover : palette.accent, 6.0f);
            ui::label(drawList, this->welcomeOpenRect, "打开工程", IM_COL32_WHITE, 12.0f, 1);
            ui::fill(drawList, this->welcomeCopyRect, copyHover ? palette.hover : palette.panel, 6.0f);
            ui::stroke(drawList, this->welcomeCopyRect, palette.border, 6.0f);
            ui::label(drawList, this->welcomeCopyRect, "复制并新建工程", palette.text, 11.5f, 1);
            ui::label(drawList, Rect(card.x + 20, card.y + card.h - 30, card.w - 40, 18),
                "工程通过 theme.json 识别；复制会生成独立副本并打开。", palette.textFaint, 9.5f, 1);
        }
        else
        {
            ui::iconCentered(drawList, Rect(x, y + height * 0.5f - 48, width, 32), "description", palette.textFaint, 28.0f);
            ui::label(drawList, Rect(x, y + height * 0.5f - 8, width, 22), "从左侧项目树打开页面或组件",
                palette.textMuted, 12.0f, 1);
            ui::label(drawList, Rect(x, y + height * 0.5f + 16, width, 20), "JSON 将使用 bklv 实时渲染到画布",
                palette.textFaint, 10.5f, 1);
        }
    }
    else if (this->previewRootView == nullptr && !this->app.previewError.empty())
    {
        const float bannerWidth = std::min(width - 60.0f, 620.0f);
        Rect banner(x + (width - bannerWidth) * 0.5f, y + 22, bannerWidth, 74);
        ui::fill(drawList, banner, uiAlpha(palette.red, 0.12f), 8.0f);
        ui::stroke(drawList, banner, uiAlpha(palette.red, 0.6f), 8.0f);
        ui::icon(drawList, banner.x + 14, banner.y + 14, "error", palette.red, 18.0f);
        ui::textClipped(drawList, banner.x + 40, banner.y + 13, banner.w - 54, "预览渲染失败", palette.text, 13.0f);
        ui::textClipped(drawList, banner.x + 40, banner.y + 36, banner.w - 54, this->app.previewError, palette.textMuted,
            11.0f);
    }
    else if (this->previewRootView == nullptr && this->app.hasDoc())
    {
        Document* document = this->app.doc();
        const Json& root = document->root;
        std::string kind = "JSON 文档";
        if (root.contains("layers")) kind = "控件外观 JSON";
        else if (root.contains("files") && root.contains("name")) kind = "主题清单 JSON";
        else if (document->relative.rfind("tokens/", 0) == 0) kind = "主题变量 JSON";
        else if (root.contains("import") || root.contains("box")) kind = "外观 / 组合 JSON";

        const bool themeDoc = document->relative == "theme.json" ||
            (root.contains("files") && root.contains("name"));
        if (themeDoc)
        {
            Rect card(x + 24, y + 22, width - 48, height - 44);
            ui::fill(drawList, card, palette.panelAlt, 10.0f);
            ui::stroke(drawList, card, palette.border, 10.0f);
            ui::icon(drawList, card.x + 18, card.y + 16, "info", palette.accent, 20.0f);
            ui::textClipped(drawList, card.x + 48, card.y + 16, card.w - 70, "bklv 主题清单",
                palette.text, 15.0f);
            ui::textClipped(drawList, card.x + 48, card.y + 40, card.w - 70, document->relative,
                palette.textMuted, 10.5f);
            float rowY = card.y + 76.0f;
            const std::string name = root.value("name", std::string());
            const std::string scheme = root.value("colorScheme", std::string());
            const std::string author = root.value("author", std::string());
            const std::string description = root.value("description", std::string());
            const int fileCount = root.contains("files") && root["files"].is_array()
                ? (int)root["files"].size() : 0;
            const int tokenCount = (int)this->app.project.tokens.size();
            const char* labels[] = { "名称", "配色", "作者", "文件", "全局变量", "描述" };
            const std::string values[] = { name, scheme, author, std::to_string(fileCount) + " 个",
                std::to_string(tokenCount) + " 个", description };
            for (int i = 0; i < 6; ++i)
            {
                ui::textClipped(drawList, card.x + 22, rowY, 62, labels[i], palette.textMuted, 10.5f);
                ui::textClipped(drawList, card.x + 92, rowY, card.w - 118, values[i], palette.text, 11.0f);
                rowY += 25.0f;
            }
            rowY += 10.0f;
            ui::textClipped(drawList, card.x + 22, rowY, card.w - 44, "files[] 清单", palette.textMuted, 10.5f);
            rowY += 24.0f;
            if (root.contains("files") && root["files"].is_array())
            {
                for (const Json& entry : root["files"])
                {
                    if (!entry.is_string() || rowY > card.y + card.h - 24) break;
                    ui::textClipped(drawList, card.x + 34, rowY, card.w - 56, entry.get<std::string>(),
                        palette.text, 10.0f);
                    rowY += 19.0f;
                }
            }
            return;
        }

        if (document->relative.rfind("tokens/", 0) == 0)
        {
            Rect card(x + 24, y + 22, width - 48, height - 44);
            ui::fill(drawList, card, palette.panelAlt, 10.0f);
            ui::stroke(drawList, card, palette.border, 10.0f);
            const bool colors = document->relative.find("color") != std::string::npos;
            ui::icon(drawList, card.x + 18, card.y + 16, colors ? "palette" : "tune",
                palette.accent, 20.0f);
            ui::textClipped(drawList, card.x + 48, card.y + 16, card.w - 70,
                colors ? "全局颜色变量" : "全局数值变量", palette.text, 15.0f);
            ui::textClipped(drawList, card.x + 48, card.y + 40, card.w - 70,
                document->relative + "  ·  双击变量面板中的值可编辑", palette.textMuted, 10.5f);

            std::vector<const Token*> rows;
            for (const Token& token : this->app.project.tokens)
                if (token.file == document->relative) rows.push_back(&token);
            const float headerY = card.y + 74.0f;
            ui::fill(drawList, Rect(card.x + 14, headerY, card.w - 28, 26), palette.panel);
            ui::textClipped(drawList, card.x + 24, headerY + 6, 240, "变量", palette.textMuted, 10.0f);
            ui::textClipped(drawList, card.x + 280, headerY + 6, card.w - 310, "值", palette.textMuted, 10.0f);
            const float top = headerY + 28.0f;
            const float lineHeight = 23.0f;
            const float maxScroll = std::max(0.0f, (float)rows.size() * lineHeight - (card.h - 120.0f));
            this->app.jsonPreviewScroll = std::clamp(this->app.jsonPreviewScroll, 0.0f, maxScroll);
            const int first = std::max(0, (int)(this->app.jsonPreviewScroll / lineHeight));
            float rowY = top;
            for (int i = first; i < (int)rows.size(); ++i)
            {
                if (rowY > card.y + card.h - 26) break;
                const Token& token = *rows[(size_t)i];
                if (token.type == TokenType::COLOR)
                {
                    ImU32 color = 0;
                    if (token.value.is_string() && parseColor(token.value.get<std::string>(), color))
                    {
                        ui::fill(drawList, Rect(card.x + 24, rowY + 5, 13, 13), color, 3.0f);
                        ui::stroke(drawList, Rect(card.x + 24, rowY + 5, 13, 13), palette.border, 3.0f);
                    }
                }
                ui::textClipped(drawList, card.x + 44, rowY + 4, card.w - 360, token.path,
                    palette.text, 10.5f);
                std::string valueText = token.value.is_string() ? token.value.get<std::string>()
                    : token.value.dump();
                ui::textClipped(drawList, card.x + 280, rowY + 4, card.w - 310, valueText,
                    palette.textMuted, 10.5f);
                rowY += lineHeight;
            }
            return;
        }

        Rect card(x + 24, y + 22, width - 48, height - 44);
        ui::fill(drawList, card, palette.panelAlt, 10.0f);
        ui::stroke(drawList, card, palette.border, 10.0f);
        ui::icon(drawList, card.x + 16, card.y + 14, "description", palette.accent, 17.0f);
        ui::textClipped(drawList, card.x + 42, card.y + 12, card.w - 60, kind, palette.text, 13.0f);
        ui::textClipped(drawList, card.x + 42, card.y + 34, card.w - 60,
            document->relative + "  ·  已解析  ·  " + std::to_string(root.size()) + " 个顶层键  ·  左侧「代码预览」可编辑",
            palette.textMuted, 10.0f);

        const Rect codeArea(card.x + 12, card.y + 60, card.w - 24, card.h - 74);
        ui::fill(drawList, codeArea, palette.inputBg, 6.0f);
        ui::stroke(drawList, codeArea, palette.border, 6.0f);
        const float lineHeight = 16.0f;
        const float gutterWidth = 38.0f;
        const std::string text = prettyJson(root);
        std::vector<std::string> lines;
        std::stringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) lines.push_back(line);
        if (lines.empty()) lines.push_back("");
        const float maxScroll = std::max(0.0f, (float)lines.size() * lineHeight + 10.0f - codeArea.h);
        this->app.jsonPreviewScroll = std::clamp(this->app.jsonPreviewScroll, 0.0f, maxScroll);
        drawList->PushClipRect(ImVec2(codeArea.x + 1, codeArea.y + 1),
            ImVec2(codeArea.x + codeArea.w - 1, codeArea.y + codeArea.h - 1), true);
        ui::fill(drawList, Rect(codeArea.x, codeArea.y, gutterWidth, codeArea.h), palette.panel);
        ui::vline(drawList, codeArea.x + gutterWidth, codeArea.y, codeArea.y + codeArea.h, palette.border);
        const int firstLine = std::max(0, (int)(this->app.jsonPreviewScroll / lineHeight));
        for (int i = firstLine; i < (int)lines.size(); ++i)
        {
            const float lineY = codeArea.y + 5 + i * lineHeight - this->app.jsonPreviewScroll;
            if (lineY > codeArea.y + codeArea.h) break;
            if (lineY + lineHeight < codeArea.y) continue;
            ui::textRight(drawList, codeArea.x + gutterWidth - 6, lineY, std::to_string(i + 1),
                palette.textFaint, 8.5f);
            ui::text(drawList, codeArea.x + gutterWidth + 7, lineY, lines[(size_t)i], palette.text, 10.0f);
        }
        drawList->PopClipRect();
        ui::scrollbar(drawList, Rect(codeArea.x + codeArea.w - 7, codeArea.y, 7, codeArea.h),
            this->app.jsonPreviewScroll, std::max(codeArea.h, (float)lines.size() * lineHeight + 10.0f),
            codeArea.h, palette.scrollbar);
    }
}

void CanvasPanel::drawChildren(ImDrawList* drawList, float x, float y, float width, float height,
    bklv::FrameContext* ctx)
{
    const Palette& palette = paletteOf(this->app);
    const bool appearance = this->app.hasDoc() && this->previewRootView != nullptr &&
        !this->app.doc()->root.contains("type");
    if (appearance)
    {
        bklv::View* root = this->previewRootView;
        const Rect pageRect(root->getX(), root->getY(), root->getWidth(), root->getHeight());
        if (pageRect.w > 1.0f && pageRect.h > 1.0f)
        {
            drawList->PushClipRect(ImVec2(pageRect.x, pageRect.y),
                ImVec2(pageRect.x + pageRect.w, pageRect.y + pageRect.h), true);
            const float cell = 9.0f;
            for (float cellY = pageRect.y; cellY < pageRect.y + pageRect.h; cellY += cell)
            {
                for (float cellX = pageRect.x; cellX < pageRect.x + pageRect.w; cellX += cell)
                {
                    const int ix = (int)((cellX - pageRect.x) / cell);
                    const int iy = (int)((cellY - pageRect.y) / cell);
                    const ImU32 color = ((ix + iy) & 1) ? IM_COL32(244, 246, 249, 255)
                                                        : IM_COL32(229, 232, 237, 255);
                    drawList->AddRectFilled(ImVec2(cellX, cellY),
                        ImVec2(std::min(cellX + cell, pageRect.x + pageRect.w),
                            std::min(cellY + cell, pageRect.y + pageRect.h)), color);
                }
            }
            drawList->PopClipRect();
        }
    }

    bklv::Box::drawChildren(drawList, x, y, width, height, ctx);
    if (!this->app.hasDoc() || this->previewRootView == nullptr)
        return;

    bklv::View* root = this->previewRootView;
    const Rect pageRect(root->getX(), root->getY(), root->getWidth(), root->getHeight());
    ui::stroke(drawList, pageRect, uiAlpha(palette.border, 0.9f), 0.0f, 1.0f);

    if (this->app.hasDoc() && !this->app.doc()->root.contains("type"))
    {
        const std::string label = "外观预览";
        const float tagWidth = ui::textWidth(label, 9.5f) + 16.0f;
        Rect tag(pageRect.x, pageRect.y - 21.0f, tagWidth, 18.0f);
        if (tag.y < this->canvasArea.y + 4.0f) tag.y = pageRect.y + pageRect.h + 5.0f;
        ui::fill(drawList, tag, palette.accent, 3.0f);
        ui::textClipped(drawList, tag.x + 8, tag.y + 2, tag.w - 14, label, IM_COL32_WHITE, 9.5f);
    }

    if (this->app.showGuides)
    {
        for (const auto& entry : this->preview.pointerToView)
        {
            bklv::View* view = entry.second;
            if (view == nullptr) continue;
            const Rect rect(view->getX(), view->getY(), view->getWidth(), view->getHeight());
            if (rect.w < 2 || rect.h < 2) continue;
            ui::stroke(drawList, rect, uiAlpha(palette.accent, 0.07f), 0.0f, 1.0f);
        }
    }

    auto drawOutline = [&](const std::string& pointer, bool selected) {
        auto it = this->preview.pointerToView.find(pointer);
        if (it == this->preview.pointerToView.end() || it->second == nullptr) return;
        bklv::View* view = it->second;
        Rect rect(view->getX() - 1, view->getY() - 1, view->getWidth() + 2, view->getHeight() + 2);
        if (selected)
        {
            ui::softShadow(drawList, rect, 0.0f, uiAlpha(palette.accent, 0.35f), 8.0f, 0.0f);
            ui::stroke(drawList, rect, palette.accent, 0.0f, 1.6f);
            const float handle = 4.0f;
            const ImVec2 corners[4] = {
                ImVec2(rect.x, rect.y), ImVec2(rect.x + rect.w, rect.y),
                ImVec2(rect.x, rect.y + rect.h), ImVec2(rect.x + rect.w, rect.y + rect.h)
            };
            for (const ImVec2& corner : corners)
            {
                Rect handleRect(corner.x - handle, corner.y - handle, handle * 2, handle * 2);
                ui::fill(drawList, handleRect, palette.panelRaised, 1.0f);
                ui::stroke(drawList, handleRect, palette.accent, 1.0f);
            }

            const Node* node = findNode(this->app.doc()->tree, pointer);
            const std::string label = node != nullptr ? this->app.nodeLabel(*node) : std::string("节点");
            const float labelWidth = ui::textWidth(label, 10.0f) + 18.0f;
            float labelY = rect.y - 20.0f;
            if (labelY < this->canvasArea.y + 4) labelY = rect.y + rect.h + 5.0f;
            Rect tag(rect.x, labelY, labelWidth, 17);
            ui::fill(drawList, tag, palette.accent, 3.0f);
            ui::textClipped(drawList, tag.x + 7, tag.y + 2, tag.w - 12, label, IM_COL32_WHITE, 9.5f);
        }
        else
        {
            ui::stroke(drawList, rect, uiAlpha(palette.teal, 0.85f), 0.0f, 1.0f);
        }
    };

    if (!this->app.canvasPreviewMode)
    {
        if (this->app.hoverNode != "@none" && this->app.hoverNode != this->app.selectedNode)
            drawOutline(this->app.hoverNode, false);
        drawOutline(this->app.selectedNode, true);
    }

    if (this->app.canvasPreviewMode)
    {
        Rect tag(this->canvasArea.x + this->canvasArea.w - 84, this->canvasArea.y + 12, 70, 22);
        ui::fill(drawList, tag, uiAlpha(palette.green, 0.16f), 5.0f);
        ui::textVC(drawList, tag.x + 10, tag.y, tag.h, "预览中", palette.green, 10.0f);
    }

    if (!this->app.previewError.empty())
    {
        const float bannerWidth = std::min(this->canvasArea.w - 28.0f, 620.0f);
        Rect banner(this->canvasArea.x + 14, this->canvasArea.y + this->canvasArea.h - 38, bannerWidth, 26);
        ui::fill(drawList, banner, uiAlpha(palette.red, 0.16f), 5.0f);
        ui::stroke(drawList, banner, uiAlpha(palette.red, 0.6f), 5.0f);
        ui::icon(drawList, banner.x + 7, banner.y + 6, "warning", palette.red, 14.0f);
        ui::textClipped(drawList, banner.x + 27, banner.y + 6, banner.w - 34, this->app.previewError, palette.red,
            10.0f);
    }
    else if (!this->app.previewWarning.empty() && this->previewRootView != nullptr)
    {
        const float bannerWidth = std::min(this->canvasArea.w - 28.0f, 620.0f);
        Rect banner(this->canvasArea.x + 14, this->canvasArea.y + this->canvasArea.h - 38, bannerWidth, 26);
        ui::fill(drawList, banner, uiAlpha(palette.yellow, 0.13f), 5.0f);
        ui::stroke(drawList, banner, uiAlpha(palette.yellow, 0.5f), 5.0f);
        ui::icon(drawList, banner.x + 7, banner.y + 6, "warning", palette.yellow, 14.0f);
        ui::textClipped(drawList, banner.x + 27, banner.y + 6, banner.w - 34, this->app.previewWarning,
            palette.yellow, 10.0f);
    }
}

void CanvasPanel::handleInput(UiInput& input)
{
    const Rect area(this->getX(), this->getY(), this->getWidth(), this->getHeight());
    if (!area.contains(input.mouse))
    {
        this->app.hoverNode = "@none";
        return;
    }
    this->canvasArea = area;

    if (!this->app.hasDoc())
    {
        if (!this->app.hasProject())
        {
            if (this->welcomeOpenRect.contains(input.mouse))
            {
                this->app.tooltip = "选择包含 theme.json 的工程目录";
                if (input.pressed) this->app.runAction("project_open");
                return;
            }
            if (this->welcomeCopyRect.contains(input.mouse))
            {
                this->app.tooltip = "选择源工程，随后选择新工程父目录";
                if (input.pressed) this->app.runAction("project_copy");
                return;
            }
        }
        return;
    }

    if (this->app.doc() != nullptr && !this->app.doc()->root.contains("type"))
    {
        if (input.wheel != 0.0f)
        {
            const std::string text = prettyJson(this->app.doc()->root);
            const float lineHeight = 16.0f;
            const int lineCount = (int)std::count(text.begin(), text.end(), '\n') + 1;
            const float visible = std::max(40.0f, this->getHeight() - 150.0f);
            this->app.jsonPreviewScroll = std::clamp(this->app.jsonPreviewScroll - input.wheel * 48.0f,
                0.0f, std::max(0.0f, lineCount * lineHeight + 10.0f - visible));
        }
        return;
    }

    if (input.wheel != 0.0f)
    {
        if (input.command)
        {
            const float before = this->app.canvasZoom;
            this->app.canvasFit = false;
            this->app.canvasZoom = std::clamp(this->app.canvasZoom + input.wheel * 0.06f, 0.12f, 2.5f);
            if (std::fabs(before - this->app.canvasZoom) > 0.0001f)
            {
                this->app.previewDirty = true;
                this->app.canvasPanX = 0.0f;
                this->app.canvasPanY = 0.0f;
            }
        }
        else
        {
            if (input.shift) this->app.canvasPanX -= input.wheel * 32.0f;
            else this->app.canvasPanY -= input.wheel * 32.0f;
        }
        return;
    }

    if (input.pressed)
    {
        bool selected = false;
        if (this->previewRootView != nullptr)
        {
            bklv::View* hit = hitPreviewView(this->previewRootView, input.mouse);
            while (hit != nullptr)
            {
                auto it = this->preview.viewToPointer.find(hit);
                if (it != this->preview.viewToPointer.end())
                {
                    this->app.selectNode(it->second, true);
                    selected = true;
                    break;
                }
                hit = hit->getParent();
            }
        }
        if (!selected)
        {
            if (input.doubleClicked)
            {
                this->app.fitCanvas();
                this->app.selectedNode.clear();
                return;
            }
            this->app.canvasPanning = true;
            this->app.canvasPanStart = input.mouse;
        }
    }

    if (this->app.canvasPanning && input.down)
    {
        this->app.canvasPanX += input.delta.x;
        this->app.canvasPanY += input.delta.y;
    }
    if (input.released)
        this->app.canvasPanning = false;

    if (!input.down && this->previewRootView != nullptr)
    {
        bklv::View* hit = hitPreviewView(this->previewRootView, input.mouse);
        std::string hover;
        while (hit != nullptr)
        {
            auto it = this->preview.viewToPointer.find(hit);
            if (it != this->preview.viewToPointer.end())
            {
                hover = it->second;
                break;
            }
            hit = hit->getParent();
        }
        this->app.hoverNode = hover.empty() ? "@none" : hover;
    }
}

// ---------------------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------------------

InspectorPanel::InspectorPanel(DesignerApp& app)
    : DesignerPanel(app, "inspector")
{
}

void InspectorPanel::buildRows()
{
    this->contentArea = Rect(this->getX(), this->getY() + 34.0f + 92.0f + 34.0f, this->getWidth(),
        std::max(0.0f, this->getHeight() - 34.0f - 92.0f - 34.0f));
    this->contentHeight = 0.0f;
}

void InspectorPanel::drawAppearanceEditor(ImDrawList* drawList, float x, float y, float width,
    float height)
{
    const Palette& palette = paletteOf(this->app);
    Document* document = this->app.doc();
    if (document == nullptr) return;

    this->appearanceHits.clear();
    this->contentHeight = 0.0f;

    std::string kind = "外观 / 组合 JSON";
    if (document->root.contains("files") && document->root.contains("name")) kind = "主题清单 JSON";
    else if (document->relative.rfind("tokens/", 0) == 0) kind = "主题变量 JSON";

    Rect info(x + 10, y + 42, width - 20, 76);
    ui::fill(drawList, info, palette.panelAlt, 8.0f);
    ui::stroke(drawList, info, palette.border, 8.0f);
    ui::icon(drawList, info.x + 14, info.y + 14, "description", palette.accent, 18.0f);
    ui::textClipped(drawList, info.x + 42, info.y + 12, info.w - 56, kind, palette.text, 12.0f);
    ui::textClipped(drawList, info.x + 42, info.y + 32, info.w - 56, document->relative,
        palette.textMuted, 9.5f);
    ui::textClipped(drawList, info.x + 14, info.y + 52, info.w - 28,
        "属性直接写入 JSON；图层为当前文件内声明的 layers。", palette.textFaint, 9.5f);

    const Rect area(x + 10, y + 126, width - 20, std::max(40.0f, height - 134));
    this->contentArea = area;
    drawList->PushClipRect(area.min(), area.max(), true);
    const float startY = area.y - this->app.inspectorScroll;
    float cy = startY;

    float selectedHeaderY = -1.0f;
    auto section = [&](const std::string& title) {
        if (cy + 24 >= area.y && cy <= area.y + area.h)
        {
            ui::textClipped(drawList, area.x + 6, cy + 3, area.w - 12, title, palette.textMuted, 10.0f);
            ui::fill(drawList, Rect(area.x + 6, cy + 21, area.w - 12, 1), palette.borderSoft);
        }
        cy += 28.0f;
    };

    auto row = [&](const std::string& pointer, const std::string& label, TokenType type,
                   const std::string& options = std::string()) {
        Rect rowRect(area.x + 6, cy, area.w - 12, 26);
        const bool visible = rowRect.y + rowRect.h >= area.y && rowRect.y <= area.y + area.h;
        const bool hover = visible && this->app.hovering(rowRect);
        if (visible)
        {
            if (hover) ui::fill(drawList, rowRect, palette.hover, 4.0f);
            ui::textClipped(drawList, rowRect.x + 6, rowRect.y + 5, 108, label, palette.textMuted, 10.0f);
        }

        const Json* value = document->valueAt(pointer);
        std::string text;
        ImU32 swatch = 0;
        bool hasSwatch = false;
        if (value != nullptr)
        {
            if (type == TokenType::COLOR && value->is_string())
            {
                Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                    std::string(), *value);
                if (resolved.is_string()) hasSwatch = parseColor(resolved.get<std::string>(), swatch);
                text = value->get<std::string>();
            }
            else
                text = styleValueText(*value);
        }

        const float valueX = rowRect.x + 116;
        const float valueW = rowRect.w - 126;
        if (visible)
        {
            if (type == TokenType::COLOR)
            {
                Rect sw(valueX, rowRect.y + 7, 13, 13);
                ui::fill(drawList, sw, hasSwatch ? swatch : palette.panel, 3.0f);
                ui::stroke(drawList, sw, palette.border, 3.0f);
                ui::textClipped(drawList, valueX + 20, rowRect.y + 5, valueW - 22, text.empty() ? "—" : text,
                    palette.text, 10.0f);
            }
            else if (type == TokenType::BOOLEAN)
            {
                const bool on = value != nullptr && ((value->is_boolean() && value->get<bool>()) ||
                    (value->is_string() && value->get<std::string>() == "true"));
                Rect toggle(valueX, rowRect.y + 7, 30, 13);
                ui::fill(drawList, toggle, on ? palette.accent : palette.panel, 6.0f);
                ui::fill(drawList, Rect(toggle.x + (on ? 17 : 2), toggle.y + 1, 11, 11),
                    on ? IM_COL32_WHITE : palette.textFaint, 5.5f);
            }
            else
            {
                ui::textClipped(drawList, valueX, rowRect.y + 5, valueW - 14,
                    text.empty() ? "—" : text, palette.text, 10.0f);
                if (!options.empty())
                    ui::chevron(drawList, valueX + valueW - 12, rowRect.y + 10, 7, true,
                        palette.textMuted);
            }
        }

        const bool isRef = value != nullptr && value->is_string() &&
            (value->get<std::string>().rfind("@", 0) == 0 || value->get<std::string>().rfind("&", 0) == 0);
        if (isRef && visible)
        {
            Rect clearRect(rowRect.x + rowRect.w - 20, rowRect.y + 5, 18, 18);
            ui::iconCentered(drawList, clearRect, "close", palette.textFaint, 11.0f);
            this->appearanceHits.push_back({ clearRect, "appearance-clearref", pointer, options, type });
        }

        std::string action = "appearance";
        if (!options.empty()) action = "appearance-enum";
        else if (type == TokenType::COLOR) action = "appearance-color";
        else if (type == TokenType::BOOLEAN) action = "appearance-toggle";
        this->appearanceHits.push_back({ rowRect, action, pointer, options, type });
        cy += 26.0f;
    };

    auto arrayRows = [&](const std::string& pointer, const char* const labels[4]) {
        const Json* array = document->valueAt(pointer);
        if (array == nullptr || !array->is_array()) return;
        for (int i = 0; i < 4; ++i)
            row(pointer + "/" + std::to_string(i), labels[i], TokenType::NUMBER);
    };

    section("文档");
    row("/$comment", "说明", TokenType::STRING);
    if (document->root.contains("import")) row("/import", "导入", TokenType::STRING);
    if (document->root.contains("children")) row("/children", "子节点数", TokenType::NUMBER);

    section("盒模型 box");
    row("/box/width", "宽度", TokenType::NUMBER);
    row("/box/height", "高度", TokenType::NUMBER);
    row("/box/minWidth", "最小宽度", TokenType::NUMBER);
    row("/box/minHeight", "最小高度", TokenType::NUMBER);
    row("/box/axis", "主轴方向", TokenType::STRING, "row|column");
    row("/box/alignItems", "交叉轴对齐", TokenType::STRING,
        "flexStart|center|flexEnd|stretch|baseline|auto");
    row("/box/justifyContent", "主轴对齐", TokenType::STRING,
        "flexStart|center|flexEnd|spaceBetween|spaceAround|spaceEvenly");
    row("/box/gap", "间距", TokenType::NUMBER);
    row("/box/grow", "grow", TokenType::NUMBER);
    row("/box/shrink", "shrink", TokenType::NUMBER);
    row("/box/measureContent", "内容测量", TokenType::STRING, "max|sum");
    row("/box/backgroundColor", "背景", TokenType::COLOR);
    row("/box/cornerRadius", "圆角", TokenType::NUMBER);
    row("/box/borderThickness", "边框宽度", TokenType::NUMBER);
    row("/box/borderColor", "边框颜色", TokenType::COLOR);
    const char* edges[4] = { "上", "右", "下", "左" };
    const Json* boxPadding = document->valueAt("/box/padding");
    if (boxPadding != nullptr && boxPadding->is_number())
        row("/box/padding", "内边距", TokenType::NUMBER);
    else
        arrayRows("/box/padding", edges);
    const Json* boxMargin = document->valueAt("/box/margin");
    if (boxMargin != nullptr && boxMargin->is_number())
        row("/box/margin", "外边距", TokenType::NUMBER);
    else
        arrayRows("/box/margin", edges);

    section("图层 layers");
    const Json* layers = document->valueAt("/layers");
    if (layers != nullptr && layers->is_array())
    {
        for (size_t i = 0; i < layers->size(); ++i)
        {
            const Json& layer = (*layers)[i];
            if (!layer.is_object()) continue;
            if (layer.contains("import"))
            {
                const std::string layerPointer = "/layers/" + std::to_string(i);
                if (layerPointer == this->app.selectedNode) selectedHeaderY = cy;
                Rect header(area.x + 6, cy, area.w - 12, 24);
                if (header.y + header.h >= area.y && header.y <= area.y + area.h)
                {
                    ui::fill(drawList, header, layerPointer == this->app.selectedNode
                        ? palette.accentSoft : palette.panel, 4.0f);
                    ui::textClipped(drawList, header.x + 6, header.y + 4, header.w - 12,
                        "导入 " + styleValueText(layer["import"]), palette.text, 10.0f);
                }
                cy += 26.0f;
                row(layerPointer + "/import", "导入路径", TokenType::STRING);
                continue;
            }
            const std::string op = layer.value("op", std::string("rect"));
            const std::string id = layer.value("id", std::string());
            const std::string base = "/layers/" + std::to_string(i);
            if (base == this->app.selectedNode) selectedHeaderY = cy;
            Rect header(area.x + 6, cy, area.w - 12, 24);
            if (header.y + header.h >= area.y && header.y <= area.y + area.h)
            {
                ui::fill(drawList, header, base == this->app.selectedNode
                    ? palette.accentSoft : palette.panel, 4.0f);
                ui::textClipped(drawList, header.x + 6, header.y + 4, header.w - 12,
                    "图层 " + std::to_string(i) + " · " + op + (id.empty() ? "" : " · " + id),
                    palette.text, 10.0f);
            }
            cy += 26.0f;

            row(base + "/op", "类型", TokenType::STRING,
                "rect|text|image|shadow|glass|custom|extension");
            row(base + "/id", "id", TokenType::STRING);
            row(base + "/width", "宽度", TokenType::NUMBER);
            row(base + "/height", "高度", TokenType::NUMBER);
            row(base + "/align", "水平对齐", TokenType::STRING, "left|center|right");
            row(base + "/valign", "垂直对齐", TokenType::STRING, "top|center|bottom");
            row(base + "/opacity", "不透明度", TokenType::NUMBER);
            arrayRows(base + "/inset", edges);
            const Json* offset = document->valueAt(base + "/offset");
            if (offset != nullptr && offset->is_array())
            {
                row(base + "/offset/0", "X 偏移", TokenType::NUMBER);
                row(base + "/offset/1", "Y 偏移", TokenType::NUMBER);
            }
            if (op == "rect")
            {
                row(base + "/fill", "填充", TokenType::COLOR);
                row(base + "/stroke", "描边", TokenType::COLOR);
                row(base + "/strokeWidth", "描边宽度", TokenType::NUMBER);
                row(base + "/radius", "圆角", TokenType::NUMBER);
            }
            else if (op == "text")
            {
                row(base + "/color", "文字颜色", TokenType::COLOR);
                if (document->valueAt(base + "/textColor") != nullptr)
                    row(base + "/textColor", "文字颜色(别名)", TokenType::COLOR);
                row(base + "/fontSize", "字号", TokenType::NUMBER);
                row(base + "/data_id", "数据槽", TokenType::STRING);
                row(base + "/font_kind", "字体", TokenType::STRING, "text|icon");
            }
            else if (op == "image")
            {
                row(base + "/src", "图片", TokenType::STRING);
                row(base + "/data_id", "数据槽", TokenType::STRING);
                row(base + "/scaling", "缩放", TokenType::STRING, "fit|fill|stretch|center");
                row(base + "/radius", "圆角", TokenType::NUMBER);
            }
            else if (op == "shadow")
            {
                row(base + "/fill", "阴影颜色", TokenType::COLOR);
                row(base + "/feather", "羽化", TokenType::NUMBER);
                row(base + "/spread", "外扩", TokenType::NUMBER);
                row(base + "/radius", "圆角", TokenType::NUMBER);
                row(base + "/cutout", "挖空", TokenType::BOOLEAN);
                row(base + "/inner", "内阴影", TokenType::BOOLEAN);
            }
            else if (op == "glass")
            {
                row(base + "/fill", "染色", TokenType::COLOR);
                row(base + "/radius", "圆角", TokenType::NUMBER);
            }
        }
    }
    else
    {
        if (cy + 20 >= area.y && cy <= area.y + area.h)
            ui::textClipped(drawList, area.x + 8, cy, area.w - 16, "没有 layers", palette.textFaint, 10.0f);
        cy += 24.0f;
    }

    section("状态 states");
    const Json* states = document->valueAt("/states");
    if (states != nullptr && states->is_object())
    {
        for (auto it = states->begin(); it != states->end(); ++it)
        {
            if (cy + 20 >= area.y && cy <= area.y + area.h)
                ui::textClipped(drawList, area.x + 8, cy, area.w - 16,
                    "状态：" + it.key() + "（在代码页编辑）", palette.textFaint, 10.0f);
            cy += 22.0f;
        }
    }
    else
    {
        if (cy + 20 >= area.y && cy <= area.y + area.h)
            ui::textClipped(drawList, area.x + 8, cy, area.w - 16, "没有 states", palette.textFaint, 10.0f);
        cy += 22.0f;
    }

    this->contentHeight = std::max(area.h, cy - startY + 20.0f);
    const float maxScroll = std::max(0.0f, this->contentHeight - area.h);
    if (this->app.selectedNode.rfind("/layers/", 0) == 0 && selectedHeaderY >= 0.0f)
    {
        if (this->lastAppearanceSelection != this->app.selectedNode)
        {
            this->app.inspectorScroll = std::clamp(selectedHeaderY - area.y - 6.0f, 0.0f, maxScroll);
            this->lastAppearanceSelection = this->app.selectedNode;
        }
    }
    else if (!this->lastAppearanceSelection.empty())
    {
        this->lastAppearanceSelection.clear();
        if (this->app.selectedNode.empty()) this->app.inspectorScroll = 0.0f;
    }
    drawList->PopClipRect();
    ui::scrollbar(drawList, Rect(area.x + area.w - 7, area.y, 7, area.h), this->app.inspectorScroll,
        std::max(area.h, this->contentHeight), area.h, palette.scrollbar);
}

void InspectorPanel::drawNodeCard(ImDrawList* drawList, const Rect& area, float& y)
{
    const Palette& palette = paletteOf(this->app);
    (void)y;
    Rect card(area.x + 12, area.y + 42, area.w - 24, 80);
    ui::fill(drawList, card, palette.panelAlt, 8.0f);
    ui::stroke(drawList, card, palette.border, 8.0f);

    if (!this->app.hasDoc())
    {
        ui::iconCentered(drawList, Rect(card.x + 12, card.y + 20, 32, 32), "widgets", palette.textFaint, 20.0f);
        ui::textVC(drawList, card.x + 54, card.y, card.h, "未选择节点", palette.textMuted, 12.0f);
        return;
    }

    const Json* node = this->app.selectedNodeJson();
    if (node == nullptr) return;
    const std::string type = node->value("type", std::string("Box"));
    const std::string name = type;

    Rect icon(card.x + 12, card.y + 20, 34, 34);
    ui::fill(drawList, icon, palette.accentSoft, 7.0f);
    ui::iconCentered(drawList, icon, typeIcon(type).c_str(), palette.accent, 18.0f);
    ui::textClipped(drawList, card.x + 56, card.y + 16, card.w - 68, name, palette.text, 13.0f);
    ui::textClipped(drawList, card.x + 56, card.y + 36, card.w - 68, type, palette.textMuted, 10.0f);
    ui::textClipped(drawList, card.x + 56, card.y + 52, card.w - 68, this->app.nodeBreadcrumb(), palette.textFaint,
        9.5f);
}

void InspectorPanel::draw(ImDrawList* drawList, float x, float y, float width, float height,
    bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    ui::fill(drawList, Rect(x, y, width, height), palette.panel);
    Rect header(x, y, width, 34);
    panelHeader(drawList, this->app, header, "属性检查器", "tune");

    Document* document = this->app.doc();
    if (document != nullptr && !document->root.contains("type"))
    {
        const bool appearance = document->root.contains("layers") || document->root.contains("box") ||
            document->root.contains("import");
        if (appearance)
        {
            this->drawAppearanceEditor(drawList, x, y, width, height);
            return;
        }

        std::string kind = "主题清单 JSON";
        if (document->relative.rfind("tokens/", 0) == 0) kind = "主题变量 JSON";
        else if (document->root.contains("include")) kind = "视图 include JSON";
        Rect card(x + 12, y + 44, width - 24, 128);
        ui::fill(drawList, card, palette.panelAlt, 8.0f);
        ui::stroke(drawList, card, palette.border, 8.0f);
        ui::icon(drawList, card.x + 14, card.y + 16, "info", palette.accent, 18.0f);
        ui::textClipped(drawList, card.x + 42, card.y + 14, card.w - 58, kind, palette.text, 12.5f);
        ui::textClipped(drawList, card.x + 42, card.y + 36, card.w - 58, document->relative,
            palette.textMuted, 10.0f);
        ui::textClipped(drawList, card.x + 14, card.y + 66, card.w - 28,
            document->root.contains("include")
                ? "include 文件已在画布展开，节点树暂不展开 include。" 
                : "该文件是主题元数据/变量表，没有图层属性。", palette.textMuted, 10.5f);
        ui::textClipped(drawList, card.x + 14, card.y + 88, card.w - 28,
            "画布显示摘要；左侧「代码预览」可编辑。", palette.textFaint, 10.0f);
        return;
    }

    this->buildRows();
    float cursorY = y + 34.0f;
    this->drawNodeCard(drawList, Rect(x, y, width, height), cursorY);

    static const char* tabs[2] = { "属性", "样式" };
    this->hits.clear();
    this->chipRects.clear();
    for (int i = 0; i < 2; ++i)
    {
        Rect tab(x + 12 + i * 62.0f, y + 34.0f + 88.0f, 56, 28);
        this->tabRects[i] = tab;
        const bool active = (int)this->app.inspectorTab == i;
        if (active)
        {
            ui::fill(drawList, Rect(tab.x, tab.y + tab.h - 2, tab.w, 2), palette.accent);
        }
        else if (this->app.hovering(tab))
        {
            ui::fill(drawList, tab, palette.hover, 4.0f);
        }
        ui::label(drawList, tab, tabs[i], active ? palette.text : palette.textMuted, 11.0f, 1);
    }
    ui::hline(drawList, y + 34.0f + 115.0f, x, x + width, palette.borderSoft);

    const Rect area = this->contentArea;
    if (!this->app.hasDoc())
    {
        ui::iconCentered(drawList, Rect(x, area.y + 80, width, 30), "tune", palette.textFaint, 24.0f);
        ui::label(drawList, Rect(x, area.y + 118, width, 22), "选择一个节点来编辑属性", palette.textMuted, 11.0f, 1);
        return;
    }

    drawList->PushClipRect(area.min(), area.max(), true);
    float cy = area.y + 10 - this->app.inspectorScroll;
    const float startY = cy;

    float selectedHeaderY = -1.0f;
    auto section = [&](const std::string& title) {
        Rect row(area.x + 12, cy, area.w - 24, 24);
        ui::textVC(drawList, row.x + 2, row.y, row.h, title, palette.textMuted, 10.0f);
        ui::fill(drawList, Rect(row.x + 2, row.y + row.h - 1, row.w - 4, 1), palette.borderSoft);
        cy += 28.0f;
    };

    auto propRow = [&](const std::string& key, const std::string& label, bool showChain = true,
                       TokenType typeHint = TokenType::STRING, bool enumRow = false) {
        Document* document = this->app.doc();
        const Json* style = document->style(this->app.selectedNode);
        Json value;
        bool present = false;
        if (style != nullptr)
        {
            if (style->contains(key))
            {
                value = (*style)[key];
                present = true;
            }
            else
            {
                static const std::pair<const char*, const char*> aliases[] = {
                    { "alignItems", "align" }, { "justifyContent", "justify" },
                    { "horizontalAlign", "textAlign" }, { "textColor", "color" },
                    { "backgroundColor", "background" }, { "cornerRadius", "radius" },
                    { "borderThickness", "borderWidth" }, { "clipsToBounds", "clip" },
                    { "scalingType", "scaling" }, { "left", "x" }, { "top", "y" },
                };
                for (const auto& alias : aliases)
                {
                    if (key == alias.second && style->contains(alias.first))
                    {
                        value = (*style)[alias.first];
                        present = true;
                        break;
                    }
                }
            }
        }

        const float rowHeight = 30.0f;
        Rect row(area.x + 10, cy, area.w - 20, rowHeight);
        const bool hover = this->app.hovering(row);
        if (hover)
            ui::fill(drawList, row, palette.hover, 4.0f);
        ui::textVC(drawList, row.x + 4, row.y, row.h, label, palette.textMuted, 11.0f);

        const float valueX = row.x + 92.0f;
        const float valueW = row.w - 100.0f;
        const float chainW = showChain ? 22.0f : 0.0f;
        Rect valueRect(valueX, row.y + 4, valueW - chainW - 4, row.h - 8);

        std::string refName;
        bool isLocalRef = false;
        const bool isRef = present && refNameOf(value, refName, isLocalRef);
        Json resolved = present ? PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                               this->app.selectedNode, value) : Json();

        ImU32 swatch = 0;
        std::string display;
        if (present)
        {
            if (typeHint == TokenType::COLOR)
            {
                const std::string text = resolved.is_string() ? resolved.get<std::string>() : std::string();
                parseColor(text, swatch);
                display = isRef ? refName : text;
            }
            else if (typeHint == TokenType::BOOLEAN)
            {
                display = (resolved.is_boolean() && resolved.get<bool>()) || (resolved.is_number() && resolved.get<double>() != 0)
                    ? "true" : "false";
            }
            else
            {
                display = isRef ? refName : formatValue(resolved);
            }
        }

        const std::string editKey = "prop:" + this->app.selectedNode + ":" + key;
        const bool editing = this->app.edit.active && this->app.edit.key == editKey;
        const std::string editBuffer = editing ? this->app.edit.buffer : std::string();

        if (typeHint == TokenType::BOOLEAN)
        {
            Rect toggle(valueRect.x, row.y + 7, 30, 16);
            const bool on = present && display == "true";
            ui::fill(drawList, toggle, on ? palette.accent : palette.panelRaised, 8.0f);
            ui::dot(drawList, toggle.x + (on ? 21.0f : 9.0f), toggle.y + 8, 6.0f,
                on ? IM_COL32_WHITE : palette.textFaint);
            RowHit hit;
            hit.rect = toggle;
            hit.action = "toggle";
            hit.key = key;
            hit.type = typeHint;
            this->hits.push_back(hit);
        }
        else if (typeHint == TokenType::COLOR)
        {
            Rect colorBox(valueRect.x, row.y + 7, 16, 16);
            ui::fill(drawList, colorBox, swatch != 0 ? swatch : palette.panelRaised, 3.0f);
            ui::stroke(drawList, colorBox, palette.border, 3.0f);
            ui::textClipped(drawList, valueRect.x + 24, row.y + 7, valueRect.w - 30,
                present ? (isRef ? (isLocalRef ? "var · " : "fx · ") + refName : display) : "—",
                isRef ? (isLocalRef ? palette.teal : palette.accent) : palette.text, 10.5f);
            RowHit hit;
            hit.rect = Rect(valueRect.x, valueRect.y, valueRect.w, valueRect.h);
            hit.action = "prop";
            hit.key = key;
            hit.type = typeHint;
            this->hits.push_back(hit);
            hit.rect = colorBox;
            hit.action = "swatch";
            this->hits.push_back(hit);
        }
        else if (enumRow)
        {
            ui::fill(drawList, valueRect, palette.inputBg, 4.0f);
            ui::stroke(drawList, valueRect, palette.border, 4.0f);
            ui::textClipped(drawList, valueRect.x + 8, valueRect.y + 4, valueRect.w - 24,
                present ? display : "—", palette.text, 10.5f);
            ui::chevron(drawList, valueRect.x + valueRect.w - 12, valueRect.y + valueRect.h * 0.5f, 3.5f, true,
                palette.textFaint);
            RowHit hit;
            hit.rect = valueRect;
            hit.action = "enum";
            hit.key = key;
            hit.type = typeHint;
            this->hits.push_back(hit);
        }
        else
        {
            ui::fill(drawList, valueRect, palette.inputBg, 4.0f);
            ui::stroke(drawList, valueRect, editing ? palette.accent : palette.border, 4.0f);
            if (editing)
                drawInlineValue(drawList, this->app, Rect(valueRect.x, valueRect.y, valueRect.w, valueRect.h),
                    editBuffer, this->app.edit.caret, palette.text, 10.5f);
            else
                ui::textClipped(drawList, valueRect.x + 8, valueRect.y + 4, valueRect.w - 14,
                    present ? display : "—", isRef ? palette.accent : palette.text, 10.5f);
            RowHit hit;
            hit.rect = valueRect;
            hit.action = "prop";
            hit.key = key;
            hit.type = typeHint;
            this->hits.push_back(hit);
        }

        if (showChain)
        {
            Rect chain(row.x + row.w - 22, row.y + 5, 20, 20);
            ui::fill(drawList, chain, isRef ? (isLocalRef ? uiAlpha(palette.teal, 0.18f) : palette.accentSoft)
                                           : palette.panelAlt, 4.0f);
            ui::iconCodeCentered(drawList, chain, 0xE157,
                isRef ? (isLocalRef ? palette.teal : palette.accent) : palette.textFaint, 12.0f);
            RowHit hit;
            hit.rect = chain;
            hit.action = "chain";
            hit.key = key;
            hit.type = typeHint;
            this->hits.push_back(hit);
        }

        if (isRef && typeHint != TokenType::BOOLEAN)
        {
            Rect clearRect(row.x + row.w - 46, row.y + 5, 18, 20);
            ui::iconCentered(drawList, clearRect, "close", palette.textFaint, 11.0f);
            RowHit clearHit;
            clearHit.rect = clearRect;
            clearHit.action = "clearref";
            clearHit.key = key;
            this->hits.push_back(clearHit);
        }

        DropTarget target;
        target.rect = row;
        target.node = this->app.selectedNode;
        target.key = key;
        target.type = typeHint;
        this->app.dropTargets.push_back(target);

        cy += rowHeight + 2.0f;
    };

    const Json* node = this->app.selectedNodeJson();
    const std::string type = node != nullptr ? node->value("type", std::string("Box")) : "Box";

    if (this->app.inspectorTab == InspectorTab::PROPERTIES)
    {
        section("基础属性");
        propRow("width", "宽度", true, TokenType::NUMBER);
        propRow("height", "高度", true, TokenType::NUMBER);
        propRow("minWidth", "最小宽度", true, TokenType::NUMBER);
        propRow("minHeight", "最小高度", true, TokenType::NUMBER);
        propRow("maxWidth", "最大宽度", true, TokenType::NUMBER);
        propRow("maxHeight", "最大高度", true, TokenType::NUMBER);

        const bool isBox = type != "Label" && type != "Image" && type != "Button";
        if (type == "Label")
        {
            section("文本");
            propRow("text", "内容", false, TokenType::STRING);
            propRow("fontSize", "字号", true, TokenType::NUMBER);
            propRow("lineHeight", "行高倍数", true, TokenType::NUMBER);
            propRow("horizontalAlign", "水平对齐", true, TokenType::STRING, true);
            propRow("verticalAlign", "垂直对齐", true, TokenType::STRING, true);
            propRow("singleLine", "单行", false, TokenType::BOOLEAN);
        }
        else if (type == "Image")
        {
            section("图片");
            propRow("src", "资源路径", false, TokenType::STRING);
            propRow("scalingType", "缩放方式", true, TokenType::STRING, true);
            propRow("scale", "显示比例", true, TokenType::NUMBER);
            propRow("offsetX", "X 偏移", true, TokenType::NUMBER);
            propRow("offsetY", "Y 偏移", true, TokenType::NUMBER);
            propRow("cornerRadius", "圆角", true, TokenType::NUMBER);
            propRow("placeholderColor", "占位色", true, TokenType::COLOR);
        }
        else if (type == "Button")
        {
            section("按钮");
            propRow("text", "文字", false, TokenType::STRING);
            propRow("icon", "图标", false, TokenType::STRING);
            propRow("iconFont", "图标字体", true, TokenType::STRING, true);
            propRow("variant", "外观变体", false, TokenType::STRING);
            propRow("enabled", "启用", false, TokenType::BOOLEAN);
        }

        section("定位");
        propRow("position", "定位方式", true, TokenType::STRING, true);
        propRow("left", "X 坐标", true, TokenType::NUMBER);
        propRow("top", "Y 坐标", true, TokenType::NUMBER);
        propRow("right", "右偏移", true, TokenType::NUMBER);
        propRow("bottom", "下偏移", true, TokenType::NUMBER);

        section("布局");
        if (isBox)
        {
            propRow("axis", "主轴方向", true, TokenType::STRING, true);
            propRow("direction", "书写方向", true, TokenType::STRING, true);
            propRow("justifyContent", "主轴对齐", true, TokenType::STRING, true);
            propRow("alignItems", "交叉轴对齐", true, TokenType::STRING, true);
            propRow("alignContent", "行间对齐", true, TokenType::STRING, true);
            propRow("flexWrap", "允许换行", false, TokenType::BOOLEAN);
            propRow("gap", "间距", true, TokenType::NUMBER);
            propRow("gapRow", "行间距", true, TokenType::NUMBER);
            propRow("gapColumn", "列间距", true, TokenType::NUMBER);
            propRow("padding", "内边距", true, TokenType::NUMBER);
            propRow("paddingTop", "上内边距", true, TokenType::NUMBER);
            propRow("paddingRight", "右内边距", true, TokenType::NUMBER);
            propRow("paddingBottom", "下内边距", true, TokenType::NUMBER);
            propRow("paddingLeft", "左内边距", true, TokenType::NUMBER);
            propRow("flexBasis", "弹性基准", true, TokenType::NUMBER);
        }
        propRow("alignSelf", "自身对齐", true, TokenType::STRING, true);
        propRow("margin", "外边距", true, TokenType::NUMBER);
        propRow("marginTop", "上外边距", true, TokenType::NUMBER);
        propRow("marginRight", "右外边距", true, TokenType::NUMBER);
        propRow("marginBottom", "下外边距", true, TokenType::NUMBER);
        propRow("marginLeft", "左外边距", true, TokenType::NUMBER);
        propRow("grow", "伸展", true, TokenType::NUMBER);
        propRow("shrink", "收缩", true, TokenType::NUMBER);
    }
    else
    {
        section("外观");
        if (type != "Button")
        {
            propRow("backgroundColor", "背景", true, TokenType::COLOR);
            propRow("cornerRadius", "圆角", true, TokenType::NUMBER);
            propRow("borderThickness", "边框宽度", true, TokenType::NUMBER);
            propRow("borderColor", "边框颜色", true, TokenType::COLOR);
        }
        propRow("clipsToBounds", "裁剪内容", false, TokenType::BOOLEAN);
        propRow("visibility", "可见性", true, TokenType::STRING, true);
        propRow("alpha", "不透明度", true, TokenType::NUMBER);
        if (type == "Label")
        {
            section("文字样式");
            propRow("textColor", "文字颜色", true, TokenType::COLOR);
        }
    }

    this->contentHeight = cy - startY + 20.0f;

    drawList->PopClipRect();
    if (this->contentHeight > area.h)
    {
        const float maxScroll = this->contentHeight - area.h;
        this->app.inspectorScroll = std::clamp(this->app.inspectorScroll, 0.0f, maxScroll);
        ui::scrollbar(drawList, Rect(area.x + area.w - 7, area.y, 7, area.h), this->app.inspectorScroll,
            this->contentHeight, area.h, palette.scrollbar);
    }
    else
    {
        this->app.inspectorScroll = 0.0f;
    }
}

void InspectorPanel::drawSection(ImDrawList* drawList, const std::string& title, const Rect& area, float& y,
    const std::vector<std::pair<std::string, std::string>>& props)
{
    (void)drawList;
    (void)area;
    (void)y;
    (void)title;
    (void)props;
}

void InspectorPanel::drawPropRow(ImDrawList* drawList, const Rect& area, float& y, const std::string& key,
    const std::string& label, const Json& value, const std::string& valueText, bool isRef,
    const std::string& refName, ImU32 swatch, TokenType type, bool showChain)
{
    (void)drawList;
    (void)area;
    (void)y;
    (void)key;
    (void)label;
    (void)value;
    (void)valueText;
    (void)isRef;
    (void)refName;
    (void)swatch;
    (void)type;
    (void)showChain;
}

void InspectorPanel::drawLocalVars(ImDrawList* drawList, const Rect& area, float& y)
{
    const Palette& palette = paletteOf(this->app);
    Document* document = this->app.doc();
    Json* vars = document != nullptr ? document->vars(this->app.selectedNode) : nullptr;

    Rect title(area.x + 12, y, area.w - 24, 26);
    ui::textVC(drawList, title.x + 2, title.y, title.h, "局部变量", palette.textMuted, 10.0f);
    ui::textRight(drawList, title.x + title.w - 28, title.y + 5, vars != nullptr ? std::to_string(vars->size()) : "0",
        palette.textFaint, 9.5f);
    this->addLocalRect = Rect(title.x + title.w - 22, title.y + 3, 20, 20);
    ui::fill(drawList, this->addLocalRect, palette.panelAlt, 4.0f);
    ui::iconCentered(drawList, this->addLocalRect, "add", palette.textMuted, 13.0f);
    ui::fill(drawList, Rect(title.x + 2, title.y + title.h - 1, title.w - 4, 1), palette.borderSoft);
    y += 30.0f;

    if (vars == nullptr || vars->empty())
    {
        ui::textClipped(drawList, area.x + 14, y, area.w - 28, "当前节点没有局部变量。", palette.textFaint, 10.5f);
        y += 28.0f;
        return;
    }

    for (auto it = vars->begin(); it != vars->end(); ++it)
    {
        const std::string name = it.key();
        const Json& value = it.value();
        std::string ref;
        bool local = false;
        const bool isRef = refNameOf(value, ref, local);
        Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root, this->app.selectedNode,
            value);
        const TokenType type = resolved.is_number() ? TokenType::NUMBER
            : (resolved.is_boolean() ? TokenType::BOOLEAN
                                     : (resolved.is_string() && !resolved.get<std::string>().empty() &&
                                               resolved.get<std::string>()[0] == '#'
                                           ? TokenType::COLOR
                                           : TokenType::STRING));

        Rect row(area.x + 10, y, area.w - 20, 30);
        if (this->app.hovering(row))
            ui::fill(drawList, row, palette.hover, 4.0f);
        ui::textVC(drawList, row.x + 4, row.y, row.h, name, palette.text, 11.0f);
        const float valueX = row.x + 94.0f;
        ImU32 swatch = 0;
        std::string display;
        if (type == TokenType::COLOR)
        {
            const std::string text = resolved.is_string() ? resolved.get<std::string>() : std::string();
            parseColor(text, swatch);
            display = isRef ? ref : text;
        }
        else
        {
            display = isRef ? ref : formatValue(resolved);
        }

        Rect valueRect(valueX, row.y + 4, row.w - 100.0f - 24.0f, row.h - 8);
        const std::string editKey = "localval:" + this->app.selectedNode + ":" + name;
        const bool editing = this->app.edit.active && this->app.edit.key == editKey;
        ui::fill(drawList, valueRect, palette.inputBg, 4.0f);
        ui::stroke(drawList, valueRect, editing ? palette.accent : palette.border, 4.0f);
        if (editing)
            drawInlineValue(drawList, this->app, valueRect, this->app.edit.buffer, this->app.edit.caret,
                palette.text, 10.0f);
        else if (type == TokenType::COLOR)
        {
            Rect swatchRect(valueRect.x + 5, valueRect.y + 5, 12, 12);
            ui::fill(drawList, swatchRect, swatch, 3.0f);
            ui::textClipped(drawList, valueRect.x + 22, valueRect.y + 4, valueRect.w - 26,
                isRef ? (local ? "var · " : "fx · ") + ref : display, isRef ? palette.accent : palette.text, 10.0f);
        }
        else
        {
            ui::textClipped(drawList, valueRect.x + 7, valueRect.y + 4, valueRect.w - 12,
                isRef ? ref : display, isRef ? palette.accent : palette.text, 10.0f);
        }

        RowHit valueHit;
        valueHit.rect = valueRect;
        valueHit.action = "local";
        valueHit.key = name;
        valueHit.type = type;
        this->hits.push_back(valueHit);
        if (type == TokenType::COLOR)
        {
            RowHit swatchHit = valueHit;
            swatchHit.action = "localcolor";
            swatchHit.rect = Rect(valueRect.x + 3, valueRect.y + 3, 16, 16);
            this->hits.push_back(swatchHit);
        }

        Rect del(row.x + row.w - 20, row.y + 5, 18, 20);
        ui::iconCentered(drawList, del, "close", palette.textFaint, 12.0f);
        RowHit delHit;
        delHit.rect = del;
        delHit.action = "localdel";
        delHit.key = name;
        this->hits.push_back(delHit);

        y += 32.0f;
    }
}

void InspectorPanel::drawGlobalRefs(ImDrawList* drawList, const Rect& area, float& y)
{
    const Palette& palette = paletteOf(this->app);
    Document* document = this->app.doc();
    const Json* node = document != nullptr ? document->node(this->app.selectedNode) : nullptr;
    const std::vector<std::string> refs = node != nullptr ? collectGlobalRefs(*node) : std::vector<std::string>();

    Rect title(area.x + 12, y, area.w - 24, 26);
    ui::textVC(drawList, title.x + 2, title.y, title.h, "全局变量引用", palette.textMuted, 10.0f);
    ui::textRight(drawList, title.x + title.w - 4, title.y + 5, std::to_string(refs.size()), palette.textFaint, 9.5f);
    ui::fill(drawList, Rect(title.x + 2, title.y + title.h - 1, title.w - 4, 1), palette.borderSoft);
    y += 30.0f;

    if (refs.empty())
    {
        ui::textClipped(drawList, area.x + 14, y, area.w - 28, "该节点没有引用全局变量。", palette.textFaint, 10.5f);
        y += 28.0f;
        return;
    }

    for (const std::string& path : refs)
    {
        const Token* token = this->app.project.getToken(path);
        Rect row(area.x + 10, y, area.w - 20, 28);
        if (this->app.hovering(row))
            ui::fill(drawList, row, palette.hover, 4.0f);
        ImU32 swatch = 0;
        if (token != nullptr && token->type == TokenType::COLOR)
        {
            parseColor(token->value.is_string() ? token->value.get<std::string>() : std::string(), swatch);
            Rect sw(row.x + 4, row.y + 8, 12, 12);
            ui::fill(drawList, sw, swatch, 3.0f);
            ui::stroke(drawList, sw, palette.border, 3.0f);
        }
        else
        {
            ui::icon(drawList, row.x + 3, row.y + 7, "link", palette.textFaint, 13.0f);
        }
        ui::textClipped(drawList, row.x + 22, row.y + 7, row.w - 130, path, palette.accent, 10.5f);
        if (token != nullptr)
            ui::textRight(drawList, row.x + row.w - 6, row.y + 8, formatValue(token->value), palette.textFaint, 9.5f);
        RowHit hit;
        hit.rect = row;
        hit.action = "ref";
        hit.value = path;
        this->hits.push_back(hit);
        y += 30.0f;
    }
}

void InspectorPanel::handleInput(UiInput& input)
{
    const Rect self(this->getX(), this->getY(), this->getWidth(), this->getHeight());
    if (!self.contains(input.mouse)) return;

    Document* document = this->app.doc();
    const bool appearance = document != nullptr && !document->root.contains("type") &&
        (document->root.contains("layers") || document->root.contains("box") ||
            document->root.contains("import"));
    if (appearance)
    {
        if (this->contentArea.contains(input.mouse) && input.wheel != 0.0f)
        {
            const float maxScroll = std::max(0.0f, this->contentHeight - this->contentArea.h);
            this->app.inspectorScroll = std::clamp(this->app.inspectorScroll - input.wheel * 34.0f,
                0.0f, maxScroll);
            return;
        }
        for (const RowHit& hit : this->appearanceHits)
        {
            if (!hit.rect.contains(input.mouse)) continue;
            if (!input.pressed) return;
            const Json* value = document->valueAt(hit.key);
            if (hit.action == "appearance-color")
            {
                ImU32 color = 0;
                if (value != nullptr)
                {
                    Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                        std::string(), *value);
                    if (resolved.is_string()) parseColor(resolved.get<std::string>(), color);
                }
                if (color == 0) color = IM_COL32(59, 130, 246, 255);
                this->app.openColorPopup("appearance-color:" + hit.key, hit.rect, color);
                return;
            }
            if (hit.action == "appearance-clearref")
            {
                if (value != nullptr)
                {
                    Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                        std::string(), *value);
                    document->setValueAt(hit.key, resolved);
                    this->app.markDocumentChanged();
                }
                return;
            }
            if (hit.action == "appearance-toggle")
            {
                const bool on = value != nullptr && ((value->is_boolean() && value->get<bool>()) ||
                    (value->is_string() && value->get<std::string>() == "true"));
                document->setValueAt(hit.key, !on);
                this->app.markDocumentChanged();
                return;
            }
            if (hit.action == "appearance-enum")
            {
                std::vector<MenuItem> items;
                std::stringstream stream(hit.value);
                std::string option;
                while (std::getline(stream, option, '|'))
                {
                    MenuItem item;
                    item.label = option;
                    item.action = "set_appearance";
                    item.value = option;
                    items.push_back(item);
                }
                this->app.openPopup("appearance-enum:" + hit.key, hit.rect, items, 190.0f);
                return;
            }
            const std::string type = hit.type == TokenType::NUMBER ? "number" :
                (hit.type == TokenType::BOOLEAN ? "bool" : "text");
            this->app.beginEdit("appearance:" + type + ":" + hit.key,
                value != nullptr ? styleValueText(*value) : std::string(), hit.rect,
                hit.type == TokenType::NUMBER);
            return;
        }
        return;
    }

    if (document != nullptr && !document->root.contains("type"))
        return;

    for (int i = 0; i < 2; ++i)
    {
        if (this->tabRects[i].contains(input.mouse) && input.pressed)
        {
            this->app.inspectorTab = (InspectorTab)i;
            this->app.inspectorScroll = 0.0f;
            return;
        }
    }

    if (this->contentArea.contains(input.mouse))
    {
        if (input.wheel != 0.0f)
        {
            const float maxScroll = std::max(0.0f, this->contentHeight - this->contentArea.h);
            this->app.inspectorScroll = std::clamp(this->app.inspectorScroll - input.wheel * 34.0f, 0.0f, maxScroll);
            return;
        }
    }

    if (this->addLocalRect.contains(input.mouse))
    {
        this->app.tooltip = "新建局部变量";
        if (input.pressed) this->app.openDialog("new_local");
        return;
    }

    for (const RowHit& hit : this->hits)
    {
        if (!hit.rect.contains(input.mouse)) continue;
        if (!input.pressed) return;
        Document* document = this->app.doc();
        if (document == nullptr) return;

        if (hit.action == "prop")
        {
            if (this->app.edit.active && this->app.edit.key == "prop:" + this->app.selectedNode + ":" + hit.key)
                return;
            const Json* style = document->style(this->app.selectedNode);
            const Json* value = style != nullptr && style->contains(hit.key) ? &(*style)[hit.key] : nullptr;
            std::string ref;
            bool local = false;
            if (value != nullptr && refNameOf(*value, ref, local))
            {
                this->app.openPopup("varpick:" + this->app.selectedNode + ":" + hit.key, hit.rect,
                    this->variablePickerItems(hit.key), 310.0f);
                return;
            }
            if (hit.type == TokenType::COLOR)
            {
                ImU32 color = 0;
                if (value != nullptr)
                {
                    Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                        this->app.selectedNode, *value);
                    parseColor(resolved.is_string() ? resolved.get<std::string>() : std::string(), color);
                }
                if (color == 0) color = IM_COL32(59, 130, 246, 255);
                this->app.openColorPopup("color:prop:" + this->app.selectedNode + ":" + hit.key, hit.rect, color);
                return;
            }
            std::string current = value != nullptr ? styleValueText(*value) : std::string();
            this->app.beginEdit("prop:" + this->app.selectedNode + ":" + hit.key, current, hit.rect,
                hit.type == TokenType::NUMBER);
            return;
        }
        if (hit.action == "swatch")
        {
            const Json* style = document->style(this->app.selectedNode);
            const Json* value = style != nullptr && style->contains(hit.key) ? &(*style)[hit.key] : nullptr;
            std::string ref;
            bool local = false;
            if (value != nullptr && refNameOf(*value, ref, local))
            {
                this->app.openPopup("varpick:" + this->app.selectedNode + ":" + hit.key, hit.rect,
                    this->variablePickerItems(hit.key), 310.0f);
                return;
            }
            ImU32 color = 0;
            if (value != nullptr)
            {
                Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                    this->app.selectedNode, *value);
                parseColor(resolved.is_string() ? resolved.get<std::string>() : std::string(), color);
            }
            if (color == 0) color = IM_COL32(59, 130, 246, 255);
            this->app.openColorPopup("color:prop:" + this->app.selectedNode + ":" + hit.key, hit.rect, color);
            return;
        }
        if (hit.action == "toggle")
        {
            const Json* style = document->style(this->app.selectedNode);
            const Json* value = style != nullptr && style->contains(hit.key) ? &(*style)[hit.key] : nullptr;
            Json resolved = value != nullptr ? PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                                                    this->app.selectedNode, *value) : Json(false);
            const bool current = resolved.is_boolean() ? resolved.get<bool>() : false;
            this->app.setStyleLiteral(this->app.selectedNode, hit.key, !current);
            return;
        }
        if (hit.action == "enum")
        {
            std::vector<MenuItem> items;
            std::vector<std::string> options;
            if (hit.key == "axis") options = { "row", "column" };
            else if (hit.key == "direction") options = { "inherit", "ltr", "rtl" };
            else if (hit.key == "justifyContent")
                options = { "flexStart", "center", "flexEnd", "spaceBetween", "spaceAround", "spaceEvenly" };
            else if (hit.key == "alignItems")
                options = { "flexStart", "center", "flexEnd", "stretch", "baseline", "auto" };
            else if (hit.key == "alignSelf")
                options = { "auto", "flexStart", "center", "flexEnd", "stretch", "baseline" };
            else if (hit.key == "alignContent")
                options = { "flexStart", "center", "flexEnd", "stretch", "spaceBetween", "spaceAround" };
            else if (hit.key == "horizontalAlign") options = { "left", "center", "right" };
            else if (hit.key == "verticalAlign") options = { "top", "center", "bottom" };
            else if (hit.key == "scalingType") options = { "fit", "fill", "stretch", "center" };
            else if (hit.key == "position") options = { "relative", "absolute" };
            else if (hit.key == "visibility") options = { "visible", "invisible", "gone" };
            else if (hit.key == "iconFont") options = { "material", "switch" };
            const Json* style = document->style(this->app.selectedNode);
            if (style != nullptr && style->contains(hit.key))
            {
                Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                    this->app.selectedNode, (*style)[hit.key]);
                (void)resolved;
            }
            for (const std::string& option : options)
            {
                MenuItem item;
                item.label = option;
                item.action = "set_enum";
                item.value = option;
                items.push_back(item);
            }
            this->app.openPopup("enum:" + this->app.selectedNode + ":" + hit.key, hit.rect, items, 170.0f);
            return;
        }
        if (hit.action == "chain")
        {
            this->app.openPopup("varpick:" + this->app.selectedNode + ":" + hit.key, hit.rect,
                this->variablePickerItems(hit.key), 310.0f);
            return;
        }
        if (hit.action == "local")
        {
            if (hit.type == TokenType::COLOR)
            {
                Json* vars = document->vars(this->app.selectedNode);
                if (vars != nullptr && vars->contains(hit.key))
                {
                    ImU32 color = 0;
                    Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                        this->app.selectedNode, (*vars)[hit.key]);
                    parseColor(resolved.is_string() ? resolved.get<std::string>() : std::string(), color);
                    this->app.openColorPopup("color:local:" + this->app.selectedNode + ":" + hit.key, hit.rect, color);
                    return;
                }
            }
            const Json* vars = document->vars(this->app.selectedNode);
            const Json* value = vars != nullptr && vars->contains(hit.key) ? &(*vars)[hit.key] : nullptr;
            std::string ref;
            bool local = false;
            if (value != nullptr && refNameOf(*value, ref, local))
            {
                this->app.beginEdit("localval:" + this->app.selectedNode + ":" + hit.key, ref, hit.rect);
                return;
            }
            this->app.beginEdit("localval:" + this->app.selectedNode + ":" + hit.key,
                value != nullptr ? styleValueText(*value) : std::string(), hit.rect, hit.type == TokenType::NUMBER);
            return;
        }
        if (hit.action == "localcolor")
        {
            Json* vars = document->vars(this->app.selectedNode);
            if (vars != nullptr && vars->contains(hit.key))
            {
                ImU32 color = 0;
                Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                    this->app.selectedNode, (*vars)[hit.key]);
                parseColor(resolved.is_string() ? resolved.get<std::string>() : std::string(), color);
                this->app.openColorPopup("color:local:" + this->app.selectedNode + ":" + hit.key, hit.rect, color);
            }
            return;
        }
        if (hit.action == "localdel")
        {
            if (document->eraseLocalVar(this->app.selectedNode, hit.key))
            {
                this->app.markDocumentChanged();
                this->app.addToast("已删除局部变量 " + hit.key);
            }
            return;
        }
        if (hit.action == "ref")
        {
            this->app.selectToken(hit.value);
            this->app.addToast("已选中全局变量 " + hit.value);
            return;
        }
        if (hit.action == "clearref")
        {
            const Json* style = document->style(this->app.selectedNode);
            if (style != nullptr && style->contains(hit.key))
            {
                Json resolved = PreviewBuilder::resolveStyleValue(this->app.project, document->root,
                    this->app.selectedNode, (*style)[hit.key]);
                this->app.setStyleLiteral(this->app.selectedNode, hit.key, resolved);
            }
            return;
        }
        return;
    }
}

std::vector<MenuItem> InspectorPanel::variablePickerItems(const std::string& key)
{
    std::vector<MenuItem> items;
    const TokenType wanted = this->app.propType(key);
    MenuItem clear;
    clear.label = "解除引用";
    clear.action = "set_ref";
    clear.value = "$none";
    items.push_back(clear);

    Document* document = this->app.doc();
    if (document != nullptr)
    {
        const Json* vars = document->vars(std::string());
        for (const char* group : { "colors", "numbers" })
        {
            const TokenType type = std::string(group) == "colors" ? TokenType::COLOR : TokenType::NUMBER;
            if (!this->app.isTokenCompatible(type, wanted)) continue;
            if (vars == nullptr || !vars->contains(group) || !(*vars)[group].is_object()) continue;
            for (auto it = (*vars)[group].begin(); it != (*vars)[group].end(); ++it)
            {
                MenuItem item;
                item.label = std::string("局部 · ") + (type == TokenType::COLOR ? "&color." : "&number.") + it.key();
                item.action = "set_ref";
                item.value = (type == TokenType::COLOR ? "&color." : "&number.") + it.key();
                items.push_back(item);
            }
        }
    }

    for (const Token& token : this->app.project.tokens)
    {
        if (!this->app.isTokenCompatible(token.type, wanted)) continue;
        MenuItem item;
        item.label = std::string("全局 · ") + token.path;
        item.action = "set_ref";
        item.value = token.type == TokenType::COLOR ? ("@theme/" + token.path) : ("@style/" + token.path);
        items.push_back(item);
        if (items.size() > 180) break;
    }
    return items;
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

StatusBarPanel::StatusBarPanel(DesignerApp& app)
    : DesignerPanel(app, "status")
{
}

void StatusBarPanel::draw(ImDrawList* drawList, float x, float y, float width, float height,
    bklv::FrameContext* ctx)
{
    (void)ctx;
    const Palette& palette = paletteOf(this->app);
    ui::fill(drawList, Rect(x, y, width, height), palette.header);
    ui::hline(drawList, y, x, x + width, palette.border);

    Document* document = this->app.doc();
    const bool dirty = document != nullptr && document->dirty;
    const bool error = !this->app.previewError.empty();
    ui::dot(drawList, x + 15, y + height * 0.5f, 3.2f, error ? palette.red : (dirty ? palette.yellow : palette.green));
    std::string status = error ? "预览异常" : (dirty ? "未保存更改" : "就绪");
    if (document != nullptr)
        status += "  ·  " + document->relative;
    ui::textVC(drawList, x + 25, y, height, status, palette.textMuted, 10.0f);

    if (this->app.hasDoc())
    {
        const std::string selection = this->app.nodeBreadcrumb();
        ui::textClipped(drawList, x + width * 0.32f, y + 5, width * 0.4f, selection, palette.textFaint, 9.5f);
        const std::string right = "bklv 实时渲染  ·  " + std::to_string(this->app.canvasResolutionW) + "×" +
            std::to_string(this->app.canvasResolutionH) + "  ·  " + formatZoom(this->app.canvasZoom) +
            "  ·  " + std::to_string((int)this->app.files.size()) + " 文件";
        ui::textRight(drawList, x + width - 14, y + 5, right, palette.textFaint, 9.5f);
    }
    else
    {
        ui::textRight(drawList, x + width - 14, y + 5, "未打开文档", palette.textFaint, 9.5f);
    }
}

// ---------------------------------------------------------------------------
// Overlay
// ---------------------------------------------------------------------------

OverlayPanel::OverlayPanel(DesignerApp& app)
    : DesignerPanel(app, "overlay")
{
    this->setClipsToBounds(false);
}

void OverlayPanel::draw(ImDrawList* drawList, float x, float y, float width, float height, bklv::FrameContext* ctx)
{
    (void)ctx;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    this->app.drawOverlay(drawList);
}

void OverlayPanel::handleInput(UiInput& input)
{
    (void)input;
}

} // namespace designer
