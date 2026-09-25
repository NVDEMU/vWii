#pragma once

#include <array>
#include <cstdint>

namespace vwii::hardware {

class Hollywood;

class VideoInterface {
public:
    explicit VideoInterface(Hollywood& hollywood);

    void Reset();
    void Tick(uint64_t cycles);

    [[nodiscard]] uint32_t Read32(uint32_t address) const;
    void Write32(uint32_t address, uint32_t value);

    [[nodiscard]] uint64_t FrameCount() const { return frame_count_; }
    [[nodiscard]] uint32_t XfbAddressTop() const;
    [[nodiscard]] uint32_t XfbAddressBottom() const;
    void VBlank();

private:
    static constexpr uint32_t Base = 0xCC002000;
    static constexpr uint32_t Size = 0x100;

    Hollywood& hollywood_;
    std::array<uint16_t, Size / 2> registers_{};
    uint64_t frame_count_{};
    uint64_t cycle_accumulator_{};
};

} // namespace vwii::hardware
