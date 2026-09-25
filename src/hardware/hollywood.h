#pragma once

#include <cstdint>

namespace vwii::hardware {

class Hollywood {
public:
    void Reset();

    [[nodiscard]] uint32_t Read32(uint32_t address) const;
    void Write32(uint32_t address, uint32_t value);

    void RaisePpcInterrupt(unsigned source);
    void ClearPpcInterrupt(unsigned source);

    // Starlet/HLE side uses this when an IPC command has completed.
    void CompleteIpcReply();

    [[nodiscard]] bool PpcInterruptPending() const;
    [[nodiscard]] uint32_t PpcIrqFlags() const { return ppc_irq_flags_; }
    [[nodiscard]] uint32_t PpcIrqMask() const { return ppc_irq_mask_; }

private:
    uint32_t ipc_ppc_msg_{};
    uint32_t ipc_ppc_ctrl_{};
    uint32_t ipc_arm_msg_{};
    uint32_t ipc_arm_ctrl_{};

    uint32_t timer_{};
    uint32_t alarm_{};

    uint32_t ppc_irq_flags_{};
    uint32_t ppc_irq_mask_{};

    uint32_t arm_irq_flags_{};
    uint32_t arm_irq_mask_{};

    uint32_t compat_{};
    uint32_t boot0_{};
    uint32_t clocks_{};
    uint32_t resets_{};
};

} // namespace vwii::hardware
