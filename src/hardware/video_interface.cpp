#include "hardware/video_interface.h"

#include "hardware/hollywood.h"
#include "memory/memory.h"

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

namespace {

uint32_t PhysicalToEffective(uint32_t address) {
    // VI stores physical external-memory addresses. The CPU-side memory
    // implementation exposes the corresponding MEM1/MEM2 effective aliases.
    if (address < memory::Memory::MEM1_SIZE)
        return memory::Memory::MEM1_BASE + address;

    constexpr uint32_t Mem2PhysicalBase = 0x10000000;
    if (address >= Mem2PhysicalBase &&
        address < Mem2PhysicalBase + memory::Memory::MEM2_SIZE) {
        return memory::Memory::MEM2_BASE +
               (address - Mem2PhysicalBase);
    }

    return address;
}

} // namespace

uint32_t VideoInterface::XfbAddressTop() const {
    const uint32_t packed = Read32(Base + 0x1C);
    const uint32_t fbb = packed & 0x00FFFFFFu;
    const bool poff = (packed & (1u << 28)) != 0;
    const uint32_t physical = poff ? (fbb << 5) : fbb;
    return PhysicalToEffective(physical);
}

uint32_t VideoInterface::XfbAddressBottom() const {
    const uint32_t packed = Read32(Base + 0x24);
    const uint32_t fbb = packed & 0x00FFFFFFu;
    const bool poff = (packed & (1u << 28)) != 0;
    const uint32_t physical = poff ? (fbb << 5) : fbb;
    return PhysicalToEffective(physical);
}

XfbInfo VideoInterface::CurrentXfb() const {
    // HSCALEW: WPL is the displayed framebuffer width in 16-pixel units and
    // STD is the external framebuffer stride in 16-pixel units.
    const uint16_t picture = registers_[0x48 / 2];
    const uint16_t vertical = registers_[0x00 / 2];

    const uint32_t width_pixels =
        ((picture >> 8) & 0x7Fu) * 16u;
    const uint32_t stride_bytes =
        (picture & 0x7Fu) * 32u;
    const uint32_t active_lines =
        (vertical >> 4) & 0x3FFu;

    return {
        XfbAddressTop(),
        width_pixels,
        stride_bytes,
        active_lines
    };
}

} // namespace vwii::hardware
