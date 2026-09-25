#include "core/emulator.h"
#include "frontend/frontend.h"
#include "video/video_backend.h"

#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

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
        << "  vwii --boot <game.rvz> --run <instructions>\n"
        << "\n"
        << "The GUI accepts RVZ files by drag-and-drop.\n";
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
                return 0;
            }

            vwii::frontend::Frontend frontend;
            if (!frontend.Initialize("vWii", 960, 720)) {
                std::cerr << "Failed to initialize the vWii frontend.\n";
                return 1;
            }

            vwii::frontend::Status status;
            status.game_id = result.disc.game_id;
            status.loaded = true;

            uint64_t instructions = 0;
            while (frontend.PumpEvents()) {
                constexpr uint64_t InstructionsPerFrame = 5000;
                if (!emulator.CPU().Halted()) {
                    emulator.RunForInstructions(InstructionsPerFrame);
                    instructions += InstructionsPerFrame;
                }

                status.pc = emulator.CPU().GetPC();
                status.instructions = instructions;
                status.halted = emulator.CPU().Halted();

                frontend.Present(status);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            frontend.Shutdown();
            emulator.Shutdown();
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

    // No arguments launches the GUI. Users can drag an RVZ onto the window.
    vwii::frontend::Frontend frontend;
    if (!frontend.Initialize("vWii", 960, 720)) {
        std::cerr << "Failed to initialize the vWii frontend.\n";
        return 1;
    }

    vwii::frontend::Status status;
    uint64_t instructions = 0;

    while (frontend.PumpEvents()) {
        const std::string dropped = frontend.ConsumeDroppedFile();

        if (!dropped.empty()) {
            const auto result = emulator.LoadWiiGame(dropped);
            if (result.success) {
                status.game_id = result.disc.game_id;
                status.loaded = true;
                status.halted = false;
                std::cout << "Loaded " << dropped << " (" << status.game_id << ")\n";
            } else {
                std::cerr << "Wii boot preparation failed: "
                          << result.error << "\n";
            }
        }

        if (status.loaded && !emulator.CPU().Halted()) {
            constexpr uint64_t InstructionsPerFrame = 5000;
            emulator.RunForInstructions(InstructionsPerFrame);
            instructions += InstructionsPerFrame;
        }

        status.pc = emulator.CPU().GetPC();
        status.instructions = instructions;
        status.halted = emulator.CPU().Halted();

        frontend.Present(status);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    frontend.Shutdown();
    emulator.Shutdown();
    return 0;
}
