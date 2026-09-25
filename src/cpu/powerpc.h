#pragma once

#include <array>
#include <cstdint>

namespace vwii::memory {
class Memory;
}

namespace vwii::cpu {

class PowerPC {
public:
    explicit PowerPC(memory::Memory& memory);

    void Reset(uint32_t entry_point = 0x80000000);
    void Step();

    [[nodiscard]] uint32_t GetPC() const { return pc_; }
    [[nodiscard]] uint32_t GetGPR(unsigned index) const { return gpr_.at(index & 31U); }

    void SetGPR(unsigned index, uint32_t value) { gpr_.at(index & 31U) = value; }
    void SetMSR(uint32_t value) { msr_ = value; }
    [[nodiscard]] uint32_t GetLR() const { return lr_; }
    [[nodiscard]] uint32_t GetCTR() const { return ctr_; }
    [[nodiscard]] uint32_t GetCR() const { return cr_; }
    [[nodiscard]] uint32_t GetXER() const { return xer_; }
    [[nodiscard]] uint32_t GetMSR() const { return msr_; }
    [[nodiscard]] uint32_t GetFPSCR() const { return fpscr_; }
    [[nodiscard]] uint64_t GetFPR(unsigned index) const { return fpr_.at(index & 31U); }
    [[nodiscard]] uint32_t GetSRR0() const { return srr0_; }
    [[nodiscard]] uint32_t GetSRR1() const { return srr1_; }
    [[nodiscard]] bool Halted() const { return halted_; }

private:
    static constexpr uint32_t MSR_EE = 0x00008000;
    static constexpr uint32_t MSR_PR = 0x00004000;
    static constexpr uint32_t MSR_FP = 0x00002000;
    static constexpr uint32_t MSR_ME = 0x00001000;
    static constexpr uint32_t MSR_IR = 0x00000020;
    static constexpr uint32_t MSR_DR = 0x00000010;

    void Execute(uint32_t instruction, uint32_t cia);
    void RaiseException(uint32_t vector, uint32_t reason);

    void SetCR0FromResult(uint32_t value);
    void SetCRField(unsigned field, unsigned value);
    [[nodiscard]] unsigned GetCRBit(unsigned bit) const;
    [[nodiscard]] bool ConditionBit(unsigned bo, unsigned bi);

    [[nodiscard]] uint32_t ReadBaseRegister(unsigned index) const;
    [[nodiscard]] uint32_t ReadSPR(unsigned spr) const;
    void WriteSPR(unsigned spr, uint32_t value);

    memory::Memory& memory_;
    std::array<uint32_t, 32> gpr_{};
    std::array<uint64_t, 32> fpr_{};

    uint32_t pc_{0x80000000};
    uint32_t lr_{};
    uint32_t ctr_{};
    uint32_t xer_{};
    uint32_t cr_{};
    uint32_t msr_{};
    uint32_t srr0_{};
    uint32_t srr1_{};
    uint32_t fpscr_{};
    uint64_t timebase_{};
    uint32_t reservation_address_{};
    bool reservation_valid_{};

    bool halted_{};
};

} // namespace vwii::cpu
