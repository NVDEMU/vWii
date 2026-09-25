#include "core/emulator.h"
#include "video/video_backend.h"

#include <iostream>
#include <string_view>

namespace {

void PrintUsage() {
    std::cout
        << "vWii - Wii emulator foundation\n"
        << "\n"
        << "Usage:\n"
        << "  vwii [--version]\n"
        << "  vwii [--self-test]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        const std::string_view command = argv[1];

        if (command == "--version") {
            std::cout << "vWii 0.1.0\n";
            return 0;
        }

        if (command != "--self-test") {
            PrintUsage();
            return 1;
        }
    }

    vwii::core::Emulator emulator;
    if (!emulator.Initialize()) {
        std::cerr << "Failed to initialize vWii.\n";
        return 1;
    }

    auto video = vwii::video::CreateNullBackend();
    if (!video->Initialize()) {
        std::cerr << "Failed to initialize video backend.\n";
        return 1;
    }

    // addi r3, r0, 42
    emulator.Memory().Write32(0x80000000, 0x3860002A);
    emulator.Step();

    if (emulator.CPU().GetGPR(3) != 42) {
        std::cerr << "PowerPC self-test failed.\n";
        return 1;
    }

    video->Present({640, 480, 1});
    video->Shutdown();
    emulator.Shutdown();

    std::cout << "vWii initialized successfully.\n";
    std::cout << "PowerPC interpreter self-test passed.\n";
    return 0;
}
