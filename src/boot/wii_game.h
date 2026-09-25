#pragma once

#include "boot/image_loader.h"
#include "disc/disc_image.h"

#include <string>

namespace vwii::boot {

struct WiiBootResult {
    bool success{};
    disc::DiscInfo disc;
    LoadResult dol_result;
    std::string error;
};

WiiBootResult LoadWiiGame(const std::string& path,
                          memory::Memory& memory);

} // namespace vwii::boot
