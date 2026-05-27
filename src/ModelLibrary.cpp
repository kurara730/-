#include "ModelLibrary.h"

#include <utility>

void ModelLibrary::Register(std::wstring id, std::wstring path)
{
    ModelAsset asset{};
    asset.id = id;
    asset.path = std::move(path);
    models_[std::move(id)] = std::move(asset);
}

const ModelAsset* ModelLibrary::Find(const std::wstring& id) const
{
    const auto it = models_.find(id);
    if (it == models_.end()) return nullptr;
    return &it->second;
}
