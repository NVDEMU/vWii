#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vwii::disc {
class DiscImage;
}

namespace vwii::filesystem {

struct FileEntry {
    std::string path;
    uint32_t offset{};
    uint32_t size{};
    bool directory{};
};

class WiiFST {
public:
    bool Load(disc::DiscImage& disc);

    [[nodiscard]] const std::vector<FileEntry>& Entries() const {
        return entries_;
    }

    [[nodiscard]] const FileEntry* Find(std::string_view path) const;

    bool ReadFile(disc::DiscImage& disc,
                  std::string_view path,
                  std::vector<uint8_t>& output) const;

private:
    std::vector<FileEntry> entries_;
};

} // namespace vwii::filesystem
