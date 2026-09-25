#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace vwii::memory {

class Memory {
public:
    static constexpr uint32_t MEM1_BASE = 0x80000000;
    static constexpr uint32_t MEM1_SIZE = 24 * 1024 * 1024;

    static constexpr uint32_t MEM2_BASE = 0x90000000;
    static constexpr uint32_t MEM2_SIZE = 64 * 1024 * 1024;

    Memory();

    void Reset();

    [[nodiscard]] uint8_t Read8(uint32_t address) const;
    [[nodiscard]] uint16_t Read16(uint32_t address) const;
    [[nodiscard]] uint32_t Read32(uint32_t address) const;

    void Write8(uint32_t address, uint8_t value);
    void Write16(uint32_t address, uint16_t value);
    void Write32(uint32_t address, uint32_t value);

private:
    [[nodiscard]] std::pair<const uint8_t*, std::size_t> Translate(uint32_t address) const;
    [[nodiscard]] std::pair<uint8_t*, std::size_t> TranslateMutable(uint32_t address);

    std::vector<uint8_t> mem1_;
    std::vector<uint8_t> mem2_;
};

} // namespace vwii::memory
