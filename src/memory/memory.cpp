#include "memory/memory.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace vwii::memory {

namespace {

constexpr uint32_t HollywoodBase = 0x0D800000;
constexpr uint32_t HollywoodMirrorBase = 0xCD800000;
constexpr uint32_t HollywoodIpcMirrorBase = 0xCD000000;
constexpr uint32_t HollywoodSize = 0x400;
constexpr uint32_t PeripheralSize = 0x10000;

} // namespace

Memory::Memory()
    : mem1_(MEM1_SIZE, 0),
      mem2_(MEM2_SIZE, 0),
      peripheral_regs_(PeripheralSize, 0) {
}

void Memory::Reset() {
    std::fill(mem1_.begin(), mem1_.end(), 0);
    std::fill(mem2_.begin(), mem2_.end(), 0);
    std::fill(peripheral_regs_.begin(), peripheral_regs_.end(), 0);
    hollywood_.Reset();
}

bool Memory::IsHollywoodRegister(uint32_t address) const {
    return (address >= HollywoodBase && address < HollywoodBase + HollywoodSize) ||
           (address >= HollywoodMirrorBase && address < HollywoodMirrorBase + HollywoodSize) ||
           (address >= HollywoodIpcMirrorBase && address < HollywoodIpcMirrorBase + HollywoodSize);
}

uint32_t Memory::HollywoodRegisterAddress(uint32_t address) const {
    if (address >= HollywoodMirrorBase)
        return HollywoodBase + (address - HollywoodMirrorBase);
    if (address >= HollywoodIpcMirrorBase)
        return HollywoodBase + (address - HollywoodIpcMirrorBase);
    return address;
}

bool Memory::IsPeripheralRegister(uint32_t address) const {
    return (address >= 0xCC000000 && address < 0xCC000000 + PeripheralSize) ||
           (address >= 0xCD000000 && address < 0xCD000000 + PeripheralSize);
}

std::size_t Memory::PeripheralOffset(uint32_t address) const {
    return static_cast<std::size_t>(address & (PeripheralSize - 1));
}

std::pair<const uint8_t*, std::size_t> Memory::Translate(uint32_t address) const {
    if (address >= MEM1_BASE && address < MEM1_BASE + MEM1_SIZE)
        return {mem1_.data(), static_cast<std::size_t>(address - MEM1_BASE)};

    if (address >= MEM2_BASE && address < MEM2_BASE + MEM2_SIZE)
        return {mem2_.data(), static_cast<std::size_t>(address - MEM2_BASE)};

    if (address >= 0xC0000000 && address < 0xC0000000 + MEM1_SIZE)
        return {mem1_.data(), static_cast<std::size_t>(address - 0xC0000000)};

    if (address >= 0xD0000000 && address < 0xD0000000 + MEM2_SIZE)
        return {mem2_.data(), static_cast<std::size_t>(address - 0xD0000000)};

    throw std::out_of_range("vWii: unmapped memory read");
}

std::pair<uint8_t*, std::size_t> Memory::TranslateMutable(uint32_t address) {
    if (address >= MEM1_BASE && address < MEM1_BASE + MEM1_SIZE)
        return {mem1_.data(), static_cast<std::size_t>(address - MEM1_BASE)};

    if (address >= MEM2_BASE && address < MEM2_BASE + MEM2_SIZE)
        return {mem2_.data(), static_cast<std::size_t>(address - MEM2_BASE)};

    if (address >= 0xC0000000 && address < 0xC0000000 + MEM1_SIZE)
        return {mem1_.data(), static_cast<std::size_t>(address - 0xC0000000)};

    if (address >= 0xD0000000 && address < 0xD0000000 + MEM2_SIZE)
        return {mem2_.data(), static_cast<std::size_t>(address - 0xD0000000)};

    throw std::out_of_range("vWii: unmapped memory write");
}

std::size_t Memory::RegionRemaining(uint32_t address) const {
    if (address >= MEM1_BASE && address < MEM1_BASE + MEM1_SIZE)
        return static_cast<std::size_t>(MEM1_BASE + MEM1_SIZE - address);

    if (address >= MEM2_BASE && address < MEM2_BASE + MEM2_SIZE)
        return static_cast<std::size_t>(MEM2_BASE + MEM2_SIZE - address);

    if (address >= 0xC0000000 && address < 0xC0000000 + MEM1_SIZE)
        return static_cast<std::size_t>(0xC0000000 + MEM1_SIZE - address);

    if (address >= 0xD0000000 && address < 0xD0000000 + MEM2_SIZE)
        return static_cast<std::size_t>(0xD0000000 + MEM2_SIZE - address);

    throw std::out_of_range("vWii: unmapped memory block access");
}

uint8_t Memory::Read8(uint32_t address) const {
    if (IsHollywoodRegister(address)) {
        const uint32_t value = hollywood_.Read32(HollywoodRegisterAddress(address & ~3u));
        const unsigned shift = 8 * (3 - (address & 3u));
        return static_cast<uint8_t>(value >> shift);
    }

    if (IsPeripheralRegister(address))
        return peripheral_regs_[PeripheralOffset(address)];

    const auto [base, offset] = Translate(address);
    return base[offset];
}

uint16_t Memory::Read16(uint32_t address) const {
    return static_cast<uint16_t>((Read8(address) << 8) | Read8(address + 1));
}

uint32_t Memory::Read32(uint32_t address) const {
    if (IsHollywoodRegister(address)) {
        if ((address & 3u) != 0)
            throw std::out_of_range("vWii: unaligned Hollywood 32-bit read");
        return hollywood_.Read32(HollywoodRegisterAddress(address));
    }

    return (static_cast<uint32_t>(Read8(address)) << 24) |
           (static_cast<uint32_t>(Read8(address + 1)) << 16) |
           (static_cast<uint32_t>(Read8(address + 2)) << 8) |
           static_cast<uint32_t>(Read8(address + 3));
}

void Memory::Write8(uint32_t address, uint8_t value) {
    if (IsHollywoodRegister(address)) {
        const uint32_t register_address = HollywoodRegisterAddress(address & ~3u);
        uint32_t current = hollywood_.Read32(register_address);
        const unsigned shift = 8 * (3 - (address & 3u));
        current = (current & ~(0xFFu << shift)) |
                  (static_cast<uint32_t>(value) << shift);
        hollywood_.Write32(register_address, current);
        return;
    }

    if (IsPeripheralRegister(address)) {
        peripheral_regs_[PeripheralOffset(address)] = value;
        return;
    }

    const auto [base, offset] = TranslateMutable(address);
    base[offset] = value;
}

void Memory::Write16(uint32_t address, uint16_t value) {
    Write8(address, static_cast<uint8_t>(value >> 8));
    Write8(address + 1, static_cast<uint8_t>(value));
}

void Memory::Write32(uint32_t address, uint32_t value) {
    if (IsHollywoodRegister(address)) {
        if ((address & 3u) != 0)
            throw std::out_of_range("vWii: unaligned Hollywood 32-bit write");
        hollywood_.Write32(HollywoodRegisterAddress(address), value);
        return;
    }

    Write8(address, static_cast<uint8_t>(value >> 24));
    Write8(address + 1, static_cast<uint8_t>(value >> 16));
    Write8(address + 2, static_cast<uint8_t>(value >> 8));
    Write8(address + 3, static_cast<uint8_t>(value));
}

void Memory::WriteBlock(uint32_t address, std::span<const uint8_t> data) {
    std::size_t written = 0;

    while (written < data.size()) {
        const uint32_t current = address + static_cast<uint32_t>(written);

        if (IsHollywoodRegister(current)) {
            Write8(current, data[written]);
            ++written;
            continue;
        }

        const auto [base, offset] = TranslateMutable(current);
        const std::size_t available = RegionRemaining(current);
        const std::size_t chunk = std::min(data.size() - written, available);

        std::memcpy(base + offset, data.data() + written, chunk);
        written += chunk;
    }
}

void Memory::Fill(uint32_t address, std::size_t size, uint8_t value) {
    std::size_t written = 0;

    while (written < size) {
        const uint32_t current = address + static_cast<uint32_t>(written);

        if (IsHollywoodRegister(current)) {
            Write8(current, value);
            ++written;
            continue;
        }

        const auto [base, offset] = TranslateMutable(current);
        const std::size_t available = RegionRemaining(current);
        const std::size_t chunk = std::min(size - written, available);

        std::memset(base + offset, value, chunk);
        written += chunk;
    }
}

} // namespace vwii::memory
