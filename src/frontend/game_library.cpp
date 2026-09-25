#include "frontend/game_library.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>

namespace vwii::frontend {

namespace {

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

} // namespace

GameLibrary::GameLibrary() = default;

std::filesystem::path GameLibrary::ConfigDirectory() const {
    char* pref = SDL_GetPrefPath("NVDEMU", "vWii");
    if (!pref)
        return {};

    const std::filesystem::path result(pref);
    SDL_free(pref);
    return result;
}

std::filesystem::path GameLibrary::ConfigFile() const {
    const auto directory = ConfigDirectory();
    return directory.empty() ? std::filesystem::path{} :
                               directory / "game_folders.cfg";
}

std::filesystem::path GameLibrary::RecentFile() const {
    const auto directory = ConfigDirectory();
    return directory.empty() ? std::filesystem::path{} :
                               directory / "recent_games.cfg";
}

bool GameLibrary::IsGameFile(const std::filesystem::path& path) {
    if (!path.has_extension())
        return false;

    const std::string ext = Lowercase(path.extension().string());
    return ext == ".rvz" || ext == ".iso" || ext == ".wia" ||
           ext == ".wbfs" || ext == ".dol" || ext == ".elf";
}

std::string GameLibrary::DisplayName(const std::filesystem::path& path) {
    std::string name = path.stem().string();
    if (name.empty())
        name = path.filename().string();
    return name;
}

void GameLibrary::Load() {
    folders_.clear();
    games_.clear();
    recent_.clear();

    const auto config_dir = ConfigDirectory();
    if (!config_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(config_dir / "games", ec);

        // The bundled library folder gives a new installation an immediately
        // usable location without forcing a folder dialog before first launch.
        AddFolder(config_dir / "games");
    }

    const auto folders_file = ConfigFile();
    if (!folders_file.empty()) {
        std::ifstream input(folders_file);
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty())
                AddFolder(line);
        }
    }

    const auto recent_file = RecentFile();
    if (!recent_file.empty()) {
        std::ifstream input(recent_file);
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty())
                recent_.emplace_back(line);
        }
    }

    Save();
    Rescan();
}

void GameLibrary::Save() const {
    const auto config_dir = ConfigDirectory();
    if (!config_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(config_dir, ec);
    }

    const auto folders_file = ConfigFile();
    if (!folders_file.empty()) {
        std::ofstream output(folders_file, std::ios::trunc);
        for (const auto& folder : folders_)
            output << folder.string() << '\n';
    }

    const auto recent_file = RecentFile();
    if (!recent_file.empty()) {
        std::ofstream output(recent_file, std::ios::trunc);
        for (const auto& path : recent_)
            output << path.string() << '\n';
    }
}

bool GameLibrary::AddFolder(const std::filesystem::path& folder) {
    std::error_code ec;
    const auto absolute = std::filesystem::weakly_canonical(folder, ec);
    const auto normalized = ec ? folder.lexically_normal() : absolute;

    if (!std::filesystem::exists(normalized, ec) ||
        !std::filesystem::is_directory(normalized, ec))
        return false;

    if (std::find(folders_.begin(), folders_.end(), normalized) != folders_.end())
        return false;

    folders_.push_back(normalized);
    Save();
    Rescan();
    return true;
}

bool GameLibrary::RemoveFolder(std::size_t index) {
    if (index >= folders_.size())
        return false;

    folders_.erase(folders_.begin() + static_cast<std::ptrdiff_t>(index));
    Save();
    Rescan();
    return true;
}

void GameLibrary::Rescan() {
    games_.clear();

    std::error_code ec;
    for (const auto& folder : folders_) {
        if (!std::filesystem::is_directory(folder, ec))
            continue;

        std::filesystem::recursive_directory_iterator iterator(
            folder,
            std::filesystem::directory_options::skip_permission_denied,
            ec);
        const std::filesystem::recursive_directory_iterator end;

        for (; iterator != end; iterator.increment(ec)) {
            if (ec)
                break;

            if (!iterator->is_regular_file(ec))
                continue;

            const auto path = iterator->path();
            if (!IsGameFile(path))
                continue;

            GameEntry entry;
            entry.path = path;
            entry.title = DisplayName(path);
            entry.extension = Lowercase(path.extension().string());
            entry.size = iterator->file_size(ec);
            if (ec)
                entry.size = 0;

            games_.push_back(std::move(entry));
        }
    }

    std::sort(games_.begin(), games_.end(),
              [](const GameEntry& a, const GameEntry& b) {
                  return a.title < b.title;
              });

    Save();
}

void GameLibrary::MarkRecent(const std::filesystem::path& path) {
    std::error_code ec;
    const auto absolute = std::filesystem::weakly_canonical(path, ec);
    const auto normalized = ec ? path.lexically_normal() : absolute;

    recent_.erase(
        std::remove(recent_.begin(), recent_.end(), normalized),
        recent_.end());

    recent_.insert(recent_.begin(), normalized);

    constexpr std::size_t MaxRecent = 10;
    if (recent_.size() > MaxRecent)
        recent_.resize(MaxRecent);

    Save();
}

} // namespace vwii::frontend
