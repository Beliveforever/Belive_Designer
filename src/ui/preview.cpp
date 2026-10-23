#include "ui/preview.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bklv/core/json_view.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/ui_scale.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/appearance_children.hpp"

namespace designer
{

namespace
{

void stripDesignerFields(Json& value)
{
    if (value.is_object())
    {
        // bklv view JSON has no id/name; older files may still carry them.
        value.erase("id");
        value.erase("name");
        // The designer's old shadow extension is not part of bklv view JSON.
        value.erase("shadow");
        value.erase("shadowColor");
        value.erase("shadowBlur");
        value.erase("shadowOffsetY");
        for (auto it = value.begin(); it != value.end(); ++it)
            stripDesignerFields(it.value());
    }
    else if (value.is_array())
    {
        for (Json& item : value) stripDesignerFields(item);
    }
}

fs::path resolveViewInclude(const std::string& raw, const fs::path& projectRoot)
{
    std::vector<fs::path> candidates;
    const auto add = [&](const fs::path& path) {
        if (path.empty()) return;
        for (const fs::path& existing : candidates)
            if (existing == path) return;
        candidates.push_back(path);
    };

    const fs::path rawPath(raw);
    if (rawPath.is_absolute())
        add(rawPath);
    else
    {
        add(projectRoot / rawPath);
        add(projectRoot / "resources" / rawPath);
        add(projectRoot.parent_path() / rawPath);
        if (raw.rfind("resources/", 0) == 0)
        {
            const std::string stripped = raw.substr(10);
            add(projectRoot / stripped);
            add(projectRoot.parent_path() / stripped);
            add(projectRoot.parent_path().parent_path() / stripped);
        }
    }
    std::error_code code;
    for (const fs::path& candidate : candidates)
        if (fs::exists(candidate, code) && fs::is_regular_file(candidate, code))
            return candidate;
    return fs::path();
}

bool expandViewIncludes(Json& value, const fs::path& projectRoot, std::set<std::string>& stack,
    std::string& error)
{
    if (value.is_object())
    {
        if (value.contains("include"))
        {
            if (!value["include"].is_string())
            {
                error = "include 必须是非空字符串";
                return false;
            }
            const std::string raw = value["include"].get<std::string>();
            const fs::path file = resolveViewInclude(raw, projectRoot);
            if (file.empty())
            {
                error = "include 文件不存在：" + raw;
                return false;
            }
            std::string canonical = file.lexically_normal().string();
            if (stack.find(canonical) != stack.end())
            {
                error = "循环 include：" + raw;
                return false;
            }
            std::string parseError;
            Json included = parseJsonFile(file, parseError);
            if (!included.is_object())
            {
                error = "include 解析失败：" + raw + " " + parseError;
                return false;
            }
            stack.insert(canonical);
            if (!expandViewIncludes(included, projectRoot, stack, error))
            {
                stack.erase(canonical);
                return false;
            }
            stack.erase(canonical);
            value = std::move(included);
            return true;
        }
        for (auto it = value.begin(); it != value.end(); ++it)
            if (!expandViewIncludes(it.value(), projectRoot, stack, error)) return false;
    }
    else if (value.is_array())
    {
        for (Json& item : value)
            if (!expandViewIncludes(item, projectRoot, stack, error)) return false;
    }
    return true;
}

void rewriteResourcePaths(Json& value, const fs::path& resourcesRoot)
{
    if (value.is_string())
    {
        std::string text = value.get<std::string>();
        if (text.rfind("@res/", 0) == 0)
        {
            const std::string relative = text.substr(5);
            const fs::path local = resourcesRoot / relative;
            std::error_code code;
            if (fs::exists(local, code))
                text = local.lexically_normal().string();
            else
                text = bklv::resourcePath("resources/" + relative);
            value = text;
        }
        return;
    }
    if (value.is_object())
    {
        for (auto it = value.begin(); it != value.end(); ++it)
            rewriteResourcePaths(it.value(), resourcesRoot);
    }
    else if (value.is_array())
    {
        for (Json& item : value) rewriteResourcePaths(item, resourcesRoot);
    }
}

} // namespace

void PreviewBuilder::registerTypes()
{
    // bklv's built-in view/widget types are registered by App; the designer no
    // longer needs a custom DShadowBox because view JSON only uses bklv fields.
}

PreviewResult PreviewBuilder::build(const Project& project, const Json& documentRoot, float zoom)
{
    PreviewResult result;
    if (!documentRoot.is_object())
    {
        result.error = "文档根节点必须是 JSON 对象";
        return result;
    }

    Json source = documentRoot;
    if (source.contains("type") || source.contains("include"))
    {
        std::set<std::string> includeStack;
        std::string includeError;
        if (!expandViewIncludes(source, project.root, includeStack, includeError))
        {
            result.error = includeError;
            return result;
        }
    }
    const bool appearanceSource = !documentRoot.contains("type") &&
        (documentRoot.contains("layers") || documentRoot.contains("box") ||
            documentRoot.contains("import"));
    if (!appearanceSource) stripDesignerFields(source);
    rewriteResourcePaths(source, project.root / "resources");

    if (!source.contains("type"))
    {
        const bool appearance = source.contains("layers") || source.contains("box") ||
            source.contains("import");
        if (!appearance)
        {
            result.error = "该文件不是 bklv 视图或外观文档";
            return result;
        }

        const float previousScale = bklv::getUiScale();
        bklv::setUiScale(std::max(0.05f, zoom));
        std::unique_ptr<bklv::AppearanceView> view(
            new bklv::AppearanceView("base/box", false));
        std::string error;
        if (!view->applyAppearanceJson(source.dump(), error))
        {
            bklv::setUiScale(previousScale);
            result.error = error.empty() ? "外观 JSON 编译失败" : error;
            return result;
        }

        const Json* box = source.contains("box") && source["box"].is_object()
            ? &source["box"] : nullptr;
        auto hasNumber = [&](const char* key) {
            return box != nullptr && box->contains(key) && (*box)[key].is_number();
        };
        const bool hasChildren = source.contains("children") && source["children"].is_array() &&
            !source["children"].empty();
        if (!hasNumber("width")) view->setWidth((hasChildren ? 360.0f : 300.0f) * std::max(0.05f, zoom));
        if (!hasNumber("height")) view->setHeight((hasChildren ? 120.0f : 44.0f) * std::max(0.05f, zoom));
        bklv::appearance::buildChildren(*view, view->getActiveAppearance().getChildren());
        bklv::setUiScale(previousScale);

        view->setText("text", "Button");
        view->setText("title", "Title");
        view->setText("info", "Information");
        view->setText("value", "Value");
        view->setText("initial", "A");
        view->setText("wireless", "Wi-Fi");
        view->setText("badge", "3");
        view->setText("icon", "★");
        view->setText("arrowLeft", "<");
        view->setText("arrowRight", ">");
        view->setGrow(0.0f);
        result.root = view.release();
        return result;
    }
    if (!source.contains("width") && !source.contains("height"))
    {
        source["width"] = project.canvasWidth;
        source["height"] = project.canvasHeight;
    }

    const float previousScale = bklv::getUiScale();
    bklv::setUiScale(std::max(0.05f, zoom));
    std::vector<bklv::InstantiatedView> map;
    bklv::View* root = bklv::createFromJSONString(source.dump(), &map, nullptr);
    bklv::setUiScale(previousScale);

    if (root == nullptr)
    {
        result.error = bklv::getLastLoadErrors();
        if (result.error.empty()) result.error = "bklv 无法实例化该视图";
        return result;
    }

    result.root = root;
    result.nodeCount = (int)map.size();
    for (const bklv::InstantiatedView& entry : map)
    {
        result.pointerToView[entry.pointer] = entry.view;
        result.viewToPointer[entry.view] = entry.pointer;
    }

    const std::string errors = bklv::getLastLoadErrors();
    if (!errors.empty())
    {
        std::string line;
        std::stringstream stream(errors);
        while (std::getline(stream, line))
            if (!line.empty()) result.warnings.push_back(line);
    }
    return result;
}

Json PreviewBuilder::resolveStyleValue(const Project& project, const Json& documentRoot,
    const std::string& nodePointer, const Json& value, std::string* error)
{
    const Resolved resolved = resolveValue(documentRoot, nodePointer, value, project);
    if (!resolved.ok)
    {
        if (error != nullptr) *error = resolved.error;
        return value;
    }
    return resolved.value;
}

} // namespace designer
