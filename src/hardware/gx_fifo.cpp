#include "hardware/gx_fifo.h"

namespace vwii::hardware {

void GXFifo::Reset() {
    registers_.fill(0);
    fifo_writes_ = 0;
    last_fifo_word_ = 0;
}

uint32_t GXFifo::Read32(uint32_t address) const {
    if (address < RegisterBase ||
        address >= RegisterBase + RegisterSize ||
        (address & 3u) != 0)
        return 0;

    return registers_[(address - RegisterBase) / 4];
}

void GXFifo::Write32(uint32_t address, uint32_t value) {
    if (address >= FifoBase &&
        address < FifoBase + FifoSize) {
        WriteFifo32(value);
        return;
    }

    if (address < RegisterBase ||
        address >= RegisterBase + RegisterSize ||
        (address & 3u) != 0)
        return;

    registers_[(address - RegisterBase) / 4] = value;
}

void GXFifo::WriteFifo32(uint32_t value) {
    last_fifo_word_ = value;
    ++fifo_writes_;
}

} // namespace vwii::hardware
