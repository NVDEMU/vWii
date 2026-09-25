# vWii

vWii is a Wii emulator project targeting Windows and macOS.

## Current status

vWii is an early-stage emulator. Compatibility is expected to be low while the PowerPC, IOS, video, audio, and Wii hardware implementations are developed.

Current milestones include:

- CMake-based Windows/macOS project
- Windows and macOS CI
- PowerPC interpreter foundation
- Wii MEM1/MEM2 memory model
- RVZ disc-image reader using Zstandard
- Wii partition discovery
- RVZ decrypted partition-data access
- Main DOL extraction from Wii game partitions
- DOL loading into emulated memory
- Tag-based GitHub Releases
- Windows x64 ZIP release
- macOS .app packaged inside a DMG

## Booting a Wii game

The current command-line boot path accepts RVZ images:

```text
vwii --boot "My Wii Game.rvz"
```

It extracts the game's Main DOL and prepares it for the PowerPC core.

For early debugging, a limited instruction-run mode is available:

```text
vwii --boot "My Wii Game.rvz" --run 1000
```

The emulator does not yet implement the full Wii boot environment, IOS services, GPU pipeline, audio system, or complete Wii device model, so successful DOL loading does not mean a retail game is fully playable yet.

## Releases

Create a version tag such as:

```bash
git tag v0.2.0
git push origin v0.2.0
```

GitHub Actions then builds:

- vWii-v0.2.0-Windows-x64.zip
- vWii-v0.2.0-macOS.dmg

The macOS DMG contains vWii.app.

## Building locally

### Windows

Use Visual Studio 2022 with CMake:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### macOS

Install Xcode Command Line Tools and CMake:

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Architecture direction

The project uses Dolphin as an architectural reference for subsystem separation while keeping the vWii implementation separate.

Planned major subsystems:

1. PowerPC interpreter and JIT
2. MMU, BATs, TLBs, caches, exceptions, and timing
3. Broadway/Hollywood hardware model
4. Flipper/GX graphics pipeline
5. Audio/DSP
6. IOS and ES/HLE services
7. Wii filesystem/NAND
8. Disc interface and DVD device
9. Wiimote/GameCube/USB input
10. Save states, debugger, logging, and configuration
11. Cross-platform graphical frontend

Dolphin's public RVZ documentation confirms that RVZ is based on WIA, supports Zstandard, and stores Wii partition data in a decrypted/hash-stripped representation. citeturn971787search0turn630050view0

## Scope note

vWii is being developed as a separate implementation rather than copying Dolphin source code.

Use game dumps, system software, and other copyrighted material only when you have the legal right to use them.
