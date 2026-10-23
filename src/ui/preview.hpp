#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "bklv/core/view.hpp"
#include "core/model.hpp"

namespace designer
{

struct PreviewResult
{
    bklv::View* root = nullptr;
    std::unordered_map<std::string, bklv::View*> pointerToView;
    std::unordered_map<bklv::View*, std::string> viewToPointer;
    std::string error;
    std::vector<std::string> warnings;
    int nodeCount = 0;
};

class PreviewBuilder
{
  public:
    // Registers designer preview view types (DShadowBox) once.
    static void registerTypes();

    // Build a bklv view tree from a document root using the project's tokens.
    static PreviewResult build(const Project& project, const Json& documentRoot, float zoom);

    // Resolve a style property for the inspector (returns literal JSON when possible).
    static Json resolveStyleValue(const Project& project, const Json& documentRoot,
        const std::string& nodePointer, const Json& value, std::string* error = nullptr);
};

} // namespace designer
