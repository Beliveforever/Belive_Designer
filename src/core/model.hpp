#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <nlohmann/json.hpp>

namespace designer
{

using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;

enum class TokenType
{
    COLOR,
    NUMBER,
    STRING,
    BOOLEAN,
};

const char* tokenTypeName(TokenType type);

struct Token
{
    std::string path;      // dotted, e.g. color.primary
    TokenType type = TokenType::STRING;
    Json value;
    std::string file;      // path relative to project root
    std::vector<std::string> segments;
};

struct Project
{
    fs::path root;
    std::string title;
    std::string author;
    std::string description;
    Json meta;
    int canvasWidth = 1280;
    int canvasHeight = 720;
    std::string entry;
    std::vector<Token> tokens;
    std::map<std::string, Json> tokenFiles;   // relative path -> parsed content
    std::vector<std::string> tokenFileOrder;  // stable order

    int findToken(const std::string& path) const;
    const Token* getToken(const std::string& path) const;
    bool setTokenValue(const std::string& path, const Json& value, std::string& error);
    bool tokenValue(const std::string& path, Json& out) const;
};

// Parse a color string ("#RRGGBB" / "#RRGGBBAA") to ImU32 (ABGR for ImGui).
// Returns false when the string is not a valid color.
bool parseColor(const std::string& text, ImU32& out);
std::string colorToHex(ImU32 color);
ImU32 colorWithAlpha(ImU32 color, float alpha);

bool loadProject(const fs::path& root, Project& project, std::string& error);
bool reloadTokens(Project& project, std::string& error);
bool saveTokenFile(Project& project, const std::string& relativeFile, std::string& error);

Json parseJsonFile(const fs::path& path, std::string& error);
bool writeJsonFile(const fs::path& path, const Json& value, std::string& error, bool backup);
std::string prettyJson(const Json& value);

// ---- value references -------------------------------------------------
//
// A bklv view value may be:
//   literal                       12 / "#3B82F6" / true / "text"
//   "@theme/color.x.y"            global color token
//   "@style/metric.x.y"           global number token
//   "&color.name" / "&number.name" local vars of the current document
struct Resolved
{
    bool ok = false;
    Json value;
    bool isRef = false;
    bool isLocal = false;
    std::string ref;      // @theme/color.x, @style/metric.x, &color.x, &number.x
    std::string error;
};

Resolved resolveValue(const Json& root, const std::string& nodePtr, const Json& value, const Project& project,
    int depth = 0);

// bklv local vars live in the document root: vars.colors / vars.numbers.
const Json* findLocalVar(const Json& root, const std::string& block, const std::string& name);

// Collect @theme/ and @style/ references from a view JSON.
std::vector<std::string> collectGlobalRefs(const Json& node);

// Count exact reference strings (e.g. "@theme/color.x", "&color.x") in a document.
int countRefsInJson(const Json& value, const std::string& reference);

// Rewrite exact reference strings in a document.
int renameRefsInJson(Json& value, const std::string& oldRef, const std::string& newRef);

// ---- node tree ---------------------------------------------------------
struct Node
{
    std::string pointer;
    std::string type;
    std::string id;
    std::string name;
    std::string sourceFile;    // imported appearance file (relative to theme root)
    std::string sourcePointer; // pointer inside sourceFile
    int depth = 0;
    bool hasChildren = false;
    Node* parent = nullptr;
    std::vector<std::unique_ptr<Node>> children;
};

void buildNodeTree(const Json& root, Node& out);
void buildAppearanceTree(const Json& root, Node& out, const fs::path& themeRoot = fs::path(),
    const std::string& relative = std::string());
Node* findNode(Node& root, const std::string& pointer);
const Node* findNode(const Node& root, const std::string& pointer);
Json* jsonAt(Json& root, const std::string& pointer);
const Json* jsonAt(const Json& root, const std::string& pointer);

// Build a new flat bklv view node (attributes at the top level, no style/id/name).
Json makeNode(const std::string& type);

// True when childPointer is inside parentPointer (or equal).
bool isDescendantPath(const std::string& parentPointer, const std::string& childPointer);

// Parent pointer of a node pointer ("" for the root) and index in parent's children array.
std::string parentPointer(const std::string& pointer);
int childIndex(const std::string& pointer);

// ---- project files -----------------------------------------------------
struct FileEntry
{
    std::string relative;
    std::string name;
    bool isDirectory = false;
    int depth = 0;
};

void scanProjectFiles(const fs::path& root, std::vector<FileEntry>& out);

} // namespace designer
