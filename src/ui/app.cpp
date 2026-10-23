#include "ui/app.hpp"

#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>

#include <SDL3/SDL_dialog.h>
#include <imgui.h>

#include "bklv/core/app.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/ui_scale.hpp"
#include "ui/panels.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace designer
{

namespace
{

void SDLCALL onNativeFolderPicked(void* userdata, const char* const* files, int)
{
    auto* self = static_cast<DesignerApp*>(userdata);
    if (self == nullptr) return;
    std::lock_guard<std::mutex> lock(self->nativeMutex);
    self->nativePickedPath = (files != nullptr && files[0] != nullptr) ? files[0] : std::string();
    self->nativePickedCancelled = (files != nullptr && files[0] == nullptr);
    self->nativePickedReady = true;
}

} // namespace


namespace
{

std::string readClipboard()
{
    const char* text = ImGui::GetClipboardText();
    return text != nullptr ? std::string(text) : std::string();
}

void setClipboard(const std::string& text)
{
    ImGui::SetClipboardText(text.c_str());
}

std::string baseName(const std::string& path)
{
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string directoryOf(const std::string& path)
{
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

bool validIdentifier(const std::string& text)
{
    if (text.empty()) return false;
    for (char ch : text)
        if (!(std::isalnum((unsigned char)ch) || ch == '_' || ch == '-' || ch == '.'))
            return false;
    return true;
}

Json* navigateJson(Json& value, const std::vector<std::string>& segments)
{
    Json* current = &value;
    for (const std::string& segment : segments)
    {
        if (!current->is_object()) return nullptr;
        if (!current->contains(segment)) return nullptr;
        current = &(*current)[segment];
    }
    return current;
}

bool setDotted(Json& value, const std::vector<std::string>& segments, const Json& newValue)
{
    if (segments.empty()) return false;
    Json* current = &value;
    for (size_t i = 0; i + 1 < segments.size(); ++i)
    {
        if (!current->is_object()) return false;
        if (!current->contains(segments[i]) || !(*current)[segments[i]].is_object())
            (*current)[segments[i]] = Json::object();
        current = &(*current)[segments[i]];
    }
    if (!current->is_object()) return false;
    (*current)[segments.back()] = newValue;
    return true;
}

bool eraseDotted(Json& value, const std::vector<std::string>& segments)
{
    if (segments.empty()) return false;
    Json* parent = &value;
    for (size_t i = 0; i + 1 < segments.size(); ++i)
    {
        if (!parent->is_object() || !parent->contains(segments[i])) return false;
        parent = &(*parent)[segments[i]];
    }
    if (!parent->is_object() || !parent->contains(segments.back())) return false;
    parent->erase(segments.back());
    return true;
}

std::string trim(const std::string& text)
{
    size_t begin = 0, end = text.size();
    while (begin < end && std::isspace((unsigned char)text[begin])) begin++;
    while (end > begin && std::isspace((unsigned char)text[end - 1])) end--;
    return text.substr(begin, end - begin);
}

void countRefsInto(const Json& value, std::map<std::string, int>& counts)
{
    if (value.is_object())
    {
        if (value.contains("$ref") && value["$ref"].is_string())
            counts[value["$ref"].get<std::string>()]++;
        for (auto it = value.begin(); it != value.end(); ++it)
            countRefsInto(it.value(), counts);
    }
    else if (value.is_array())
    {
        for (const Json& item : value) countRefsInto(item, counts);
    }
}

std::vector<std::string> splitDots(const std::string& text)
{
    std::vector<std::string> result;
    std::string current;
    for (char ch : text)
    {
        if (ch == '.')
        {
            result.push_back(current);
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    if (!current.empty()) result.push_back(current);
    return result;
}

} // namespace

DesignerApp::DesignerApp()
{
    this->palette = &lightPalette();
    this->code.showDocument = true;
    this->hoverNode = "@none";
}

void DesignerApp::attach(bklv::App& app)
{
    this->host = &app;
    this->root = new bklv::Box(bklv::Axis::COLUMN);
    this->root->setWidthPercentage(100.0f);
    this->root->setHeightPercentage(100.0f);
    this->root->setBackgroundColor(this->palette->windowBg);

    this->menuBar = new MenuBarPanel(*this);
    this->menuBar->setHeight(46.0f);

    this->body = new bklv::Box(bklv::Axis::ROW);
    this->body->setWidthPercentage(100.0f);
    this->body->setGrow(1.0f);
    this->body->setFlexBasis(0.0f);

    this->leftColumn = new bklv::Box(bklv::Axis::COLUMN);
    this->leftColumn->setWidth(this->leftWidth);
    this->leftColumn->setHeightPercentage(100.0f);

    this->projectPanel = new ProjectPanel(*this);
    this->projectPanel->setWidthPercentage(100.0f);
    this->projectPanel->setHeight(this->projectHeight);
    this->leftColumn->addView(this->projectPanel);

    this->leftNodePanel = new NodeTreePanel(*this);
    this->leftNodePanel->setWidthPercentage(100.0f);
    this->leftNodePanel->setGrow(1.0f);
    this->leftNodePanel->setFlexBasis(0.0f);
    this->leftColumn->addView(this->leftNodePanel);

    this->varsColumn = new bklv::Box(bklv::Axis::COLUMN);
    this->varsColumn->setWidth(this->varsWidth);
    this->varsColumn->setHeightPercentage(100.0f);

    this->varsPanel = new GlobalVarsPanel(*this);
    this->varsPanel->setWidthPercentage(100.0f);
    this->varsPanel->setHeightPercentage(100.0f);
    this->varsColumn->addView(this->varsPanel);

    this->centerColumn = new bklv::Box(bklv::Axis::COLUMN);
    this->centerColumn->setGrow(1.0f);
    this->centerColumn->setFlexBasis(0.0f);
    this->centerColumn->setHeightPercentage(100.0f);

    this->tabsPanel = new DocTabsPanel(*this);
    this->tabsPanel->setWidthPercentage(100.0f);
    this->tabsPanel->setHeight(34.0f);
    this->centerColumn->addView(this->tabsPanel);

    this->toolbarPanel = new CanvasToolbarPanel(*this);
    this->toolbarPanel->setWidthPercentage(100.0f);
    this->toolbarPanel->setHeight(38.0f);
    this->centerColumn->addView(this->toolbarPanel);

    this->canvasPanel = new CanvasPanel(*this);
    this->canvasPanel->setWidthPercentage(100.0f);
    this->canvasPanel->setGrow(1.0f);
    this->canvasPanel->setFlexBasis(0.0f);
    this->centerColumn->addView(this->canvasPanel);

    this->rightColumn = new bklv::Box(bklv::Axis::COLUMN);
    this->rightColumn->setWidth(this->rightWidth);
    this->rightColumn->setHeightPercentage(100.0f);

    this->inspectorPanel = new InspectorPanel(*this);
    this->inspectorPanel->setWidthPercentage(100.0f);
    this->inspectorPanel->setGrow(1.0f);
    this->inspectorPanel->setFlexBasis(0.0f);
    this->rightColumn->addView(this->inspectorPanel);

    this->body->addView(this->leftColumn);
    this->body->addView(this->varsColumn);
    this->body->addView(this->centerColumn);
    this->body->addView(this->rightColumn);

    this->statusPanel = new StatusBarPanel(*this);
    this->statusPanel->setHeight(24.0f);

    this->overlayPanel = new OverlayPanel(*this);
    this->overlayPanel->setPositionType(bklv::PositionType::ABSOLUTE);
    this->overlayPanel->setPositionLeft(0.0f);
    this->overlayPanel->setPositionTop(0.0f);
    this->overlayPanel->setWidthPercentage(100.0f);
    this->overlayPanel->setHeightPercentage(100.0f);

    this->root->addView(this->menuBar);
    this->root->addView(this->body);
    this->root->addView(this->statusPanel);
    this->root->addView(this->overlayPanel);

    app.setRootView(this->root);
    app.setClearColor(0.118f, 0.118f, 0.118f);
    app.onUpdate = [this] { this->tick(); };
    app.onDrawWindows = nullptr;

    this->applyThemePalette();

    this->applyLayout();
}

void DesignerApp::applyThemePalette()
{
    this->palette = this->lightTheme ? &lightPalette() : &darkPalette();
    if (this->root != nullptr)
        this->root->setBackgroundColor(this->palette->windowBg);
    if (this->host != nullptr)
    {
        ImU32 color = this->palette->windowBg;
        this->host->setClearColor((color & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f,
            ((color >> 16) & 0xFF) / 255.0f);
    }
}

void DesignerApp::applyLayout()
{
    if (this->leftColumn == nullptr)
        return;

    auto visibility = [](bklv::View* view, bool visible) {
        if (view == nullptr) return;
        const bklv::Visibility target = visible ? bklv::Visibility::VISIBLE : bklv::Visibility::GONE;
        if (view->getVisibility() != target) view->setVisibility(target);
    };

    if (this->root != nullptr)
    {
        const float bodyHeight = this->root->getHeight() - 70.0f;
        if (bodyHeight > 360.0f)
        {
            if (this->showProject && this->showNodeTree)
                this->projectHeight = std::min(this->projectHeight, std::max(220.0f, bodyHeight - 170.0f));
        }
    }

    const bool leftAny = this->showProject || this->showNodeTree;
    const bool rightAny = this->showInspector;

    visibility(this->leftColumn, leftAny);
    visibility(this->varsColumn, this->showVars);
    visibility(this->rightColumn, rightAny);
    visibility(this->projectPanel, this->showProject);
    visibility(this->varsPanel, this->showVars);
    visibility(this->leftNodePanel, this->showNodeTree);
    visibility(this->inspectorPanel, this->showInspector);
    visibility(this->statusPanel, this->showStatus);

    if (this->appliedLeftWidth != this->leftWidth)
    {
        this->leftColumn->setWidth(this->leftWidth);
        this->appliedLeftWidth = this->leftWidth;
    }
    if (this->appliedVarsWidth != this->varsWidth)
    {
        this->varsColumn->setWidth(this->varsWidth);
        this->appliedVarsWidth = this->varsWidth;
    }
    if (this->appliedRightWidth != this->rightWidth)
    {
        this->rightColumn->setWidth(this->rightWidth);
        this->appliedRightWidth = this->rightWidth;
    }
    if (this->appliedProjectHeight != this->projectHeight)
    {
        this->projectPanel->setHeight(this->projectHeight);
        this->appliedProjectHeight = this->projectHeight;
    }
}

Document* DesignerApp::doc()
{
    if (!this->hasDoc()) return nullptr;
    return this->documents[(size_t)this->activeDoc].get();
}

const Document* DesignerApp::doc() const
{
    if (!this->hasDoc()) return nullptr;
    return this->documents[(size_t)this->activeDoc].get();
}

int DesignerApp::findDocument(const std::string& relative) const
{
    for (size_t i = 0; i < this->documents.size(); ++i)
        if (this->documents[i]->relative == relative)
            return (int)i;
    return -1;
}

void DesignerApp::refreshFiles()
{
    this->files.clear();
    if (!this->project.root.empty())
        scanProjectFiles(this->project.root, this->files);
}

void DesignerApp::syncVariableScope()
{
    Document* document = this->doc();
    bool hasLocal = false;
    if (document != nullptr)
    {
        const Json* vars = document->vars(std::string());
        if (vars != nullptr)
        {
            for (const char* group : { "colors", "numbers" })
                if (vars->contains(group) && (*vars)[group].is_object() && !(*vars)[group].empty())
                    hasLocal = true;
        }
    }
    if (hasLocal)
        this->varScope = VarScope::LOCAL;
    else if (!this->project.tokens.empty())
        this->varScope = VarScope::GLOBAL;
    else
        this->varScope = VarScope::LOCAL;
    this->selectedToken.clear();
    this->tokenScroll = 0.0f;
}

bool DesignerApp::projectIsValid(const fs::path& root) const
{
    std::error_code code;
    return !root.empty() && fs::exists(root / "theme.json", code);
}

void DesignerApp::requestProjectFolder(NativePickMode mode)
{
    this->nativePickMode = mode;
    {
        std::lock_guard<std::mutex> lock(this->nativeMutex);
        this->nativePickedReady = false;
        this->nativePickedPath.clear();
        this->nativePickedCancelled = false;
    }
    std::string start;
    if (!this->project.root.empty())
        start = this->project.root.parent_path().string();
    else
    {
        const char* home = std::getenv("HOME");
        if (home != nullptr) start = home;
    }
    SDL_ShowOpenFolderDialog(onNativeFolderPicked, this, nullptr, start.empty() ? nullptr : start.c_str(), false);
}

void DesignerApp::pollNativeFolder()
{
    std::string path;
    bool cancelled = false;
    NativePickMode mode = NativePickMode::NONE;
    {
        std::lock_guard<std::mutex> lock(this->nativeMutex);
        if (!this->nativePickedReady) return;
        this->nativePickedReady = false;
        path = this->nativePickedPath;
        cancelled = this->nativePickedCancelled;
        mode = this->nativePickMode;
        this->nativePickMode = NativePickMode::NONE;
    }
    if (mode == NativePickMode::NONE) return;
    if (path.empty())
    {
        if (!cancelled) this->addToast("无法打开文件选择器", true);
        return;
    }

    const fs::path folder(path);
    if (!this->projectIsValid(folder))
    {
        this->dialog.open = true;
        this->dialog.kind = "alert";
        this->dialog.title = "不是 bklv 工程";
        this->dialog.message = "所选目录缺少 theme.json：\n" + folder.string() +
            "\n\n请选择包含 theme.json 的主题工程目录。";
        this->dialog.alertOnly = true;
        return;
    }

    if (mode == NativePickMode::OPEN_PROJECT)
    {
        this->openProject(folder, true);
        return;
    }
    if (mode == NativePickMode::COPY_SOURCE)
    {
        this->copySourcePath = folder.string();
        this->addToast("已选择源工程，请选择新工程的父目录");
        this->requestProjectFolder(NativePickMode::COPY_DESTINATION);
        return;
    }
    if (mode == NativePickMode::COPY_DESTINATION)
    {
        if (this->copySourcePath.empty())
        {
            this->addToast("源工程路径丢失，请重新复制", true);
            return;
        }
        this->copyProjectTo(this->copySourcePath, folder);
    }
}

void DesignerApp::copyProjectTo(const fs::path& source, const fs::path& destinationParent)
{
    std::error_code code;
    const fs::path canonicalSource = fs::weakly_canonical(source, code);
    const fs::path canonicalDest = fs::weakly_canonical(destinationParent, code);
    if (!canonicalSource.empty() && !canonicalDest.empty())
    {
        const std::string sourceText = canonicalSource.string() + "/";
        const std::string destText = canonicalDest.string() + "/";
        if (destText.rfind(sourceText, 0) == 0)
        {
            this->addToast("新工程不能建在源工程目录内部", true);
            return;
        }
    }
    fs::create_directories(destinationParent, code);
    if (code)
    {
        this->addToast("无法创建目标目录：" + code.message(), true);
        return;
    }

    std::string base = source.filename().string();
    if (base.empty()) base = "project";
    fs::path target = destinationParent / (base + "_copy");
    int suffix = 2;
    while (fs::exists(target, code))
    {
        target = destinationParent / (base + "_copy" + std::to_string(suffix));
        suffix++;
    }

    const auto skip = [](const fs::path& path) {
        const std::string name = path.filename().string();
        return name == ".git" || name == "build" || name == "dist" || name == ".DS_Store" ||
            path.extension() == ".bak" || path.extension() == ".tmp";
    };
    std::function<bool(const fs::path&, const fs::path&)> copyTree =
        [&](const fs::path& from, const fs::path& to) -> bool {
            fs::create_directories(to, code);
            if (code) return false;
            for (const fs::directory_entry& entry : fs::directory_iterator(from, code))
            {
                if (code) return false;
                if (skip(entry.path())) continue;
                const fs::path destination = to / entry.path().filename();
                if (entry.is_directory())
                {
                    if (!copyTree(entry.path(), destination)) return false;
                }
                else if (entry.is_regular_file())
                {
                    fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing, code);
                    if (code) return false;
                }
            }
            return true;
        };

    if (!copyTree(source, target))
    {
        this->addToast("复制工程失败：" + code.message(), true);
        return;
    }

    std::string metaError;
    Json meta = parseJsonFile(target / "theme.json", metaError);
    if (meta.is_object())
    {
        meta["name"] = target.filename().string();
        const std::string oldDescription = meta.value("description", std::string());
        meta["description"] = oldDescription.empty() ? std::string("复制的 bklv 工程") : oldDescription + "（副本）";
        std::string writeError;
        writeJsonFile(target / "theme.json", meta, writeError, false);
    }

    this->addToast("已复制并创建新工程：" + target.filename().string());
    this->openProject(target, true);
}

void DesignerApp::openProject(const fs::path& root, bool report)
{
    Project loaded;
    std::string error;
    if (!loadProject(root, loaded, error))
    {
        this->addToast(error, true);
        this->dialog.open = true;
        this->dialog.kind = "alert";
        this->dialog.title = "打开工程失败";
        this->dialog.message = error;
        this->dialog.alertOnly = true;
        return;
    }

    this->project = std::move(loaded);
    this->refreshFiles();
    this->expandedDirs.clear();
    for (const FileEntry& entry : this->files)
    {
        if (!entry.isDirectory) continue;
        // Keep the view folder open; tokens/resources/base stay collapsed until asked.
        if (entry.relative == "view" || entry.relative.rfind("view/", 0) == 0)
            this->expandedDirs.insert(entry.relative);
    }

    this->documents.clear();
    this->activeDoc = -1;
    this->selectedNode.clear();
    this->selectedToken.clear();
    this->expandedNodes.clear();
    this->docExpandedNodes.clear();
    this->nodeTreeScroll = 0.0f;
    this->canvasResolutionW = this->project.canvasWidth;
    this->canvasResolutionH = this->project.canvasHeight;
    this->canvasZoom = 0.7f;
    this->canvasFit = true;
    this->canvasPanX = 0.0f;
    this->canvasPanY = 0.0f;
    this->previewDirty = true;
    this->previewError.clear();
    this->previewWarning.clear();
    this->documentRevision++;
    this->refCountsDirty = true;


    if (!this->project.entry.empty())
        this->openDocument(this->project.entry, true);

    if (report)
        this->addToast("已打开工程：" + this->project.title);
}

void DesignerApp::openDocument(const std::string& relative, bool selectDefault)
{
    if (relative.empty()) return;
    const int existing = this->findDocument(relative);
    if (existing >= 0)
    {
        this->switchDocument(existing);
        return;
    }

    std::unique_ptr<Document> document(new Document());
    if (!document->load(this->project, relative))
    {
        this->addToast("打开失败：" + document->loadError, true);
        return;
    }

    this->documents.push_back(std::move(document));
    this->activeDoc = (int)this->documents.size() - 1;
    this->expandedNodes.clear();
    std::function<void(const Node&)> expandAll = [&](const Node& node) {
        this->expandedNodes.insert(node.pointer);
        for (const auto& child : node.children) expandAll(*child);
    };
    expandAll(this->doc()->tree);
    if (selectDefault)
        this->selectDefaultNode();
    else
    {
        this->selectedNode.clear();
        this->selectedNode = std::string();
    }
    this->code.active = false;
    this->code.dirty = false;
    this->code.error.clear();
    this->code.scroll = 0.0f;
    this->nodeTreeScroll = 0.0f;
    this->jsonPreviewScroll = 0.0f;
    this->previewDirty = true;
    this->documentRevision++;
    this->canvasFit = true;
    this->syncVariableScope();
    if (this->leftNodePanel != nullptr)
    {
        const Json& root = this->doc()->root;
        const bool structural = root.contains("type") || root.contains("layers") ||
            root.contains("box") || root.contains("import");
        this->leftNodePanel->setTab(structural ? 0 : 1);
    }
}

void DesignerApp::switchDocument(int index)
{
    if (index < 0 || index >= (int)this->documents.size())
        return;
    if (this->activeDoc >= 0 && this->activeDoc < (int)this->documents.size())
        this->docExpandedNodes[this->documents[(size_t)this->activeDoc]->relative] = this->expandedNodes;

    this->activeDoc = index;
    auto it = this->docExpandedNodes.find(this->documents[(size_t)index]->relative);
    if (it != this->docExpandedNodes.end())
    {
        this->expandedNodes = it->second;
    }
    else
    {
        this->expandedNodes.clear();
        std::function<void(const Node&)> expandAll = [&](const Node& node) {
            this->expandedNodes.insert(node.pointer);
            for (const auto& child : node.children) expandAll(*child);
        };
        expandAll(this->doc()->tree);
    }

    if (this->selectedNode.empty() || this->doc()->node(this->selectedNode) == nullptr)
        this->selectDefaultNode();
    this->previewDirty = true;
    this->documentRevision++;
    this->canvasFit = true;
    this->code.active = false;
    this->code.dirty = false;
    this->code.error.clear();
    this->jsonPreviewScroll = 0.0f;
    this->syncVariableScope();
    if (this->leftNodePanel != nullptr)
    {
        const Json& root = this->doc()->root;
        const bool structural = root.contains("type") || root.contains("layers") ||
            root.contains("box") || root.contains("import");
        this->leftNodePanel->setTab(structural ? 0 : 1);
    }
}

void DesignerApp::closeDocument(int index, bool force)
{
    if (index < 0 || index >= (int)this->documents.size())
        return;
    if (this->documents[(size_t)index]->dirty && !force)
    {
        this->dialog.open = true;
        this->dialog.kind = "unsaved_close";
        this->dialog.title = "文档有未保存的修改";
        this->dialog.message = this->documents[(size_t)index]->relative + " 尚未保存。";
        this->dialog.context = std::to_string(index);
        this->dialog.choices = { "保存", "丢弃", "取消" };
        this->dialog.selectedChoice = 0;
        return;
    }

    this->documents.erase(this->documents.begin() + index);
    if (this->documents.empty())
    {
        this->activeDoc = -1;
        this->selectedNode.clear();
        this->previewDirty = true;
        this->documentRevision++;
        return;
    }
    if (this->activeDoc >= (int)this->documents.size())
        this->activeDoc = (int)this->documents.size() - 1;
    if (this->activeDoc > index)
        this->activeDoc--;
    if (this->activeDoc < 0) this->activeDoc = 0;
    this->selectedNode.clear();
    this->selectDefaultNode();
    this->previewDirty = true;
    this->documentRevision++;
}

void DesignerApp::selectDefaultNode()
{
    if (!this->hasDoc())
    {
        this->selectedNode.clear();
        return;
    }
    this->selectNode(std::string(), true);
}

void DesignerApp::openImportedNode(const std::string& relativeFile, const std::string& pointer)
{
    if (relativeFile.empty()) return;
    this->openDocument(relativeFile, true);
    if (this->doc() == nullptr) return;
    if (!pointer.empty() && this->doc()->node(pointer) != nullptr)
        this->selectNode(pointer, true);
    this->addToast("已打开导入源文件：" + relativeFile);
}

namespace
{
int appearanceLayerIndex(const std::string& pointer)
{
    if (pointer.rfind("/layers/", 0) != 0) return -1;
    try
    {
        size_t used = 0;
        const int index = std::stoi(pointer.substr(8), &used);
        if (used == 0) return -1;
        return index;
    }
    catch (const std::exception&)
    {
        return -1;
    }
}

std::string uniqueLayerId(const Json& layers, const std::string& prefix)
{
    for (int i = 0; i < 10000; ++i)
    {
        const std::string id = prefix + std::to_string(i);
        bool used = false;
        for (const Json& layer : layers)
            if (layer.is_object() && layer.value("id", std::string()) == id) { used = true; break; }
        if (!used) return id;
    }
    return prefix;
}
} // namespace

void DesignerApp::addAppearanceLayer(const std::string& type)
{
    Document* document = this->doc();
    if (document == nullptr) return;
    Json candidate = document->root;
    if (!candidate.contains("layers") || !candidate["layers"].is_array())
        candidate["layers"] = Json::array();
    Json& layers = candidate["layers"];

    Json layer = Json::object();
    layer["op"] = type;
    if (type == "text")
    {
        layer["id"] = uniqueLayerId(layers, "text");
        layer["data_id"] = "text";
        layer["fontSize"] = 16;
        layer["color"] = "@theme/color.text.primary";
        layer["align"] = "center";
        layer["valign"] = "center";
        layer["inset"] = Json::array({ 0, 12, 0, 12 });
    }
    else if (type == "image")
    {
        layer["id"] = uniqueLayerId(layers, "image");
        layer["data_id"] = "image";
        layer["scaling"] = "fit";
        layer["inset"] = Json::array({ 8, 8, 8, 8 });
    }
    else if (type == "shadow")
    {
        layer["id"] = uniqueLayerId(layers, "shadow");
        layer["fill"] = "@theme/color.common.shadow";
        layer["feather"] = 8;
        layer["radius"] = 8;
        layer["cutout"] = true;
    }
    else if (type == "glass")
    {
        layer["id"] = uniqueLayerId(layers, "glass");
        layer["fill"] = "#FFFFFF22";
        layer["radius"] = 8;
    }
    else if (type == "custom")
    {
        layer["id"] = uniqueLayerId(layers, "custom");
    }
    else
    {
        layer["op"] = "rect";
        layer["id"] = uniqueLayerId(layers, "rect");
        layer["fill"] = "@theme/color.common.transparent";
        layer["stroke"] = "@theme/color.border.default";
        layer["strokeWidth"] = 1;
        layer["radius"] = 8;
    }

    size_t insert = layers.size();
    if (type == "shadow")
        insert = 0;
    else if (type == "rect" || type == "glass")
    {
        for (size_t i = 0; i < layers.size(); ++i)
        {
            const std::string op = layers[i].is_object() ? layers[i].value("op", std::string()) : std::string();
            if (op == "text" || op == "image" || op == "custom")
            {
                insert = i;
                break;
            }
        }
    }
    layers.insert(layers.begin() + (long)insert, layer);

    std::string compileError;
    if (bklv::Appearance::compile(candidate.dump(), bklv::getTheme(), compileError,
            document->relative) == nullptr)
    {
        this->notifyError("创建图层失败：" + compileError);
        return;
    }

    document->pushUndo();
    document->root = std::move(candidate);
    document->dirty = true;
    document->rebuild();
    this->selectedNode = "/layers/" + std::to_string(insert);
    this->scrollNodeIntoView = true;
    this->markDocumentChanged();
    this->addToast("已创建图层 " + layer.value("id", std::string()));
}

void DesignerApp::importAppearanceLayer(const std::string& relative)
{
    if (relative.empty()) return;
    Document* document = this->doc();
    if (document == nullptr) return;

    Json candidate = document->root;
    if (!candidate.contains("layers") || !candidate["layers"].is_array())
        candidate["layers"] = Json::array();
    Json& layers = candidate["layers"];
    Json layer = Json::object();
    layer["import"] = relative;
    layers.push_back(layer);
    const int index = (int)layers.size() - 1;

    std::string compileError;
    if (bklv::Appearance::compile(candidate.dump(), bklv::getTheme(), compileError,
            document->relative) == nullptr)
    {
        this->notifyError("导入外观失败：" + compileError);
        return;
    }

    document->pushUndo();
    document->root = std::move(candidate);
    document->dirty = true;
    document->rebuild();
    this->selectedNode = "/layers/" + std::to_string(index);
    this->scrollNodeIntoView = true;
    this->markDocumentChanged();
    this->addToast("已导入外观 " + relative);
}

void DesignerApp::deleteAppearanceLayer()
{
    Document* document = this->doc();
    if (document == nullptr) return;
    const int index = appearanceLayerIndex(this->selectedNode);
    if (index < 0) return;
    if (!document->root.contains("layers") || !document->root["layers"].is_array()) return;
    Json& layers = document->root["layers"];
    if (index >= (int)layers.size()) return;
    document->pushUndo();
    layers.erase(layers.begin() + index);
    document->dirty = true;
    document->rebuild();
    this->selectedNode.clear();
    this->scrollNodeIntoView = true;
    this->markDocumentChanged();
    this->addToast("已删除图层");
}

void DesignerApp::duplicateAppearanceLayer()
{
    Document* document = this->doc();
    if (document == nullptr) return;
    const int index = appearanceLayerIndex(this->selectedNode);
    if (index < 0) return;
    if (!document->root.contains("layers") || !document->root["layers"].is_array()) return;
    Json& layers = document->root["layers"];
    if (index >= (int)layers.size() || !layers[index].is_object()) return;
    document->pushUndo();
    Json copy = layers[index];
    if (copy.contains("id") && copy["id"].is_string())
        copy["id"] = uniqueLayerId(layers, copy["id"].get<std::string>());
    layers.insert(layers.begin() + index + 1, copy);
    document->dirty = true;
    document->rebuild();
    this->selectedNode = "/layers/" + std::to_string(index + 1);
    this->scrollNodeIntoView = true;
    this->markDocumentChanged();
    this->addToast("已复制图层");
}

void DesignerApp::openAppearanceImportPicker()
{
    Document* document = this->doc();
    if (document == nullptr) return;
    std::vector<MenuItem> items;
    for (const FileEntry& file : this->files)
    {
        if (file.isDirectory) continue;
        if (file.name.size() < 6 || file.name.substr(file.name.size() - 5) != ".json") continue;
        if (file.relative == document->relative) continue;
        if (file.relative.rfind("view/", 0) == 0 || file.relative.rfind("tokens/", 0) == 0) continue;
        if (file.relative == "theme.json") continue;
        MenuItem item;
        item.label = file.relative;
        item.action = "appearance_import";
        item.value = file.relative.substr(0, file.relative.size() - 5);
        item.icon = "palette";
        items.push_back(item);
    }
    if (items.empty())
    {
        MenuItem item;
        item.label = "没有可导入的外观 JSON";
        item.disabled = true;
        items.push_back(item);
    }
    const Rect anchor(this->mousePosition.x, this->mousePosition.y, 220.0f, 0.0f);
    this->openPopup("appearance-import", anchor, items, 360.0f);
}

void DesignerApp::selectNode(const std::string& pointer, bool reveal)
{
    if (!this->hasDoc())
        return;
    if (!pointer.empty() && this->doc()->node(pointer) == nullptr)
        return;
    if (this->code.active)
    {
        this->code.active = false;
        this->code.error.clear();
    }
    this->selectedNode = pointer;
    this->expandedNodes.insert(pointer);
    if (pointer.empty())
    {
        this->scrollNodeIntoView = true;
        return;
    }

    std::string current = pointer;
    while (!current.empty())
    {
        this->expandedNodes.insert(current);
        current = parentPointer(current);
    }
    this->expandedNodes.insert(std::string());
    if (reveal)
    {
        this->scrollToNode(pointer);
        this->scrollNodeIntoView = true;
    }
}

void DesignerApp::selectNodeById(const std::string& id)
{
    if (!this->hasDoc()) return;
    if (id.empty() || id[0] == '/' || id == "root")
    {
        this->selectNode(id == "root" ? std::string() : id, true);
        return;
    }
    std::function<const Node*(const Node&)> find = [&](const Node& node) -> const Node* {
        if (node.name == id || node.type == id) return &node;
        for (const auto& child : node.children)
            if (const Node* found = find(*child)) return found;
        return nullptr;
    };
    const Node* found = find(this->doc()->tree);
    if (found != nullptr) this->selectNode(found->pointer, true);
}

void DesignerApp::selectToken(const std::string& path)
{
    this->selectedToken = path;
    this->refCountsDirty = true;
}

void DesignerApp::scrollToNode(const std::string& pointer)
{
    // Scroll positions are managed by the panels; storing a hint is enough for our tree height.
    if (this->hasDoc())
    {
        const Node* node = findNode(this->doc()->tree, pointer);
        if (node != nullptr)
        {
            const float target = std::max(0.0f, node->depth * 0.0f);
            (void)target;
        }
    }
}

void DesignerApp::ensureNodeExpanded(const std::string& pointer)
{
    std::string current = pointer;
    while (!current.empty())
    {
        this->expandedNodes.insert(current);
        current = parentPointer(current);
    }
}

void DesignerApp::saveActive(bool saveAs)
{
    Document* document = this->doc();
    if (document == nullptr)
    {
        this->addToast("没有打开的文档", true);
        return;
    }
    if (saveAs)
    {
        this->openDialog("save_as");
        return;
    }
    std::string error;
    if (!document->save(this->project, error))
    {
        this->addToast(error, true);
        return;
    }
    this->refCountsDirty = true;
    this->addToast("已保存 " + document->relative);
}

void DesignerApp::saveAll()
{
    this->saveActive(false);
}

void DesignerApp::reloadActive()
{
    Document* document = this->doc();
    if (document == nullptr) return;
    if (document->dirty)
    {
        this->dialog.open = true;
        this->dialog.kind = "reload_confirm";
        this->dialog.title = "重新加载文档";
        this->dialog.message = "未保存的修改会被丢弃，继续？";
        this->dialog.choices = { "重新加载", "取消" };
        this->dialog.selectedChoice = 0;
        return;
    }
    std::string error;
    if (!document->load(this->project, document->relative))
    {
        this->addToast(document->loadError, true);
        return;
    }
    this->selectDefaultNode();
    this->markDocumentChanged();
    this->addToast("已重新加载 " + document->relative);
}

void DesignerApp::newDocument(const std::string& kind)
{
    if (!this->hasProject())
    {
        this->addToast("请先打开一个 bklv 工程", true);
        return;
    }
    if (kind == "new_page")
        this->openDialog("new_page");
    else
        this->openDialog("new_component");
}

void DesignerApp::duplicateSelected()
{
    Document* document = this->doc();
    if (document != nullptr && !document->root.contains("type") &&
        this->selectedNode.rfind("/layers/", 0) == 0)
    {
        this->duplicateAppearanceLayer();
        return;
    }
    if (document == nullptr || this->selectedNode.empty())
    {
        this->addToast("请先选择一个节点", true);
        return;
    }
    std::string newPointer;
    if (!document->duplicateNode(this->selectedNode, &newPointer))
    {
        this->addToast("无法复制该节点", true);
        return;
    }
    this->selectedNode = newPointer;
    this->ensureNodeExpanded(newPointer);
    this->markDocumentChanged();
    this->addToast("已复制节点");
}

void DesignerApp::deleteSelected()
{
    Document* document = this->doc();
    if (document != nullptr && !document->root.contains("type") &&
        this->selectedNode.rfind("/layers/", 0) == 0)
    {
        this->deleteAppearanceLayer();
        return;
    }
    if (document == nullptr || this->selectedNode.empty())
    {
        this->addToast("根节点不能删除", true);
        return;
    }
    if (!document->deleteNode(this->selectedNode))
    {
        this->addToast("无法删除该节点", true);
        return;
    }
    this->selectedNode = parentPointer(this->selectedNode);
    if (this->selectedNode.empty()) this->selectedNode = std::string();
    this->markDocumentChanged();
    this->addToast("已删除节点");
}

void DesignerApp::moveSelectedNode(int direction)
{
    Document* document = this->doc();
    if (document == nullptr || this->selectedNode.empty()) return;
    if (!document->reorderNode(this->selectedNode, direction))
    {
        this->addToast(direction < 0 ? "已经是最前面的节点" : "已经是最后面的节点", false);
        return;
    }
    this->markDocumentChanged();
    const std::string parent = parentPointer(this->selectedNode);
    const int index = childIndex(this->selectedNode) + direction;
    this->selectedNode = parent.empty() ? ("/children/" + std::to_string(index))
                                       : (parent + "/children/" + std::to_string(index));
}

void DesignerApp::addChildNode(const std::string& type)
{
    Document* document = this->doc();
    if (document == nullptr) return;

    std::string parent = this->selectedNode;
    if (!parent.empty())
    {
        const Json* selected = document->node(parent);
        const std::string selectedType = selected != nullptr ? selected->value("type", std::string()) : std::string();
        if (selectedType == "Label" || selectedType == "Image" || selectedType == "Button")
            parent = parentPointer(parent);
    }

    Json node = makeNode(type);
    if (type == "Button")
    {
        node["width"] = 160;
        node["height"] = 44;
        node["text"] = "按钮";
        node["enabled"] = true;
    }

    int index = -1;
    Json* parentNode = document->node(parent);
    if (parentNode != nullptr && parentNode->contains("children") && (*parentNode)["children"].is_array())
        index = (int)(*parentNode)["children"].size();
    if (parent.empty() && index < 0) index = (int)document->root.value("children", Json::array()).size();
    if (!document->insertNode(parent, index, node))
    {
        this->addToast("无法添加节点", true);
        return;
    }

    const std::string pointer = parent.empty() ? ("/children/" + std::to_string(index))
                                                : (parent + "/children/" + std::to_string(index));
    this->selectedNode = pointer;
    this->ensureNodeExpanded(pointer);
    this->markDocumentChanged();
    this->addToast("已添加 " + type + " 节点");
}

void DesignerApp::undo()
{
    Document* document = this->doc();
    if (document == nullptr || !document->undo()) return;
    if (this->selectedNode.empty() || document->node(this->selectedNode) == nullptr)
        this->selectDefaultNode();
    this->markDocumentChanged();
    this->addToast("已撤销");
}

void DesignerApp::redo()
{
    Document* document = this->doc();
    if (document == nullptr || !document->redo()) return;
    if (this->selectedNode.empty() || document->node(this->selectedNode) == nullptr)
        this->selectDefaultNode();
    this->markDocumentChanged();
    this->addToast("已重做");
}

void DesignerApp::markDocumentChanged()
{
    this->previewDirty = true;
    this->documentRevision++;
    this->refCountsDirty = true;
    this->code.dirty = true;
    if (Document* document = this->doc())
        document->dirty = true;
}

void DesignerApp::requestPreview(bool fit)
{
    if (fit) this->canvasFit = true;
    this->previewDirty = true;
    this->documentRevision++;
}

void DesignerApp::fitCanvas()
{
    this->canvasFit = true;
    this->canvasPanX = 0.0f;
    this->canvasPanY = 0.0f;
    if (this->canvasPanel != nullptr)
        this->canvasZoom = this->canvasPanel->fitZoom();
    this->previewDirty = true;
}

void DesignerApp::addToast(const std::string& text, bool error)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    Toast toast;
    toast.text = text;
    toast.start = ImGui::GetTime();
    toast.duration = error ? 4.5f : 3.0f;
    toast.error = error;
    this->toasts.push_back(toast);
    while (this->toasts.size() > 4)
        this->toasts.pop_front();
}

void DesignerApp::syncInputModal()
{
    if (this->overlayPanel == nullptr) return;
    const bool open = this->menuPopup.open || this->pickerPopup.open || this->colorPopup.open ||
        this->dialog.open;
    if (open && !this->inputModalPushed)
    {
        bklv::getInputManager().pushModal(this->overlayPanel, false);
        this->inputModalPushed = true;
    }
    else if (!open && this->inputModalPushed)
    {
        bklv::getInputManager().popModal();
        this->inputModalPushed = false;
    }
}

bool DesignerApp::hovering(const Rect& rect) const
{
    if (this->menuPopup.open || this->pickerPopup.open || this->colorPopup.open || this->dialog.open)
        return false;
    return rect.contains(this->mousePosition);
}

void DesignerApp::notifyError(const std::string& text)
{
    this->addToast(text, true);
}

const Json* DesignerApp::selectedNodeJson() const
{
    const Document* document = this->doc();
    if (document == nullptr) return nullptr;
    return document->node(this->selectedNode);
}

Json* DesignerApp::selectedStyleMutable()
{
    Document* document = this->doc();
    if (document == nullptr || this->selectedNode.empty()) return nullptr;
    return document->style(this->selectedNode, true);
}

void DesignerApp::setStyleLiteral(const std::string& node, const std::string& key, const Json& value, bool record)
{
    Document* document = this->doc();
    if (document == nullptr) return;
    if (record)
    {
        if (!document->setStyleValue(node, key, value)) return;
    }
    else
    {
        Json* style = document->style(node, true);
        if (style == nullptr) return;
        (*style)[key] = value;
        document->dirty = true;
    }
    this->markDocumentChanged();
}

void DesignerApp::setLocalLiteral(const std::string& node, const std::string& name, const Json& value, bool record)
{
    Document* document = this->doc();
    if (document == nullptr) return;
    if (record)
    {
        if (!document->setLocalVar(node, name, value)) return;
    }
    else
    {
        Json* vars = document->vars(node, true);
        if (vars == nullptr) return;
        (*vars)[name] = value;
        document->dirty = true;
    }
    this->markDocumentChanged();
}

TokenType DesignerApp::propType(const std::string& key) const
{
    if (key == "backgroundColor" || key == "borderColor" || key == "textColor" ||
        key == "placeholderColor")
        return TokenType::COLOR;
    if (key == "text" || key == "src" || key == "icon" || key == "iconFont" || key == "variant" ||
        key == "axis" || key == "direction" || key == "justifyContent" || key == "alignItems" ||
        key == "alignSelf" || key == "alignContent" || key == "horizontalAlign" ||
        key == "verticalAlign" || key == "scalingType" || key == "position" || key == "visibility")
        return TokenType::STRING;
    if (key == "clipsToBounds" || key == "singleLine" || key == "flexWrap" || key == "enabled")
        return TokenType::BOOLEAN;
    return TokenType::NUMBER;
}

bool DesignerApp::isTokenCompatible(TokenType token, TokenType target) const
{
    return token == target;
}

void DesignerApp::setPropertyReference(const std::string& node, const std::string& key, const std::string& tokenPath,
    bool local)
{
    if (tokenPath.empty() || tokenPath == "$none") return;
    this->setStyleLiteral(node, key, tokenPath);
    if (local || tokenPath.rfind("&", 0) == 0)
        this->addToast("已引用局部变量 " + tokenPath);
    else
        this->addToast("已引用全局变量 " + tokenPath);
}

void DesignerApp::beginVariableDrop(const std::string& token, TokenType type, const Rect& source)
{
    this->drag.pressed = true;
    this->drag.active = false;
    this->drag.start = this->mousePosition;
    this->drag.tokenPath = token;
    this->drag.tokenType = type;
    this->drag.sourceRect = source;
}

void DesignerApp::dropVariable()
{
    if (!this->drag.active)
    {
        this->drag.pressed = false;
        this->drag.active = false;
        return;
    }

    const ImVec2 mouse = this->mousePosition;
    bool handled = false;
    for (const DropTarget& target : this->dropTargets)
    {
        if (!target.rect.contains(mouse))
            continue;
        handled = true;
        if (!this->isTokenCompatible(this->drag.tokenType, target.type))
        {
            this->notifyError("变量类型不匹配：这里需要" + std::string(tokenTypeName(target.type)));
            break;
        }
        if (!this->hasDoc()) break;
        this->setPropertyReference(target.node, target.key, this->drag.tokenPath,
            this->drag.tokenPath.rfind("&", 0) == 0);
        break;
    }

    (void)handled;
    this->drag.pressed = false;
    this->drag.active = false;
}

void DesignerApp::beginEdit(const std::string& key, const std::string& initial, const Rect& rect, bool numeric,
    bool multiline)
{
    if (this->edit.active && this->edit.key == key)
        return;
    if (this->edit.active)
        this->commitEdit(false);
    if (this->code.active)
    {
        this->code.active = false;
        this->code.error.clear();
    }
    this->edit.active = true;
    this->edit.key = key;
    this->edit.buffer = initial;
    this->edit.caret = (int)initial.size();
    this->edit.numeric = numeric;
    this->edit.multiline = multiline;
    this->edit.rect = rect;
}

void DesignerApp::setCaretFromPoint(float x)
{
    if (!this->edit.active) return;
    float best = 0.0f;
    int bestIndex = 0;
    for (int i = 0; i <= (int)this->edit.buffer.size(); ++i)
    {
        const std::string prefix = this->edit.buffer.substr(0, (size_t)i);
        const float width = ui::textWidth(prefix, 12.0f);
        if (std::fabs(width - x) < std::fabs(best - x))
        {
            best = width;
            bestIndex = i;
        }
    }
    this->edit.caret = bestIndex;
}

void DesignerApp::handleEditKeys(UiInput& input)
{
    if (!this->edit.active)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const bool command = io.KeySuper || io.KeyCtrl;

    if (command && ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        const std::string paste = readClipboard();
        this->edit.buffer.insert((size_t)this->edit.caret, paste);
        this->edit.caret += (int)paste.size();
    }
    if (command && ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        setClipboard(this->edit.buffer);
    }

    for (ImWchar character : io.InputQueueCharacters)
    {
        if (character < 32) continue;
        if (character < 128)
        {
            char c = (char)character;
            if (this->edit.numeric && !(std::isdigit((unsigned char)c) || c == '.' || c == '-' || c == '%' ||
                                           c == 'x' || c == 'X' || c == 'a' || c == 'u' || c == 't' || c == 'o'))
                continue;
        }
        std::string utf8;
        if (character < 0x80)
        {
            utf8.push_back((char)character);
        }
        else if (character < 0x800)
        {
            utf8.push_back((char)(0xC0 | (character >> 6)));
            utf8.push_back((char)(0x80 | (character & 0x3F)));
        }
        else if (character < 0x10000)
        {
            utf8.push_back((char)(0xE0 | (character >> 12)));
            utf8.push_back((char)(0x80 | ((character >> 6) & 0x3F)));
            utf8.push_back((char)(0x80 | (character & 0x3F)));
        }
        else
        {
            utf8.push_back((char)(0xF0 | (character >> 18)));
            utf8.push_back((char)(0x80 | ((character >> 12) & 0x3F)));
            utf8.push_back((char)(0x80 | ((character >> 6) & 0x3F)));
            utf8.push_back((char)(0x80 | (character & 0x3F)));
        }
        this->edit.buffer.insert((size_t)this->edit.caret, utf8);
        this->edit.caret += (int)utf8.size();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true) && this->edit.caret > 0)
    {
        size_t start = (size_t)this->edit.caret - 1;
        while (start > 0 && ((unsigned char)this->edit.buffer[start] & 0xC0) == 0x80) start--;
        this->edit.buffer.erase(start, (size_t)this->edit.caret - start);
        this->edit.caret = (int)start;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, true) && this->edit.caret < (int)this->edit.buffer.size())
    {
        size_t end = (size_t)this->edit.caret + 1;
        while (end < this->edit.buffer.size() && ((unsigned char)this->edit.buffer[end] & 0xC0) == 0x80) end++;
        this->edit.buffer.erase((size_t)this->edit.caret, end - (size_t)this->edit.caret);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true) && this->edit.caret > 0)
    {
        size_t index = (size_t)this->edit.caret - 1;
        while (index > 0 && ((unsigned char)this->edit.buffer[index] & 0xC0) == 0x80) index--;
        this->edit.caret = (int)index;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) && this->edit.caret < (int)this->edit.buffer.size())
    {
        size_t index = (size_t)this->edit.caret + 1;
        while (index < this->edit.buffer.size() && ((unsigned char)this->edit.buffer[index] & 0xC0) == 0x80) index++;
        this->edit.caret = (int)index;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) this->edit.caret = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_End, false)) this->edit.caret = (int)this->edit.buffer.size();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        this->commitEdit(true);
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
    {
        if (this->edit.multiline)
        {
            this->edit.buffer.insert((size_t)this->edit.caret, "\n");
            this->edit.caret++;
            return;
        }
        const bool dialogField = this->edit.key.rfind("dialog:", 0) == 0;
        this->commitEdit(false);
        if (dialogField && this->dialog.open)
            this->confirmDialog();
        return;
    }

    if (this->edit.key == "search")
    {
        this->fileFilter = this->edit.buffer;
    }
}

void DesignerApp::commitEdit(bool cancel)
{
    if (!this->edit.active)
        return;

    const std::string key = this->edit.key;
    const std::string buffer = this->edit.buffer;
    this->edit.active = false;
    this->edit.key.clear();
    this->edit.rect = Rect();

    if (cancel)
        return;

    auto parseNumber = [&](const std::string& text, float& out) -> bool {
        try
        {
            size_t used = 0;
            out = std::stof(text, &used);
            return used > 0;
        }
        catch (const std::exception&)
        {
            return false;
        }
    };

    if (key.rfind("appearance:", 0) == 0)
    {
        const std::string rest = key.substr(11);
        const size_t split = rest.find(':');
        if (split == std::string::npos) return;
        const std::string type = rest.substr(0, split);
        const std::string pointer = rest.substr(split + 1);
        Document* document = this->doc();
        if (document == nullptr) return;
        const std::string text = trim(buffer);
        Json value;
        if (type == "number")
        {
            float number = 0.0f;
            if (text.rfind("@", 0) == 0 || text.rfind("&", 0) == 0 || text == "auto" ||
                (!text.empty() && text.back() == '%'))
                value = text;
            else if (!parseNumber(text, number))
            {
                this->notifyError("请输入数字、auto、百分比或主题引用");
                return;
            }
            else
            {
                const long long integral = (long long)number;
                value = (std::fabs(number - (double)integral) < 0.0001) ? Json(integral) : Json(number);
            }
        }
        else if (type == "color")
        {
            ImU32 color = 0;
            if (!parseColor(text, color))
            {
                this->notifyError("颜色格式应为 #RRGGBB 或 #RRGGBBAA");
                return;
            }
            value = text;
        }
        else if (type == "bool")
            value = (text == "true");
        else
            value = buffer;
        if (!document->setValueAt(pointer, value))
        {
            this->notifyError("无法写入外观属性");
            return;
        }
        this->markDocumentChanged();
        return;
    }

    if (key.rfind("prop:", 0) == 0)
    {
        const std::string rest = key.substr(5);
        const size_t split = rest.find(':');
        if (split == std::string::npos) return;
        const std::string node = rest.substr(0, split);
        const std::string prop = rest.substr(split + 1);
        if (doc() == nullptr || doc()->node(node) == nullptr) return;

        const TokenType type = this->propType(prop);
        if (type == TokenType::NUMBER)
        {
            const std::string text = trim(buffer);
            Json value;
            const bool sizeKey = prop == "width" || prop == "height" || prop == "minWidth" ||
                prop == "minHeight" || prop == "maxWidth" || prop == "maxHeight" || prop == "flexBasis" ||
                prop == "left" || prop == "top" || prop == "right" || prop == "bottom";
            if (sizeKey && (text == "auto" || (!text.empty() && text.back() == '%')))
                value = text;
            else
            {
                float number = 0.0f;
                if (!parseNumber(text, number))
                {
                    this->notifyError("请输入数字" + std::string(sizeKey ? "、auto 或百分比" : ""));
                    return;
                }
                const long long integral = (long long)number;
                value = (std::fabs(number - (double)integral) < 0.0001) ? Json(integral) : Json(number);
            }
            this->setStyleLiteral(node, prop, value);
        }
        else if (type == TokenType::COLOR)
        {
            ImU32 color = 0;
            if (!parseColor(trim(buffer), color))
            {
                this->notifyError("颜色格式应为 #RRGGBB 或 #RRGGBBAA");
                return;
            }
            this->setStyleLiteral(node, prop, trim(buffer));
        }
        else if (type == TokenType::BOOLEAN)
        {
            this->setStyleLiteral(node, prop, trim(buffer) == "true");
        }
        else
        {
            this->setStyleLiteral(node, prop, buffer);
        }
        return;
    }

    if (key.rfind("varval:", 0) == 0)
    {
        const std::string path = key.substr(7);
        const Token* token = this->project.getToken(path);
        if (token == nullptr) return;
        Json value;
        if (token->type == TokenType::NUMBER)
        {
            float number = 0.0f;
            if (!parseNumber(trim(buffer), number))
            {
                this->notifyError("变量需要数值");
                return;
            }
            value = number;
        }
        else if (token->type == TokenType::BOOLEAN)
        {
            value = (trim(buffer) == "true");
        }
        else if (token->type == TokenType::COLOR)
        {
            ImU32 color = 0;
            if (!parseColor(trim(buffer), color))
            {
                this->notifyError("颜色格式应为 #RRGGBB");
                return;
            }
            value = trim(buffer);
        }
        else
        {
            value = buffer;
        }
        std::string error;
        if (!this->project.setTokenValue(path, value, error))
        {
            this->notifyError(error);
            return;
        }
        this->refCountsDirty = true;
        this->previewDirty = true;
        this->documentRevision++;
        this->addToast("已更新变量 " + path);
        return;
    }

    if (key.rfind("localval:", 0) == 0)
    {
        const std::string name = key.substr(9);
        const std::string text = trim(buffer);
        Json value;
        ImU32 color = 0;
        float number = 0.0f;
        if (parseColor(text, color))
            value = text;
        else if (parseNumber(text, number))
        {
            const long long integral = (long long)number;
            value = (std::fabs(number - (double)integral) < 0.0001) ? Json(integral) : Json(number);
        }
        else
        {
            this->notifyError("局部变量只支持颜色或数值");
            return;
        }
        this->setLocalLiteral(std::string(), name, value);
        return;
    }

    if (key.rfind("nodename:", 0) == 0)
    {
        const std::string node = key.substr(9);
        Document* document = this->doc();
        Json* value = document != nullptr ? document->node(node) : nullptr;
        if (value == nullptr) return;
        if (trim(buffer).empty())
        {
            this->notifyError("节点名称不能为空");
            return;
        }
        document->pushUndo();
        (*value)["name"] = trim(buffer);
        document->dirty = true;
        document->rebuild();
        this->markDocumentChanged();
        return;
    }

    if (key.rfind("dialog:", 0) == 0)
    {
        if (key == "dialog:1") this->dialog.input1 = buffer;
        else if (key == "dialog:2") this->dialog.input2 = buffer;
        return;
    }

    if (key == "colorhex")
    {
        ImU32 color = 0;
        if (!parseColor(trim(buffer), color))
        {
            this->notifyError("颜色格式应为 #RRGGBB");
            return;
        }
        this->applyColor(color);
        return;
    }

    if (key == "search")
    {
        this->fileFilter = buffer;
    }
}

void DesignerApp::openMenu(int index, const Rect& anchor)
{
    this->menuIndex = index;
    this->pickerPopup.open = false;
    this->colorPopup.open = false;

    std::vector<MenuItem> items;
    auto item = [](const std::string& label, const std::string& shortcut, const std::string& action,
                    const std::string& icon = std::string(), bool checked = false, bool disabled = false,
                    bool danger = false) {
        MenuItem value;
        value.label = label;
        value.shortcut = shortcut;
        value.action = action;
        value.icon = icon;
        value.checked = checked;
        value.disabled = disabled;
        value.danger = danger;
        return value;
    };
    auto separator = [] {
        MenuItem value;
        value.separator = true;
        return value;
    };

    if (index == 0)
    {
        items.push_back(item("新建页面", "", "new_page", "description"));
        items.push_back(item("新建组件", "", "new_component", "widgets"));
        items.push_back(separator());
        items.push_back(item("打开工程…", "⌘O", "project_open", "folder_open"));
        items.push_back(item("复制并新建工程…", "", "project_copy", "content_copy"));
        items.push_back(item("保存", "⌘S", "save", "save"));
        items.push_back(item("另存为…", "", "save_as", "file_download"));
        items.push_back(separator());
        items.push_back(item("重新加载", "⌘R", "reload", "refresh", false, !this->hasDoc()));
        items.push_back(item("关闭当前文档", "", "close_doc", "close", false, !this->hasDoc()));
    }
    else if (index == 1)
    {
        items.push_back(item("撤销", "⌘Z", "undo", "undo", false, !this->hasDoc()));
        items.push_back(item("重做", "⇧⌘Z", "redo", "redo", false, !this->hasDoc()));
        items.push_back(separator());
        items.push_back(item("复制节点", "⌘D", "duplicate", "content_copy", false,
            !this->hasDoc() || this->selectedNode.empty()));
        items.push_back(item("复制为 JSON", "", "copy_node", "description", false,
            !this->hasDoc() || this->selectedNode.empty()));
        items.push_back(item("粘贴节点", "⌘V", "paste_node", "content_paste", false,
            readClipboard().empty()));
        items.push_back(item("删除节点", "Del", "delete", "delete", false,
            !this->hasDoc() || this->selectedNode.empty(), true));
        items.push_back(separator());
        items.push_back(item("上移节点", "⌘[", "move_up", "expand_less", false,
            !this->hasDoc() || this->selectedNode.empty()));
        items.push_back(item("下移节点", "⌘]", "move_down", "expand_more", false,
            !this->hasDoc() || this->selectedNode.empty()));
    }
    else if (index == 2)
    {
        items.push_back(item("项目树", "", "toggle_project", "folder_open", this->showProject));
        items.push_back(item("全局变量", "", "toggle_vars", "palette", this->showVars));
        items.push_back(item("节点树", "", "toggle_nodes", "layers", this->showNodeTree));
        items.push_back(item("属性检查器", "", "toggle_inspector", "tune", this->showInspector));
        items.push_back(item("状态栏", "", "toggle_status", "info", this->showStatus));
        items.push_back(separator());
        items.push_back(item("显示网格", "", "toggle_grid", "grid_on", this->showGrid));
        items.push_back(item("显示节点边界", "", "toggle_guides", "crop_square", this->showGuides));
        items.push_back(item("适配画布", "⌘0", "fit", "center_focus_strong"));
        items.push_back(separator());
        items.push_back(item("恢复默认布局", "", "reset_layout", "dashboard"));
    }
    else
    {
        items.push_back(item("文字大小：小", "", "text_small", "text_fields", this->textScale < 0.95f));
        items.push_back(item("文字大小：标准", "", "text_normal", "text_fields",
            this->textScale >= 0.95f && this->textScale < 1.08f));
        items.push_back(item("文字大小：大", "", "text_large", "text_fields", this->textScale >= 1.08f));
        items.push_back(item("增大界面字号", "", "text_inc", "zoom_in"));
        items.push_back(item("减小界面字号", "", "text_dec", "zoom_out"));
        items.push_back(separator());
        items.push_back(item("深色主题", "", "theme_dark", "palette", !this->lightTheme));
        items.push_back(item("浅色主题", "", "theme_light", "palette", this->lightTheme));
        items.push_back(separator());
        items.push_back(item("关于设计器", "", "about", "info"));
    }

    this->menuPopup.open = true;
    this->menuPopup.id = "menu";
    this->menuPopup.anchor = anchor;
    this->menuPopup.items = std::move(items);
    this->menuPopup.width = 236.0f;
    this->menuPopup.hover = -1;
    this->menuPopup.scroll = 0.0f;
}

void DesignerApp::openPopup(const std::string& id, const Rect& anchor, std::vector<MenuItem> items, float width,
    bool alignRight)
{
    this->pickerPopup.open = true;
    this->pickerPopup.id = id;
    this->pickerPopup.anchor = anchor;
    this->pickerPopup.items = std::move(items);
    this->pickerPopup.width = width;
    this->pickerPopup.alignRight = alignRight;
    this->pickerPopup.hover = -1;
    this->pickerPopup.scroll = 0.0f;
    this->menuPopup.open = false;
    this->colorPopup.open = false;
}

void DesignerApp::closePopups()
{
    this->menuPopup.open = false;
    this->pickerPopup.open = false;
    this->colorPopup.open = false;
    this->menuIndex = -1;
}

void DesignerApp::choosePopup(const MenuItem& item)
{
    if (item.disabled || item.separator)
        return;

    const std::string popupId = this->pickerPopup.id;
    if (item.action == "set_appearance")
    {
        const std::string pointer = popupId.substr(std::string("appearance-enum:").size());
        Document* document = this->doc();
        if (document != nullptr && document->setValueAt(pointer, item.value))
            this->markDocumentChanged();
        this->closePopups();
        return;
    }
    if (item.action == "set_enum")
    {
        // id: enum:<node>:<key>
        const std::string rest = popupId.substr(5);
        const size_t split = rest.find(':');
        if (split != std::string::npos && this->hasDoc())
        {
            const std::string node = rest.substr(0, split);
            const std::string key = rest.substr(split + 1);
            this->setStyleLiteral(node, key, item.value);
        }
        this->closePopups();
        return;
    }
    if (item.action == "set_ref")
    {
        // id: varpick:<node>:<key>
        const std::string rest = popupId.substr(8);
        const size_t split = rest.find(':');
        if (split != std::string::npos)
        {
            const std::string node = rest.substr(0, split);
            const std::string key = rest.substr(split + 1);
            if (item.value == "$none")
            {
                Document* document = this->doc();
                Json* style = document != nullptr ? document->style(node, false) : nullptr;
                if (style != nullptr && style->contains(key))
                {
                    Json resolved = PreviewBuilder::resolveStyleValue(this->project, document->root, node,
                        (*style)[key]);
                    this->setStyleLiteral(node, key, resolved);
                }
            }
            else
            {
                this->setPropertyReference(node, key, item.value, item.value.rfind("&", 0) == 0);
            }
        }
        this->closePopups();
        return;
    }

    this->closePopups();
    this->runAction(item.action, item.value);
}

void DesignerApp::openColorPopup(const std::string& id, const Rect& anchor, ImU32 color)
{
    this->colorPopup.open = true;
    this->colorPopup.id = id;
    this->colorPopup.anchor = anchor;
    this->colorPopup.initial = color;
    this->colorPopup.pushedUndo = false;
    float r = (color & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 16) & 0xFF) / 255.0f;
    ImGui::ColorConvertRGBtoHSV(r, g, b, this->colorPopup.h, this->colorPopup.s, this->colorPopup.v);
    this->menuPopup.open = false;
    this->pickerPopup.open = false;
}

void DesignerApp::applyColor(ImU32 color)
{
    if (!this->colorPopup.open) return;
    const std::string id = this->colorPopup.id;
    const std::string hex = colorToHex(color);

    if (id.rfind("color:prop:", 0) == 0)
    {
        const std::string rest = id.substr(11);
        const size_t split = rest.find(':');
        if (split == std::string::npos) return;
        const std::string node = rest.substr(0, split);
        const std::string key = rest.substr(split + 1);
        Document* document = this->doc();
        if (document == nullptr) return;
        if (!this->colorPopup.pushedUndo)
        {
            document->pushUndo();
            this->colorPopup.pushedUndo = true;
        }
        Json* style = document->style(node, true);
        if (style == nullptr) return;
        (*style)[key] = hex;
        document->dirty = true;
        this->markDocumentChanged();
    }
    else if (id.rfind("appearance-color:", 0) == 0)
    {
        const std::string pointer = id.substr(17);
        Document* document = this->doc();
        if (document == nullptr) return;
        Json* value = document->valueAt(pointer);
        if (!this->colorPopup.pushedUndo)
        {
            document->pushUndo();
            this->colorPopup.pushedUndo = true;
        }
        if (value == nullptr)
            document->setValueAt(pointer, hex, false);
        else
            *value = hex;
        document->dirty = true;
        this->markDocumentChanged();
    }
    else if (id.rfind("color:token:", 0) == 0)
    {
        const std::string path = id.substr(12);
        std::string error;
        if (!this->project.setTokenValue(path, hex, error))
        {
            this->notifyError(error);
            return;
        }
        this->previewDirty = true;
        this->documentRevision++;
        this->refCountsDirty = true;
    }
    else if (id.rfind("color:local:", 0) == 0)
    {
        const std::string name = id.substr(12);
        Document* document = this->doc();
        if (document == nullptr) return;
        if (!this->colorPopup.pushedUndo)
        {
            document->pushUndo();
            this->colorPopup.pushedUndo = true;
        }
        Json* vars = document->vars(std::string(), true);
        if (vars == nullptr) return;
        if (!vars->is_object()) { document->root["vars"] = Json::object(); vars = &document->root["vars"]; }
        if (!vars->contains("colors") || !(*vars)["colors"].is_object()) (*vars)["colors"] = Json::object();
        (*vars)["colors"][name] = hex;
        document->dirty = true;
        this->markDocumentChanged();
    }
}

void DesignerApp::openDialog(const std::string& kind)
{
    const std::string context = this->dialogContext;
    this->dialogContext.clear();
    this->dialog = Dialog();
    this->dialog.open = true;
    this->dialog.kind = kind;
    this->dialog.focus = 0;
    this->dialog.context = context;

    if (kind == "new_page")
    {
        this->dialog.title = "新建页面";
        this->dialog.label1 = "页面名称";
        this->dialog.input1 = "page_2";
        this->dialog.label2 = "画布尺寸（宽x高）";
        this->dialog.input2 = std::to_string(this->project.canvasWidth) + "x" +
            std::to_string(this->project.canvasHeight);
    }
    else if (kind == "new_component")
    {
        this->dialog.title = "新建组件";
        this->dialog.label1 = "组件名称";
        this->dialog.input1 = "component_2";
        this->dialog.label2 = "初始尺寸（宽x高）";
        this->dialog.input2 = "320x160";
    }
    else if (kind == "open_project")
    {
        this->dialog.title = "打开工程";
        this->dialog.label1 = "工程路径（相对 projects/ 或绝对路径）";
        this->dialog.input1 = this->project.root.empty() ? "projects/my_project" : this->project.root.string();
        this->dialog.choices = this->listProjectDirectories();
    }
    else if (kind == "save_as")
    {
        this->dialog.title = "另存为";
        this->dialog.label1 = "相对工程根目录的路径";
        this->dialog.input1 = this->hasDoc() ? this->doc()->relative : "pages/untitled.json";
        this->dialog.confirmText = "保存";
    }
    else if (kind == "rename_file")
    {
        this->dialog.title = "重命名文件";
        this->dialog.label1 = "新文件名";
        this->dialog.input1 = baseName(this->dialog.context);
    }
    else if (kind == "delete_file")
    {
        this->dialog.title = "删除文件";
        this->dialog.message = "确定删除 " + this->dialog.context + " ？此操作不可撤销。";
        this->dialog.danger = true;
        this->dialog.confirmText = "删除";
    }
    else if (kind == "local_var_rename")
    {
        const std::string& ref = this->selectedToken;
        const bool color = ref.rfind("&color.", 0) == 0;
        const std::string name = ref.empty() ? std::string() : ref.substr(color ? 7 : 8);
        this->dialog.title = "重命名局部变量";
        this->dialog.label1 = "变量名（不含 &color. / &number.）";
        this->dialog.input1 = name;
        this->dialog.context = std::string(color ? "color:" : "number:") + name;
    }
    else if (kind == "new_token")
    {
        this->dialog.title = "新建全局变量";
        this->dialog.label1 = "变量路径（如 color.brand）";
        this->dialog.input1 = "color.brand";
        this->dialog.label2 = "值（颜色 / 数字 / 文本）";
        this->dialog.input2 = "#3B82F6";
    }
    else if (kind == "rename_token")
    {
        this->dialog.title = "重命名全局变量";
        this->dialog.label1 = "新路径";
        this->dialog.input1 = this->selectedToken;
    }
    else if (kind == "delete_token")
    {
        this->dialog.title = "删除全局变量";
        this->dialog.message = "确定删除变量 " + this->selectedToken + " ？";
        this->dialog.danger = true;
        this->dialog.confirmText = "删除";
    }
    else if (kind == "new_local")
    {
        this->dialog.title = "新建局部变量";
        this->dialog.label1 = "变量名（如 bgColor）";
        this->dialog.input1 = "newVar";
        this->dialog.label2 = "值（颜色 / 数字 / 变量路径）";
        this->dialog.input2 = "#3B82F6";
    }
    else if (kind == "rename_node")
    {
        const Json* node = this->selectedNodeJson();
        this->dialog.title = "重命名节点";
        this->dialog.label1 = "名称";
        this->dialog.input1 = node != nullptr ? node->value("name", node->value("id", std::string("节点"))) : "节点";
    }
    else if (kind == "about")
    {
        this->dialog.title = "关于 UI 设计器";
        this->dialog.message =
            "Belive Designer 1.0\n\n基于 BekUI (bklv) 的 JSON 界面设计器。\n"
            "画布使用真实的 bklv 视图渲染，文档、变量与属性检查器共享同一份数据模型。\n\n"
            "快捷键：⌘S 保存 · ⌘Z 撤销 · ⌘D 复制节点 · Del 删除 · ⌘0 适配画布";
        this->dialog.alertOnly = true;
        this->dialog.confirmText = "关闭";
    }
    else if (kind == "alert")
    {
        this->dialog.alertOnly = true;
        this->dialog.confirmText = "知道了";
    }
    else if (kind == "unsaved_close")
    {
        this->dialog.confirmText = "保存";
    }
    else if (kind == "reload_confirm")
    {
        this->dialog.confirmText = "重新加载";
    }
    else if (kind == "delete_node")
    {
        this->dialog.title = "删除节点";
        this->dialog.message = "确定删除选中的节点及其子节点？";
        this->dialog.danger = true;
        this->dialog.confirmText = "删除";
    }

    if (!this->dialog.input1.empty())
        this->beginEdit("dialog:1", this->dialog.input1, Rect());
    else if (!this->dialog.input2.empty())
        this->beginEdit("dialog:2", this->dialog.input2, Rect());
}

void DesignerApp::confirmDialog()
{
    if (!this->dialog.open) return;
    const std::string kind = this->dialog.kind;
    const std::string input1 = trim(this->dialog.input1);
    const std::string input2 = trim(this->dialog.input2);
    auto close = [this] {
        this->dialog = Dialog();
        if (this->edit.active) this->commitEdit(true);
    };

    if (kind == "alert" || kind == "about")
    {
        close();
        return;
    }

    if (kind == "new_page" || kind == "new_component")
    {
        if (!validIdentifier(input1))
        {
            this->dialog.error = "名称只能包含字母、数字、下划线、短横线和点";
            return;
        }
        int width = this->project.canvasWidth;
        int height = this->project.canvasHeight;
        if (!input2.empty())
        {
            const size_t x = input2.find('x');
            if (x != std::string::npos)
            {
                try
                {
                    width = std::stoi(input2.substr(0, x));
                    height = std::stoi(input2.substr(x + 1));
                }
                catch (const std::exception&)
                {
                }
            }
        }
        const std::string relative = (kind == "new_page" ? "view/" : "view/components/") + input1 + ".json";
        if (fs::exists(this->project.root / relative))
        {
            this->dialog.error = "文件已存在：" + relative;
            return;
        }
        Json root = makeDocumentRoot("Box", input1);
        root["width"] = width;
        root["height"] = height;
        std::string error;
        if (!writeJsonFile(this->project.root / relative, root, error, false))
        {
            this->dialog.error = error;
            return;
        }
        if (this->project.meta.contains("files") && this->project.meta["files"].is_array())
        {
            bool listed = false;
            for (const Json& entry : this->project.meta["files"])
                if (entry.is_string() && entry.get<std::string>() == relative) { listed = true; break; }
            if (!listed)
            {
                this->project.meta["files"].push_back(relative);
                std::string themeError;
                writeJsonFile(this->project.root / "theme.json", this->project.meta, themeError, false);
            }
        }
        close();
        this->refreshFiles();
        this->openDocument(relative, true);
        this->addToast("已创建 " + relative);
        return;
    }

    if (kind == "open_project")
    {
        fs::path path(input1);
        if (!path.is_absolute())
        {
            const std::string direct = input1;
            if (fs::exists(direct)) path = fs::absolute(direct);
            else path = fs::path(bklv::resourcePath("projects")) / input1;
        }
        if (!fs::exists(path / "project.json"))
        {
            this->dialog.error = "该目录下没有 project.json：" + path.string();
            return;
        }
        close();
        this->openProject(path, true);
        return;
    }

    if (kind == "save_as")
    {
        Document* document = this->doc();
        if (document == nullptr) { close(); return; }
        std::string relative = input1;
        if (relative.size() < 5 || relative.substr(relative.size() - 5) != ".json")
            relative += ".json";
        std::string error;
        if (!document->saveAs(this->project, relative, error))
        {
            this->dialog.error = error;
            return;
        }
        close();
        this->refreshFiles();
        this->addToast("已另存为 " + relative);
        this->markDocumentChanged();
        return;
    }

    if (kind == "rename_file")
    {
        const fs::path oldPath = this->project.root / this->dialog.context;
        const std::string directory = directoryOf(this->dialog.context);
        const fs::path newPath = this->project.root / (directory.empty() ? input1 : directory + "/" + input1);
        std::error_code code;
        if (input1.empty() || fs::exists(newPath, code))
        {
            this->dialog.error = "目标文件已存在或名称为空";
            return;
        }
        fs::rename(oldPath, newPath, code);
        if (code)
        {
            this->dialog.error = "重命名失败：" + code.message();
            return;
        }
        const std::string oldRelative = this->dialog.context;
        const std::string newRelative = directory.empty() ? input1 : directory + "/" + input1;
        for (auto& document : this->documents)
        {
            if (document->relative != oldRelative) continue;
            document->relative = newRelative;
            document->path = this->project.root / newRelative;
            document->title = baseName(newRelative);
        }
        close();
        this->refreshFiles();
        this->addToast("已重命名");
        return;
    }

    if (kind == "delete_file")
    {
        std::error_code code;
        fs::remove(this->project.root / this->dialog.context, code);
        if (code)
        {
            this->dialog.error = "删除失败：" + code.message();
            return;
        }
        const int index = this->findDocument(this->dialog.context);
        if (index >= 0) this->closeDocument(index, true);
        close();
        this->refreshFiles();
        this->addToast("已删除文件");
        return;
    }

    if (kind == "local_var_rename")
    {
        Document* document = this->doc();
        if (document == nullptr) { close(); return; }
        if (input1.empty() || input1.find(' ') != std::string::npos)
        {
            this->dialog.error = "变量名不能为空或包含空格";
            return;
        }
        const size_t split = this->dialog.context.find(':');
        const std::string block = split == std::string::npos ? "color" : this->dialog.context.substr(0, split);
        const std::string oldName = split == std::string::npos ? std::string() : this->dialog.context.substr(split + 1);
        if (!document->renameLocalVar(block, oldName, input1))
        {
            this->dialog.error = "重命名失败：变量不存在或新名称已存在";
            return;
        }
        this->selectedToken = std::string(block == "color" ? "&color." : "&number.") + input1;
        this->markDocumentChanged();
        this->addToast("已重命名局部变量");
        close();
        return;
    }

    if (kind == "new_token")
    {
        if (input1.empty() || input1.find(' ') != std::string::npos)
        {
            this->dialog.error = "变量路径不能为空或包含空格";
            return;
        }
        if (this->project.getToken(input1) != nullptr)
        {
            this->dialog.error = "变量已存在";
            return;
        }
        for (const Token& token : this->project.tokens)
        {
            if (token.path.rfind(input1 + ".", 0) == 0)
            {
                this->dialog.error = "不能把变量建在另一个变量下面：" + token.path;
                return;
            }
            if (input1.rfind(token.path + ".", 0) == 0)
            {
                this->dialog.error = "上级路径 " + token.path + " 已经是一个变量";
                return;
            }
        }
        Json value;
        ImU32 color = 0;
        float number = 0.0f;
        const bool isColor = parseColor(input2, color);
        if (isColor)
        {
            value = input2;
        }
        else
        {
            bool isNumber = false;
            try
            {
                size_t used = 0;
                number = std::stof(input2, &used);
                if (used == input2.size())
                {
                    value = number;
                    isNumber = true;
                }
            }
            catch (const std::exception&)
            {
            }
            if (!isNumber)
            {
                this->dialog.error = "全局变量只支持颜色（#RRGGBB）或数值";
                return;
            }
        }

        std::string file;
        if (isColor)
        {
            for (const std::string& relative : this->project.tokenFileOrder)
                if (relative.find("color") != std::string::npos) { file = relative; break; }
            if (file.empty()) file = this->project.tokenFileOrder.empty() ? "tokens/color.json" : this->project.tokenFileOrder[0];
        }
        else if (value.is_number())
        {
            for (const std::string& relative : this->project.tokenFileOrder)
                if (relative.find("spacing") != std::string::npos) { file = relative; break; }
            if (file.empty()) file = this->project.tokenFileOrder.empty() ? "tokens/spacing.json" : this->project.tokenFileOrder[0];
        }
        else
        {
            for (const std::string& relative : this->project.tokenFileOrder)
                if (relative.find("typography") != std::string::npos) { file = relative; break; }
            if (file.empty()) file = this->project.tokenFileOrder.empty() ? "tokens/typography.json" : this->project.tokenFileOrder[0];
        }

        auto fileIt = this->project.tokenFiles.find(file);
        if (fileIt == this->project.tokenFiles.end())
        {
            this->project.tokenFiles[file] = Json::object();
            this->project.tokenFileOrder.push_back(file);
            fileIt = this->project.tokenFiles.find(file);
        }
        if (!setDotted(fileIt->second, splitDots(input1), value))
        {
            this->dialog.error = "无法写入变量路径";
            return;
        }
        std::string error;
        if (!saveTokenFile(this->project, file, error))
        {
            this->dialog.error = error;
            return;
        }
        reloadTokens(this->project, error);
        this->selectedToken = input1;
        this->refCountsDirty = true;
        this->previewDirty = true;
        close();
        this->addToast("已创建变量 " + input1);
        return;
    }

    if (kind == "rename_token")
    {
        if (input1.empty() || this->project.getToken(input1) != nullptr)
        {
            this->dialog.error = "名称为空或已存在同名变量";
            return;
        }
        for (const Token& token : this->project.tokens)
        {
            if (token.path == this->selectedToken) continue;
            if (token.path.rfind(input1 + ".", 0) == 0)
            {
                this->dialog.error = "目标路径与已有变量冲突：" + token.path;
                return;
            }
            if (input1.rfind(token.path + ".", 0) == 0)
            {
                this->dialog.error = "上级路径 " + token.path + " 已经是一个变量";
                return;
            }
        }
        std::string error;
        if (!this->renameTokenGlobal(this->selectedToken, input1, error))
        {
            this->dialog.error = error;
            return;
        }
        close();
        this->addToast("已重命名变量");
        return;
    }

    if (kind == "delete_token")
    {
        const int count = this->tokenRefCount(this->selectedToken);
        if (count > 0)
        {
            this->dialog.error = "该变量仍被引用 " + std::to_string(count) + " 次，请先修改引用";
            return;
        }
        const int index = this->project.findToken(this->selectedToken);
        if (index >= 0)
        {
            Token& token = this->project.tokens[(size_t)index];
            auto fileIt = this->project.tokenFiles.find(token.file);
            if (fileIt != this->project.tokenFiles.end())
            {
                eraseDotted(fileIt->second, token.segments);
                std::string error;
                if (!saveTokenFile(this->project, token.file, error))
                {
                    this->dialog.error = error;
                    return;
                }
            }
        }
        std::string error;
        reloadTokens(this->project, error);
        this->selectedToken.clear();
        this->refCountsDirty = true;
        this->previewDirty = true;
        close();
        this->addToast("已删除变量");
        return;
    }

    if (kind == "new_local")
    {
        if (this->selectedNode.empty() || !this->hasDoc())
        {
            this->dialog.error = "请先选择一个节点";
            return;
        }
        if (input1.empty() || input1.find(' ') != std::string::npos)
        {
            this->dialog.error = "变量名不能为空或包含空格";
            return;
        }
        Json value;
        ImU32 color = 0;
        float number = 0.0f;
        if (parseColor(input2, color)) value = input2;
        else project.getToken(input2) != nullptr ? value = Json{ { "$ref", input2 } }
                                                : value = input2;
        if (!value.is_object() && !parseColor(input2, color))
        {
            try
            {
                size_t used = 0;
                number = std::stof(input2, &used);
                if (used == input2.size()) value = number;
            }
            catch (const std::exception&)
            {
            }
        }
        if (!this->doc()->setLocalVar(this->selectedNode, input1, value))
        {
            this->dialog.error = "无法写入局部变量";
            return;
        }
        close();
        this->markDocumentChanged();
        this->addToast("已添加局部变量 " + input1);
        return;
    }

    if (kind == "rename_node")
    {
        Json* node = this->selectedNode.empty() || !this->hasDoc() ? nullptr : this->doc()->node(this->selectedNode);
        if (node == nullptr)
        {
            this->dialog.error = "请先选择一个节点";
            return;
        }
        this->doc()->pushUndo();
        (*node)["name"] = input1;
        this->doc()->dirty = true;
        this->doc()->rebuild();
        this->markDocumentChanged();
        close();
        return;
    }

    if (kind == "unsaved_close")
    {
        int index = 0;
        try { index = std::stoi(this->dialog.context); } catch (const std::exception&) { }
        if (this->dialog.selectedChoice == 0)
        {
            if (this->documents[(size_t)index]->dirty)
            {
                std::string error;
                if (!this->documents[(size_t)index]->save(this->project, error))
                {
                    this->dialog.error = error;
                    return;
                }
            }
            close();
            this->closeDocument(index, true);
        }
        else if (this->dialog.selectedChoice == 1)
        {
            close();
            this->closeDocument(index, true);
        }
        else
        {
            close();
        }
        return;
    }

    if (kind == "reload_confirm")
    {
        if (this->dialog.selectedChoice != 0)
        {
            close();
            return;
        }
        Document* document = this->doc();
        if (document != nullptr)
        {
            std::string error;
            if (!document->load(this->project, document->relative))
                this->addToast(document->loadError, true);
            else
            {
                this->selectDefaultNode();
                this->markDocumentChanged();
                this->addToast("已重新加载");
            }
        }
        close();
        return;
    }

    if (kind == "delete_node")
    {
        close();
        this->deleteSelected();
        return;
    }

    close();
}

void DesignerApp::runAction(const std::string& action, const std::string& value)
{
    if (action == "save") this->saveActive(false);
    else if (action == "save_as") this->openDialog("save_as");
    else if (action == "reload") this->reloadActive();
    else if (action == "new_page" || action == "new_component") this->newDocument(action);
    else if (action == "appearance_add") this->addAppearanceLayer(value);
    else if (action == "appearance_import_pick") this->openAppearanceImportPicker();
    else if (action == "appearance_import") this->importAppearanceLayer(value);
    else if (action == "project_open") this->requestProjectFolder(NativePickMode::OPEN_PROJECT);
    else if (action == "project_copy") this->requestProjectFolder(NativePickMode::COPY_SOURCE);
    else if (action == "close_doc" && this->hasDoc()) this->closeDocument(this->activeDoc);
    else if (action == "undo") this->undo();
    else if (action == "redo") this->redo();
    else if (action == "duplicate") this->duplicateSelected();
    else if (action == "delete") this->deleteSelected();
    else if (action == "move_up") this->moveSelectedNode(-1);
    else if (action == "move_down") this->moveSelectedNode(1);
    else if (action == "copy_node")
    {
        if (const Json* node = this->selectedNodeJson())
        {
            setClipboard(node->dump(2));
            this->addToast("节点 JSON 已复制到剪贴板");
        }
    }
    else if (action == "paste_node")
    {
        Document* document = this->doc();
        if (document == nullptr || !document->root.is_object())
        {
            this->addToast("没有可粘贴的文档", true);
            return;
        }
        Json node;
        try
        {
            node = Json::parse(readClipboard());
        }
        catch (const std::exception&)
        {
            this->addToast("剪贴板内容不是合法 JSON", true);
            return;
        }
        std::string parent = this->selectedNode;
        if (!parent.empty())
        {
            const Json* selected = document->node(parent);
            const std::string type = selected != nullptr ? selected->value("type", std::string()) : std::string();
            if (type == "Label" || type == "Image")
                parent = parentPointer(parent);
        }
        if (!document->insertNode(parent, -1, node))
            this->addToast("粘贴失败", true);
        else
        {
            Json* parentNode = document->node(parent);
            const int index = parentNode != nullptr && parentNode->contains("children")
                ? (int)(*parentNode)["children"].size() - 1 : 0;
            this->selectedNode = parent.empty() ? ("/children/" + std::to_string(index))
                                                : (parent + "/children/" + std::to_string(index));
            this->markDocumentChanged();
            this->addToast("已粘贴节点");
        }
    }
    else if (action == "toggle_project") { this->showProject = !this->showProject; this->applyLayout(); }
    else if (action == "toggle_vars") { this->showVars = !this->showVars; this->applyLayout(); }
    else if (action == "toggle_nodes") { this->showNodeTree = !this->showNodeTree; this->applyLayout(); }
    else if (action == "toggle_inspector") { this->showInspector = !this->showInspector; this->applyLayout(); }
    else if (action == "toggle_status") { this->showStatus = !this->showStatus; this->applyLayout(); }
    else if (action == "toggle_grid") this->showGrid = !this->showGrid;
    else if (action == "toggle_guides") this->showGuides = !this->showGuides;
    else if (action == "fit") this->fitCanvas();
    else if (action == "zoom_100") { this->canvasZoom = 1.0f; this->canvasFit = false; this->canvasPanX = 0; this->canvasPanY = 0; this->previewDirty = true; }
    else if (action == "zoom_in") { this->canvasFit = false; this->canvasZoom = std::min(2.0f, this->canvasZoom + 0.1f); this->previewDirty = true; }
    else if (action == "zoom_out") { this->canvasFit = false; this->canvasZoom = std::max(0.15f, this->canvasZoom - 0.1f); this->previewDirty = true; }
    else if (action == "theme_dark") { this->lightTheme = false; this->applyThemePalette(); }
    else if (action == "theme_light") { this->lightTheme = true; this->applyThemePalette(); }
    else if (action == "text_small") { this->textScale = 0.9f; ui::setTextScale(this->textScale); this->addToast("界面字号：小"); }
    else if (action == "text_normal") { this->textScale = 1.0f; ui::setTextScale(this->textScale); this->addToast("界面字号：标准"); }
    else if (action == "text_large") { this->textScale = 1.15f; ui::setTextScale(this->textScale); this->addToast("界面字号：大"); }
    else if (action == "text_inc")
    {
        this->textScale = std::min(1.35f, this->textScale + 0.05f);
        ui::setTextScale(this->textScale);
        this->addToast("界面字号：" + std::to_string((int)std::round(this->textScale * 100.0f)) + "%");
    }
    else if (action == "text_dec")
    {
        this->textScale = std::max(0.8f, this->textScale - 0.05f);
        ui::setTextScale(this->textScale);
        this->addToast("界面字号：" + std::to_string((int)std::round(this->textScale * 100.0f)) + "%");
    }
    else if (action == "reset_layout")
    {
        this->leftWidth = 280.0f; this->varsWidth = 244.0f; this->rightWidth = 310.0f;
        this->projectHeight = 330.0f;
        this->showProject = this->showVars = this->showNodeTree = this->showInspector = true;
        this->showStatus = true;
        this->applyLayout();
        this->addToast("已恢复默认布局");
    }
    else if (action == "about") this->openDialog("about");
    else if (action == "toggle_preview")
    {
        this->canvasPreviewMode = !this->canvasPreviewMode;
        this->addToast(this->canvasPreviewMode ? "已进入预览模式" : "已返回设计模式");
    }
    else if (action == "local_var_add")
    {
        Document* document = this->doc();
        if (document == nullptr) { this->addToast("请先打开一个视图文档", true); return; }
        const bool color = this->tabTokenType == TokenType::COLOR;
        const char* group = color ? "colors" : "numbers";
        const Json* vars = document->vars(std::string());
        std::string base = color ? "color" : "number";
        int n = 1;
        std::string name;
        do { name = base + std::to_string(n++); }
        while (vars != nullptr && vars->contains(group) && (*vars)[group].is_object() &&
               (*vars)[group].contains(name));
        document->setLocalVar(std::string(), name, color ? Json("#3B82F6") : Json(8));
        this->selectedToken = std::string(color ? "&color." : "&number.") + name;
        this->markDocumentChanged();
        this->addToast("已新建局部变量 " + name);
    }
    else if (action == "local_var_rename")
    {
        if (this->selectedToken.rfind("&", 0) != 0)
        {
            this->addToast("请选择一个局部变量", true);
            return;
        }
        this->openDialog("local_var_rename");
    }
    else if (action == "local_var_delete")
    {
        Document* document = this->doc();
        if (document == nullptr || this->selectedToken.rfind("&", 0) != 0) return;
        const bool color = this->selectedToken.rfind("&color.", 0) == 0;
        const std::string name = this->selectedToken.substr(color ? 7 : 8);
        const int refs = this->tokenRefCount(this->selectedToken);
        if (refs > 0)
        {
            this->addToast("仍有 " + std::to_string(refs) + " 处引用，请先解除引用", true);
            return;
        }
        if (document->eraseLocalVar(std::string(), name))
        {
            this->selectedToken.clear();
            this->markDocumentChanged();
            this->addToast("已删除局部变量 " + name);
        }
    }
    else if (action == "var_new") this->openDialog("new_token");
    else if (action == "var_rename" && !this->selectedToken.empty()) this->openDialog("rename_token");
    else if (action == "var_delete" && !this->selectedToken.empty()) this->openDialog("delete_token");
    else if (action == "var_refresh")
    {
        std::string error;
        if (!reloadTokens(this->project, error)) this->addToast(error, true);
        this->refCountsDirty = true;
        this->previewDirty = true;
    }
    else if (action == "node_rename") this->addToast("bklv 视图节点没有名称字段，请重命名文件或修改 Label.text");
    else if (action == "node_add_box") this->addChildNode("Box");
    else if (action == "node_add_label") this->addChildNode("Label");
    else if (action == "node_add_image") this->addChildNode("Image");
    else if (action == "node_add_button") this->addChildNode("Button");
    else if (action == "node_refresh") this->markDocumentChanged();
    else if (action == "open_file") this->openDocument(value, true);
    else if (action == "select_token") this->selectToken(value);
    else if (action == "canvas_ratio")
    {
        if (value == "16:9") { this->canvasResolutionH = this->canvasResolutionW * 9 / 16; }
        else if (value == "16:10") { this->canvasResolutionH = this->canvasResolutionW * 10 / 16; }
        else if (value == "4:3") { this->canvasResolutionH = this->canvasResolutionW * 3 / 4; }
        else if (value == "9:16") { this->canvasResolutionH = this->canvasResolutionW * 16 / 9; }
        this->markDocumentChanged();
        this->fitCanvas();
    }
    else if (action == "canvas_resolution")
    {
        const size_t x = value.find('x');
        if (x != std::string::npos)
        {
            try
            {
                this->canvasResolutionW = std::stoi(value.substr(0, x));
                this->canvasResolutionH = std::stoi(value.substr(x + 1));
            }
            catch (const std::exception&)
            {
            }
        }
        this->fitCanvas();
    }
    else if (action == "continue_node")
    {
        this->selectedNode = value;
        this->ensureNodeExpanded(value);
    }
    else if (action == "file_rename_ctx")
    {
        this->dialogContext = value;
        this->openDialog("rename_file");
    }
    else if (action == "json_apply")
    {
        if (this->leftNodePanel != nullptr) this->leftNodePanel->applyJsonCode();
    }
    else if (action == "file_delete_ctx")
    {
        this->dialogContext = value;
        this->openDialog("delete_file");
    }
}

void DesignerApp::handleShortcuts(UiInput& input)
{
    (void)input;
    ImGuiIO& io = ImGui::GetIO();
    if (io.AppFocusLost)
        return;
    const bool command = io.KeySuper || io.KeyCtrl;
    if (command && ImGui::IsKeyPressed(ImGuiKey_S, false)) this->saveActive(false);
    else if (command && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if (io.KeyShift) this->redo();
        else this->undo();
    }
    else if (command && ImGui::IsKeyPressed(ImGuiKey_D, false)) this->duplicateSelected();
    else if (command && ImGui::IsKeyPressed(ImGuiKey_O, false)) this->requestProjectFolder(NativePickMode::OPEN_PROJECT);
    else if (command && ImGui::IsKeyPressed(ImGuiKey_0, false)) this->fitCanvas();
    else if (command && ImGui::IsKeyPressed(ImGuiKey_Equal, false))
    {
        this->canvasFit = false;
        this->canvasZoom = std::min(2.0f, this->canvasZoom + 0.1f);
        this->previewDirty = true;
    }
    else if (command && ImGui::IsKeyPressed(ImGuiKey_Minus, false))
    {
        this->canvasFit = false;
        this->canvasZoom = std::max(0.15f, this->canvasZoom - 0.1f);
        this->previewDirty = true;
    }
    else if (command && ImGui::IsKeyPressed(ImGuiKey_R, false)) this->reloadActive();
    else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
    {
        // Guard against stray key events while the window is still being mapped.
        if (this->frameCounter < 10)
            return;
        if (this->hasDoc() && !this->selectedNode.empty())
            this->deleteSelected();
    }

    else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        this->closePopups();
        this->selectedNode.clear();
    }
}

void DesignerApp::handleSplitters(UiInput& input)
{
    if (this->root == nullptr) return;
    const float bodyY = 46.0f;
    const float bodyBottom = this->root->getHeight() - (this->showStatus ? 24.0f : 0.0f);
    const float bodyHeight = bodyBottom - bodyY;
    const float rightEdge = this->root->getWidth() - (this->showInspector ? this->rightWidth : 0.0f);
    const bool leftAny = this->showProject || this->showNodeTree;
    const float leftEdgeWidth = leftAny ? this->leftWidth : 0.0f;
    const float varsX = leftEdgeWidth + this->varsWidth;

    struct Line
    {
        int mode;
        bool vertical;
        float position;
    };

    std::vector<Line> lines;
    if (leftAny)
        lines.push_back({ 1, true, this->leftWidth });
    if (this->showVars)
        lines.push_back({ 6, true, varsX });
    if (this->showInspector)
        lines.push_back({ 2, true, rightEdge });
    if (this->showProject && this->showNodeTree)
        lines.push_back({ 3, false, bodyY + this->projectHeight });

    if (input.pressed && this->resizeMode == 0)
    {
        for (const Line& line : lines)
        {
            const float distance = line.vertical ? std::fabs(input.mouse.x - line.position)
                                                 : std::fabs(input.mouse.y - line.position);
            const bool inside = line.vertical
                ? (input.mouse.y >= bodyY && input.mouse.y <= bodyBottom)
                : (input.mouse.x >= 0.0f && input.mouse.x <= this->leftWidth);
            if (distance <= 4.0f && inside)
            {
                this->resizeMode = line.mode;
                this->resizeStartMouse = line.vertical ? input.mouse.x : input.mouse.y;
                this->resizeStartValue = line.mode == 1 ? this->leftWidth
                    : line.mode == 2 ? this->rightWidth
                    : line.mode == 3 ? this->projectHeight
                                     : this->varsWidth;
                break;
            }
        }
    }

    if (this->resizeMode != 0)
    {
        if (input.down)
        {
            const bool vertical = this->resizeMode == 1 || this->resizeMode == 2 || this->resizeMode == 6;
            const float delta = vertical ? input.mouse.x - this->resizeStartMouse
                                         : input.mouse.y - this->resizeStartMouse;
            if (this->resizeMode == 1)
                this->leftWidth = std::clamp(this->resizeStartValue + delta, 220.0f,
                    std::min(440.0f, this->root->getWidth() * 0.32f));
            else if (this->resizeMode == 2)
                this->rightWidth = std::clamp(this->resizeStartValue - delta, 280.0f,
                    std::max(280.0f, this->root->getWidth() * 0.38f));
            else if (this->resizeMode == 6)
                this->varsWidth = std::clamp(this->resizeStartValue + delta, 210.0f,
                    std::min(420.0f, this->root->getWidth() * 0.32f));
            else if (this->resizeMode == 3)
                this->projectHeight = std::clamp(this->resizeStartValue + delta, 170.0f, bodyHeight - 200.0f);
            this->applyLayout();
            if (this->canvasFit)
                this->previewDirty = true;
        }
        else
        {
            this->resizeMode = 0;
        }
    }

    bool hovering = false;
    for (const Line& line : lines)
    {
        const float distance = line.vertical ? std::fabs(input.mouse.x - line.position)
                                             : std::fabs(input.mouse.y - line.position);
        if (distance <= 4.0f)
            hovering = true;
    }
    if (this->resizeMode == 1 || this->resizeMode == 2 || this->resizeMode == 6)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    else if (this->resizeMode == 3)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    else if (hovering)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

void DesignerApp::drawSplitters(ImDrawList* drawList)
{
    if (this->root == nullptr) return;
    const float bodyY = 46.0f;
    const float bodyBottom = this->root->getHeight() - (this->showStatus ? 24.0f : 0.0f);
    const float rightEdge = this->root->getWidth() - (this->showInspector ? this->rightWidth : 0.0f);
    const bool leftAny = this->showProject || this->showNodeTree;
    const float leftEdgeWidth = leftAny ? this->leftWidth : 0.0f;
    const float varsX = leftEdgeWidth + this->varsWidth;

    if (leftAny)
        ui::fill(drawList, Rect(this->leftWidth - 1, bodyY, 2, bodyBottom - bodyY),
            uiAlpha(this->palette->border, 0.8f));
    if (this->showVars)
        ui::fill(drawList, Rect(varsX - 1, bodyY, 2, bodyBottom - bodyY),
            uiAlpha(this->palette->border, 0.8f));
    if (this->showInspector)
        ui::fill(drawList, Rect(rightEdge - 1, bodyY, 2, bodyBottom - bodyY),
            uiAlpha(this->palette->border, 0.8f));
    if (this->showProject && this->showNodeTree)
        ui::fill(drawList, Rect(0, bodyY + this->projectHeight, this->leftWidth, 1),
            uiAlpha(this->palette->border, 0.9f));
}

std::vector<std::string> DesignerApp::listProjectDirectories() const
{
    std::vector<std::string> result;
    std::error_code code;
    const fs::path base(bklv::resourcePath("projects"));
    if (!fs::exists(base, code)) return result;
    for (const fs::directory_entry& entry : fs::directory_iterator(base, code))
    {
        if (entry.is_directory() && fs::exists(entry.path() / "theme.json", code))
            result.push_back(entry.path().filename().string());
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::string DesignerApp::activeDocRelative() const
{
    return this->hasDoc() ? this->documents[(size_t)this->activeDoc]->relative : std::string();
}

std::string DesignerApp::nodeLabel(const Node& node) const
{
    if (!node.name.empty() && node.name != node.id) return node.name;
    if (!node.id.empty()) return node.id;
    return node.type;
}

std::string DesignerApp::nodeBreadcrumb() const
{
    if (!this->hasDoc()) return std::string("未选择节点");
    std::vector<std::string> chain;
    std::string pointer = this->selectedNode;
    while (!pointer.empty())
    {
        const Node* node = findNode(this->doc()->tree, pointer);
        if (node != nullptr) chain.push_back(this->nodeLabel(*node));
        pointer = parentPointer(pointer);
    }
    chain.push_back(this->nodeLabel(this->doc()->tree));
    std::reverse(chain.begin(), chain.end());
    std::string result;
    for (size_t i = 0; i < chain.size(); ++i)
    {
        if (i > 0) result += " / ";
        result += chain[i];
    }
    return result;
}

void DesignerApp::tokenCountsRefresh() const
{
    if (!this->refCountsDirty) return;
    this->refCounts.clear();
    for (const auto& document : this->documents)
        countRefsInto(document->root, this->refCounts);

    std::error_code code;
    for (const std::string& directory : { std::string("pages"), std::string("components") })
    {
        const fs::path base = this->project.root / directory;
        if (!fs::exists(base, code)) continue;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(base, code))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const std::string relative = fs::relative(entry.path(), this->project.root).generic_string();
            bool open = false;
            for (const auto& document : this->documents)
                if (document->relative == relative) { open = true; break; }
            if (open) continue;
            std::string error;
            Json content = parseJsonFile(entry.path(), error);
            if (content.is_object()) countRefsInto(content, this->refCounts);
        }
    }
    this->refCountsDirty = false;
}

int DesignerApp::tokenRefCount(const std::string& reference) const
{
    if (!this->hasDoc()) return 0;
    return countRefsInJson(this->doc()->root, reference);
}

bool DesignerApp::renameTokenGlobal(const std::string& oldPath, const std::string& newPath, std::string& error)
{
    const int index = this->project.findToken(oldPath);
    if (index < 0)
    {
        error = "变量不存在：" + oldPath;
        return false;
    }
    if (this->project.getToken(newPath) != nullptr)
    {
        error = "目标变量已存在：" + newPath;
        return false;
    }
    Token token = this->project.tokens[(size_t)index];
    auto fileIt = this->project.tokenFiles.find(token.file);
    if (fileIt == this->project.tokenFiles.end())
    {
        error = "变量来源文件未加载";
        return false;
    }
    if (!eraseDotted(fileIt->second, token.segments))
    {
        error = "无法从源文件移除旧变量";
        return false;
    }
    if (!setDotted(fileIt->second, splitDots(newPath), token.value))
    {
        error = "无法写入新变量路径";
        return false;
    }
    if (!saveTokenFile(this->project, token.file, error))
        return false;

    for (auto& document : this->documents)
    {
        const int changed = renameRefsInJson(document->root, oldPath, newPath);
        if (changed > 0)
        {
            document->dirty = true;
            document->rebuild();
        }
    }

    std::error_code code;
    for (const std::string& directory : { std::string("pages"), std::string("components") })
    {
        const fs::path base = this->project.root / directory;
        if (!fs::exists(base, code)) continue;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(base, code))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const std::string relative = fs::relative(entry.path(), this->project.root).generic_string();
            bool open = false;
            for (const auto& document : this->documents)
                if (document->relative == relative) { open = true; break; }
            if (open) continue;
            std::string parseError;
            Json content = parseJsonFile(entry.path(), parseError);
            if (!content.is_object()) continue;
            if (renameRefsInJson(content, oldPath, newPath) > 0)
            {
                std::string writeError;
                if (!writeJsonFile(entry.path(), content, writeError, true))
                {
                    error = writeError;
                    return false;
                }
            }
        }
    }

    std::string reloadError;
    if (!reloadTokens(this->project, reloadError))
    {
        error = reloadError;
        return false;
    }
    this->selectedToken = newPath;
    this->refCountsDirty = true;
    this->previewDirty = true;
    this->documentRevision++;
    return true;
}

void DesignerApp::drawTooltip(ImDrawList* drawList)
{
    if (this->tooltip.empty() || this->dialog.open || this->menuPopup.open || this->pickerPopup.open ||
        this->colorPopup.open)
        return;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float width = ui::textWidth(this->tooltip, 12.0f) + 20.0f;
    const float height = 26.0f;
    float x = mouse.x + 14.0f;
    float y = mouse.y + 20.0f;
    const ImGuiIO& io = ImGui::GetIO();
    if (x + width > io.DisplaySize.x - 8.0f) x = io.DisplaySize.x - width - 8.0f;
    if (y + height > io.DisplaySize.y - 8.0f) y = mouse.y - height - 10.0f;
    Rect rect(x, y, width, height);
    ui::fill(drawList, rect, uiAlpha(this->palette->panelRaised, 0.98f), 6.0f);
    ui::stroke(drawList, rect, this->palette->border, 6.0f);
    ui::textVC(drawList, x + 10, y, height, this->tooltip, this->palette->text, 12.0f);
}

void DesignerApp::tick()
{
    ui::setTextScale(this->textScale);

    if (!this->initialised)
    {
        this->initialised = true;
        this->applyThemePalette();
        this->applyLayout();
    }

    if (std::fabs(bklv::getUiScale() - 1.0f) > 0.001f)
    {
        bklv::setUiScale(1.0f);
        bklv::App::requestLayout();
    }

    this->applyLayout();

    ImGuiIO& io = ImGui::GetIO();
    UiInput input;
    input.mouse = io.MousePos;
    input.delta = io.MouseDelta;
    input.down = ImGui::IsMouseDown(0);
    input.pressed = ImGui::IsMouseClicked(0);
    input.released = ImGui::IsMouseReleased(0);
    input.rightPressed = ImGui::IsMouseClicked(1);
    input.doubleClicked = ImGui::IsMouseDoubleClicked(0);
    input.wheel = io.MouseWheel;
    input.shift = io.KeyShift;
    input.command = io.KeySuper || io.KeyCtrl;

    this->frameCounter++;
    for (const ScriptedClick& click : this->scriptedClicks)
    {
        if (this->frameCounter == click.frame)
        {
            input.mouse = click.position;
            input.pressed = true;
            input.down = true;
            input.released = false;
        }
        else if (this->frameCounter == click.frame + 1)
        {
            input.mouse = click.position;
            input.pressed = false;
            input.down = false;
            input.released = true;
        }
    }

    for (const ScriptedEdit& script : this->scriptedEdits)
    {
        if (this->frameCounter != script.frame) continue;
        if (this->edit.active && (script.key.empty() || this->edit.key == script.key))
        {
            this->edit.buffer = script.text;
            this->edit.caret = (int)this->edit.buffer.size();
            if (script.commit)
                this->commitEdit(false);
        }
    }

    for (const ScriptedAction& script : this->scriptedActions)
    {
        if (this->frameCounter == script.frame)
            this->runAction(script.action, script.value);
    }

    for (const ScriptedCode& script : this->scriptedCodes)
    {
        if (this->frameCounter == script.frame && this->code.active)
        {
            this->code.text = script.text;
            this->code.caret = (int)this->code.text.size();
        }
    }

    for (const ScriptedDrag& drag : this->scriptedDrags)
    {
        const int duration = 6;
        if (this->frameCounter < drag.frame || this->frameCounter > drag.frame + duration) continue;
        const float t = (float)(this->frameCounter - drag.frame) / (float)duration;
        input.mouse = ImVec2(drag.from.x + (drag.to.x - drag.from.x) * t,
            drag.from.y + (drag.to.y - drag.from.y) * t);
        input.down = this->frameCounter < drag.frame + duration;
        input.pressed = this->frameCounter == drag.frame;
        input.released = this->frameCounter == drag.frame + duration;
        input.delta = ImVec2((drag.to.x - drag.from.x) / duration, (drag.to.y - drag.from.y) / duration);
    }

    this->syncInputModal();
    this->pollNativeFolder();
    this->mousePosition = input.mouse;
    this->tooltip.clear();
    this->tooltipPos = input.mouse;
    if (!this->drag.active && !this->drag.pressed && !input.down)
        this->dropTargets.clear();

    // Commit inline editing when clicking outside the active field.
    if (this->edit.active && input.pressed && !this->edit.rect.contains(input.mouse))
    {
        if (this->edit.key.rfind("dialog:", 0) == 0)
        {
            if (!this->dialog.open)
                this->commitEdit(false);
        }
        else if (this->edit.key != "search")
        {
            this->commitEdit(false);
        }
    }

    if (this->edit.active)
        this->handleEditKeys(input);
    else if (!this->code.active && !this->dialog.open && !this->menuPopup.open && !this->pickerPopup.open &&
        !this->colorPopup.open)
        this->handleShortcuts(input);

    if (this->dialog.open || this->menuPopup.open || this->pickerPopup.open || this->colorPopup.open)
    {
        this->overlayInput(input);
    }
    else
    {
        this->handleSplitters(input);
        if (this->resizeMode == 0)
        {
            if (this->menuBar) this->menuBar->handleInput(input);
            if (this->projectPanel) this->projectPanel->handleInput(input);
            if (this->varsPanel) this->varsPanel->handleInput(input);
            if (this->leftNodePanel) this->leftNodePanel->handleInput(input);
            if (this->tabsPanel) this->tabsPanel->handleInput(input);
            if (this->toolbarPanel) this->toolbarPanel->handleInput(input);
            if (this->canvasPanel) this->canvasPanel->handleInput(input);
            if (this->inspectorPanel) this->inspectorPanel->handleInput(input);
        }
    }

    // Variable drag lifecycle
    if (this->drag.pressed)
    {
        if (input.down)
        {
            const float moved = std::fabs(input.mouse.x - this->drag.start.x) + std::fabs(input.mouse.y - this->drag.start.y);
            if (moved > 4.0f) this->drag.active = true;
        }
        if (input.released)
        {
            this->dropVariable();
        }
    }

    if (!this->drag.active && !this->drag.pressed && !input.down)
        this->dropTargets.clear();

    // Node drag lifecycle
    if (this->nodeDrag.pressed)
    {
        if (input.down)
        {
            const float moved = std::fabs(input.mouse.x - this->nodeDrag.start.x) + std::fabs(input.mouse.y - this->nodeDrag.start.y);
            if (moved > 5.0f) this->nodeDrag.active = true;
        }
        if (input.released)
        {
            if (this->nodeDrag.active && this->hasDoc())
            {
                Document* document = this->doc();
                const std::string from = this->nodeDrag.pointer;
                const std::string target = this->nodeDrag.dropTarget;
                const Node* movingNode = findNode(document->tree, from);
                const std::string movingId = movingNode != nullptr ? movingNode->id : std::string();
                if (!target.empty() && from != target)
                {
                    bool ok = false;
                    if (this->nodeDrag.dropMode == 1)
                    {
                        ok = document->moveNode(from, target, -1);
                    }
                    else
                    {
                        const std::string parent = parentPointer(target);
                        int index = childIndex(target);
                        if (index >= 0)
                        {
                            if (this->nodeDrag.dropMode == 2) index++;
                            ok = document->moveNode(from, parent, index);
                        }
                    }
                    if (ok)
                    {
                        this->markDocumentChanged();
                        if (!movingId.empty()) this->selectNodeById(movingId);
                        this->addToast("节点顺序已更新");
                    }
                    else
                    {
                        this->addToast("不能把节点移动到自身或它的子节点中", true);
                    }
                }
            }
            this->nodeDrag = NodeDrag();
        }
    }

    if (!input.down)
    {
        this->canvasPanning = false;
        this->blueprintPanning = false;
    }

    // Drop old toasts
    const double now = ImGui::GetTime();
    while (!this->toasts.empty() && now - this->toasts.front().start > this->toasts.front().duration)
        this->toasts.pop_front();

    if (this->canvasPanel != nullptr)
        this->canvasPanel->rebuildPreview();
}

Rect DesignerApp::popupRect(const PopupState& popup, int itemCount) const
{
    (void)itemCount;
    const float itemHeight = 30.0f;
    const float padding = 6.0f;
    float height = padding * 2.0f;
    for (const MenuItem& item : popup.items)
        height += item.separator ? 9.0f : itemHeight;
    height = std::min(height, popup.maxHeight);
    ImVec2 viewport = ImGui::GetIO().DisplaySize;
    float x = popup.alignRight ? popup.anchor.x + popup.anchor.w - popup.width : popup.anchor.x;
    float y = popup.anchor.y + popup.anchor.h;
    if (x < 8) x = 8;
    if (x + popup.width > viewport.x - 8) x = viewport.x - popup.width - 8;
    if (y + height > viewport.y - 8) y = popup.anchor.y - height;
    if (y < 8) y = 8;
    return Rect(x, y, popup.width, height);
}

int DesignerApp::popupHit(const PopupState& popup, ImVec2 point) const
{
    Rect bounds = this->popupRect(popup, (int)popup.items.size());
    if (!bounds.contains(point)) return -1;
    float y = bounds.y + 6.0f - popup.scroll;
    for (size_t i = 0; i < popup.items.size(); ++i)
    {
        const float height = popup.items[i].separator ? 9.0f : 30.0f;
        if (point.y >= y && point.y < y + height)
        {
            if (point.y < bounds.y + 4.0f || point.y > bounds.y + bounds.h - 4.0f) return -1;
            return popup.items[i].separator ? -1 : (int)i;
        }
        y += height;
    }
    return -1;
}

void DesignerApp::overlayInput(UiInput& input)
{
    ImGuiIO& io = ImGui::GetIO();

    if (this->dialog.open)
    {
        const float width = 460.0f;
        const float height = this->dialog.alertOnly ? 190.0f : (this->dialog.message.empty() ? 260.0f : 280.0f);
        Rect rect((io.DisplaySize.x - width) * 0.5f, (io.DisplaySize.y - height) * 0.5f, width, height);
        const Rect confirmRect(rect.x + rect.w - 196.0f, rect.y + rect.h - 54.0f, 86.0f, 32.0f);
        const Rect cancelRect(rect.x + rect.w - 100.0f, rect.y + rect.h - 54.0f, 86.0f, 32.0f);

        if (input.pressed)
        {
            if (!rect.contains(input.mouse))
            {
                if (!this->dialog.alertOnly)
                {
                    this->dialog = Dialog();
                    if (this->edit.active) this->commitEdit(true);
                }
                return;
            }
            if (confirmRect.contains(input.mouse))
            {
                this->confirmDialog();
                return;
            }
            if (cancelRect.contains(input.mouse) && !this->dialog.alertOnly)
            {
                this->dialog = Dialog();
                if (this->edit.active) this->commitEdit(true);
                return;
            }
            if (!this->dialog.alertOnly)
            {
                if (!this->dialog.label1.empty() && !this->dialog.input1.empty() ||
                    (!this->dialog.label1.empty() && this->dialog.kind.rfind("new_", 0) == 0))
                {
                }
                // input fields are handled by geometry in overlay draw; approximate here
                const float inputY = rect.y + 84.0f;
                Rect field1(rect.x + 24.0f, inputY, rect.w - 48.0f, 32.0f);
                if (field1.contains(input.mouse))
                {
                    this->beginEdit("dialog:1", this->dialog.input1, field1);
                    return;
                }
                if (!this->dialog.input2.empty() || !this->dialog.label2.empty())
                {
                    Rect field2(rect.x + 24.0f, inputY + 64.0f, rect.w - 48.0f, 32.0f);
                    if (field2.contains(input.mouse))
                    {
                        this->beginEdit("dialog:2", this->dialog.input2, field2);
                        return;
                    }
                }
                {
                    const float listTop = rect.y + 84.0f + ((this->dialog.input2.empty() && this->dialog.label2.empty()) ? 0.0f : 64.0f);
                    float y = listTop;
                    for (size_t i = 0; i < this->dialog.choices.size(); ++i)
                    {
                        Rect choice(rect.x + 24.0f, y, rect.w - 48.0f, 24.0f);
                        if (choice.contains(input.mouse))
                        {
                            this->dialog.selectedChoice = (int)i;
                            if (this->dialog.kind == "open_project")
                            {
                                this->dialog.input1 = this->dialog.choices[i];
                                if (this->edit.active && this->edit.key == "dialog:1")
                                {
                                    this->edit.buffer = this->dialog.input1;
                                    this->edit.caret = (int)this->edit.buffer.size();
                                }
                            }
                            this->commitEdit(false);
                            return;
                        }
                        y += 26.0f;
                    }
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !this->dialog.alertOnly)
        {
            this->dialog = Dialog();
            if (this->edit.active) this->commitEdit(true);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && !this->edit.active)
        {
            this->confirmDialog();
        }
        return;
    }

    if (this->colorPopup.open)
    {
        const float width = 208.0f;
        const float height = 226.0f;
        ImVec2 viewport = io.DisplaySize;
        float x = this->colorPopup.anchor.x;
        float y = this->colorPopup.anchor.y + this->colorPopup.anchor.h + 4.0f;
        if (x + width > viewport.x - 8) x = viewport.x - width - 8;
        if (y + height > viewport.y - 8) y = this->colorPopup.anchor.y - height - 4.0f;
        if (y < 8) y = 8;
        Rect rect(x, y, width, height);
        const Rect sv(rect.x + 12, rect.y + 12, width - 24, 120);
        const Rect hue(rect.x + 12, rect.y + 142, width - 24, 12);
        const Rect hex(rect.x + 12, rect.y + 168, width - 24, 28);
        const Rect close(rect.x + width - 30, rect.y + 8, 20, 20);

        if (input.pressed)
        {
            if (close.contains(input.mouse))
            {
                this->colorPopup.open = false;
                return;
            }
            if (hex.contains(input.mouse))
            {
                float r = 0, g = 0, b = 0;
                ImGui::ColorConvertHSVtoRGB(this->colorPopup.h, this->colorPopup.s, this->colorPopup.v, r, g, b);
                const ImU32 color = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
                this->beginEdit("colorhex", colorToHex(color), hex);
                return;
            }
            if (sv.contains(input.mouse))
            {
                this->colorPopup.dragSv = true;
            }
            else if (hue.contains(input.mouse))
            {
                this->colorPopup.dragHue = true;
            }
            else if (!rect.contains(input.mouse))
            {
                this->colorPopup.open = false;
                return;
            }
        }

        if (this->colorPopup.dragSv && input.down)
        {
            this->colorPopup.s = std::clamp((input.mouse.x - sv.x) / sv.w, 0.0f, 1.0f);
            this->colorPopup.v = 1.0f - std::clamp((input.mouse.y - sv.y) / sv.h, 0.0f, 1.0f);
            float r = 0, g = 0, b = 0;
            ImGui::ColorConvertHSVtoRGB(this->colorPopup.h, this->colorPopup.s, this->colorPopup.v, r, g, b);
            this->applyColor(IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255));
        }
        if (this->colorPopup.dragHue && input.down)
        {
            this->colorPopup.h = std::clamp((input.mouse.x - hue.x) / hue.w, 0.0f, 1.0f);
            float r = 0, g = 0, b = 0;
            ImGui::ColorConvertHSVtoRGB(this->colorPopup.h, this->colorPopup.s, this->colorPopup.v, r, g, b);
            this->applyColor(IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255));
        }
        if (!input.down)
        {
            if (this->colorPopup.dragSv || this->colorPopup.dragHue)
            {
                // Persist token colours to disk once the drag ends.
                if (this->colorPopup.id.rfind("color:token:", 0) == 0)
                {
                    const std::string path = this->colorPopup.id.substr(12);
                    const Token* token = this->project.getToken(path);
                    if (token != nullptr)
                    {
                        std::string error;
                        this->project.setTokenValue(path, token->value, error);
                    }
                }
            }
            this->colorPopup.dragSv = false;
            this->colorPopup.dragHue = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            this->colorPopup.open = false;
        return;
    }

    if (this->menuPopup.open || this->pickerPopup.open)
    {
        PopupState& popup = this->menuPopup.open ? this->menuPopup : this->pickerPopup;
        Rect bounds = this->popupRect(popup, (int)popup.items.size());
        if (bounds.contains(input.mouse) && input.wheel != 0.0f)
        {
            const float content = popup.items.size() * 30.0f;
            popup.scroll = std::clamp(popup.scroll - input.wheel * 40.0f, 0.0f,
                std::max(0.0f, content - (bounds.h - 12.0f)));
            return;
        }
        const int hit = this->popupHit(popup, input.mouse);
        popup.hover = hit;

        if (input.pressed)
        {
            if (hit >= 0)
            {
                MenuItem item = popup.items[(size_t)hit];
                if (this->menuPopup.open && item.action.rfind("menu:", 0) == 0)
                {
                    (void)item;
                }
                if (this->menuPopup.open)
                {
                    this->closePopups();
                    this->runAction(item.action, item.value);
                }
                else
                {
                    this->choosePopup(item);
                }
                return;
            }
            if (!bounds.contains(input.mouse))
            {
                if (this->menuPopup.open)
                    this->menuPopup.open = false;
                else
                    this->pickerPopup.open = false;
                return;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            this->menuPopup.open = false;
            this->pickerPopup.open = false;
        }
    }
}

void DesignerApp::drawOverlay(ImDrawList* drawList)
{
    if (drawList == nullptr) return;

    ImGuiIO& io = ImGui::GetIO();

    this->drawSplitters(drawList);

    // Toasts
    float toastY = 58.0f;
    for (const Toast& toast : this->toasts)
    {
        const float elapsed = (float)(ImGui::GetTime() - toast.start);
        const float alpha = elapsed > toast.duration - 0.4f
            ? std::max(0.0f, (toast.duration - elapsed) / 0.4f) : 1.0f;
        const float width = std::min(420.0f, ui::textWidth(toast.text, 12.0f) + 54.0f);
        Rect rect(io.DisplaySize.x - width - 18.0f, toastY, width, 38.0f);
        ui::fill(drawList, rect, uiAlpha(this->palette->panelRaised, alpha * 0.98f), 8.0f);
        ui::stroke(drawList, rect, uiAlpha(toast.error ? this->palette->red : this->palette->border, alpha), 8.0f);
        ui::dot(drawList, rect.x + 15, rect.y + 19, 4.0f,
            uiAlpha(toast.error ? this->palette->red : this->palette->green, alpha));
        ui::textClipped(drawList, rect.x + 28, rect.y + 12, rect.w - 38, toast.text, this->palette->text, 12.0f,
            alpha);
        toastY += 46.0f;
    }

    // Menus and pickers
    if (this->menuPopup.open || this->pickerPopup.open)
    {
        PopupState& popup = this->menuPopup.open ? this->menuPopup : this->pickerPopup;
        Rect bounds = this->popupRect(popup, (int)popup.items.size());
        ui::softShadow(drawList, bounds, 10.0f, IM_COL32(0, 0, 0, 160), 12.0f, 4.0f);
        ui::fill(drawList, bounds, this->palette->panelRaised, 8.0f);
        ui::stroke(drawList, bounds, this->palette->border, 8.0f);
        drawList->PushClipRect(ImVec2(bounds.x + 1, bounds.y + 1),
            ImVec2(bounds.x + bounds.w - 1, bounds.y + bounds.h - 1), true);
        float y = bounds.y + 6.0f - popup.scroll;
        for (size_t i = 0; i < popup.items.size(); ++i)
        {
            const MenuItem& item = popup.items[i];
            const float itemHeight = item.separator ? 9.0f : 30.0f;
            if (y + itemHeight < bounds.y || y > bounds.y + bounds.h)
            {
                y += itemHeight;
                continue;
            }
            if (item.separator)
            {
                ui::fill(drawList, Rect(bounds.x + 8, y + 4, bounds.w - 16, 1), this->palette->borderSoft);
                y += 9.0f;
                continue;
            }
            Rect row(bounds.x + 4, y, bounds.w - 8, 30.0f);
            if ((int)i == popup.hover && !item.disabled)
                ui::fill(drawList, row, this->palette->hover, 5.0f);
            ImU32 color = item.disabled ? this->palette->textFaint
                : item.danger ? this->palette->red
                              : this->palette->text;
            if (!item.icon.empty())
                ui::icon(drawList, row.x + 10, row.y + 7, item.icon.c_str(), color, 16.0f);
            const float textLeft = row.x + (item.icon.empty() ? 14.0f : 34.0f);
            const float rightReserve = item.shortcut.empty() ? 12.0f : std::max(34.0f, ui::textWidth(item.shortcut, 11.0f) + 18.0f);
            ui::textClipped(drawList, textLeft, row.y + 8, row.w - (textLeft - row.x) - rightReserve, item.label, color, 12.5f);
            if (item.checked)
                ui::check(drawList, Rect(row.x + row.w - 24, row.y + 8, 14, 14), this->palette->accent, 1.7f);
            if (!item.shortcut.empty())
                ui::textRight(drawList, row.x + row.w - 10, row.y + 8, item.shortcut, this->palette->textFaint, 11.0f);
            y += 30.0f;
        }
        drawList->PopClipRect();
        if (bounds.y + 6.0f + popup.items.size() * 30.0f > bounds.y + bounds.h)
        {
            ui::scrollbar(drawList, Rect(bounds.x + bounds.w - 7, bounds.y + 4, 7, bounds.h - 8), popup.scroll,
                popup.items.size() * 30.0f, bounds.h - 12.0f, this->palette->scrollbar);
        }
    }

    // Colour picker
    if (this->colorPopup.open)
    {
        const float width = 208.0f;
        const float height = 226.0f;
        ImVec2 viewport = io.DisplaySize;
        float x = this->colorPopup.anchor.x;
        float y = this->colorPopup.anchor.y + this->colorPopup.anchor.h + 4.0f;
        if (x + width > viewport.x - 8) x = viewport.x - width - 8;
        if (y + height > viewport.y - 8) y = this->colorPopup.anchor.y - height - 4.0f;
        if (y < 8) y = 8;
        Rect rect(x, y, width, height);
        ui::softShadow(drawList, rect, 10.0f, IM_COL32(0, 0, 0, 160), 14.0f, 4.0f);
        ui::fill(drawList, rect, this->palette->panelRaised, 8.0f);
        ui::stroke(drawList, rect, this->palette->border, 8.0f);
        const Rect sv(rect.x + 12, rect.y + 12, width - 24, 120);
        const Rect hue(rect.x + 12, rect.y + 142, width - 24, 12);
        const Rect hex(rect.x + 12, rect.y + 168, width - 24, 28);
        const Rect close(rect.x + width - 30, rect.y + 8, 20, 20);

        // hue gradient
        for (int i = 0; i < 24; ++i)
        {
            float r = 0, g = 0, b = 0;
            ImGui::ColorConvertHSVtoRGB((float)i / 23.0f, 1.0f, 1.0f, r, g, b);
            Rect strip(hue.x + hue.w * i / 24.0f, hue.y, hue.w / 24.0f + 1.0f, hue.h);
            ui::fill(drawList, strip, IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255), 0);
        }
        // sv square
        float hr = 0, hg = 0, hb = 0;
        ImGui::ColorConvertHSVtoRGB(this->colorPopup.h, 1.0f, 1.0f, hr, hg, hb);
        const ImU32 hueColor = IM_COL32((int)(hr * 255), (int)(hg * 255), (int)(hb * 255), 255);
        drawList->AddRectFilledMultiColor(sv.min(), sv.max(), IM_COL32_WHITE, hueColor, hueColor, IM_COL32_WHITE);
        drawList->AddRectFilledMultiColor(sv.min(), sv.max(), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 255), IM_COL32(0, 0, 0, 255));
        ui::stroke(drawList, sv, this->palette->border, 2.0f);
        const float handleX = sv.x + sv.w * this->colorPopup.s;
        const float handleY = sv.y + sv.h * (1.0f - this->colorPopup.v);
        drawList->AddCircle(ImVec2(handleX, handleY), 5.0f, IM_COL32_WHITE, 0, 1.6f);
        ui::fill(drawList, Rect(hue.x + hue.w * this->colorPopup.h - 1.5f, hue.y - 2, 3, hue.h + 4),
            IM_COL32_WHITE, 1.5f);
        ui::fill(drawList, hex, this->palette->inputBg, 5.0f);
        ui::stroke(drawList, hex, this->palette->border, 5.0f);
        float currentR = 0, currentG = 0, currentB = 0;
        ImGui::ColorConvertHSVtoRGB(this->colorPopup.h, this->colorPopup.s, this->colorPopup.v, currentR,
            currentG, currentB);
        const std::string currentHex = colorToHex(IM_COL32((int)(currentR * 255), (int)(currentG * 255),
            (int)(currentB * 255), 255));
        ui::textVC(drawList, hex.x + 8, hex.y, hex.h,
            this->edit.active && this->edit.key == "colorhex" ? this->edit.buffer : currentHex,
            this->palette->text, 12.0f);
        if (this->edit.active && this->edit.key == "colorhex")
        {
            const float caretX = hex.x + 8 + ui::textWidth(this->edit.buffer.substr(0, (size_t)this->edit.caret), 12.0f);
            ui::fill(drawList, Rect(caretX, hex.y + 6, 1, hex.h - 12), this->palette->accent);
        }
        ui::iconCentered(drawList, close, "close", this->palette->textMuted, 14.0f);
    }

    // Dialog
    if (this->dialog.open)
    {
        const float width = 460.0f;
        const float height = this->dialog.alertOnly ? 190.0f : (this->dialog.message.empty() ? 260.0f : 280.0f);
        const Rect rect((io.DisplaySize.x - width) * 0.5f, (io.DisplaySize.y - height) * 0.5f, width, height);
        ui::fill(drawList, Rect(0, 0, io.DisplaySize.x, io.DisplaySize.y), IM_COL32(0, 0, 0, 110));
        ui::softShadow(drawList, rect, 12.0f, IM_COL32(0, 0, 0, 190), 20.0f, 8.0f);
        ui::fill(drawList, rect, this->palette->panelRaised, 12.0f);
        ui::stroke(drawList, rect, this->palette->border, 12.0f);
        ui::text(drawList, rect.x + 24, rect.y + 22, this->dialog.title, this->palette->text, 16.0f);
        ui::fill(drawList, Rect(rect.x + 24, rect.y + 50, rect.w - 48, 1), this->palette->borderSoft);

        float y = rect.y + 76.0f;
        if (!this->dialog.message.empty())
        {
            float textY = y;
            std::stringstream stream(this->dialog.message);
            std::string line;
            while (std::getline(stream, line))
            {
                ui::text(drawList, rect.x + 24, textY, line, this->palette->textMuted, 12.5f);
                textY += 20.0f;
            }
            y = textY + 10.0f;
        }

        if (!this->dialog.label1.empty())
        {
            ui::text(drawList, rect.x + 24, y, this->dialog.label1, this->palette->textMuted, 11.5f);
            y += 20.0f;
            Rect field1(rect.x + 24, y, rect.w - 48, 32);
            if (this->edit.active && this->edit.key == "dialog:1") this->edit.rect = field1;
            ui::fill(drawList, field1, this->palette->inputBg, 6.0f);
            ui::stroke(drawList, field1,
                this->edit.active && this->edit.key == "dialog:1" ? this->palette->accent : this->palette->border, 6.0f);
            const std::string shown = this->edit.active && this->edit.key == "dialog:1" ? this->edit.buffer : this->dialog.input1;
            ui::textClipped(drawList, field1.x + 9, field1.y + 9, field1.w - 18, shown, this->palette->text, 12.5f);
            if (this->edit.active && this->edit.key == "dialog:1")
            {
                const float caretX = field1.x + 9 +
                    ui::textWidth(this->edit.buffer.substr(0, (size_t)this->edit.caret), 12.5f);
                ui::fill(drawList, Rect(caretX, field1.y + 7, 1, field1.h - 14), this->palette->accent);
            }
            y += 44.0f;
        }
        if (!this->dialog.label2.empty())
        {
            ui::text(drawList, rect.x + 24, y, this->dialog.label2, this->palette->textMuted, 11.5f);
            y += 20.0f;
            Rect field2(rect.x + 24, y, rect.w - 48, 32);
            if (this->edit.active && this->edit.key == "dialog:2") this->edit.rect = field2;
            ui::fill(drawList, field2, this->palette->inputBg, 6.0f);
            ui::stroke(drawList, field2,
                this->edit.active && this->edit.key == "dialog:2" ? this->palette->accent : this->palette->border, 6.0f);
            const std::string shown = this->edit.active && this->edit.key == "dialog:2" ? this->edit.buffer : this->dialog.input2;
            ui::textClipped(drawList, field2.x + 9, field2.y + 9, field2.w - 18, shown, this->palette->text, 12.5f);
            if (this->edit.active && this->edit.key == "dialog:2")
            {
                const float caretX = field2.x + 9 +
                    ui::textWidth(this->edit.buffer.substr(0, (size_t)this->edit.caret), 12.5f);
                ui::fill(drawList, Rect(caretX, field2.y + 7, 1, field2.h - 14), this->palette->accent);
            }
            y += 42.0f;
        }

        if (!this->dialog.choices.empty())
        {
            for (size_t i = 0; i < this->dialog.choices.size(); ++i)
            {
                Rect choice(rect.x + 24, y, rect.w - 48, 24);
                if ((int)i == this->dialog.selectedChoice)
                    ui::fill(drawList, choice, this->palette->selected, 4.0f);
                ui::textVC(drawList, choice.x + 8, choice.y, choice.h, this->dialog.choices[i],
                    (int)i == this->dialog.selectedChoice ? this->palette->text : this->palette->textMuted, 12.0f);
                y += 26.0f;
            }
        }

        if (!this->dialog.error.empty())
            ui::text(drawList, rect.x + 24, rect.y + rect.h - 78, this->dialog.error, this->palette->red, 11.0f);

        const Rect confirmRect(rect.x + rect.w - 196.0f, rect.y + rect.h - 54.0f, 86.0f, 32.0f);
        const Rect cancelRect(rect.x + rect.w - 100.0f, rect.y + rect.h - 54.0f, 86.0f, 32.0f);
        ui::fill(drawList, confirmRect, this->dialog.danger ? this->palette->red : this->palette->accent, 6.0f);
        ui::label(drawList, confirmRect, this->dialog.confirmText, IM_COL32_WHITE, 12.5f, 1);
        if (!this->dialog.alertOnly)
        {
            ui::fill(drawList, cancelRect, this->palette->hover, 6.0f);
            ui::stroke(drawList, cancelRect, this->palette->border, 6.0f);
            ui::label(drawList, cancelRect, this->dialog.cancelText, this->palette->textMuted, 12.5f, 1);
        }
    }

    // Drag ghost for variables
    if (this->drag.active)
    {
        const ImVec2 mouse = io.MousePos;
        const std::string label = this->drag.tokenPath;
        const float width = ui::textWidth(label, 12.0f) + 34.0f;
        Rect ghost(mouse.x + 12, mouse.y + 10, width, 30);
        ui::fill(drawList, ghost, uiAlpha(this->palette->panelRaised, 0.97f), 7.0f);
        ui::stroke(drawList, ghost, this->palette->accent, 7.0f);
        ui::icon(drawList, ghost.x + 8, ghost.y + 7, "drag_indicator", this->palette->accent, 16.0f);
        ui::textVC(drawList, ghost.x + 28, ghost.y, ghost.h, label, this->palette->text, 12.0f);
    }

    // Tooltips last
    this->drawTooltip(drawList);
}

} // namespace designer
