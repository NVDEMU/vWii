#include "core/emulator.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void PutBE32(std::vector<uint8_t>& data, std::size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value >> 24);
    data[offset + 1] = static_cast<uint8_t>(value >> 16);
    data[offset + 2] = static_cast<uint8_t>(value >> 8);
    data[offset + 3] = static_cast<uint8_t>(value);
}

} // namespace

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

    // Minimal synthetic DOL: one text segment containing addi r3,r0,42.
    std::vector<uint8_t> dol(0x120, 0);
    PutBE32(dol, 0x00, 0x100);          // text file offset
    PutBE32(dol, 0x48, 0x80001000);     // text address
    PutBE32(dol, 0x90, 4);              // text size
    PutBE32(dol, 0xD8, 0x80001000);     // entry point
    PutBE32(dol, 0x100, 0x3860002A);    // addi r3,r0,42

    const auto dol_result = emulator.LoadImage(dol);
    assert(dol_result.success);
    assert(dol_result.type == vwii::boot::ImageType::Dol);
    assert(dol_result.entry_point == 0x80001000);
    assert(emulator.HasLoadedImage());
    assert(emulator.Memory().Read32(0x80001000) == 0x3860002A);
    assert(emulator.CPU().GetPC() == 0x80001000);

    // Minimal big-endian ELF32: one PT_LOAD containing the same instruction.
    std::vector<uint8_t> elf(0x70, 0);
    elf[0] = 0x7F;
    elf[1] = 'E';
    elf[2] = 'L';
    elf[3] = 'F';
    elf[4] = 1;                         // ELF32
    elf[5] = 2;                         // big endian
    elf[6] = 1;                         // version
    elf[0x12] = 0;
    elf[0x13] = 20;                     // EM_PPC
    PutBE32(elf, 0x18, 0x80002000);      // entry
    PutBE32(elf, 0x1C, 52);              // program header offset
    elf[0x2A] = 0;
    elf[0x2B] = 32;                     // program header size
    elf[0x2C] = 0;
    elf[0x2D] = 1;                      // one program header
    PutBE32(elf, 52, 1);                // PT_LOAD
    PutBE32(elf, 56, 84);               // file offset
    PutBE32(elf, 60, 0x80002000);       // virtual address
    PutBE32(elf, 68, 4);                // file size
    PutBE32(elf, 72, 8);                // memory size
    PutBE32(elf, 84, 0x3860002A);       // instruction

    const auto elf_result = emulator.LoadImage(elf);
    assert(elf_result.success);
    assert(elf_result.type == vwii::boot::ImageType::Elf32);
    assert(elf_result.entry_point == 0x80002000);
    assert(emulator.Memory().Read32(0x80002000) == 0x3860002A);
    assert(emulator.Memory().Read32(0x80002004) == 0);

    emulator.Shutdown();
    std::cout << "All vWii core tests passed.\n";
    return 0;
}
