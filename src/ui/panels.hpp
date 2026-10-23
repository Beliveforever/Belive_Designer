#pragma once

#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "ui/app.hpp"
#include "ui/preview.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace designer
{

struct RowHit
{
    Rect rect;
    std::string action;
    std::string key;
    std::string value;
    TokenType type = TokenType::STRING;
};

struct TabRect
{
    Rect rect;
    Rect closeRect;
    int index = 0;
};

class MenuBarPanel : public DesignerPanel
{
  public:
    explicit MenuBarPanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);

  private:
    Rect menuRect(int index) const;
    Rect iconButtonRect(int index) const;
};

class ProjectPanel : public DesignerPanel
{
  public:
    explicit ProjectPanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);

  private:
    void buildRows();
    std::vector<FileRow> rows;
    float contentHeight = 0.0f;
    Rect listArea;
    int hovered = -1;
    Rect addRect;
    Rect refreshRect;
    Rect searchRect;
    int contextIndex = -1;
};

class GlobalVarsPanel : public DesignerPanel
{
  public:
    explicit GlobalVarsPanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);
    void buildRows();

  private:
    std::vector<TokenRow> rows;
    float contentHeight = 0.0f;
    Rect listArea;
    Rect detailsArea;
    Rect addRect;
    Rect renameRect;
    Rect deleteRect;
    Rect tabRects[4];
    Rect typeRects[4];
    int hovered = -1;
    int contextIndex = -1;
};

class NodeTreePanel : public DesignerPanel
{
  public:
    explicit NodeTreePanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);
    void buildRows();
    const std::vector<TreeRow>& getRows() const { return this->rows; }
    const Rect& getListArea() const { return this->listArea; }
    void applyJsonCode();
    void setTab(int value);

  private:
    void drawCodeView(ImDrawList* drawList, const Rect& area);
    void handleCodeInput(UiInput& input, const Rect& area);
    void handleCodeKeys(UiInput& input);
    void syncCodeFromDocument();

    std::vector<TreeRow> rows;
    float contentHeight = 0.0f;
    Rect listArea;
    Rect addRect;
    Rect duplicateRect;
    Rect deleteRect;
    int tab = 0;
    Rect tabRects[2];
    Rect codeToolbar;
    Rect codeNodeTab;
    Rect codeDocTab;
    Rect codeEditButton;
    Rect codeApplyButton;
    Rect codeRevertButton;
    Rect codeArea;
    Rect codeTabs[2];
    int hovered = -1;
};

class DocTabsPanel : public DesignerPanel
{
  public:
    explicit DocTabsPanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);

  private:
    std::vector<TabRect> tabs;
};

class CanvasToolbarPanel : public DesignerPanel
{
  public:
    explicit CanvasToolbarPanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);

  private:
    Rect modeDesign;
    Rect modePreview;
    Rect ratioRect;
    Rect resolutionRect;
    Rect zoomOut;
    Rect zoomValueRect;
    Rect zoomIn;
    Rect fitRect;
    Rect gridRect;
};

class CanvasPanel : public bklv::Box
{
  public:
    explicit CanvasPanel(DesignerApp& app);
    DesignerApp& app;
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void drawChildren(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);
    void rebuildPreview();
    void clearPreview();
    float fitZoom() const;

    PreviewResult preview;
    bklv::View* previewRootView = nullptr;
    bool built = false;
    std::string builtDoc;
    float builtZoom = -1.0f;
    float builtPanX = 0.0f;
    float builtPanY = 0.0f;
    int builtW = 0;
    int builtH = 0;
    int builtRevision = -1;
    float lastPanelW = -1.0f;
    float lastPanelH = -1.0f;
    Rect canvasArea;
    Rect welcomeOpenRect;
    Rect welcomeCopyRect;
};

class InspectorPanel : public DesignerPanel
{
  public:
    explicit InspectorPanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);

  private:
    void buildRows();
    void drawNodeCard(ImDrawList* drawList, const Rect& area, float& y);
    void drawSection(ImDrawList* drawList, const std::string& title, const Rect& area, float& y,
        const std::vector<std::pair<std::string, std::string>>& props);
    void drawPropRow(ImDrawList* drawList, const Rect& area, float& y, const std::string& key,
        const std::string& label, const Json& value, const std::string& valueText, bool isRef,
        const std::string& refName, ImU32 swatch, TokenType type, bool showChain);
    void drawLocalVars(ImDrawList* drawList, const Rect& area, float& y);
    void drawGlobalRefs(ImDrawList* drawList, const Rect& area, float& y);
    void drawAppearanceEditor(ImDrawList* drawList, float x, float y, float width, float height);
    std::vector<MenuItem> variablePickerItems(const std::string& key);

    std::vector<RowHit> hits;
    std::vector<RowHit> appearanceHits;
    std::string lastAppearanceSelection;
    std::vector<Rect> chipRects;
    Rect contentArea;
    Rect tabRects[2];
    Rect addLocalRect;
    float contentHeight = 0.0f;
};

class StatusBarPanel : public DesignerPanel
{
  public:
    explicit StatusBarPanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
};

class OverlayPanel : public DesignerPanel
{
  public:
    explicit OverlayPanel(DesignerApp& app);
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        bklv::FrameContext* ctx) override;
    void handleInput(UiInput& input);
};

} // namespace designer
