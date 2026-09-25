#include "cpu/powerpc.h"

#include "memory/memory.h"

#include <bit>
#include <cstdint>
#include <cmath>

namespace vwii::cpu {

namespace {

constexpr uint32_t Opcode(uint32_t instruction) { return instruction >> 26; }
constexpr unsigned RD(uint32_t instruction) { return (instruction >> 21) & 31; }
constexpr unsigned RA(uint32_t instruction) { return (instruction >> 16) & 31; }
constexpr unsigned RB(uint32_t instruction) { return (instruction >> 11) & 31; }
constexpr unsigned RS(uint32_t instruction) { return RD(instruction); }
constexpr int16_t SIMM(uint32_t instruction) { return static_cast<int16_t>(instruction); }
constexpr uint16_t UIMM(uint32_t instruction) { return static_cast<uint16_t>(instruction); }
constexpr unsigned XO(uint32_t instruction) { return (instruction >> 1) & 0x3FF; }
constexpr bool Rc(uint32_t instruction) { return (instruction & 1) != 0; }

constexpr unsigned SPR(uint32_t instruction) {
    return ((instruction >> 16) & 0x1F) |
           ((instruction >> 6) & 0x3E0);
}

constexpr unsigned SH(uint32_t instruction) { return (instruction >> 11) & 31; }
constexpr unsigned MB(uint32_t instruction) { return (instruction >> 6) & 31; }
constexpr unsigned ME(uint32_t instruction) { return (instruction >> 1) & 31; }
constexpr unsigned BO(uint32_t instruction) { return (instruction >> 21) & 31; }
constexpr unsigned BI(uint32_t instruction) { return (instruction >> 16) & 31; }

constexpr int32_t SignExtend(uint32_t value, unsigned bits) {
    const uint32_t shift = 32 - bits;
    return static_cast<int32_t>(value << shift) >> shift;
}

constexpr int32_t BranchDisp(uint32_t instruction) {
    return SignExtend((instruction >> 2) & 0x00FFFFFF, 24) << 2;
}

constexpr uint32_t RotateLeft(uint32_t value, unsigned shift) {
    return (value << shift) | (value >> ((32 - shift) & 31));
}

constexpr uint32_t ByteSwap32(uint32_t value) {
    return (value >> 24) |
           ((value >> 8) & 0x0000FF00u) |
           ((value << 8) & 0x00FF0000u) |
           (value << 24);
}

uint32_t Mask32(unsigned mb, unsigned me) {
    uint32_t mask = 0;
    for (unsigned bit = mb;; bit = (bit + 1) & 31) {
        mask |= 1u << (31 - bit);
        if (bit == me)
            break;
    }
    return mask;
}

double FPRDouble(uint64_t value) {
    return std::bit_cast<double>(value);
}

uint64_t MakeFPR(double value) {
    return std::bit_cast<uint64_t>(value);
}

uint64_t MakeFPR(float value) {
    return MakeFPR(static_cast<double>(value));
}

} // namespace

PowerPC::PowerPC(memory::Memory& memory)
    : memory_(memory) {
    Reset();
}

void PowerPC::Reset(uint32_t entry_point) {
    gpr_.fill(0);
    fpr_.fill(0);

    pc_ = entry_point;
    lr_ = 0;
    ctr_ = 0;
    xer_ = 0;
    cr_ = 0;
    msr_ = 0;
    srr0_ = 0;
    srr1_ = 0;
    fpscr_ = 0;
    timebase_ = 0;
    halted_ = false;
}

uint32_t PowerPC::ReadBaseRegister(unsigned index) const {
    return index == 0 ? 0 : gpr_[index & 31];
}

unsigned PowerPC::GetCRBit(unsigned bit) const {
    return (cr_ >> (31 - bit)) & 1;
}

void PowerPC::SetCRField(unsigned field, unsigned value) {
    const unsigned shift = 28 - field * 4;
    cr_ = (cr_ & ~(0xFu << shift)) |
          ((value & 0xF) << shift);
}

void PowerPC::SetCR0FromResult(uint32_t value) {
    if (value == 0)
        SetCRField(0, 2);
    else if (value & 0x80000000)
        SetCRField(0, 8);
    else
        SetCRField(0, 4);
}

bool PowerPC::ConditionBit(unsigned bo, unsigned bi) const {
    const bool condition = GetCRBit(bi) != 0;

    if ((bo & 4) == 0) {
        --ctr_;
        const bool ctr_zero = ctr_ == 0;
        const bool branch_if_ctr_zero = (bo & 2) != 0;
        if (branch_if_ctr_zero != ctr_zero)
            return false;
    }

    const bool branch_if_true = (bo & 16) != 0;
    return branch_if_true ? condition : !condition;
}

uint32_t PowerPC::ReadSPR(unsigned spr) const {
    switch (spr) {
    case 1:  return xer_;
    case 8:  return lr_;
    case 9:  return ctr_;
    case 26: return srr0_;
    case 27: return srr1_;
    default: return 0;
    }
}

void PowerPC::WriteSPR(unsigned spr, uint32_t value) {
    switch (spr) {
    case 1:  xer_ = value; break;
    case 8:  lr_ = value; break;
    case 9:  ctr_ = value; break;
    case 26: srr0_ = value; break;
    case 27: srr1_ = value; break;
    default: break;
    }
}

void PowerPC::RaiseException(uint32_t vector, uint32_t reason) {
    srr0_ = pc_;
    srr1_ = msr_ | reason;
    msr_ &= ~(MSR_EE | MSR_PR | MSR_IR | MSR_DR);
    pc_ = 0x80000000u + vector;
}

void PowerPC::Step() {
    if (halted_)
        return;

    if ((msr_ & MSR_EE) && memory_.ExternalInterruptPending()) {
        RaiseException(0x500, 0x00008000);
        return;
    }

    const uint32_t cia = pc_;
    try {
        const uint32_t instruction = memory_.Read32(cia);
        pc_ += 4;
        Execute(instruction, cia);
    } catch (...) {
        RaiseException(0x300, 0);
    }

    ++timebase_;
}

void PowerPC::Execute(uint32_t instruction, uint32_t cia) {
    switch (Opcode(instruction)) {
    case 10: { // CMPLI
        const unsigned bf = (instruction >> 23) & 7;
        const uint32_t lhs = ReadBaseRegister(RA(instruction));
        const uint32_t rhs = UIMM(instruction);

        SetCRField(bf, lhs < rhs ? 8 : lhs == rhs ? 2 : 4);
        break;
    }

    case 11: { // CMPI
        const unsigned bf = (instruction >> 23) & 7;
        const int32_t lhs = static_cast<int32_t>(ReadBaseRegister(RA(instruction)));
        const int32_t rhs = SIMM(instruction);

        SetCRField(bf, lhs < rhs ? 8 : lhs == rhs ? 2 : 4);
        break;
    }

    case 7: { // MULLI
        const int32_t result =
            static_cast<int32_t>(ReadBaseRegister(RA(instruction))) *
            static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = static_cast<uint32_t>(result);
        break;
    }

    case 8: { // SUBFIC
        const int32_t result =
            static_cast<int32_t>(SIMM(instruction)) -
            static_cast<int32_t>(ReadBaseRegister(RA(instruction)));
        gpr_[RD(instruction)] = static_cast<uint32_t>(result);
        break;
    }

    case 12: { // ADDIC
        const uint32_t result =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = result;
        break;
    }

    case 13: { // ADDIC.
        const uint32_t result =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = result;
        SetCR0FromResult(result);
        break;
    }

    case 14: { // ADDI
        gpr_[RD(instruction)] =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        break;
    }

    case 15: { // ADDIS
        gpr_[RD(instruction)] =
            ReadBaseRegister(RA(instruction)) +
            (static_cast<uint32_t>(static_cast<int32_t>(SIMM(instruction))) << 16);
        break;
    }

    case 16: { // BC
        const bool taken = ConditionBit(BO(instruction), BI(instruction));
        const int32_t displacement =
            SignExtend((instruction >> 2) & 0x3FFF, 14) << 2;
        const bool absolute = (instruction & 2) != 0;

        if (instruction & 1)
            lr_ = pc_;

        if (taken)
            pc_ = absolute ? static_cast<uint32_t>(displacement)
                           : static_cast<uint32_t>(cia + displacement);
        break;
    }

    case 17: // SC
        RaiseException(0xC00, 0);
        break;

    case 18: { // B / BL
        const int32_t displacement = BranchDisp(instruction);
        const bool absolute = (instruction & 2) != 0;

        if (instruction & 1)
            lr_ = pc_;

        pc_ = absolute ? static_cast<uint32_t>(displacement)
                       : static_cast<uint32_t>(cia + displacement);
        break;
    }

    case 19: {
        const unsigned xo = XO(instruction);

        if (xo == 16) { // BCLR
            const bool taken = ConditionBit(BO(instruction), BI(instruction));
            const uint32_t target = lr_ & ~3u;

            if (taken)
                pc_ = target;
            break;
        }

        if (xo == 50) { // RFI
            msr_ = srr1_;
            pc_ = srr0_;
            break;
        }

        if (xo == 150) { // ISYNC
            break;
        }

        if (xo == 528) { // BCCTR
            const bool taken = ConditionBit(BO(instruction), BI(instruction));
            if (taken)
                pc_ = ctr_ & ~3u;
            break;
        }

        RaiseException(0x700, 0);
        break;
    }

    case 20: { // RLWIMI
        const unsigned rs = RS(instruction);
        const unsigned ra = RA(instruction);
        const uint32_t mask = Mask32(MB(instruction), ME(instruction));
        const uint32_t rotated = RotateLeft(gpr_[rs], SH(instruction));
        gpr_[ra] = (gpr_[ra] & ~mask) | (rotated & mask);
        if (Rc(instruction))
            SetCR0FromResult(gpr_[ra]);
        break;
    }

    case 21: { // RLWINM
        gpr_[RA(instruction)] =
            RotateLeft(gpr_[RS(instruction)], SH(instruction)) &
            Mask32(MB(instruction), ME(instruction));

        if (Rc(instruction))
            SetCR0FromResult(gpr_[RA(instruction)]);
        break;
    }

    case 23: { // RLWNM
        const unsigned rs = RS(instruction);
        const unsigned ra = RA(instruction);
        const unsigned shift = gpr_[RB(instruction)] & 31u;
        gpr_[ra] =
            RotateLeft(gpr_[rs], shift) &
            Mask32(MB(instruction), ME(instruction));
        if (Rc(instruction))
            SetCR0FromResult(gpr_[ra]);
        break;
    }

    case 24: { // ORI
        gpr_[RD(instruction)] =
            gpr_[RA(instruction)] | UIMM(instruction);
        break;
    }

    case 25: { // ORIS
        gpr_[RD(instruction)] =
            gpr_[RA(instruction)] |
            (static_cast<uint32_t>(UIMM(instruction)) << 16);
        break;
    }

    case 26: { // XORI
        gpr_[RD(instruction)] =
            gpr_[RA(instruction)] ^ UIMM(instruction);
        break;
    }

    case 27: { // XORIS
        gpr_[RD(instruction)] =
            gpr_[RA(instruction)] ^
            (static_cast<uint32_t>(UIMM(instruction)) << 16);
        break;
    }

    case 28: { // ANDI.
        gpr_[RD(instruction)] =
            gpr_[RA(instruction)] & UIMM(instruction);
        SetCR0FromResult(gpr_[RD(instruction)]);
        break;
    }

    case 29: { // ANDIS.
        gpr_[RD(instruction)] =
            gpr_[RA(instruction)] &
            (static_cast<uint32_t>(UIMM(instruction)) << 16);
        SetCR0FromResult(gpr_[RD(instruction)]);
        break;
    }

    case 46: { // LMW
        const unsigned rd = RD(instruction);
        uint32_t address =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));

        for (unsigned reg = rd; reg < 32; ++reg) {
            gpr_[reg] = memory_.Read32(address);
            address += 4;
        }
        break;
    }

    case 47: { // STMW
        const unsigned rs = RS(instruction);
        uint32_t address =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));

        for (unsigned reg = rs; reg < 32; ++reg) {
            memory_.Write32(address, gpr_[reg]);
            address += 4;
        }
        break;
    }

    case 31: {
        const unsigned ra = RA(instruction);
        const unsigned rb = RB(instruction);

        switch (XO(instruction)) {
        case 0: { // CMPW
            const unsigned bf = (instruction >> 23) & 7;
            const int32_t lhs = static_cast<int32_t>(gpr_[ra]);
            const int32_t rhs = static_cast<int32_t>(gpr_[rb]);
            SetCRField(bf, lhs < rhs ? 8 : lhs == rhs ? 2 : 4);
            break;
        }

        case 11: { // MULHWU
            const uint64_t result =
                static_cast<uint64_t>(gpr_[ra]) * gpr_[rb];
            gpr_[RD(instruction)] = static_cast<uint32_t>(result >> 32);
            break;
        }

        case 19: // MFCR
            gpr_[RD(instruction)] = cr_;
            break;

        case 8: { // SUBFC
            const uint32_t result = gpr_[rb] - gpr_[ra];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 10: { // ADDC
            const uint32_t result = gpr_[ra] + gpr_[rb];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 20: { // LWARX
            const uint32_t address = ReadBaseRegister(ra) +
                                     ReadBaseRegister(rb);
            gpr_[RD(instruction)] = memory_.Read32(address);
            break;
        }

        case 21: { // LZX? reserved
            RaiseException(0x700, 0);
            break;
        }

        case 23: { // LWZX
            const uint32_t address = ReadBaseRegister(ra) +
                                     ReadBaseRegister(rb);
            gpr_[RD(instruction)] = memory_.Read32(address);
            break;
        }

        case 26: { // CNTLZW
            const uint32_t value = gpr_[RS(instruction)];
            gpr_[RD(instruction)] = value == 0 ? 32u : static_cast<uint32_t>(std::countl_zero(value));
            if (Rc(instruction))
                SetCR0FromResult(gpr_[RD(instruction)]);
            break;
        }

        case 24: { // SLW
            const unsigned shift = gpr_[rb] & 63;
            gpr_[RD(instruction)] =
                shift >= 32 ? 0 : gpr_[RS(instruction)] << shift;
            if (Rc(instruction))
                SetCR0FromResult(gpr_[RD(instruction)]);
            break;
        }

        case 28: { // AND
            const uint32_t result = gpr_[RS(instruction)] & gpr_[rb];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 40: { // SUBF
            const uint32_t result = gpr_[rb] - gpr_[ra];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 75: { // MULHW
            const int64_t result =
                static_cast<int64_t>(static_cast<int32_t>(gpr_[ra])) *
                static_cast<int64_t>(static_cast<int32_t>(gpr_[rb]));
            gpr_[RD(instruction)] = static_cast<uint32_t>(result >> 32);
            break;
        }

        case 104: { // NEG
            const uint32_t result = 0u - gpr_[ra];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 235: { // MULLW
            const uint32_t result = gpr_[ra] * gpr_[rb];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 266: { // ADD
            const uint32_t result = gpr_[ra] + gpr_[rb];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 279: { // LHZX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            gpr_[RD(instruction)] = memory_.Read16(address);
            break;
        }

        case 343: { // LHAX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            gpr_[RD(instruction)] = static_cast<uint32_t>(
                static_cast<int32_t>(static_cast<int16_t>(memory_.Read16(address))));
            break;
        }

        case 87: { // LBZX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            gpr_[RD(instruction)] = memory_.Read8(address);
            break;
        }

        case 534: { // LWBRX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            const uint32_t value = memory_.Read32(address);
            gpr_[RD(instruction)] = ByteSwap32(value);
            break;
        }

        case 790: { // LHBRX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            const uint16_t value = memory_.Read16(address);
            gpr_[RD(instruction)] =
                static_cast<uint32_t>((value >> 8) | (value << 8));
            break;
        }

        case 151: { // STWX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            memory_.Write32(address, gpr_[RS(instruction)]);
            break;
        }

        case 662: { // STWBRX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            memory_.Write32(address, ByteSwap32(gpr_[RS(instruction)]));
            break;
        }

        case 918: { // STHBRX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            const uint16_t value = static_cast<uint16_t>(gpr_[RS(instruction)]);
            memory_.Write16(address, static_cast<uint16_t>((value >> 8) | (value << 8)));
            break;
        }

        case 316: { // XOR
            const uint32_t result = gpr_[RS(instruction)] ^ gpr_[rb];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 339: // MFSPR
            gpr_[RD(instruction)] = ReadSPR(SPR(instruction));
            break;

        case 467: // MTSPR
            WriteSPR(SPR(instruction), gpr_[RS(instruction)]);
            break;

        case 536: { // SRW
            const unsigned shift = gpr_[rb] & 63;
            gpr_[RD(instruction)] =
                shift >= 32 ? 0 : gpr_[RS(instruction)] >> shift;
            if (Rc(instruction))
                SetCR0FromResult(gpr_[RD(instruction)]);
            break;
        }


        case 150: { // STWCX.
            const uint32_t address = ReadBaseRegister(ra) +
                                     ReadBaseRegister(rb);
            memory_.Write32(address, gpr_[RS(instruction)]);
            SetCRField(0, 2);
            break;
        }

        case 83: // MFMSR
            gpr_[RD(instruction)] = msr_;
            break;

        case 146: // MTMSR
            msr_ = gpr_[RS(instruction)];
            break;

        default:
            RaiseException(0x700, 0);
            break;
        }
        break;
    }

    case 32: { // LWZ
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = memory_.Read32(address);
        break;
    }

    case 33: { // LWZU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = memory_.Read32(address);
        gpr_[ra] = address;
        break;
    }

    case 34: { // LBZ
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = memory_.Read8(address);
        break;
    }

    case 35: { // LBZU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = memory_.Read8(address);
        gpr_[ra] = address;
        break;
    }

    case 36: { // STW
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        memory_.Write32(address, gpr_[RS(instruction)]);
        break;
    }

    case 37: { // STWU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        memory_.Write32(address, gpr_[RS(instruction)]);
        gpr_[ra] = address;
        break;
    }

    case 38: { // STB
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        memory_.Write8(address, static_cast<uint8_t>(gpr_[RS(instruction)]));
        break;
    }

    case 39: { // STBU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        memory_.Write8(address, static_cast<uint8_t>(gpr_[RS(instruction)]));
        gpr_[ra] = address;
        break;
    }

    case 40: { // STH
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        memory_.Write16(address, static_cast<uint16_t>(gpr_[RS(instruction)]));
        break;
    }

    case 41: { // STHU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        memory_.Write16(address, static_cast<uint16_t>(gpr_[RS(instruction)]));
        gpr_[ra] = address;
        break;
    }

    case 42: { // LHZ
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = memory_.Read16(address);
        break;
    }

    case 43: { // LHZU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        gpr_[RD(instruction)] = memory_.Read16(address);
        gpr_[ra] = address;
        break;
    }

    case 48: { // LFS
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        const float value =
            std::bit_cast<float>(memory_.Read32(address));
        fpr_[RD(instruction)] = MakeFPR(value);
        break;
    }

    case 49: { // LFSU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        const float value =
            std::bit_cast<float>(memory_.Read32(address));
        fpr_[RD(instruction)] = MakeFPR(value);
        gpr_[ra] = address;
        break;
    }

    case 50: { // LFD
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        fpr_[RD(instruction)] = memory_.Read32(address);
        fpr_[RD(instruction)] <<= 32;
        fpr_[RD(instruction)] |= memory_.Read32(address + 4);
        break;
    }

    case 51: { // LFDU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        fpr_[RD(instruction)] = static_cast<uint64_t>(memory_.Read32(address)) << 32 |
                                memory_.Read32(address + 4);
        gpr_[ra] = address;
        break;
    }

    case 52: { // STFS
        const double value = FPRDouble(fpr_[RS(instruction)]);
        memory_.Write32(
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction)),
            std::bit_cast<uint32_t>(static_cast<float>(value)));
        break;
    }

    case 53: { // STFSU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        const double value = FPRDouble(fpr_[RS(instruction)]);
        memory_.Write32(address, std::bit_cast<uint32_t>(
            static_cast<float>(value)));
        gpr_[ra] = address;
        break;
    }

    case 54: { // STFD
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) +
            static_cast<int32_t>(SIMM(instruction));
        const uint64_t value = fpr_[RS(instruction)];
        memory_.Write32(address, static_cast<uint32_t>(value >> 32));
        memory_.Write32(address + 4, static_cast<uint32_t>(value));
        break;
    }

    case 55: { // STFDU
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) +
            static_cast<int32_t>(SIMM(instruction));
        const uint64_t value = fpr_[RS(instruction)];
        memory_.Write32(address, static_cast<uint32_t>(value >> 32));
        memory_.Write32(address + 4, static_cast<uint32_t>(value));
        gpr_[ra] = address;
        break;
    }

    case 59: { // Single-precision A-form arithmetic
        const unsigned fd = RD(instruction);
        const unsigned fa = RA(instruction);
        const unsigned fb = RB(instruction);
        const unsigned xo = XO(instruction);

        const float a = static_cast<float>(FPRDouble(fpr_[fa]));
        const float b = static_cast<float>(FPRDouble(fpr_[fb]));

        float result = 0.0f;
        switch (xo) {
        case 18: result = a / b; break; // FDIVS
        case 20: result = a - b; break; // FSUBS
        case 21: result = a + b; break; // FADDS
        case 25: result = a * b; break; // FMULS
        default:
            RaiseException(0x700, 0);
            break;
        }

        if (xo == 18 || xo == 20 || xo == 21 || xo == 25)
            fpr_[fd] = MakeFPR(result);
        break;
    }

    case 63: { // Double-precision arithmetic/control
        const unsigned fd = RD(instruction);
        const unsigned fa = RA(instruction);
        const unsigned fb = RB(instruction);
        const unsigned xo = XO(instruction);

        switch (xo) {
        case 18: // FDIV
            fpr_[fd] = MakeFPR(FPRDouble(fpr_[fa]) / FPRDouble(fpr_[fb]));
            break;
        case 20: // FSUB
            fpr_[fd] = MakeFPR(FPRDouble(fpr_[fa]) - FPRDouble(fpr_[fb]));
            break;
        case 21: // FADD
            fpr_[fd] = MakeFPR(FPRDouble(fpr_[fa]) + FPRDouble(fpr_[fb]));
            break;
        case 25: // FMUL
            fpr_[fd] = MakeFPR(FPRDouble(fpr_[fa]) * FPRDouble(fpr_[fb]));
            break;
        case 40: // FNEG
            fpr_[fd] = MakeFPR(-FPRDouble(fpr_[fb]));
            break;
        case 72: // FMR
            fpr_[fd] = fpr_[fb];
            break;
        case 136: // FNABS
            fpr_[fd] = MakeFPR(-std::fabs(FPRDouble(fpr_[fb])));
            break;
        case 264: // FABS
            fpr_[fd] = MakeFPR(std::fabs(FPRDouble(fpr_[fb])));
            break;
        case 15: { // FCTIWZ
            const int32_t integer = static_cast<int32_t>(
                FPRDouble(fpr_[fb]));
            fpr_[fd] = static_cast<uint64_t>(
                static_cast<uint32_t>(integer)) << 32;
            break;
        }
        case 32: { // FCMPO
            const double a = FPRDouble(fpr_[fa]);
            const double b = FPRDouble(fpr_[fb]);
            const unsigned bf = (instruction >> 23) & 7;

            if (std::isnan(a) || std::isnan(b))
                SetCRField(bf, 1);
            else if (a < b)
                SetCRField(bf, 8);
            else if (a == b)
                SetCRField(bf, 2);
            else
                SetCRField(bf, 4);
            break;
        }
        default:
            RaiseException(0x700, 0);
            break;
        }
        break;
    }

    default:
        RaiseException(0x700, 0);
        break;
    }
}

} // namespace vwii::cpu
