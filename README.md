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
- SDL3 cross-platform GUI with RVZ drag-and-drop
- persistent host-backed NAND filesystem

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

The emulator is still low-compatibility. The current boot path bypasses executable apploader code and starts the extracted Main DOL with a reconstructed Wii boot environment. IOS/DIs, NAND, Hollywood IPC, CPU exceptions, and common PowerPC/FPU operations are present, but GX/VI rendering, DSP/audio, complete IOS/ES behavior, MMU/TLBs, and controller hardware are still incomplete.


### GUI

Launching vWii without arguments opens the GUI. Drag a legally obtained .rvz file onto the window to start a game.

### NAND

The HLE IOS filesystem stores persistent data under the user's application-data directory. This gives titles a real host-backed place for save/configuration files even before a full NAND emulation layer is implemented.

## Releases

Create a version tag such as:

```bash
git tag v0.2.0
git push origin v0.2.0
```

GitHub Actions then builds:

- vWii-v0.3.0-Windows-x64.zip
- vWii-v0.3.0-macOS.dmg

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

Dolphin documents RVZ as a WIA-derived format with Zstandard support and decrypted/hash-stripped Wii partition data, which is the format handled by vWii's current disc backend. citeturn111428view0

## Scope note

vWii is being developed as a separate implementation rather than copying Dolphin source code.

Use game dumps, system software, and other copyrighted material only when you have the legal right to use them.
