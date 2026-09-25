#pragma once

#include "boot/image_loader.h"
#include "boot/wii_game.h"
#include "cpu/powerpc.h"
#include "disc/disc_image.h"
#include "ios/ios_hle.h"
#include "input/wiimote_keyboard.h"
#include "filesystem/wii_fst.h"
#include "memory/memory.h"
#include "system/scheduler.h"

#include <cstdint>
#include <memory>
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
    [[nodiscard]] ios::IOSHLE& IOS() { return ios_; }
    [[nodiscard]] input::WiiRemoteKeyboard& Wiimote() { return wiimote_; }
    [[nodiscard]] system::Scheduler& Scheduler() { return scheduler_; }
    [[nodiscard]] filesystem::WiiFST& GameFST() { return game_fst_; }

    [[nodiscard]] bool IsInitialized() const { return initialized_; }
    [[nodiscard]] bool HasLoadedImage() const { return loaded_image_; }
    [[nodiscard]] const disc::DiscImage* Disc() const { return disc_.get(); }

private:
    memory::Memory memory_;
    cpu::PowerPC cpu_;
    input::WiiRemoteKeyboard wiimote_;
    ios::IOSHLE ios_;
    system::Scheduler scheduler_;

    std::unique_ptr<disc::DiscImage> disc_;
    filesystem::WiiFST game_fst_;

    void ScheduleVideoInterrupt();

    bool initialized_{};
    bool loaded_image_{};
};

} // namespace vwii::core
