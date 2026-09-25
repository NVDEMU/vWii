#include "memory/memory.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace vwii::memory {

Memory::Memory()
    : mem1_(MEM1_SIZE, 0), mem2_(MEM2_SIZE, 0) {
}

void Memory::Reset() {
    std::fill(mem1_.begin(), mem1_.end(), 0);
    std::fill(mem2_.begin(), mem2_.end(), 0);
}

std::pair<const uint8_t*, std::size_t> Memory::Translate(uint32_t address) const {
    if (address >= MEM1_BASE && address < MEM1_BASE + MEM1_SIZE) {
        return {mem1_.data(), static_cast<std::size_t>(address - MEM1_BASE)};
    }

    if (address >= MEM2_BASE && address < MEM2_BASE + MEM2_SIZE) {
        return {mem2_.data(), static_cast<std::size_t>(address - MEM2_BASE)};
    }

    if (address >= 0xC0000000 && address < 0xC0000000 + MEM1_SIZE) {
        return {mem1_.data(), static_cast<std::size_t>(address - 0xC0000000)};
    }

    if (address >= 0xD0000000 && address < 0xD0000000 + MEM2_SIZE) {
        return {mem2_.data(), static_cast<std::size_t>(address - 0xD0000000)};
    }

    throw std::out_of_range("vWii: unmapped memory read");
}

std::pair<uint8_t*, std::size_t> Memory::TranslateMutable(uint32_t address) {
    if (address >= MEM1_BASE && address < MEM1_BASE + MEM1_SIZE) {
        return {mem1_.data(), static_cast<std::size_t>(address - MEM1_BASE)};
    }

    if (address >= MEM2_BASE && address < MEM2_BASE + MEM2_SIZE) {
        return {mem2_.data(), static_cast<std::size_t>(address - MEM2_BASE)};
    }

    if (address >= 0xC0000000 && address < 0xC0000000 + MEM1_SIZE) {
        return {mem1_.data(), static_cast<std::size_t>(address - 0xC0000000)};
    }

    if (address >= 0xD0000000 && address < 0xD0000000 + MEM2_SIZE) {
        return {mem2_.data(), static_cast<std::size_t>(address - 0xD0000000)};
    }

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
    const auto [base, offset] = Translate(address);
    return base[offset];
}

uint16_t Memory::Read16(uint32_t address) const {
    const uint16_t hi = Read8(address);
    const uint16_t lo = Read8(address + 1);
    return static_cast<uint16_t>((hi << 8) | lo);
}

uint32_t Memory::Read32(uint32_t address) const {
    const uint32_t b0 = Read8(address);
    const uint32_t b1 = Read8(address + 1);
    const uint32_t b2 = Read8(address + 2);
    const uint32_t b3 = Read8(address + 3);
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

void Memory::Write8(uint32_t address, uint8_t value) {
    const auto [base, offset] = TranslateMutable(address);
    base[offset] = value;
}

void Memory::Write16(uint32_t address, uint16_t value) {
    Write8(address, static_cast<uint8_t>(value >> 8));
    Write8(address + 1, static_cast<uint8_t>(value));
}

void Memory::Write32(uint32_t address, uint32_t value) {
    Write8(address, static_cast<uint8_t>(value >> 24));
    Write8(address + 1, static_cast<uint8_t>(value >> 16));
    Write8(address + 2, static_cast<uint8_t>(value >> 8));
    Write8(address + 3, static_cast<uint8_t>(value));
}

void Memory::WriteBlock(uint32_t address, std::span<const uint8_t> data) {
    std::size_t written = 0;

    while (written < data.size()) {
        const uint32_t current = address + static_cast<uint32_t>(written);
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
        const auto [base, offset] = TranslateMutable(current);
        const std::size_t available = RegionRemaining(current);
        const std::size_t chunk = std::min(size - written, available);

        std::memset(base + offset, value, chunk);
        written += chunk;
    }
}

} // namespace vwii::memory
