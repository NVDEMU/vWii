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
    [[nodiscard]] uint32_t GetLR() const { return lr_; }
    [[nodiscard]] uint32_t GetCR() const { return cr_; }
    [[nodiscard]] bool Halted() const { return halted_; }

private:
    void Execute(uint32_t instruction, uint32_t cia);

    void SetCR0FromResult(uint32_t value);
    [[nodiscard]] uint32_t ReadBaseRegister(unsigned index) const;

    memory::Memory& memory_;
    std::array<uint32_t, 32> gpr_{};
    uint32_t pc_{0x80000000};
    uint32_t lr_{};
    uint32_t ctr_{};
    uint32_t xer_{};
    uint32_t cr_{};
    bool halted_{};
};

} // namespace vwii::cpu
