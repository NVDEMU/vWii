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
- Automatic nightly releases after successful main-branch builds
- Tag-based GitHub Releases for stable versions
- Windows x64 ZIP release
- macOS .app packaged inside a DMG
- SDL3 cross-platform GUI with RVZ drag-and-drop
- persistent host-backed NAND filesystem
- automatic nightly-release update checking from the GUI

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

Dolphin documents RVZ as a WIA-derived format with Zstandard support and decrypted/hash-stripped Wii partition data, which is the format handled by vWii's current disc backend.

## Scope note

vWii is being developed as a separate implementation rather than copying Dolphin source code.

Use game dumps, system software, and other copyrighted material only when you have the legal right to use them.

## Keyboard Wii Remote controls

The GUI has a built-in virtual Wii Remote driven entirely by the keyboard. SDL3 reports physical scancodes, so the default layout is independent of the user's keyboard language/layout. citeturn422338search0turn422338search4

Press F12 in the GUI to open **Settings**. The Keyboard Controls screen lets you select any binding, press Enter, then press the new key to remap it. Delete clears a binding, R restores the selected action's default, and F5 restores all defaults. Bindings are saved automatically in vWii's per-user preferences so they survive restarts. SDL provides this application-specific preference location through SDL_GetPrefPath(). citeturn738260search0

Core Wii Remote: Arrow keys = D-pad, Space = A, Right Ctrl = B, Z = 1, X = 2, = = Plus, - = Minus, Backspace = Home.

IR pointer simulation: Numpad 8/2/4/6 move the pointer, Numpad 5 centers it, Numpad 7/9 zoom it out/in.

Nunchuk: W/A/S/D = stick, Q = C, E = Z.

Classic Controller: I/J/K/L = left stick, U/O/P/[ = A/B/X/Y, N/M = L/R, Comma/Period = ZL/ZR, 0/9 = Plus/Minus.

Guitar: 1/2/3/4/5 = green/red/yellow/blue/orange frets, G/H = strum up/down, T/Y = whammy down/up.

Drums: 6/7/8/9/0 = drum pads, N = kick pedal.

Turntable: F/G/H = green/red/blue, J/L = deck movement, Comma/Period = crossfader.

UDraw / Drawsome tablets: Comma/Period and Semicolon/Slash move the pen, Apostrophe presses the pen, Right Bracket = A, Left Bracket = B.

TaTaCon: V = hit, B = rim.

Shinkansen: R/F = throttle up/down, G = brake, H = horn.

Motion simulation: I/K = pitch up/down, J/L = yaw left/right, U/O = roll left/right, T/G = X acceleration, R/F = Y acceleration, Y/H = Z acceleration, Q = shake.

Extension selection: F1 = no extension, F2 = Nunchuk, F3 = Classic, F4 = Guitar, F5 = Drums, F6 = Turntable, F7 = UDraw, F8 = Drawsome, F9 = TaTaCon, F11 = Shinkansen. F10 toggles MotionPlus.

The virtual HID layer currently covers the common Dolphin attachment set—Nunchuk, Classic, Guitar, Drums, Turntable, UDraw, Drawsome, TaTaCon, Shinkansen—and exposes MotionPlus gyro data. Dolphin's public Wii Remote implementation lists those same attachment types and treats MotionPlus as an attachable sensor. citeturn209127search0


### Nightly updater

vWii checks the GitHub **nightly** release automatically when the GUI starts. If a newer build is found, the Settings screen shows an update notice. Press F12 to open Settings, then F6 to open the platform-specific nightly download. The check uses the commit SHA embedded at build time, so an unchanged nightly is not reported repeatedly as a new version.
