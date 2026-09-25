#include "core/emulator.h"
#include "input/wiimote_keyboard.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>

namespace {

uint32_t EncodeAddi(unsigned rd, unsigned ra, int16_t immediate) {
    return (14u << 26) |
           ((rd & 31u) << 21) |
           ((ra & 31u) << 16) |
           static_cast<uint16_t>(immediate);
}

uint32_t EncodeBranch(int32_t byte_offset) {
    const uint32_t li = static_cast<uint32_t>(byte_offset >> 2) & 0x00FFFFFFu;
    return (18u << 26) | (li << 2);
}

} // namespace

int main() {
    vwii::core::Emulator emulator;
    assert(emulator.Initialize());

    // ADDI + branch.
    emulator.Memory().Write32(0x80000000, EncodeAddi(3, 0, 42));
    emulator.Memory().Write32(0x80000004, EncodeBranch(8));
    emulator.Memory().Write32(0x80000008, EncodeAddi(3, 0, 99));
    emulator.Memory().Write32(0x8000000C, EncodeAddi(4, 3, 1));

    emulator.Step();
    assert(emulator.CPU().GetGPR(3) == 42);

    emulator.Step();
    assert(emulator.CPU().GetPC() == 0x8000000C);

    emulator.Step();
    assert(emulator.CPU().GetGPR(4) == 43);

    // Hollywood IPC register access and IRQ routing.
    emulator.Memory().Write32(0x0D800034, 1u << 30);
    emulator.Memory().Hollywood().RaisePpcInterrupt(30);
    assert(emulator.Memory().ExternalInterruptPending());

    emulator.Memory().Write32(0x0D800030, 1u << 30);
    assert(!emulator.Memory().ExternalInterruptPending());

    // Simple FPU load/add/store.
    const uint32_t one = 0x3F800000;
    const uint32_t two = 0x40000000;
    emulator.Memory().Write32(0x80001000, one);
    emulator.Memory().Write32(0x80001004, two);

    // lfs f1,0(r0)  / lfs f2,4(r0)
    emulator.Memory().Write32(0x80002000, 0xC0200000);
    emulator.Memory().Write32(0x80002004, 0xC0400004);

    // fadds f3,f1,f2
    emulator.Memory().Write32(0x80002008, 0xEC61102A);

    // stfs f3,8(r0)
    emulator.Memory().Write32(0x8000200C, 0xD0600008);

    emulator.CPU().Reset(0x80002000);
    for (int i = 0; i < 4; ++i)
        emulator.Step();

    assert(emulator.Memory().Read32(0x80001008) == 0x40400000);

    // LWARX/STWCX. reservation semantics.
    emulator.Memory().Write32(0x80001100, 0xAABBCCDD);
    emulator.Memory().Write32(0x80003000, 0x7C660028); // lwarx r3,0,r6
    emulator.Memory().Write32(0x80003004, 0x7C62012D); // stwcx. r3,0,r2
    emulator.CPU().Reset(0x80003000);
    emulator.CPU().SetGPR(6, 0x80001100);
    emulator.CPU().SetGPR(2, 0x80001100);
    emulator.CPU().SetGPR(3, 0x11223344);
    emulator.Step();
    assert(emulator.CPU().GetGPR(3) == 0xAABBCCDD);
    emulator.Step();
    assert(emulator.Memory().Read32(0x80001100) == 0x11223344);

    // Virtual Wii Remote keyboard/report path.
    vwii::input::WiiRemoteKeyboard wiimote;
    wiimote.KeyEvent(vwii::input::Key::A, true);
    wiimote.KeyEvent(vwii::input::Key::NunchukZ, true);
    wiimote.SetExtension(vwii::input::Extension::Nunchuk);
    const auto wiimote_report = wiimote.BuildReport();

    assert(wiimote_report[0] == 0x37);
    assert((wiimote_report[1] & 0x08) != 0);
    assert(wiimote_report[21] & 0x01);

    wiimote.ToggleMotionPlus();
    wiimote.KeyEvent(vwii::input::Key::MotionYawRight, true);
    wiimote.Update(0.016f);

    const auto motion_report = wiimote.BuildReport();
    assert(motion_report[0] == 0x37);
    assert(motion_report[20] != 0 || motion_report[21] != 0);

    wiimote.KeyEvent(vwii::input::Key::A, false);
    wiimote.KeyEvent(vwii::input::Key::NunchukZ, false);
    wiimote.KeyEvent(vwii::input::Key::MotionYawRight, false);

    // Persistent host-backed NAND file I/O.
    const std::filesystem::path test_nand =
        std::filesystem::temp_directory_path() / "vwii-core-test-nand";

    vwii::ios::NandFS nand(emulator.Memory());
    nand.SetRoot(test_nand);

    constexpr uint32_t io_address = 0x80004000;
    emulator.Memory().Write32(io_address, 0x12345678);

    const int write_fd = nand.Open("/test.bin", 2);
    assert(write_fd >= 8);
    assert(nand.Write(write_fd, io_address, 4) == 4);
    assert(nand.Close(write_fd) == 0);

    const int read_fd = nand.Open("/test.bin", 0);
    assert(read_fd >= 8);
    emulator.Memory().Write32(io_address, 0);
    assert(nand.Read(read_fd, io_address, 4) == 4);
    assert(emulator.Memory().Read32(io_address) == 0x12345678);
    assert(nand.Close(read_fd) == 0);

    std::error_code cleanup_error;
    std::filesystem::remove_all(test_nand, cleanup_error);

    emulator.Shutdown();
    std::cout << "All vWii core tests passed.\n";
    return 0;
}
