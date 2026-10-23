#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "bklv/bklv.hpp"
#include "bklv/core/app.hpp"
#include "ui/app.hpp"

int main(int argc, char** argv)
{
    std::string project;
    std::string openFile;
    std::string screenshot;
    std::string theme;
    std::string inspector;
    std::string selectId;
    float inspectorScroll = 0.0f;
    float textScale = 1.15f;
    std::vector<std::pair<int, int>> clicks;
    std::string scriptedText;
    std::string scriptedKey;
    int scriptedFrame = -1;
    std::vector<std::pair<int, int>> dragFrom;
    std::vector<std::pair<int, int>> dragTo;
    std::vector<std::pair<int, std::string>> actions;
    std::string codeText;
    int codeFrame = 12;
    bool typeKeep = false;
    int frames = 0;
    int clickX = -1;
    int clickY = -1;
    int width = 1672;
    int height = 941;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc)
            project = argv[++i];
        else if (std::strcmp(argv[i], "--open") == 0 && i + 1 < argc)
            openFile = argv[++i];
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
            screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            frames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--theme") == 0 && i + 1 < argc)
            theme = argv[++i];
        else if (std::strcmp(argv[i], "--click") == 0 && i + 1 < argc)
        {
            const char* value = argv[++i];
            const char* comma = std::strchr(value, ',');
            if (comma != nullptr)
            {
                clickX = std::atoi(value);
                clickY = std::atoi(comma + 1);
                clicks.push_back({ clickX, clickY });
            }
        }
        else if (std::strcmp(argv[i], "--inspector") == 0 && i + 1 < argc)
            inspector = argv[++i];
        else if (std::strcmp(argv[i], "--select") == 0 && i + 1 < argc)
            selectId = argv[++i];
        else if (std::strcmp(argv[i], "--drag") == 0 && i + 1 < argc)
        {
            int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            if (std::sscanf(argv[++i], "%d,%d,%d,%d", &x1, &y1, &x2, &y2) == 4)
            {
                dragFrom.push_back({ x1, y1 });
                dragTo.push_back({ x2, y2 });
            }
        }
        else if (std::strcmp(argv[i], "--action") == 0 && i + 1 < argc)
        {
            const std::string action = argv[++i];
            int frame = 12;
            if (i + 1 < argc && std::strcmp(argv[i + 1], "--at") == 0)
            {
                frame = std::atoi(argv[i + 2]);
                i += 2;
            }
            actions.push_back({ frame, action });
        }
        else if (std::strcmp(argv[i], "--code-text") == 0 && i + 1 < argc)
            codeText = argv[++i];
        else if (std::strcmp(argv[i], "--code-frame") == 0 && i + 1 < argc)
            codeFrame = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--type") == 0 && i + 1 < argc)
            scriptedText = argv[++i];
        else if (std::strcmp(argv[i], "--type-key") == 0 && i + 1 < argc)
            scriptedKey = argv[++i];
        else if (std::strcmp(argv[i], "--type-keep") == 0)
            typeKeep = true;
        else if (std::strcmp(argv[i], "--type-frame") == 0 && i + 1 < argc)
            scriptedFrame = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--text-scale") == 0 && i + 1 < argc)
            textScale = (float)std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--scroll") == 0 && i + 1 < argc)
            inspectorScroll = (float)std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc)
            width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc)
            height = std::atoi(argv[++i]);
    }

    bklv::App app;
    designer::DesignerApp designer;
    if (theme == "light")
        designer.lightTheme = true;
    else if (theme == "dark")
        designer.lightTheme = false;
    designer.attach(app);

    if (!project.empty())
        designer.openProject(project, false);
    if (!openFile.empty())
        designer.openDocument(openFile, true);

    designer.textScale = textScale;
    if (inspector == "style")
        designer.inspectorTab = designer::InspectorTab::STYLE;
    if (!selectId.empty())
        designer.selectNodeById(selectId);
    if (inspectorScroll > 0.0f)
        designer.inspectorScroll = inspectorScroll;

    if (!screenshot.empty())
        app.requestScreenshot(screenshot, frames > 0 ? frames : 8);

    for (size_t i = 0; i < clicks.size(); ++i)
    {
        designer::DesignerApp::ScriptedClick click;
        click.frame = 8 + (int)i * 6;
        click.position = ImVec2((float)clicks[i].first, (float)clicks[i].second);
        designer.scriptedClicks.push_back(click);
    }
    for (size_t i = 0; i < dragFrom.size() && i < dragTo.size(); ++i)
    {
        designer::DesignerApp::ScriptedDrag drag;
        drag.frame = 8 + (int)i * 14;
        drag.from = ImVec2((float)dragFrom[i].first, (float)dragFrom[i].second);
        drag.to = ImVec2((float)dragTo[i].first, (float)dragTo[i].second);
        designer.scriptedDrags.push_back(drag);
    }
    for (const auto& action : actions)
    {
        designer::DesignerApp::ScriptedAction script;
        script.frame = action.first;
        script.action = action.second;
        designer.scriptedActions.push_back(script);
    }
    if (!codeText.empty())
    {
        designer::DesignerApp::ScriptedCode code;
        code.frame = codeFrame;
        code.text = codeText;
        designer.scriptedCodes.push_back(code);
    }
    if (!scriptedText.empty())
    {
        designer::DesignerApp::ScriptedEdit edit;
        edit.frame = scriptedFrame > 0 ? scriptedFrame : 12;
        edit.key = scriptedKey;
        edit.text = scriptedText;
        edit.commit = !typeKeep;
        designer.scriptedEdits.push_back(edit);
    }

    return app.run("Belive Designer", width, height);
}
