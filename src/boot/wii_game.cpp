#include "boot/wii_game.h"

#include "memory/memory.h"

#include <vector>

namespace vwii::boot {

WiiBootResult LoadWiiGame(const std::string& path, memory::Memory& memory) {
    WiiBootResult result;

    disc::DiscImage image;
    if (!image.Open(path)) {
        result.error = image.Info().error.empty()
            ? "Unable to open RVZ image"
            : image.Info().error;
        return result;
    }

    result.disc = image.Info();

    std::vector<uint8_t> dol;
    uint32_t entry_point = 0;

    if (!image.LoadGameDol(dol, entry_point)) {
        result.error =
            "The RVZ opened, but the Wii game's Main DOL could not be extracted";
        return result;
    }

    result.dol_result = LoadImage(dol, memory);
    if (!result.dol_result.success) {
        result.error = result.dol_result.error.empty()
            ? "The extracted Main DOL could not be loaded"
            : result.dol_result.error;
        return result;
    }

    result.dol_result.entry_point = entry_point;
    result.success = true;
    return result;
}

} // namespace vwii::boot
