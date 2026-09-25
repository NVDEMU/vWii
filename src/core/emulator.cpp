#include "core/emulator.h"

namespace vwii::core {

Emulator::Emulator()
    : memory_(), cpu_(memory_), ios_(memory_) {
}

bool Emulator::Initialize() {
    memory_.Reset();
    cpu_.Reset();
    ios_.Reset();
    scheduler_.Reset();
    disc_.reset();

    initialized_ = true;
    loaded_image_ = false;
    return true;
}

void Emulator::Shutdown() {
    disc_.reset();
    initialized_ = false;
    loaded_image_ = false;
}

void Emulator::Reset() {
    memory_.Reset();
    cpu_.Reset();
    ios_.Reset();
    scheduler_.Reset();

    disc_.reset();
    loaded_image_ = false;
}

void Emulator::Step() {
    if (!initialized_)
        return;

    // HLE IOS on the Starlet before running another Broadway instruction.
    ios_.Tick();

    cpu_.Step();

    // One scheduler tick per interpreted guest instruction. The value is
    // intentionally abstract until the Broadway timebase is implemented.
    scheduler_.Advance(1);
}

void Emulator::RunForInstructions(uint64_t count) {
    for (uint64_t i = 0; i < count; ++i)
        Step();
}

boot::LoadResult Emulator::LoadImage(const std::vector<uint8_t>& image) {
    memory_.Reset();
    disc_.reset();

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
    disc_.reset();

    const boot::LoadResult result = boot::LoadImageFile(path, memory_);
    if (result.success) {
        cpu_.Reset(result.entry_point);
        loaded_image_ = true;
    } else {
        loaded_image_ = false;
    }

    return result;
}

boot::WiiBootResult Emulator::LoadWiiGame(const std::string& path) {
    memory_.Reset();
    disc_.reset();

    auto candidate = std::make_unique<disc::DiscImage>();
    if (!candidate->Open(path)) {
        boot::WiiBootResult result;
        result.error = candidate->Info().error.empty()
            ? "Unable to open RVZ image"
            : candidate->Info().error;
        return result;
    }

    boot::WiiBootResult result =
        boot::LoadWiiGame(*candidate, memory_);

    if (!result.success) {
        loaded_image_ = false;
        return result;
    }

    disc_ = std::move(candidate);
    ios_.AttachDisc(disc_.get());

    cpu_.Reset(result.dol_result.entry_point);
    loaded_image_ = true;
    return result;
}

} // namespace vwii::core
