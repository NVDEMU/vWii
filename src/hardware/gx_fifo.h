#pragma once

#include <array>
#include <cstdint>

namespace vwii::hardware {

class GXFifo {
public:
    void Reset();

    [[nodiscard]] uint32_t Read32(uint32_t address) const;
    void Write32(uint32_t address, uint32_t value);

    void WriteFifo32(uint32_t value);

    [[nodiscard]] uint64_t FifoWrites() const { return fifo_writes_; }
    [[nodiscard]] uint32_t LastFifoWord() const { return last_fifo_word_; }

private:
    static constexpr uint32_t RegisterBase = 0xCC000000;
    static constexpr uint32_t RegisterSize = 0x100;
    static constexpr uint32_t FifoBase = 0xCC008000;
    static constexpr uint32_t FifoSize = 0x1000;

    std::array<uint32_t, RegisterSize / 4> registers_{};
    uint64_t fifo_writes_{};
    uint32_t last_fifo_word_{};
};

} // namespace vwii::hardware
