#include "cpu/powerpc.h"

#include "memory/memory.h"

#include <cstdint>
#include <stdexcept>

namespace vwii::cpu {

namespace {

constexpr uint32_t Opcode(uint32_t instruction) {
    return instruction >> 26;
}

constexpr unsigned RD(uint32_t instruction) {
    return static_cast<unsigned>((instruction >> 21) & 0x1F);
}

constexpr unsigned RA(uint32_t instruction) {
    return static_cast<unsigned>((instruction >> 16) & 0x1F);
}

constexpr unsigned RB(uint32_t instruction) {
    return static_cast<unsigned>((instruction >> 11) & 0x1F);
}

constexpr int16_t SIMM(uint32_t instruction) {
    return static_cast<int16_t>(instruction & 0xFFFF);
}

constexpr uint16_t UIMM(uint32_t instruction) {
    return static_cast<uint16_t>(instruction & 0xFFFF);
}

constexpr unsigned XO(uint32_t instruction) {
    return static_cast<unsigned>((instruction >> 1) & 0x3FF);
}

constexpr bool Rc(uint32_t instruction) {
    return (instruction & 1U) != 0;
}

constexpr int32_t SignExtend24(uint32_t value) {
    const uint32_t extended = value & 0x00FFFFFFU;
    return static_cast<int32_t>(extended << 8) >> 8;
}

} // namespace

PowerPC::PowerPC(memory::Memory& memory)
    : memory_(memory) {
    Reset();
}

void PowerPC::Reset(uint32_t entry_point) {
    gpr_.fill(0);
    pc_ = entry_point;
    lr_ = 0;
    ctr_ = 0;
    xer_ = 0;
    cr_ = 0;
    halted_ = false;
}

uint32_t PowerPC::ReadBaseRegister(unsigned index) const {
    return index == 0 ? 0 : gpr_[index & 31U];
}

void PowerPC::SetCR0FromResult(uint32_t value) {
    cr_ &= 0x0FFFFFFFU;

    if (value == 0) {
        cr_ |= 1U << 29;
    } else if ((value & 0x80000000U) != 0) {
        cr_ |= 1U << 28;
    } else {
        cr_ |= 1U << 30;
    }
}

void PowerPC::Step() {
    if (halted_) {
        return;
    }

    const uint32_t cia = pc_;
    const uint32_t instruction = memory_.Read32(cia);
    pc_ += 4;

    Execute(instruction, cia);
}

void PowerPC::Execute(uint32_t instruction, uint32_t cia) {
    switch (Opcode(instruction)) {
    case 14: { // ADDI
        const unsigned rd = RD(instruction);
        const uint32_t a = ReadBaseRegister(RA(instruction));
        gpr_[rd] = a + static_cast<int32_t>(SIMM(instruction));
        break;
    }

    case 15: { // ADDIS
        const unsigned rd = RD(instruction);
        const uint32_t a = ReadBaseRegister(RA(instruction));
        const uint32_t immediate = static_cast<uint32_t>(static_cast<int32_t>(SIMM(instruction))) << 16;
        gpr_[rd] = a + immediate;
        break;
    }

    case 18: { // B / BL
        const int32_t displacement = SignExtend24((instruction >> 2) & 0x00FFFFFFU);
        const bool absolute = (instruction & 2U) != 0;
        const bool link = (instruction & 1U) != 0;

        if (link) {
            lr_ = pc_;
        }

        const uint32_t target_offset = static_cast<uint32_t>(displacement << 2);
        pc_ = absolute ? target_offset : static_cast<uint32_t>(cia + target_offset);
        break;
    }

    case 32: { // LWZ
        const unsigned rd = RD(instruction);
        const uint32_t base = ReadBaseRegister(RA(instruction));
        const uint32_t address = base + static_cast<int32_t>(SIMM(instruction));
        gpr_[rd] = memory_.Read32(address);
        break;
    }

    case 36: { // STW
        const unsigned rs = RD(instruction);
        const uint32_t base = ReadBaseRegister(RA(instruction));
        const uint32_t address = base + static_cast<int32_t>(SIMM(instruction));
        memory_.Write32(address, gpr_[rs]);
        break;
    }

    case 24: { // ORI
        const unsigned ra = RD(instruction);
        const unsigned rs = RA(instruction);
        gpr_[ra] = gpr_[rs] | UIMM(instruction);
        break;
    }

    case 25: { // ORIS
        const unsigned ra = RD(instruction);
        const unsigned rs = RA(instruction);
        gpr_[ra] = gpr_[rs] | (static_cast<uint32_t>(UIMM(instruction)) << 16);
        break;
    }

    case 21: { // RLWINM
        const unsigned rs = RD(instruction);
        const unsigned ra = RA(instruction);
        const unsigned sh = (instruction >> 11) & 31U;
        const unsigned mb = (instruction >> 6) & 31U;
        const unsigned me = (instruction >> 1) & 31U;

        const uint32_t source = gpr_[rs];
        const uint32_t rotated = (source << sh) | (source >> ((32U - sh) & 31U));

        uint32_t mask = 0;
        for (unsigned bit = mb;; bit = (bit + 1U) & 31U) {
            mask |= 1U << (31U - bit);
            if (bit == me) {
                break;
            }
        }

        gpr_[ra] = rotated & mask;
        if (Rc(instruction)) {
            SetCR0FromResult(gpr_[ra]);
        }
        break;
    }

    case 31: {
        switch (XO(instruction)) {
        case 266: { // ADD
            const unsigned rd = RD(instruction);
            const uint32_t result = gpr_[RA(instruction)] + gpr_[RB(instruction)];
            gpr_[rd] = result;
            if (Rc(instruction)) {
                SetCR0FromResult(result);
            }
            break;
        }

        case 40: { // SUBF
            const unsigned rd = RD(instruction);
            const uint32_t result = gpr_[RB(instruction)] - gpr_[RA(instruction)];
            gpr_[rd] = result;
            if (Rc(instruction)) {
                SetCR0FromResult(result);
            }
            break;
        }

        default:
            throw std::runtime_error("vWii PowerPC: unsupported X-form opcode");
        }
        break;
    }

    default:
        throw std::runtime_error("vWii PowerPC: unsupported instruction");
    }
}

} // namespace vwii::cpu
