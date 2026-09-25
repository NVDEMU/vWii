#pragma once

#include <array>
#include <cstdint>

namespace vwii::hardware {

class AudioInterface {
public:
    void Reset();
    void Tick(uint64_t cycles);

    [[nodiscard]] uint32_t Read32(uint32_t address) const;
    void Write32(uint32_t address, uint32_t value);

    [[nodiscard]] uint64_t SampleCount() const { return sample_count_; }

private:
    static constexpr uint32_t Base = 0xCC006C00;
    static constexpr uint32_t Size = 0x40;

    std::array<uint32_t, Size / 4> registers_{};
    uint64_t sample_count_{};
    uint64_t cycle_accumulator_{};
};

} // namespace vwii::hardware
