#include "memory/memory.h"

#include <algorithm>
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

} // namespace vwii::memory
