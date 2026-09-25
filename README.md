# vWii

vWii is a Wii emulator project targeting Windows and macOS.

## Project status

This repository starts as a clean-room implementation with a Dolphin-inspired architecture. The first milestone is a deterministic, testable emulator core rather than a graphical frontend.

### Current foundation

- CMake-based C++ project
- Windows and macOS CI
- Wii memory map foundation (MEM1/MEM2)
- PowerPC interpreter foundation
- Core system lifecycle
- Video backend abstraction
- Basic emulator self-test

### Roadmap

1. PowerPC interpreter and exception model
2. MMU, BATs, TLBs, caches, and timing
3. Wii/Gekko/Broadway hardware registers
4. Audio/DSP
5. Flipper/Hollywood graphics pipeline
6. IOS and Wii filesystem/title support
7. Input, Wiimote, GameCube controller, and USB
8. Save states, logging, debugger, and configuration
9. JIT recompilers
10. Cross-platform graphical frontend
11. Real Wii software compatibility testing

## Building

### Windows

Use Visual Studio 2022 with the C++ desktop workload and CMake:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### macOS

Install Xcode Command Line Tools and CMake:

```bash
cmake -S . -B build
cmake --build build --config Release
```

The initial executable is intentionally dependency-free so the project can establish a reliable build before adding graphics/input dependencies.

## Important scope note

vWii is not a fork of Dolphin and does not copy Dolphin source code. Dolphin is used as an architectural reference for subsystem boundaries and development direction.

Use game dumps, system software, and other copyrighted material only when you have the legal right to use them.
