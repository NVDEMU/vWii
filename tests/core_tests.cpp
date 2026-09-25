#include "core/emulator.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    vwii::core::Emulator emulator;

    assert(emulator.Initialize());
    assert(emulator.IsInitialized());

    // addi r3, r0, 42
    emulator.Memory().Write32(0x80000000, 0x3860002A);
    emulator.Step();
    assert(emulator.CPU().GetGPR(3) == 42);
    assert(emulator.CPU().GetPC() == 0x80000004);

    // Verify PowerPC big-endian memory behavior.
    emulator.Memory().Write32(0x80000100, 0x12345678);
    assert(emulator.Memory().Read8(0x80000100) == 0x12);
    assert(emulator.Memory().Read16(0x80000100) == 0x1234);
    assert(emulator.Memory().Read32(0x80000100) == 0x12345678);

    emulator.Shutdown();
    std::cout << "All vWii core tests passed.\n";
    return 0;
}
