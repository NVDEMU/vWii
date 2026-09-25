#include "core/emulator.h"

namespace vwii::core {

Emulator::Emulator()
    : memory_(), cpu_(memory_) {
}

bool Emulator::Initialize() {
    memory_.Reset();
    cpu_.Reset();
    initialized_ = true;
    loaded_image_ = false;
    return true;
}

void Emulator::Shutdown() {
    initialized_ = false;
    loaded_image_ = false;
}

void Emulator::Reset() {
    memory_.Reset();
    cpu_.Reset();
    loaded_image_ = false;
}

void Emulator::Step() {
    if (initialized_) {
        cpu_.Step();
    }
}

void Emulator::RunForInstructions(uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
        Step();
    }
}

boot::LoadResult Emulator::LoadImage(const std::vector<uint8_t>& image) {
    memory_.Reset();

    const boot::LoadResult result = boot::LoadImage(image, memory_);
    if (result.success) {
        cpu_.Reset(result.entry_point);
        loaded_image_ = true;
    } else {
        loaded_image_ = false;
    }

    return result;
}

boot::LoadResult Emulator::LoadImageFile(const std::string& path) {
    memory_.Reset();

    const boot::LoadResult result = boot::LoadImageFile(path, memory_);
    if (result.success) {
        cpu_.Reset(result.entry_point);
        loaded_image_ = true;
    } else {
        loaded_image_ = false;
    }

    return result;
}

} // namespace vwii::core
