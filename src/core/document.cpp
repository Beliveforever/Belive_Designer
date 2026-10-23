#include "core/document.hpp"

#include <algorithm>
#include <fstream>

namespace designer
{

namespace
{

std::string titleFromRelative(const std::string& relative)
{
    const size_t slash = relative.rfind('/');
    return slash == std::string::npos ? relative : relative.substr(slash + 1);
}

} // namespace

bool Document::load(const Project& project, const std::string& relativePath)
{
    this->projectRoot = project.root;
    this->path = project.root / relativePath;
    this->relative = relativePath;
    this->title = titleFromRelative(relativePath);
    this->loadError.clear();
    this->root = parseJsonFile(this->path, this->loadError);
    if (!this->root.is_object())
    {
        if (this->loadError.empty())
            this->loadError = "文档根节点必须是对象";
        this->root = Json::object();
        return false;
    }
    this->dirty = false;
    this->clearHistory();
    this->rebuild();
    return true;
}

bool Document::reload(const Project& project)
{
    Json previous = this->root;
    const bool ok = this->load(project, this->relative);
    if (!ok)
        this->root = previous;
    return ok;
}

bool Document::save(const Project& project, std::string& error)
{
    if (!this->root.is_object())
    {
        error = "文档根节点不是对象，拒绝保存";
        return false;
    }
    if (!writeJsonFile(this->path, this->root, error, true))
        return false;
    this->dirty = false;
    return true;
}

bool Document::saveAs(const Project& project, const std::string& relativePath, std::string& error)
{
    const fs::path target = project.root / relativePath;
    if (!writeJsonFile(target, this->root, error, true))
        return false;
    this->path = target;
    this->relative = relativePath;
    this->title = titleFromRelative(relativePath);
    this->dirty = false;
    return true;
}

void Document::rebuild()
{
    const bool appearance = !this->root.contains("type") &&
        (this->root.contains("layers") || this->root.contains("box") ||
            this->root.contains("import"));
    if (appearance)
        buildAppearanceTree(this->root, this->tree, this->projectRoot, this->relative);
    else
        buildNodeTree(this->root, this->tree);
    if (!this->title.empty()) this->tree.name = this->title;
}

Json* Document::node(const std::string& pointer)
{
    return jsonAt(this->root, pointer);
}

const Json* Document::node(const std::string& pointer) const
{
    return jsonAt(this->root, pointer);
}

Json* Document::valueAt(const std::string& pointer)
{
    return jsonAt(this->root, pointer);
}

const Json* Document::valueAt(const std::string& pointer) const
{
    return jsonAt(this->root, pointer);
}

bool Document::setValueAt(const std::string& pointer, const Json& value, bool recordHistory)
{
    if (pointer.empty() || pointer[0] != '/') return false;

    std::vector<std::string> segments;
    std::string current;
    for (size_t i = 1; i <= pointer.size(); ++i)
    {
        if (i == pointer.size() || pointer[i] == '/')
        {
            std::string segment = current;
            std::string unescaped;
            for (size_t j = 0; j < segment.size(); ++j)
            {
                if (segment[j] == '~' && j + 1 < segment.size())
                {
                    if (segment[j + 1] == '0') { unescaped.push_back('~'); ++j; continue; }
                    if (segment[j + 1] == '1') { unescaped.push_back('/'); ++j; continue; }
                }
                unescaped.push_back(segment[j]);
            }
            segments.push_back(unescaped);
            current.clear();
        }
        else
            current.push_back(pointer[i]);
    }
    if (segments.empty()) return false;

    Json* node = &this->root;
    for (size_t i = 0; i < segments.size(); ++i)
    {
        const bool last = i + 1 == segments.size();
        const std::string& key = segments[i];
        if (node->is_object())
        {
            if (!node->contains(key)) (*node)[key] = Json::object();
            node = &(*node)[key];
        }
        else if (node->is_array())
        {
            size_t index = 0;
            try { index = (size_t)std::stoul(key); } catch (const std::exception&) { return false; }
            while (node->size() <= index) node->push_back(Json::object());
            node = &(*node)[index];
        }
        else
            return false;
        if (!last && !node->is_structured()) *node = Json::object();
    }

    if (recordHistory) this->pushUndo();
    *node = value;
    this->dirty = true;
    if (!this->root.contains("type"))
        this->rebuild();
    return true;
}

Json* Document::style(const std::string& pointer, bool create)
{
    Json* value = this->node(pointer);
    if (value == nullptr || !value->is_object())
        return nullptr;
    (void)create;
    return value;
}

const Json* Document::style(const std::string& pointer) const
{
    const Json* value = this->node(pointer);
    return value != nullptr && value->is_object() ? value : nullptr;
}

Json* Document::vars(const std::string& pointer, bool create)
{
    (void)pointer;
    if (!this->root.is_object())
        return nullptr;
    if (!this->root.contains("vars") || !this->root["vars"].is_object())
    {
        if (!create) return nullptr;
        this->root["vars"] = Json::object();
    }
    return &this->root["vars"];
}

const Json* Document::vars(const std::string& pointer) const
{
    (void)pointer;
    if (!this->root.is_object() || !this->root.contains("vars") || !this->root["vars"].is_object())
        return nullptr;
    return &this->root["vars"];
}

void Document::markDirty(const std::string&)
{
    this->dirty = true;
}

void Document::pushUndo()
{
    this->undoStack.push_back(this->root);
    if (this->undoStack.size() > 120)
        this->undoStack.pop_front();
    this->redoStack.clear();
}

bool Document::undo()
{
    if (this->undoStack.empty())
        return false;
    this->redoStack.push_back(this->root);
    this->root = this->undoStack.back();
    this->undoStack.pop_back();
    this->dirty = true;
    this->rebuild();
    return true;
}

bool Document::redo()
{
    if (this->redoStack.empty())
        return false;
    this->undoStack.push_back(this->root);
    this->root = this->redoStack.back();
    this->redoStack.pop_back();
    this->dirty = true;
    this->rebuild();
    return true;
}

void Document::clearHistory()
{
    this->undoStack.clear();
    this->redoStack.clear();
}

bool Document::insertNode(const std::string& parentPtr, int index, const Json& nodeValue)
{
    Json* parent = this->node(parentPtr);
    if (parent == nullptr || !parent->is_object())
        return false;
    this->pushUndo();
    if (!parent->contains("children") || !(*parent)["children"].is_array())
        (*parent)["children"] = Json::array();
    Json& children = (*parent)["children"];
    if (index < 0 || index > (int)children.size())
        index = (int)children.size();
    children.insert(children.begin() + index, nodeValue);
    this->dirty = true;
    this->rebuild();
    return true;
}

bool Document::deleteNode(const std::string& pointer)
{
    if (pointer.empty())
        return false;
    const std::string parentPtr = parentPointer(pointer);
    const int index = childIndex(pointer);
    if (index < 0)
        return false;
    Json* parent = this->node(parentPtr);
    if (parent == nullptr || !parent->is_object() || !parent->contains("children"))
        return false;
    Json& children = (*parent)["children"];
    if (!children.is_array() || index >= (int)children.size())
        return false;
    this->pushUndo();
    children.erase(children.begin() + index);
    this->dirty = true;
    this->rebuild();
    return true;
}

bool Document::duplicateNode(const std::string& pointer, std::string* newPointer)
{
    const Json* source = this->node(pointer);
    if (source == nullptr || pointer.empty())
        return false;

    Json copy = *source;
    const std::string parentPtr = parentPointer(pointer);
    const int index = childIndex(pointer);
    if (!this->insertNode(parentPtr, index + 1, copy))
        return false;
    if (newPointer != nullptr)
        *newPointer = parentPtr + "/children/" + std::to_string(index + 1);
    return true;
}

bool Document::moveNode(const std::string& pointer, const std::string& newParentPtr, int index)
{
    if (pointer.empty() || isDescendantPath(pointer, newParentPtr))
        return false;
    Json* source = this->node(pointer);
    Json* target = this->node(newParentPtr);
    if (source == nullptr || target == nullptr || !target->is_object())
        return false;

    const std::string oldParentPtr = parentPointer(pointer);
    const int oldIndex = childIndex(pointer);
    if (oldIndex < 0)
        return false;

    this->pushUndo();
    Json copy = *source;

    Json* oldParent = this->node(oldParentPtr);
    if (oldParent == nullptr || !oldParent->contains("children"))
        return false;
    auto& oldChildren = (*oldParent)["children"];
    if (!oldChildren.is_array() || oldIndex >= (int)oldChildren.size())
        return false;
    oldChildren.erase(oldChildren.begin() + oldIndex);

    Json* parent = this->node(newParentPtr);
    if (parent == nullptr || !parent->is_object())
        return false;
    if (!parent->contains("children") || !(*parent)["children"].is_array())
        (*parent)["children"] = Json::array();
    Json& children = (*parent)["children"];

    if (newParentPtr == oldParentPtr && oldIndex < index)
        index--;
    if (index < 0) index = 0;
    if (index > (int)children.size()) index = (int)children.size();
    children.insert(children.begin() + index, copy);
    this->dirty = true;
    this->rebuild();
    return true;
}

bool Document::setStyleValue(const std::string& pointer, const std::string& key, const Json& value)
{
    if (key == "type" || key == "children" || key == "vars" || key == "include" || key == "bind" ||
        key == "bindTo")
        return false;
    Json* style = this->style(pointer, true);
    if (style == nullptr)
        return false;
    this->pushUndo();
    (*style)[key] = value;
    this->dirty = true;
    return true;
}

bool Document::eraseStyleValue(const std::string& pointer, const std::string& key)
{
    Json* style = this->style(pointer, false);
    if (style == nullptr || !style->contains(key))
        return false;
    this->pushUndo();
    style->erase(key);
    this->dirty = true;
    return true;
}

bool Document::setLocalVar(const std::string& pointer, const std::string& name, const Json& value)
{
    (void)pointer;
    if (!this->root.is_object() || name.empty())
        return false;
    const bool color = value.is_string();
    if (!color && !value.is_number())
        return false;
    this->pushUndo();
    if (!this->root["vars"].is_object()) this->root["vars"] = Json::object();
    Json& vars = this->root["vars"];
    const char* group = color ? "colors" : "numbers";
    if (!vars.contains(group) || !vars[group].is_object()) vars[group] = Json::object();
    vars[group][name] = value;
    this->dirty = true;
    return true;
}

bool Document::eraseLocalVar(const std::string& pointer, const std::string& name)
{
    (void)pointer;
    Json* vars = this->vars(std::string());
    if (vars == nullptr) return false;
    bool changed = false;
    for (const char* group : { "colors", "numbers" })
    {
        if (!vars->contains(group) || !(*vars)[group].is_object()) continue;
        if ((*vars)[group].contains(name))
        {
            if (!changed) this->pushUndo();
            (*vars)[group].erase(name);
            changed = true;
        }
    }
    if (changed) this->dirty = true;
    return changed;
}

bool Document::renameLocalVar(const std::string& block, const std::string& oldName,
    const std::string& newName)
{
    if (oldName.empty() || newName.empty() || oldName == newName) return false;
    Json* vars = this->vars(std::string());
    if (vars == nullptr) return false;
    const char* group = block == "color" ? "colors" : "numbers";
    if (!vars->contains(group) || !(*vars)[group].is_object() || !(*vars)[group].contains(oldName))
        return false;
    if ((*vars)[group].contains(newName)) return false;
    this->pushUndo();
    Json& values = (*vars)[group];
    values[newName] = values[oldName];
    values.erase(oldName);
    renameRefsInJson(this->root, "&" + block + "." + oldName, "&" + block + "." + newName);
    this->dirty = true;
    this->rebuild();
    return true;
}

bool Document::reorderNode(const std::string& pointer, int direction)
{
    const std::string parentPtr = parentPointer(pointer);
    const int index = childIndex(pointer);
    if (index < 0)
        return false;
    const int target = index + direction;
    Json* parent = this->node(parentPtr);
    if (parent == nullptr || !parent->contains("children"))
        return false;
    Json& children = (*parent)["children"];
    if (target < 0 || target >= (int)children.size())
        return false;
    this->pushUndo();
    std::swap(children[index], children[target]);
    this->dirty = true;
    this->rebuild();
    return true;
}

std::string uniqueId(const Json& root, const std::string& base)
{
    (void)root;
    return base;
}

Json makeDocumentRoot(const std::string& type, const std::string& title)
{
    (void)title;
    Json root;
    root["type"] = type.empty() ? "Box" : type;
    root["width"] = 1280;
    root["height"] = 720;
    root["axis"] = "column";
    root["gap"] = 0;
    root["padding"] = 0;
    root["backgroundColor"] = "#0F172A";
    root["children"] = Json::array();
    return root;
}

} // namespace designer
