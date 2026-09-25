#include "cpu/powerpc.h"

#include "memory/memory.h"

#include <cstdint>
#include <stdexcept>

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

uint32_t Mask32(unsigned mb, unsigned me) {
    uint32_t mask = 0;
    for (unsigned bit = mb;; bit = (bit + 1) & 31) {
        mask |= 1u << (31 - bit);
        if (bit == me)
            break;
    }
    return mask;
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
    msr_ = 0;
    srr0_ = 0;
    srr1_ = 0;
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
    unsigned field = 0;
    if (value == 0)
        SetCRField(field, 2); // EQ
    else if (value & 0x80000000)
        SetCRField(field, 8); // LT
    else
        SetCRField(field, 4); // GT
}

bool PowerPC::ConditionBit(unsigned bo, unsigned bi) const {
    const bool condition = GetCRBit(bi) != 0;
    const bool decrement_ctr = (bo & 4) == 0;
    const bool branch_if_ctr_zero = (bo & 2) != 0;

    if (decrement_ctr) {
        --ctr_;
        const bool ctr_zero = ctr_ == 0;
        if (branch_if_ctr_zero != ctr_zero)
            return false;
    }

    const bool branch_if_true = (bo & 16) != 0;
    return branch_if_true ? condition : !condition;
}

uint32_t PowerPC::ReadSPR(unsigned spr) const {
    switch (spr) {
    case 1:   return xer_;
    case 8:   return lr_;
    case 9:   return ctr_;
    case 26:  return srr0_;
    case 27:  return srr1_;
    case 287: return msr_;
    default:  return 0;
    }
}

void PowerPC::WriteSPR(unsigned spr, uint32_t value) {
    switch (spr) {
    case 1:   xer_ = value; break;
    case 8:   lr_ = value; break;
    case 9:   ctr_ = value; break;
    case 26:  srr0_ = value; break;
    case 27:  srr1_ = value; break;
    case 287: msr_ = value; break;
    default: break;
    }
}

void PowerPC::RaiseException(uint32_t vector, uint32_t reason) {
    srr0_ = pc_;
    srr1_ = msr_ | reason;
    msr_ &= ~(MSR_EE | MSR_PR | MSR_IR | MSR_DR);
    pc_ = vector;
}

void PowerPC::Step() {
    if (halted_)
        return;

    if ((msr_ & MSR_EE) && memory_.ExternalInterruptPending()) {
        RaiseException(0x500, 0x00008000);
        return;
    }

    const uint32_t cia = pc_;
    const uint32_t instruction = memory_.Read32(cia);
    pc_ += 4;
    Execute(instruction, cia);
}

void PowerPC::Execute(uint32_t instruction, uint32_t cia) {
    switch (Opcode(instruction)) {
    case 10: { // CMPLI
        const unsigned bf = (instruction >> 23) & 7;
        const unsigned ra = RA(instruction);
        const uint32_t lhs = ReadBaseRegister(ra);
        const uint32_t rhs = UIMM(instruction);

        unsigned result = 0;
        if (lhs < rhs) result = 8;
        else if (lhs == rhs) result = 2;
        else result = 4;
        SetCRField(bf, result);
        break;
    }

    case 11: { // CMPI signed
        const unsigned bf = (instruction >> 23) & 7;
        const int32_t lhs = static_cast<int32_t>(ReadBaseRegister(RA(instruction)));
        const int32_t rhs = SIMM(instruction);

        unsigned result = 0;
        if (lhs < rhs) result = 8;
        else if (lhs == rhs) result = 2;
        else result = 4;
        SetCRField(bf, result);
        break;
    }

    case 14: { // ADDI
        const unsigned rd = RD(instruction);
        const uint32_t result =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        gpr_[rd] = result;
        break;
    }

    case 15: { // ADDIS
        const unsigned rd = RD(instruction);
        const uint32_t result =
            ReadBaseRegister(RA(instruction)) +
            (static_cast<uint32_t>(static_cast<int32_t>(SIMM(instruction))) << 16);
        gpr_[rd] = result;
        break;
    }

    case 16: { // BC
        const bool taken = ConditionBit(BO(instruction), BI(instruction));
        const int32_t displacement =
            SignExtend((instruction >> 2) & 0x3FFF, 14) << 2;
        const bool absolute = (instruction & 2) != 0;
        const bool link = (instruction & 1) != 0;

        if (link)
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
        const bool link = (instruction & 1) != 0;

        if (link)
            lr_ = pc_;

        pc_ = absolute ? static_cast<uint32_t>(displacement)
                       : static_cast<uint32_t>(cia + displacement);
        break;
    }

    case 21: { // RLWINM
        const unsigned rs = RS(instruction);
        const unsigned ra = RA(instruction);
        gpr_[ra] = RotateLeft(gpr_[rs], SH(instruction)) &
                   Mask32(MB(instruction), ME(instruction));

        if (Rc(instruction))
            SetCR0FromResult(gpr_[ra]);
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

    case 26: { // XORI
        const unsigned ra = RD(instruction);
        const unsigned rs = RA(instruction);
        gpr_[ra] = gpr_[rs] ^ UIMM(instruction);
        break;
    }

    case 27: { // XORIS
        const unsigned ra = RD(instruction);
        const unsigned rs = RA(instruction);
        gpr_[ra] = gpr_[rs] ^ (static_cast<uint32_t>(UIMM(instruction)) << 16);
        break;
    }

    case 28: { // ANDI.
        const unsigned ra = RD(instruction);
        const unsigned rs = RA(instruction);
        gpr_[ra] = gpr_[rs] & UIMM(instruction);
        SetCR0FromResult(gpr_[ra]);
        break;
    }

    case 29: { // ANDIS.
        const unsigned ra = RD(instruction);
        const unsigned rs = RA(instruction);
        gpr_[ra] = gpr_[rs] & (static_cast<uint32_t>(UIMM(instruction)) << 16);
        SetCR0FromResult(gpr_[ra]);
        break;
    }

    case 32: { // LWZ
        const unsigned rd = RD(instruction);
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        gpr_[rd] = memory_.Read32(address);
        break;
    }

    case 33: { // LWZU
        const unsigned rd = RD(instruction);
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) + static_cast<int32_t>(SIMM(instruction));
        gpr_[rd] = memory_.Read32(address);
        gpr_[ra] = address;
        break;
    }

    case 34: { // LBZ
        const unsigned rd = RD(instruction);
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        gpr_[rd] = memory_.Read8(address);
        break;
    }

    case 35: { // LBZU
        const unsigned rd = RD(instruction);
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) + static_cast<int32_t>(SIMM(instruction));
        gpr_[rd] = memory_.Read8(address);
        gpr_[ra] = address;
        break;
    }

    case 36: { // STW
        const unsigned rs = RS(instruction);
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        memory_.Write32(address, gpr_[rs]);
        break;
    }

    case 37: { // STWU
        const unsigned rs = RS(instruction);
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) + static_cast<int32_t>(SIMM(instruction));
        memory_.Write32(address, gpr_[rs]);
        gpr_[ra] = address;
        break;
    }

    case 38: { // STB
        const unsigned rs = RS(instruction);
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        memory_.Write8(address, static_cast<uint8_t>(gpr_[rs]));
        break;
    }

    case 39: { // STBU
        const unsigned rs = RS(instruction);
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) + static_cast<int32_t>(SIMM(instruction));
        memory_.Write8(address, static_cast<uint8_t>(gpr_[rs]));
        gpr_[ra] = address;
        break;
    }

    case 40: { // STH
        const unsigned rs = RS(instruction);
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        memory_.Write16(address, static_cast<uint16_t>(gpr_[rs]));
        break;
    }

    case 41: { // STHU
        const unsigned rs = RS(instruction);
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) + static_cast<int32_t>(SIMM(instruction));
        memory_.Write16(address, static_cast<uint16_t>(gpr_[rs]));
        gpr_[ra] = address;
        break;
    }

    case 42: { // LHZ
        const unsigned rd = RD(instruction);
        const uint32_t address =
            ReadBaseRegister(RA(instruction)) + static_cast<int32_t>(SIMM(instruction));
        gpr_[rd] = memory_.Read16(address);
        break;
    }

    case 43: { // LHZU
        const unsigned rd = RD(instruction);
        const unsigned ra = RA(instruction);
        const uint32_t address =
            ReadBaseRegister(ra) + static_cast<int32_t>(SIMM(instruction));
        gpr_[rd] = memory_.Read16(address);
        gpr_[ra] = address;
        break;
    }

    case 31: {
        const unsigned ra = RA(instruction);
        const unsigned rb = RB(instruction);

        switch (XO(instruction)) {
        case 19: // MFCR
            gpr_[RD(instruction)] = cr_;
            break;

        case 20: { // LWARX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            gpr_[RD(instruction)] = memory_.Read32(address);
            break;
        }

        case 23: { // LWZX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            gpr_[RD(instruction)] = memory_.Read32(address);
            break;
        }

        case 151: { // STWX
            const uint32_t address = ReadBaseRegister(ra) + ReadBaseRegister(rb);
            memory_.Write32(address, gpr_[RS(instruction)]);
            break;
        }

        case 339: // MFLR
            gpr_[RD(instruction)] = lr_;
            break;

        case 467: // MTSPR
            WriteSPR((RD(instruction) << 5) | ((instruction >> 16) & 31),
                     gpr_[RS(instruction)]);
            break;

        case 371: // MFTB approximation: return a monotonic instruction counter
            gpr_[RD(instruction)] = pc_;
            break;

        case 444: { // OR
            const uint32_t result =
                gpr_[RS(instruction)] | gpr_[rb];
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

        case 40: { // SUBF
            const uint32_t result = gpr_[rb] - gpr_[ra];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 28: { // AND
            const uint32_t result = gpr_[RS(instruction)] & gpr_[rb];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 316: { // XOR
            const uint32_t result = gpr_[RS(instruction)] ^ gpr_[rb];
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 24: { // SLW
            const unsigned shift = gpr_[rb] & 0x3F;
            const uint32_t result = shift >= 32 ? 0 : gpr_[RS(instruction)] << shift;
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 536: { // SRW
            const unsigned shift = gpr_[rb] & 0x3F;
            const uint32_t result = shift >= 32 ? 0 : gpr_[RS(instruction)] >> shift;
            gpr_[RD(instruction)] = result;
            if (Rc(instruction))
                SetCR0FromResult(result);
            break;
        }

        case 467: { // MTSPR shares XO above; kept unreachable for clarity.
            break;
        }

        case 16: { // MR aliases MF? Treat common mcrf-ish no-op safely.
            break;
        }

        case 598: { // SYNC
            break;
        }

        case 854: { // EIEIO
            break;
        }

        default:
            throw std::runtime_error("vWii PowerPC: unsupported X-form opcode");
        }
        break;
    }

    case 19: {
        const unsigned xo = (instruction >> 1) & 0x3FF;

        if (xo == 16) { // BCLR
            const bool taken = ConditionBit(BO(instruction), BI(instruction));
            const uint32_t target = lr_ & 0xFFFFFFFC;
            if (taken)
                pc_ = target;
            if (instruction & 1)
                lr_ = pc_;
            break;
        }

        if (xo == 528) { // BCCTR
            const bool taken = ConditionBit(BO(instruction), BI(instruction));
            const uint32_t target = ctr_ & 0xFFFFFFFC;
            if (taken)
                pc_ = target;
            if (instruction & 1)
                lr_ = pc_;
            break;
        }

        if (xo == 50) { // RFI
            msr_ = srr1_;
            pc_ = srr0_;
            break;
        }

        throw std::runtime_error("vWii PowerPC: unsupported XL-form opcode");
    }

    default:
        throw std::runtime_error("vWii PowerPC: unsupported instruction");
    }
}

} // namespace vwii::cpu
