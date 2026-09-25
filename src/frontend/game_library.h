#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace vwii::frontend {

struct GameEntry {
    std::filesystem::path path;
    std::string title;
    std::string extension;
    uintmax_t size{};
};

class GameLibrary {
public:
    GameLibrary();

    void Load();
    void Save() const;
    void Rescan();

    bool AddFolder(const std::filesystem::path& folder);
    bool RemoveFolder(std::size_t index);

    [[nodiscard]] const std::vector<std::filesystem::path>& Folders() const {
        return folders_;
    }

    [[nodiscard]] const std::vector<GameEntry>& Games() const {
        return games_;
    }

    void MarkRecent(const std::filesystem::path& path);
    [[nodiscard]] const std::vector<std::filesystem::path>& Recent() const {
        return recent_;
    }

private:
    static bool IsGameFile(const std::filesystem::path& path);
    static std::string DisplayName(const std::filesystem::path& path);

    std::filesystem::path ConfigDirectory() const;
    std::filesystem::path ConfigFile() const;
    std::filesystem::path RecentFile() const;

    std::vector<std::filesystem::path> folders_;
    std::vector<GameEntry> games_;
    std::vector<std::filesystem::path> recent_;
};

} // namespace vwii::frontend
