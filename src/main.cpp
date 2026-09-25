#include "core/emulator.h"
#include "video/video_backend.h"

#include <iostream>
#include <string_view>

namespace {

void PrintUsage() {
    std::cout
        << "vWii - Wii emulator\n"
        << "\n"
        << "Usage:\n"
        << "  vwii --version\n"
        << "  vwii --self-test\n"
        << "  vwii --load <file.dol|file.elf>\n"
        << "  vwii --boot <game.rvz>\n"
        << "  vwii --boot <game.rvz> --run <instructions>\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        const std::string_view command = argv[1];

        if (command == "--version") {
            std::cout << "vWii 0.2.0\n";
            return 0;
        }

        if (command == "--load") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            vwii::core::Emulator emulator;
            if (!emulator.Initialize()) {
                std::cerr << "Failed to initialize vWii.\n";
                return 1;
            }

            const auto result = emulator.LoadImageFile(argv[2]);
            if (!result.success) {
                std::cerr << "Load failed: " << result.error << "\n";
                return 1;
            }

            std::cout << "Loaded executable successfully.\n"
                      << "Entry point: 0x" << std::hex << result.entry_point
                      << std::dec << "\n";
            return 0;
        }

        if (command == "--boot") {
            if (argc != 3 && argc != 5) {
                PrintUsage();
                return 1;
            }

            if (argc == 5 && std::string_view(argv[3]) != "--run") {
                PrintUsage();
                return 1;
            }

            vwii::core::Emulator emulator;
            if (!emulator.Initialize()) {
                std::cerr << "Failed to initialize vWii.\n";
                return 1;
            }

            const auto result = emulator.LoadWiiGame(argv[2]);
            if (!result.success) {
                std::cerr << "Wii boot preparation failed: "
                          << result.error << "\n";
                return 1;
            }

            std::cout << "RVZ opened successfully.\n"
                      << "Game ID: " << result.disc.game_id << "\n"
                      << "Main DOL entry point: 0x" << std::hex
                      << result.dol_result.entry_point << std::dec << "\n"
                      << "Main DOL loaded into emulated memory.\n";

            if (argc == 5) {
                const uint64_t instructions = std::stoull(argv[4]);
                emulator.RunForInstructions(instructions);
                std::cout << "Executed " << instructions
                          << " PowerPC instructions.\n";
            }

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
