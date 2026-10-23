#pragma once

#include <deque>
#include <string>
#include <vector>

#include "core/model.hpp"

namespace designer
{

struct Document
{
    fs::path path;
    fs::path projectRoot;
    std::string relative; // relative to project root
    std::string title;
    Json root = Json::object();
    Node tree;
    bool dirty = false;
    std::string loadError;

    std::deque<Json> undoStack;
    std::deque<Json> redoStack;

    bool load(const Project& project, const std::string& relativePath);
    bool reload(const Project& project);
    bool save(const Project& project, std::string& error);
    bool saveAs(const Project& project, const std::string& relativePath, std::string& error);

    void rebuild();
    Json* node(const std::string& pointer);
    const Json* node(const std::string& pointer) const;
    Json* style(const std::string& pointer, bool create = false);
    const Json* style(const std::string& pointer) const;
    Json* valueAt(const std::string& pointer);
    const Json* valueAt(const std::string& pointer) const;
    bool setValueAt(const std::string& pointer, const Json& value, bool recordHistory = true);
    Json* vars(const std::string& pointer, bool create = false);
    const Json* vars(const std::string& pointer) const;

    // undo/redo
    void markDirty(const std::string& beforeJson = std::string());
    void pushUndo();
    bool undo();
    bool redo();
    void clearHistory();

    // node operations (all record undo and rebuild the tree)
    bool insertNode(const std::string& parentPointer, int index, const Json& node);
    bool deleteNode(const std::string& pointer);
    bool duplicateNode(const std::string& pointer, std::string* newPointer = nullptr);
    bool moveNode(const std::string& pointer, const std::string& newParentPointer, int index);
    bool setStyleValue(const std::string& pointer, const std::string& key, const Json& value);
    bool eraseStyleValue(const std::string& pointer, const std::string& key);
    bool setLocalVar(const std::string& pointer, const std::string& name, const Json& value);
    bool eraseLocalVar(const std::string& pointer, const std::string& name);
    bool renameLocalVar(const std::string& block, const std::string& oldName, const std::string& newName);
    bool reorderNode(const std::string& pointer, int direction);
};

std::string uniqueId(const Json& root, const std::string& base);
Json makeDocumentRoot(const std::string& type, const std::string& title);

} // namespace designer
