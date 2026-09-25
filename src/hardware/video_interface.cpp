#include "hardware/video_interface.h"

#include "hardware/hollywood.h"

namespace vwii::hardware {

VideoInterface::VideoInterface(Hollywood& hollywood)
    : hollywood_(hollywood) {
    Reset();
}

void VideoInterface::Reset() {
    registers_.fill(0);
    frame_count_ = 0;
    cycle_accumulator_ = 0;
}

void VideoInterface::Tick(uint64_t cycles) {
    cycle_accumulator_ += cycles;

    constexpr uint64_t CyclesPerFrame = 16667;
    while (cycle_accumulator_ >= CyclesPerFrame) {
        cycle_accumulator_ -= CyclesPerFrame;
        VBlank();
    }
}

void VideoInterface::VBlank() {
    ++frame_count_;
    // Hollywood/Broadway IRQ source 24 is the VI interrupt.
    hollywood_.RaisePpcInterrupt(24);
}

uint32_t VideoInterface::Read32(uint32_t address) const {
    if (address < Base || address >= Base + Size || (address & 3u) != 0)
        return 0;

    const std::size_t index = (address - Base) / 2;
    return (static_cast<uint32_t>(registers_[index]) << 16) |
           registers_[index + 1];
}

void VideoInterface::Write32(uint32_t address, uint32_t value) {
    if (address < Base || address >= Base + Size || (address & 3u) != 0)
        return;

    const std::size_t index = (address - Base) / 2;
    registers_[index] = static_cast<uint16_t>(value >> 16);
    registers_[index + 1] = static_cast<uint16_t>(value);
}

uint32_t VideoInterface::XfbAddressTop() const {
    const uint32_t packed = Read32(Base + 0x1C);
    const uint32_t fbb = packed >> 8;
    const bool poff = (packed & (1u << 4)) != 0;
    return poff ? (fbb << 5) : fbb;
}

uint32_t VideoInterface::XfbAddressBottom() const {
    const uint32_t packed = Read32(Base + 0x24);
    const uint32_t fbb = packed >> 8;
    const bool poff = (packed & (1u << 4)) != 0;
    return poff ? (fbb << 5) : fbb;
}

} // namespace vwii::hardware
