#include "hardware/hollywood.h"

namespace vwii::hardware {

namespace {

constexpr uint32_t Base = 0x0D800000;

constexpr uint32_t IPC_PPCMSG  = Base + 0x00;
constexpr uint32_t IPC_PPCCTRL = Base + 0x04;
constexpr uint32_t IPC_ARMMSG  = Base + 0x08;
constexpr uint32_t IPC_ARMCTRL = Base + 0x0C;

constexpr uint32_t TIMER       = Base + 0x10;
constexpr uint32_t ALARM       = Base + 0x14;

constexpr uint32_t PPC_IRQFLAG = Base + 0x30;
constexpr uint32_t PPC_IRQMASK = Base + 0x34;
constexpr uint32_t ARM_IRQFLAG = Base + 0x38;
constexpr uint32_t ARM_IRQMASK = Base + 0x3C;

constexpr uint32_t COMPAT      = Base + 0x180;
constexpr uint32_t BOOT0       = Base + 0x18C;
constexpr uint32_t CLOCKS      = Base + 0x190;
constexpr uint32_t RESETS      = Base + 0x194;

constexpr uint32_t IRQ_SOURCE_IPC = 30;

} // namespace

void Hollywood::Reset() {
    ipc_ppc_msg_ = 0;
    ipc_ppc_ctrl_ = 0;
    ipc_arm_msg_ = 0;
    ipc_arm_ctrl_ = 0;
    timer_ = 0;
    alarm_ = 0;
    ppc_irq_flags_ = 0;
    ppc_irq_mask_ = 0;
    arm_irq_flags_ = 0;
    arm_irq_mask_ = 0;
    compat_ = 0;
    boot0_ = 0;
    clocks_ = 0;
    resets_ = 0;
}

uint32_t Hollywood::Read32(uint32_t address) const {
    switch (address) {
    case IPC_PPCMSG:  return ipc_ppc_msg_;
    case IPC_PPCCTRL: return ipc_ppc_ctrl_;
    case IPC_ARMMSG:  return ipc_arm_msg_;
    case IPC_ARMCTRL: return ipc_arm_ctrl_;
    case TIMER:       return timer_;
    case ALARM:       return alarm_;
    case PPC_IRQFLAG: return ppc_irq_flags_;
    case PPC_IRQMASK: return ppc_irq_mask_;
    case ARM_IRQFLAG: return arm_irq_flags_;
    case ARM_IRQMASK: return arm_irq_mask_;
    case COMPAT:      return compat_;
    case BOOT0:       return boot0_;
    case CLOCKS:      return clocks_;
    case RESETS:      return resets_;
    default:
        return 0;
    }
}

void Hollywood::Write32(uint32_t address, uint32_t value) {
    switch (address) {
    case IPC_PPCMSG:
        ipc_ppc_msg_ = value;
        break;

    case IPC_PPCCTRL: {
        // X1/X2 are writable; Y1/Y2 are clear-on-write-one.
        const uint32_t writable = (1u << 0) | (1u << 2);
        const uint32_t clearable = (1u << 1) | (1u << 3);

        ipc_ppc_ctrl_ =
            (ipc_ppc_ctrl_ & ~(writable | (1u << 4) | (1u << 5))) |
            (value & (writable | (1u << 4) | (1u << 5)));

        if (value & (1u << 1))
            ipc_ppc_ctrl_ &= ~(1u << 1);
        if (value & (1u << 3))
            ipc_ppc_ctrl_ &= ~(1u << 3);

        (void)clearable;
        break;
    }

    case IPC_ARMMSG:
        ipc_arm_msg_ = value;
        break;

    case IPC_ARMCTRL:
        ipc_arm_ctrl_ = value;
        break;

    case TIMER:
        timer_ = value;
        break;

    case ALARM:
        alarm_ = value;
        break;

    case PPC_IRQFLAG:
        ppc_irq_flags_ &= ~value;
        break;

    case PPC_IRQMASK:
        ppc_irq_mask_ = value;
        break;

    case ARM_IRQFLAG:
        arm_irq_flags_ &= ~value;
        break;

    case ARM_IRQMASK:
        arm_irq_mask_ = value;
        break;

    case COMPAT:
        compat_ = value;
        break;

    case BOOT0:
        boot0_ = value;
        break;

    case CLOCKS:
        clocks_ = value;
        break;

    case RESETS:
        resets_ = value;
        break;

    default:
        break;
    }

    // Setting X1/X2 on the PPC side is the mechanism used to notify IOS.
    // The HLE Starlet will consume this later through the IOS service layer.
    if (address == IPC_PPCCTRL && ((value >> 0) & 1u)) {
        arm_irq_flags_ |= (1u << IRQ_SOURCE_IPC);
    }
}

void Hollywood::RaisePpcInterrupt(unsigned source) {
    if (source < 32)
        ppc_irq_flags_ |= 1u << source;
}

void Hollywood::ClearPpcInterrupt(unsigned source) {
    if (source < 32)
        ppc_irq_flags_ &= ~(1u << source);
}

bool Hollywood::PpcInterruptPending() const {
    return (ppc_irq_flags_ & ppc_irq_mask_) != 0;
}

} // namespace vwii::hardware
