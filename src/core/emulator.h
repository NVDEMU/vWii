#pragma once

#include "cpu/powerpc.h"
#include "memory/memory.h"

#include <cstdint>

namespace vwii::core {

class Emulator {
public:
    Emulator();

    bool Initialize();
    void Shutdown();

    void Reset();
    void Step();
    void RunForInstructions(uint64_t count);

    [[nodiscard]] cpu::PowerPC& CPU() { return cpu_; }
    [[nodiscard]] memory::Memory& Memory() { return memory_; }
    [[nodiscard]] bool IsInitialized() const { return initialized_; }

private:
    memory::Memory memory_;
    cpu::PowerPC cpu_;
    bool initialized_{};
};

} // namespace vwii::core
