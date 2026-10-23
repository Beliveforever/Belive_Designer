#include "core/model.hpp"

#include "bklv/core/theme_data.hpp"
#include "bklv/core/ui_scale.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>

#include <imgui.h>

namespace designer
{

const char* tokenTypeName(TokenType type)
{
    switch (type)
    {
        case TokenType::COLOR: return "颜色";
        case TokenType::NUMBER: return "数值";
        case TokenType::STRING: return "字符串";
        case TokenType::BOOLEAN: return "布尔";
    }
    return "字符串";
}

namespace
{

TokenType typeOfValue(const Json& value)
{
    if (value.is_number()) return TokenType::NUMBER;
    if (value.is_boolean()) return TokenType::BOOLEAN;
    if (value.is_string())
    {
        const std::string text = value.get<std::string>();
        if (!text.empty() && text[0] == '#') return TokenType::COLOR;
        if (text.rfind("@theme/color.", 0) == 0) return TokenType::COLOR;
        if (text.rfind("@style/metric.", 0) == 0) return TokenType::NUMBER;
        return TokenType::STRING;
    }
    return TokenType::STRING;
}

void flattenTokens(const Json& value, const std::string& prefix, const std::string& file,
    std::vector<Token>& out)
{
    if (value.is_object())
    {
        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (!it.key().empty() && it.key()[0] == '$')
                continue;
            flattenTokens(it.value(), prefix.empty() ? it.key() : prefix + "." + it.key(), file, out);
        }
        return;
    }

    Token token;
    token.path = prefix;
    token.type = typeOfValue(value);
    token.value = value;
    token.file = file;
    std::string current;
    for (char ch : prefix)
    {
        if (ch == '.')
        {
            if (!current.empty()) token.segments.push_back(current);
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    if (!current.empty()) token.segments.push_back(current);
    out.push_back(token);
}

Json* navigate(Json& value, const std::vector<std::string>& segments)
{
    Json* current = &value;
    for (const std::string& segment : segments)
    {
        if (!current->is_object())
            return nullptr;
        if (!current->contains(segment))
            return nullptr;
        current = &(*current)[segment];
    }
    return current;
}

} // namespace

int Project::findToken(const std::string& path) const
{
    for (size_t i = 0; i < this->tokens.size(); ++i)
        if (this->tokens[i].path == path)
            return (int)i;
    return -1;
}

const Token* Project::getToken(const std::string& path) const
{
    const int index = this->findToken(path);
    return index >= 0 ? &this->tokens[(size_t)index] : nullptr;
}

bool Project::tokenValue(const std::string& path, Json& out) const
{
    const Token* token = this->getToken(path);
    if (token == nullptr)
        return false;
    out = token->value;
    return true;
}

bool Project::setTokenValue(const std::string& path, const Json& value, std::string& error)
{
    const int index = this->findToken(path);
    if (index < 0)
    {
        error = "变量不存在：" + path;
        return false;
    }

    Token& token = this->tokens[(size_t)index];
    auto fileIt = this->tokenFiles.find(token.file);
    if (fileIt == this->tokenFiles.end())
    {
        error = "变量来源文件未加载：" + token.file;
        return false;
    }

    std::vector<std::string> fileSegments = token.segments;
    if (!fileSegments.empty()) fileSegments.erase(fileSegments.begin());
    Json* target = navigate(fileIt->second, fileSegments);
    if (target == nullptr)
    {
        error = "变量在源文件中不存在：" + path;
        return false;
    }

    *target = value;
    token.value = value;
    token.type = typeOfValue(value);
    if (!saveTokenFile(*this, token.file, error))
        return false;
    const std::string themeName = this->meta.value("name", std::string("project"));
    bklv::loadThemeIntoGlobalFromDirectory(this->root.string(), themeName);
    bklv::setUiScale(1.0f);
    return true;
}

bool parseColor(const std::string& text, ImU32& out)
{
    std::string hex;
    for (char ch : text)
        if (!std::isspace((unsigned char)ch)) hex.push_back(ch);

    if (hex.empty() || hex[0] != '#')
        return false;
    hex.erase(hex.begin());

    auto digit = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };

    if (hex.size() != 6 && hex.size() != 8)
        return false;

    int values[8];
    for (size_t i = 0; i < hex.size(); ++i)
    {
        values[i] = digit(hex[i]);
        if (values[i] < 0)
            return false;
    }

    const int r = values[0] * 16 + values[1];
    const int g = values[2] * 16 + values[3];
    const int b = values[4] * 16 + values[5];
    const int a = hex.size() == 8 ? values[6] * 16 + values[7] : 255;
    out = IM_COL32(r, g, b, a);
    return true;
}

std::string colorToHex(ImU32 color)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
        (unsigned)(color & 0xFF), (unsigned)((color >> 8) & 0xFF), (unsigned)((color >> 16) & 0xFF));
    return buffer;
}

ImU32 colorWithAlpha(ImU32 color, float alpha)
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    const unsigned a = (unsigned)((color >> 24) & 0xFF);
    const unsigned na = (unsigned)((float)a * alpha);
    return (color & 0x00FFFFFFu) | (na << 24);
}

std::string prettyJson(const Json& value)
{
    return value.dump(2) + "\n";
}

Json parseJsonFile(const fs::path& path, std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "无法打开文件：" + path.string();
        return Json();
    }
    std::stringstream buffer;
    buffer << stream.rdbuf();
    try
    {
        return Json::parse(buffer.str());
    }
    catch (const std::exception& exception)
    {
        error = std::string("JSON 解析失败：") + exception.what();
        return Json();
    }
}

bool writeJsonFile(const fs::path& path, const Json& value, std::string& error, bool backup)
{
    std::error_code code;
    if (path.has_parent_path())
        fs::create_directories(path.parent_path(), code);

    if (backup && fs::exists(path, code))
    {
        fs::path backupPath = path;
        backupPath += ".bak";
        fs::copy_file(path, backupPath, fs::copy_options::overwrite_existing, code);
        if (code)
        {
            error = "备份失败：" + code.message();
            return false;
        }
    }

    const fs::path temp = path.string() + ".tmp";
    {
        std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "无法写入临时文件：" + temp.string();
            return false;
        }
        stream << prettyJson(value);
        stream.flush();
        if (!stream)
        {
            error = "写入失败：" + temp.string();
            return false;
        }
    }

    fs::rename(temp, path, code);
    if (code)
    {
        fs::remove(path, code);
        code.clear();
        fs::rename(temp, path, code);
        if (code)
        {
            error = "保存失败：" + code.message();
            fs::remove(temp, code);
            return false;
        }
    }
    return true;
}

bool loadProject(const fs::path& root, Project& project, std::string& error)
{
    project = Project();
    project.root = fs::absolute(root);

    std::error_code code;
    if (!fs::exists(project.root / "theme.json", code))
    {
        error = "不是 bklv 工程：缺少 theme.json（请选择主题工程目录）";
        return false;
    }

    std::string parseError;
    Json meta = parseJsonFile(project.root / "theme.json", parseError);
    if (!meta.is_object())
    {
        error = "theme.json 读取失败：" + parseError;
        return false;
    }
    project.meta = meta;
    project.title = meta.value("name", project.root.filename().string());
    project.author = meta.value("author", std::string());
    project.description = meta.value("description", std::string());
    project.canvasWidth = 1280;
    project.canvasHeight = 720;

    // Theme token files: prefer theme.json's files[] manifest, then the standard paths.
    std::vector<std::string> tokenFiles;
    if (meta.contains("files") && meta["files"].is_array())
    {
        for (const Json& entry : meta["files"])
        {
            if (!entry.is_string()) continue;
            const std::string relative = entry.get<std::string>();
            if (relative == "tokens/colors.json" || relative == "colors.json" ||
                relative == "tokens/metrics.json" || relative == "metrics.json")
                tokenFiles.push_back(relative);
        }
    }
    for (const char* fallback : { "tokens/colors.json", "tokens/metrics.json", "colors.json", "metrics.json" })
    {
        if (!fs::exists(project.root / fallback, code)) continue;
        if (std::find(tokenFiles.begin(), tokenFiles.end(), fallback) == tokenFiles.end())
            tokenFiles.push_back(fallback);
    }
    for (const std::string& relative : tokenFiles)
    {
        Json content = parseJsonFile(project.root / relative, parseError);
        if (!content.is_object())
        {
            error = "变量文件读取失败：" + relative + "（" + parseError + "）";
            return false;
        }
        project.tokenFiles[relative] = content;
        project.tokenFileOrder.push_back(relative);
        const bool isColors = relative.find("colors.json") != std::string::npos;
        flattenTokens(content, isColors ? "color" : "metric", relative, project.tokens);
    }

    // First view file, preferring pages/home.json, then pages/, view/, components/.
    auto firstJson = [&](const std::string& directory) -> std::string {
        struct Candidate { std::string file; int score = 0; };
        std::vector<Candidate> files;
        const fs::path dir = project.root / directory;
        if (!fs::exists(dir, code)) return std::string();
        for (const fs::directory_entry& entry : fs::directory_iterator(dir, code))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const std::string relative = directory + "/" + entry.path().filename().string();
            const std::string name = entry.path().filename().string();
            Candidate candidate;
            candidate.file = relative;
            if (name == "home.json") candidate.score += 30;
            else if (name.find("page") != std::string::npos) candidate.score += 10;
            if (name.find("children_ref") != std::string::npos) candidate.score -= 12;
            if (name.find("include") != std::string::npos) candidate.score -= 8;
            if (name.find("list_item") != std::string::npos) candidate.score -= 4;
            std::string parseError;
            Json doc = parseJsonFile(entry.path(), parseError);
            if (!doc.is_object()) continue;
            const std::string type = doc.value("type", std::string());
            if (type == "Box") candidate.score += 3;
            if (type == "Padding") candidate.score -= 10;
            if (type.empty()) candidate.score -= 20;
            if (doc.contains("children") && doc["children"].is_array())
                candidate.score += 4 + std::min<int>(8, (int)doc["children"].size());
            files.push_back(candidate);
        }
        std::sort(files.begin(), files.end(), [](const Candidate& a, const Candidate& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.file < b.file;
        });
        return files.empty() ? std::string() : files.front().file;
    };
    project.entry = firstJson("pages");
    if (project.entry.empty()) project.entry = firstJson("view");
    if (project.entry.empty()) project.entry = firstJson("components");

    // Load the project theme into bklv so @theme/ and @style/ resolve in the canvas.
    const std::string themeName = meta.value("name", std::string("project"));
    bklv::loadThemeIntoGlobalFromDirectory(project.root.string(), themeName);
    bklv::setUiScale(1.0f);
    return true;
}

bool reloadTokens(Project& project, std::string& error)
{
    project.tokens.clear();
    for (const std::string& relative : project.tokenFileOrder)
    {
        std::string parseError;
        Json content = parseJsonFile(project.root / relative, parseError);
        if (!content.is_object())
        {
            error = "变量文件读取失败：" + relative + "（" + parseError + "）";
            return false;
        }
        project.tokenFiles[relative] = content;
        const bool isColors = relative.find("colors.json") != std::string::npos;
        flattenTokens(content, isColors ? "color" : "metric", relative, project.tokens);
    }
    const std::string themeName = project.meta.value("name", std::string("project"));
    bklv::loadThemeIntoGlobalFromDirectory(project.root.string(), themeName);
    bklv::setUiScale(1.0f);
    return true;
}

bool saveTokenFile(Project& project, const std::string& relativeFile, std::string& error)
{
    auto it = project.tokenFiles.find(relativeFile);
    if (it == project.tokenFiles.end())
    {
        error = "变量文件未加载：" + relativeFile;
        return false;
    }
    return writeJsonFile(project.root / relativeFile, it->second, error, true);
}

const Json* findLocalVar(const Json& root, const std::string& block, const std::string& name)
{
    if (!root.is_object() || !root.contains("vars") || !root["vars"].is_object()) return nullptr;
    const Json& vars = root["vars"];
    const char* group = block == "color" ? "colors" : "numbers";
    if (!vars.contains(group) || !vars[group].is_object()) return nullptr;
    const Json& values = vars[group];
    return values.contains(name) ? &values[name] : nullptr;
}

Resolved resolveValue(const Json& root, const std::string& nodePtr, const Json& value,
    const Project& project, int depth)
{
    (void)nodePtr;
    Resolved result;
    if (depth > 8)
    {
        result.error = "变量引用层级过深（可能有循环引用）";
        return result;
    }

    if (value.is_string())
    {
        const std::string text = value.get<std::string>();
        if (text.rfind("@theme/", 0) == 0 || text.rfind("@style/", 0) == 0)
        {
            const std::string path = text.rfind("@theme/", 0) == 0 ? text.substr(7) : text.substr(7);
            const Token* token = project.getToken(path);
            if (token == nullptr)
            {
                result.error = "全局变量不存在：" + text;
                return result;
            }
            result.ok = true;
            result.value = token->value;
            result.isRef = true;
            result.ref = text;
            return result;
        }
        if (text.rfind("&color.", 0) == 0 || text.rfind("&number.", 0) == 0)
        {
            const bool color = text.rfind("&color.", 0) == 0;
            const std::string name = text.substr(color ? 7 : 8);
            const Json* definition = findLocalVar(root, color ? "color" : "number", name);
            if (definition == nullptr)
            {
                result.error = "局部变量不存在：" + text;
                return result;
            }
            result = resolveValue(root, std::string(), *definition, project, depth + 1);
            result.isLocal = true;
            result.ref = text;
            return result;
        }
        result.ok = true;
        result.value = value;
        return result;
    }

    // Legacy object refs are reported clearly after the bklv-schema migration.
    if (value.is_object() && (value.contains("$ref") || value.contains("$var")))
    {
        result.error = "旧版 $ref / $var 写法已废弃，请改用 @theme/ @style/ 或 &color/ &number";
        return result;
    }

    result.ok = true;
    result.value = value;
    return result;
}

namespace
{

void collectRefsRecursive(const Json& value, std::vector<std::string>& out)
{
    if (value.is_string())
    {
        const std::string text = value.get<std::string>();
        if ((text.rfind("@theme/", 0) == 0 || text.rfind("@style/", 0) == 0) &&
            std::find(out.begin(), out.end(), text) == out.end())
            out.push_back(text);
        return;
    }
    if (value.is_object())
    {
        for (auto it = value.begin(); it != value.end(); ++it)
            collectRefsRecursive(it.value(), out);
        return;
    }
    if (value.is_array())
        for (const Json& item : value) collectRefsRecursive(item, out);
}

} // namespace

std::vector<std::string> collectGlobalRefs(const Json& node)
{
    std::vector<std::string> refs;
    collectRefsRecursive(node, refs);
    return refs;
}

int countRefsInJson(const Json& value, const std::string& reference)
{
    if (value.is_string())
        return value.get<std::string>() == reference ? 1 : 0;
    if (value.is_object())
    {
        int count = 0;
        for (auto it = value.begin(); it != value.end(); ++it)
            count += countRefsInJson(it.value(), reference);
        return count;
    }
    if (value.is_array())
    {
        int count = 0;
        for (const Json& item : value) count += countRefsInJson(item, reference);
        return count;
    }
    return 0;
}

int renameRefsInJson(Json& value, const std::string& oldRef, const std::string& newRef)
{
    int count = 0;
    if (value.is_string())
    {
        if (value.get<std::string>() == oldRef)
        {
            value = newRef;
            count++;
        }
        return count;
    }
    if (value.is_object())
    {
        for (auto it = value.begin(); it != value.end(); ++it)
            count += renameRefsInJson(it.value(), oldRef, newRef);
    }
    else if (value.is_array())
    {
        for (Json& item : value) count += renameRefsInJson(item, oldRef, newRef);
    }
    return count;
}

std::string nodeDisplayName(const Json& node, const std::string& type)
{
    if (!node.is_object()) return "Box";
    if (node.contains("id") && node["id"].is_string() && !node["id"].get<std::string>().empty())
        return node["id"].get<std::string>();
    if (type == "Label" && node.contains("text") && node["text"].is_string())
        return node["text"].get<std::string>();
    if (type == "Image" && node.contains("src") && node["src"].is_string())
    {
        std::string src = node["src"].get<std::string>();
        const size_t slash = src.rfind('/');
        if (slash != std::string::npos) src = src.substr(slash + 1);
        return src.empty() ? "Image" : src;
    }
    if (type == "Button" && node.contains("text") && node["text"].is_string())
        return node["text"].get<std::string>();
    return type.empty() ? "Box" : type;
}

void buildNodeTree(const Json& root, Node& out)
{
    out = Node();
    out.pointer = std::string();
    out.type = root.is_object() ? root.value("type", std::string("Box")) : "Box";
    out.id = root.is_object() ? root.value("id", std::string()) : std::string();
    out.name = root.is_object() ? nodeDisplayName(root, out.type) : "Box";

    std::function<void(Node&, const Json&, const std::string&, int)> build =
        [&](Node& node, const Json& json, const std::string& pointer, int depth) {
            node.pointer = pointer;
            node.depth = depth;
            node.type = json.is_object() ? json.value("type", std::string("Box")) : "Box";
            node.id = json.is_object() ? json.value("id", pointer) : pointer;
            node.name = nodeDisplayName(json, node.type);
            if (!json.is_object() || !json.contains("children") || !json["children"].is_array())
                return;
            node.hasChildren = true;
            const Json& children = json["children"];
            for (size_t i = 0; i < children.size(); ++i)
            {
                std::unique_ptr<Node> child(new Node());
                child->parent = &node;
                const std::string childPtr = pointer + "/children/" + std::to_string(i);
                build(*child, children[i], childPtr, depth + 1);
                node.children.push_back(std::move(child));
            }
        };

    if (root.is_object() && root.contains("children") && root["children"].is_array())
    {
        out.hasChildren = true;
        for (size_t i = 0; i < root["children"].size(); ++i)
        {
            std::unique_ptr<Node> child(new Node());
            child->parent = &out;
            build(*child, root["children"][i], "/children/" + std::to_string(i), 1);
            out.children.push_back(std::move(child));
        }
    }
}

void buildAppearanceTree(const Json& root, Node& out, const fs::path& themeRoot,
    const std::string& relative)
{
    out = Node();
    out.pointer = std::string();
    out.type = "Appearance";
    out.name = "外观";
    out.depth = 0;

    const auto nodeName = [](const Json& layer, const std::string& fallback) {
        if (layer.is_string()) return layer.get<std::string>();
        if (!layer.is_object()) return fallback;
        if (layer.contains("import") && layer["import"].is_string())
            return std::string("导入 ") + layer["import"].get<std::string>();
        const std::string op = layer.value("op", std::string("layer"));
        const std::string id = layer.value("id", std::string());
        return id.empty() ? op : op + " · " + id;
    };

    const auto importPaths = [](const Json& layer) {
        std::vector<std::string> result;
        if (!layer.is_object() || !layer.contains("import")) return result;
        const auto add = [&](const Json& value) {
            if (!value.is_string()) return;
            std::string path = value.get<std::string>();
            if (path.size() < 5 || path.substr(path.size() - 5) != ".json") path += ".json";
            result.push_back(path);
        };
        if (layer["import"].is_string())
            add(layer["import"]);
        else if (layer["import"].is_array())
            for (const Json& value : layer["import"]) add(value);
        return result;
    };

    std::function<void(Node&, const Json&, const std::string&, const std::string&, bool, int,
        std::set<std::string>&, const std::string&, const std::string&)> appendFile;

    const auto appendImportOne = [&](Node& parent, const std::string& path,
                                     const std::string& pointer, const std::string& sourcePointer,
                                     int depth, std::set<std::string>& stack,
                                     const std::string& currentSourceFile) {
        std::unique_ptr<Node> child(new Node());
        child->pointer = pointer;
        child->type = "Import";
        child->id = pointer;
        child->sourceFile = currentSourceFile;
        child->sourcePointer = sourcePointer;
        std::string display = path;
        if (display.size() > 5 && display.substr(display.size() - 5) == ".json")
            display = display.substr(0, display.size() - 5);
        child->name = "导入 " + display;
        child->depth = depth;
        child->parent = &parent;

        if (themeRoot.empty())
        {
            child->name += "（无法解析）";
            parent.children.push_back(std::move(child));
            return;
        }
        if (stack.find(path) != stack.end())
        {
            child->name += "（循环导入）";
            parent.children.push_back(std::move(child));
            return;
        }

        const fs::path file = themeRoot / path;
        std::error_code code;
        if (!fs::exists(file, code))
        {
            child->name += "（未找到）";
            parent.children.push_back(std::move(child));
            return;
        }
        std::string error;
        Json imported = parseJsonFile(file, error);
        if (!imported.is_object())
        {
            child->name += "（解析失败）";
            parent.children.push_back(std::move(child));
            return;
        }

        stack.insert(path);
        Node* raw = child.get();
        raw->hasChildren = true;
        appendFile(*raw, imported, pointer + "/@import:" + path, path, false, depth + 1, stack,
            std::string(), path);
        stack.erase(path);
        parent.children.push_back(std::move(child));
    };

    std::function<void(Node&, const Json&, const std::string&, const std::string&, int,
        const std::string&)> appendChildrenNodes;
    appendChildrenNodes = [&](Node& parent, const Json& children, const std::string& pointerBase,
                              const std::string& sourceBase, int depth,
                              const std::string& sourceFile) {
        if (!children.is_array() || depth > 16) return;
        parent.hasChildren = true;
        for (size_t i = 0; i < children.size(); ++i)
        {
            const Json& childJson = children[i];
            const std::string pointer = pointerBase + "/children/" + std::to_string(i);
            std::unique_ptr<Node> child(new Node());
            child->pointer = pointer;
            child->type = childJson.is_object() ?
                (childJson.contains("appearance") ? "Appearance" :
                    (childJson.contains("ref") ? "Ref" : "Box")) : "Child";
            child->id = pointer;
            child->name = nodeName(childJson, "子节点");
            child->sourceFile = sourceFile;
            child->sourcePointer = sourceBase + "/children/" + std::to_string(i);
            child->depth = depth;
            child->parent = &parent;
            if (childJson.is_object() && childJson.contains("children"))
                appendChildrenNodes(*child, childJson["children"], pointer, sourceBase, depth + 1,
                    sourceFile);
            parent.children.push_back(std::move(child));
        }
    };

    appendFile = [&](Node& parent, const Json& file, const std::string& pointerBase,
                     const std::string& fileRelative, bool local, int depth,
                     std::set<std::string>& stack, const std::string& sourceBase,
                     const std::string& sourceFile) {
        if (depth > 16) return;
        if (!file.is_object()) return;
        const std::string currentSourceFile = local ? std::string() : sourceFile;

        const std::vector<std::string> topImports = importPaths(file);
        if (!topImports.empty())
        {
            parent.hasChildren = true;
            for (size_t i = 0; i < topImports.size(); ++i)
            {
                const bool multiple = topImports.size() > 1;
                const std::string pointer = pointerBase +
                    (multiple ? "/import/" + std::to_string(i) : "/import");
                const std::string sourcePointer = sourceBase +
                    (multiple ? "/import/" + std::to_string(i) : "/import");
                appendImportOne(parent, topImports[i], pointer, sourcePointer, depth, stack,
                    currentSourceFile);
            }
        }

        if (file.contains("layers") && file["layers"].is_array())
        {
            parent.hasChildren = true;
            for (size_t i = 0; i < file["layers"].size(); ++i)
            {
                const Json& layer = file["layers"][i];
                const std::string pointer = pointerBase + "/layers/" + std::to_string(i);
                const std::string sourcePointer = sourceBase + "/layers/" + std::to_string(i);
                const std::vector<std::string> imports = importPaths(layer);
                if (!imports.empty())
                {
                    for (size_t j = 0; j < imports.size(); ++j)
                    {
                        const bool multiple = imports.size() > 1;
                        const std::string importPointer = pointer +
                            (multiple ? "/import/" + std::to_string(j) : "/import");
                        const std::string importSource = sourcePointer +
                            (multiple ? "/import/" + std::to_string(j) : "/import");
                        appendImportOne(parent, imports[j], importPointer, importSource, depth,
                            stack, currentSourceFile);
                    }
                    continue;
                }
                std::unique_ptr<Node> child(new Node());
                child->pointer = pointer;
                child->type = layer.is_object() ? layer.value("op", std::string("layer")) : "layer";
                child->id = pointer;
                child->name = nodeName(layer, "图层");
                child->sourceFile = currentSourceFile;
                child->sourcePointer = sourcePointer;
                child->depth = depth;
                child->parent = &parent;
                parent.children.push_back(std::move(child));
            }
        }

        if (file.contains("children") && file["children"].is_array())
            appendChildrenNodes(parent, file["children"], pointerBase, sourceBase, depth,
                currentSourceFile);
    };

    std::set<std::string> stack;
    if (!relative.empty()) stack.insert(relative);
    appendFile(out, root, std::string(), relative, true, 1, stack, std::string(), std::string());
}

Node* findNode(Node& root, const std::string& pointer)
{
    if (pointer.empty()) return &root;
    for (auto& child : root.children)
    {
        if (child->pointer == pointer) return child.get();
        if (pointer.rfind(child->pointer + "/", 0) == 0)
            return findNode(*child, pointer);
    }
    return nullptr;
}

const Node* findNode(const Node& root, const std::string& pointer)
{
    if (pointer.empty()) return &root;
    for (const auto& child : root.children)
    {
        if (child->pointer == pointer) return child.get();
        if (pointer.rfind(child->pointer + "/", 0) == 0)
            return findNode(*child, pointer);
    }
    return nullptr;
}

Json* jsonAt(Json& root, const std::string& pointer)
{
    if (pointer.empty()) return &root;
    try
    {
        return &root.at(Json::json_pointer(pointer));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

const Json* jsonAt(const Json& root, const std::string& pointer)
{
    if (pointer.empty()) return &root;
    try
    {
        return &root.at(Json::json_pointer(pointer));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

Json makeNode(const std::string& type)
{
    Json node;
    node["type"] = type;
    if (type == "Label")
    {
        node["text"] = "新的文本";
        node["fontSize"] = 14;
        node["textColor"] = "@theme/color.text";
    }
    else if (type == "Image")
    {
        node["src"] = "@res/assets/logo.svg";
        node["width"] = 120;
        node["height"] = 80;
        node["scalingType"] = "fit";
    }
    else if (type == "Button")
    {
        node["width"] = 160;
        node["height"] = 44;
        node["text"] = "按钮";
        node["variant"] = "";
        node["enabled"] = true;
    }
    else
    {
        node["width"] = 160;
        node["height"] = 72;
        node["axis"] = "column";
        node["gap"] = 8;
        node["padding"] = 8;
        node["cornerRadius"] = 8;
        node["backgroundColor"] = "#1B2942";
    }
    return node;
}

bool isDescendantPath(const std::string& parentPointer, const std::string& childPointer)
{
    if (parentPointer.empty()) return true;
    return childPointer == parentPointer || childPointer.rfind(parentPointer + "/", 0) == 0;
}

std::string parentPointer(const std::string& pointer)
{
    const size_t slash = pointer.rfind('/');
    if (slash == std::string::npos) return std::string();
    const size_t children = pointer.rfind("/children/");
    if (children == std::string::npos) return std::string();
    if (children == 0) return std::string();
    return pointer.substr(0, children);
}

int childIndex(const std::string& pointer)
{
    const size_t marker = pointer.rfind("/children/");
    if (marker == std::string::npos) return -1;
    try
    {
        return std::stoi(pointer.substr(marker + 10));
    }
    catch (const std::exception&)
    {
        return -1;
    }
}

void scanProjectFiles(const fs::path& root, std::vector<FileEntry>& out)
{
    out.clear();
    std::error_code code;

    std::function<void(const fs::path&, const std::string&, int)> walk =
        [&](const fs::path& directory, const std::string& relative, int depth) {
            std::vector<fs::directory_entry> entries;
            for (const fs::directory_entry& entry : fs::directory_iterator(directory, code))
            {
                if (code) return;
                const std::string filename = entry.path().filename().string();
                if (filename.empty() || filename[0] == '.') continue;
                if (entry.path().extension() == ".bak" || entry.path().extension() == ".tmp") continue;
                entries.push_back(entry);
            }

            std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
                const bool ad = a.is_directory();
                const bool bd = b.is_directory();
                if (ad != bd) return ad;
                return a.path().filename().string() < b.path().filename().string();
            });

            for (const fs::directory_entry& entry : entries)
            {
                const std::string filename = entry.path().filename().string();
                const std::string childRelative = relative.empty() ? filename : relative + "/" + filename;
                FileEntry item;
                item.relative = childRelative;
                item.name = filename;
                item.isDirectory = entry.is_directory();
                item.depth = depth;
                out.push_back(item);
                if (item.isDirectory)
                    walk(entry.path(), childRelative, depth + 1);
            }
        };

    walk(root, std::string(), 0);
}

} // namespace designer
