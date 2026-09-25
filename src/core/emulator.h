#pragma once

#include "boot/image_loader.h"
#include "boot/wii_game.h"
#include "cpu/powerpc.h"
#include "memory/memory.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vwii::core {

class Emulator {
public:
    Emulator();

    bool Initialize();
    void Shutdown();

    void Reset();
    void Step();
    void RunForInstructions(uint64_t count);

    [[nodiscard]] boot::LoadResult LoadImage(const std::vector<uint8_t>& image);
    [[nodiscard]] boot::LoadResult LoadImageFile(const std::string& path);
    [[nodiscard]] boot::WiiBootResult LoadWiiGame(const std::string& path);

    [[nodiscard]] cpu::PowerPC& CPU() { return cpu_; }
    [[nodiscard]] memory::Memory& Memory() { return memory_; }
    [[nodiscard]] bool IsInitialized() const { return initialized_; }
    [[nodiscard]] bool HasLoadedImage() const { return loaded_image_; }

private:
    memory::Memory memory_;
    cpu::PowerPC cpu_;
    bool initialized_{};
    bool loaded_image_{};
};

} // namespace vwii::core
