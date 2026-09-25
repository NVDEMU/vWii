#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vwii::memory {
class Memory;
}

namespace vwii::boot {

enum class ImageType {
    Dol,
    Elf32,
};

struct LoadResult {
    bool success{};
    ImageType type{ImageType::Dol};
    uint32_t entry_point{};
    std::string error;
};

LoadResult LoadImage(const std::vector<uint8_t>& image, memory::Memory& memory);
LoadResult LoadImageFile(const std::string& path, memory::Memory& memory);

} // namespace vwii::boot
