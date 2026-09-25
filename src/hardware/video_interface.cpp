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

    return registers_[(address - Base) / 4];
}

void VideoInterface::Write32(uint32_t address, uint32_t value) {
    if (address < Base || address >= Base + Size || (address & 3u) != 0)
        return;

    registers_[(address - Base) / 4] = value;
}

} // namespace vwii::hardware
