#include "hardware/audio_interface.h"

namespace vwii::hardware {

void AudioInterface::Reset() {
    registers_.fill(0);
    sample_count_ = 0;
    cycle_accumulator_ = 0;
}

void AudioInterface::Tick(uint64_t cycles) {
    cycle_accumulator_ += cycles;

    // Broadway AI output runs at a fixed sample clock. This is only a
    // deterministic timing model until the DSP/AI renderer exists.
    constexpr uint64_t CyclesPerSample = 1;
    while (cycle_accumulator_ >= CyclesPerSample) {
        cycle_accumulator_ -= CyclesPerSample;
        ++sample_count_;
    }
}

uint32_t AudioInterface::Read32(uint32_t address) const {
    if (address < Base || address >= Base + Size || (address & 3u) != 0)
        return 0;

    return registers_[(address - Base) / 4];
}

void AudioInterface::Write32(uint32_t address, uint32_t value) {
    if (address < Base || address >= Base + Size || (address & 3u) != 0)
        return;

    registers_[(address - Base) / 4] = value;
}

} // namespace vwii::hardware
