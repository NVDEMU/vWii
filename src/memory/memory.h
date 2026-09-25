#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "hardware/audio_interface.h"
#include "hardware/gx_fifo.h"
#include "hardware/hollywood.h"
#include "hardware/video_interface.h"

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
    void WriteBlock(uint32_t address, std::span<const uint8_t> data);
    void Fill(uint32_t address, std::size_t size, uint8_t value);

    [[nodiscard]] hardware::Hollywood& Hollywood() { return hollywood_; }
    [[nodiscard]] hardware::VideoInterface& Video() { return video_; }
    [[nodiscard]] hardware::GXFifo& GX() { return gx_; }
    [[nodiscard]] hardware::AudioInterface& Audio() { return audio_; }
    [[nodiscard]] const hardware::Hollywood& Hollywood() const { return hollywood_; }
    [[nodiscard]] bool ExternalInterruptPending() const {
        return hollywood_.PpcInterruptPending();
    }

private:
    [[nodiscard]] bool IsHollywoodRegister(uint32_t address) const;
    [[nodiscard]] uint32_t HollywoodRegisterAddress(uint32_t address) const;
    [[nodiscard]] bool IsPeripheralRegister(uint32_t address) const;
    [[nodiscard]] std::size_t PeripheralOffset(uint32_t address) const;

    [[nodiscard]] std::pair<const uint8_t*, std::size_t> Translate(uint32_t address) const;
    [[nodiscard]] std::pair<uint8_t*, std::size_t> TranslateMutable(uint32_t address);
    [[nodiscard]] std::size_t RegionRemaining(uint32_t address) const;

    std::vector<uint8_t> mem1_;
    std::vector<uint8_t> mem2_;
    std::vector<uint8_t> peripheral_regs_;
    hardware::Hollywood hollywood_{};
    hardware::VideoInterface video_;
    hardware::GXFifo gx_{};
    hardware::AudioInterface audio_{};
};

} // namespace vwii::memory
