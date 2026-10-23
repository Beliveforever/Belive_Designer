#pragma once

#include <deque>
#include <mutex>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/view.hpp"
#include "bklv/core/app.hpp"
#include "core/document.hpp"
#include "core/model.hpp"
#include "ui/preview.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace designer
{

class DesignerApp;
class ProjectPanel;
class GlobalVarsPanel;
class NodeTreePanel;
class DocTabsPanel;
class CanvasToolbarPanel;
class CanvasPanel;
class InspectorPanel;
class StatusBarPanel;
class OverlayPanel;
class MenuBarPanel;

enum class InspectorTab
{
    PROPERTIES,
    STYLE,
};

enum class VarScope
{
    LOCAL,
    GLOBAL,
};

struct MenuItem
{
    std::string label;
    std::string shortcut;
    std::string icon;
    std::string action;
    std::string value;
    bool checked = false;
    bool disabled = false;
    bool separator = false;
    bool danger = false;
};

struct PopupState
{
    bool open = false;
    std::string id;
    Rect anchor;
    std::vector<MenuItem> items;
    float width = 220.0f;
    float maxHeight = 430.0f;
    float scroll = 0.0f;
    int hover = -1;
    bool alignRight = false;
};

struct ColorPopup
{
    bool open = false;
    std::string id;        // color:prop:<node>:<key> | color:token:<path> | color:local:<node>:<name>
    Rect anchor;
    ImU32 initial = IM_COL32(59, 130, 246, 255);
    float h = 0.6f;
    float s = 0.8f;
    float v = 0.9f;
    bool dragSv = false;
    bool dragHue = false;
    bool pushedUndo = false;
};

struct Dialog
{
    bool open = false;
    std::string kind;
    std::string title;
    std::string message;
    std::string label1;
    std::string label2;
    std::string input1;
    std::string input2;
    std::string error;
    std::string context;
    std::string confirmText = "确定";
    std::string cancelText = "取消";
    std::vector<std::string> choices; // extra list (project names / save choices)
    int selectedChoice = -1;
    int focus = 0;
    bool danger = false;
    bool alertOnly = false;
};

struct Toast
{
    std::string text;
    double start = 0.0;
    float duration = 3.2f;
    bool error = false;
};

struct TreeRow
{
    Rect rect;
    std::string pointer;
    std::string type;
    std::string name;
    std::string id;
    std::string sourceFile;
    std::string sourcePointer;
    int depth = 0;
    bool hasChildren = false;
    bool expanded = false;
    float contentY = 0.0f;
};

struct TokenRow
{
    Rect rect;
    std::string tokenPath;
    TokenType type = TokenType::STRING;
    bool isGroup = false;
    int depth = 0;
    std::string label;
    std::string valueText;
    ImU32 swatch = 0;
    bool local = false;
};

struct DropTarget
{
    Rect rect;
    std::string node;
    std::string key;
    TokenType type = TokenType::STRING;
};

struct FileRow
{
    Rect rect;
    std::string relative;
    std::string name;
    bool isDirectory = false;
    int depth = 0;
    bool expanded = false;
};

class DesignerPanel : public bklv::View
{
  public:
    DesignerPanel(DesignerApp& app, const std::string& id);
    virtual void handleInput(UiInput& input) { (void)input; }
    std::string getClassString() const override { return "designer::Panel"; }

  protected:
    DesignerApp& app;
};

class DesignerApp
{
  public:
    DesignerApp();

    void attach(bklv::App& app);
    bklv::View* rootView() const { return this->root; }
    bklv::App* host = nullptr;

    void tick();
    void drawOverlay(ImDrawList* drawList);
    void overlayInput(UiInput& input);
    void drawTooltip(ImDrawList* drawList);

    // ---- model ----
    Project project;
    std::vector<std::unique_ptr<Document>> documents;
    int activeDoc = -1;
    std::string selectedNode;
    std::string selectedToken;
    TokenType tabTokenType = TokenType::COLOR;
    std::vector<FileEntry> files;
    std::set<std::string> expandedDirs;
    std::set<std::string> expandedNodes;
    std::map<std::string, std::set<std::string>> docExpandedNodes;
    std::set<std::string> expandedVarGroups;
    std::set<std::string> collapsedVarGroups;
    std::string fileFilter;

    Document* doc();
    const Document* doc() const;
    bool hasDoc() const { return this->activeDoc >= 0 && this->activeDoc < (int)this->documents.size(); }
    int findDocument(const std::string& relative) const;

    enum class NativePickMode { NONE, OPEN_PROJECT, COPY_SOURCE, COPY_DESTINATION };
    NativePickMode nativePickMode = NativePickMode::NONE;
    std::mutex nativeMutex;
    std::string nativePickedPath;
    bool nativePickedReady = false;
    bool nativePickedCancelled = false;
    std::string copySourcePath;
    void requestProjectFolder(NativePickMode mode);
    void pollNativeFolder();
    void copyProjectTo(const fs::path& source, const fs::path& destinationParent);

    void openProject(const fs::path& root, bool report = true);
    void refreshFiles();
    void openDocument(const std::string& relative, bool selectDefault = true);
    void closeDocument(int index, bool force = false);
    void switchDocument(int index);
    void selectDefaultNode();
    void selectNode(const std::string& pointer, bool reveal = true);
    void openImportedNode(const std::string& relativeFile, const std::string& pointer);
    void addAppearanceLayer(const std::string& type);
    void importAppearanceLayer(const std::string& relative);
    void deleteAppearanceLayer();
    void duplicateAppearanceLayer();
    void openAppearanceImportPicker();
    void selectNodeById(const std::string& id);
    void selectToken(const std::string& path);
    void syncVariableScope();

    void saveActive(bool saveAs);
    void reloadActive();
    void newDocument(const std::string& kind);
    void duplicateSelected();
    void deleteSelected();
    void moveSelectedNode(int direction);
    void addChildNode(const std::string& type);

    void undo();
    void redo();

    void markDocumentChanged();
    void requestPreview(bool fit);
    void fitCanvas();

    // ---- editing ----
    struct EditState
    {
        bool active = false;
        std::string key;
        std::string buffer;
        int caret = 0;
        bool numeric = false;
        bool multiline = false;
        Rect rect;
    } edit;

    void beginEdit(const std::string& key, const std::string& initial, const Rect& rect, bool numeric = false,
        bool multiline = false);
    void commitEdit(bool cancel);
    void handleEditKeys(UiInput& input);
    void setCaretFromPoint(float x);

    // ---- ui state ----
    const Palette* palette = nullptr;
    bool lightTheme = true;

    float leftWidth = 280.0f;
    float varsWidth = 244.0f;
    float rightWidth = 310.0f;
    float projectHeight = 330.0f;
    float textScale = 1.15f;
    bool showProject = true;
    bool showVars = true;
    bool showNodeTree = true;
    bool showInspector = true;
    bool showStatus = true;

    float appliedLeftWidth = -1;
    float appliedVarsWidth = -1;
    float appliedRightWidth = -1;
    float appliedProjectHeight = -1;

    int menuIndex = -1; // 0 file 1 edit 2 view 3 settings
    PopupState menuPopup;
    PopupState pickerPopup;
    ColorPopup colorPopup;
    Dialog dialog;
    std::deque<Toast> toasts;
    std::string tooltip;
    ImVec2 tooltipPos = ImVec2(0, 0);
    ImVec2 mousePosition = ImVec2(0, 0);
    std::string dialogContext;

    struct DragState
    {
        bool active = false;
        bool pressed = false;
        ImVec2 start = ImVec2(0, 0);
        std::string tokenPath;
        TokenType tokenType = TokenType::STRING;
        Rect sourceRect;
    } drag;

    std::vector<DropTarget> dropTargets;

    // canvas
    float canvasZoom = 0.75f;
    float canvasPanX = 0.0f;
    float canvasPanY = 0.0f;
    bool canvasFit = true;
    bool canvasPreviewMode = false;
    bool showGrid = true;
    bool showGuides = false;
    int canvasResolutionW = 1280;
    int canvasResolutionH = 720;
    bool previewDirty = true;
    int documentRevision = 0;
    std::string previewError;
    std::string previewWarning;
    float jsonPreviewScroll = 0.0f;
    std::string hoverNode;
    bool canvasPanning = false;
    ImVec2 canvasPanStart = ImVec2(0, 0);

    int resizeMode = 0;
    float resizeStartMouse = 0.0f;
    float resizeStartValue = 0.0f;

    mutable std::map<std::string, int> refCounts;
    mutable bool refCountsDirty = true;
    bool initialised = false;
    bool scrollNodeIntoView = false;
    struct ScriptedClick
    {
        int frame = 0;
        ImVec2 position = ImVec2(0, 0);
    };
    std::vector<ScriptedClick> scriptedClicks;
    struct ScriptedEdit
    {
        int frame = 0;
        std::string key;
        std::string text;
        bool commit = true;
    };
    std::vector<ScriptedEdit> scriptedEdits;
    struct ScriptedAction
    {
        int frame = 0;
        std::string action;
        std::string value;
    };
    std::vector<ScriptedAction> scriptedActions;
    struct ScriptedDrag
    {
        int frame = 0;
        ImVec2 from = ImVec2(0, 0);
        ImVec2 to = ImVec2(0, 0);
    };
    std::vector<ScriptedDrag> scriptedDrags;
    struct ScriptedCode
    {
        int frame = 0;
        std::string text;
    };
    std::vector<ScriptedCode> scriptedCodes;
    int frameCounter = 0;

    // bottom panel
    float nodeTreeScroll = 0.0f;
    float tokenScroll = 0.0f;
    float projectScroll = 0.0f;
    float inspectorScroll = 0.0f;
    float blueprintPanX = 0.0f;
    float blueprintPanY = 0.0f;
    float blueprintZoom = 1.0f;
    bool blueprintPanning = false;
    ImVec2 blueprintPanStart = ImVec2(0, 0);

    // inspector
    InspectorTab inspectorTab = InspectorTab::PROPERTIES;
    VarScope varScope = VarScope::LOCAL;
    std::set<std::string> collapsedSections;

    // json editor
    struct CodeEditor
    {
        bool active = false;
        bool dirty = false;
        std::string text;
        std::string original;
        int caret = 0;
        float scroll = 0.0f;
        float scrollX = 0.0f;
        std::string error;
        bool showDocument = true;
        Rect area;
    } code;

    // tree drag
    struct NodeDrag
    {
        bool active = false;
        bool pressed = false;
        ImVec2 start = ImVec2(0, 0);
        std::string pointer;
        std::string dropTarget;
        int dropMode = 0; // 0 before, 1 inside, 2 after
    } nodeDrag;

    // panels
    MenuBarPanel* menuBar = nullptr;
    ProjectPanel* projectPanel = nullptr;
    GlobalVarsPanel* varsPanel = nullptr;
    NodeTreePanel* leftNodePanel = nullptr;
    DocTabsPanel* tabsPanel = nullptr;
    CanvasToolbarPanel* toolbarPanel = nullptr;
    CanvasPanel* canvasPanel = nullptr;
    InspectorPanel* inspectorPanel = nullptr;
    StatusBarPanel* statusPanel = nullptr;
    OverlayPanel* overlayPanel = nullptr;

    bklv::Box* root = nullptr;
    bklv::Box* body = nullptr;
    bklv::Box* leftColumn = nullptr;
    bklv::Box* varsColumn = nullptr;
    bklv::Box* centerColumn = nullptr;
    bklv::Box* rightColumn = nullptr;

    // actions
    void runAction(const std::string& action, const std::string& value = std::string());
    void openMenu(int index, const Rect& anchor);
    void openPopup(const std::string& id, const Rect& anchor, std::vector<MenuItem> items, float width = 220.0f,
        bool alignRight = false);
    void closePopups();
    void choosePopup(const MenuItem& item);
    void openDialog(const std::string& kind);
    void confirmDialog();
    void addToast(const std::string& text, bool error = false);
    void saveAll();
    Rect popupRect(const PopupState& popup, int itemCount) const;
    int popupHit(const PopupState& popup, ImVec2 point) const;
    void openColorPopup(const std::string& id, const Rect& anchor, ImU32 color);
    void applyColor(ImU32 color);
    void setStyleLiteral(const std::string& node, const std::string& key, const Json& value, bool record = true);
    void setLocalLiteral(const std::string& node, const std::string& name, const Json& value, bool record = true);
    void beginVariableDrop(const std::string& token, TokenType type, const Rect& source);
    void dropVariable();
    bool isTokenCompatible(TokenType token, TokenType target) const;
    TokenType propType(const std::string& key) const;
    void setPropertyReference(const std::string& node, const std::string& key, const std::string& tokenPath,
        bool local);
    void notifyError(const std::string& text);
    bool hovering(const Rect& rect) const;
    void syncInputModal();
    bool inputModalPushed = false;
    bool hasProject() const { return !this->project.root.empty(); }
    bool projectIsValid(const fs::path& root) const;

    // files / dialogs helpers
    std::vector<std::string> listProjectDirectories() const;
    std::string activeDocRelative() const;
    void applyThemePalette();
    void applyLayout();
    void handleShortcuts(UiInput& input);
    void drawSplitters(ImDrawList* drawList);
    void handleSplitters(UiInput& input);
    std::string nodeLabel(const Node& node) const;
    std::string nodeBreadcrumb() const;
    int tokenRefCount(const std::string& tokenPath) const;
    bool renameTokenGlobal(const std::string& oldPath, const std::string& newPath, std::string& error);
    const Json* selectedNodeJson() const;
    Json* selectedStyleMutable();
    void tokenCountsRefresh() const;
    void ensureNodeExpanded(const std::string& pointer);
    void scrollToNode(const std::string& pointer);
};

} // namespace designer
