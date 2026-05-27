#pragma once

#include <string>
#include <unordered_map>

struct ModelAsset
{
    std::wstring id;
    std::wstring path; // FBX/glTF path. The loader can be added behind this record later.
};

class ModelLibrary
{
public:
    void Register(std::wstring id, std::wstring path);
    const ModelAsset* Find(const std::wstring& id) const;

private:
    std::unordered_map<std::wstring, ModelAsset> models_;
};
