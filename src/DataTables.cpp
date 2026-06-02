#include "DataTables.h"

#include "GameStateTypes.h"

#include <windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace
{
namespace fs = std::filesystem;

// AssetLoaders.cpp の AssetPath と同じ探索順で、実行ファイル隣や作業ディレクトリから
// 相対パスを解決する。Release/Debug どちらの配置でも assets/ を見つけられるようにする。
fs::path ResolveDataPath(const wchar_t* relative)
{
    const fs::path rel(relative);

    std::array<fs::path, 5> candidates{};
    candidates[0] = fs::current_path() / rel;

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    const fs::path exeDir = fs::path(modulePath).parent_path();
    candidates[1] = exeDir / rel;
    candidates[2] = exeDir.parent_path() / rel;
    candidates[3] = exeDir.parent_path().parent_path() / rel;
    candidates[4] = rel;

    for (const auto& path : candidates)
    {
        std::error_code ec;
        if (fs::exists(path, ec))
        {
            return path;
        }
    }
    return rel;
}

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return std::wstring();
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return std::wstring();
    std::wstring w(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), len);
    return w;
}

std::string Trim(const std::string& s)
{
    const size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return std::string();
    const size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    std::stringstream ss(line);
    while (std::getline(ss, field, ','))
    {
        fields.push_back(Trim(field));
    }
    return fields;
}

float ParseFloatOr(const std::string& s, float fallback)
{
    if (s.empty()) return fallback;
    try
    {
        return std::stof(s);
    }
    catch (...)
    {
        return fallback;
    }
}
}

void LoadCharacterTableFromCsv()
{
    const fs::path path = ResolveDataPath(L"assets/data/characters.csv");
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return; // ファイルが無ければ既定値のまま
    }

    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // UTF-8 BOM を除去(Excel が付ける場合がある)
    if (raw.size() >= 3 &&
        static_cast<unsigned char>(raw[0]) == 0xEF &&
        static_cast<unsigned char>(raw[1]) == 0xBB &&
        static_cast<unsigned char>(raw[2]) == 0xBF)
    {
        raw.erase(0, 3);
    }

    std::stringstream all(raw);
    std::string line;
    bool headerSkipped = false;
    while (std::getline(all, line))
    {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#')
        {
            continue; // 空行・コメント行(#)は無視
        }
        if (!headerSkipped)
        {
            headerSkipped = true; // 最初の有効行はヘッダ(id,name,...)
            continue;
        }

        const std::vector<std::string> cols = SplitCsvLine(line);
        if (cols.size() < 9)
        {
            continue; // 列が足りない行はスキップ
        }

        int id = -1;
        try
        {
            id = std::stoi(cols[0]);
        }
        catch (...)
        {
            continue;
        }
        if (id < 0 || id >= static_cast<int>(Loadouts.size()))
        {
            continue;
        }

        LoadoutPreset& lp = Loadouts[static_cast<size_t>(id)];
        if (!cols[1].empty()) lp.name = Utf8ToWide(cols[1]);
        if (!cols[2].empty()) lp.role = Utf8ToWide(cols[2]);
        if (!cols[3].empty()) lp.summary = Utf8ToWide(cols[3]);
        lp.maxHp = ParseFloatOr(cols[4], lp.maxHp);
        lp.speed = ParseFloatOr(cols[5], lp.speed);
        lp.damageMul = ParseFloatOr(cols[6], lp.damageMul);
        lp.cooldownMul = ParseFloatOr(cols[7], lp.cooldownMul);
        lp.ultStart = ParseFloatOr(cols[8], lp.ultStart);
    }
}
