# VFPGA-S3 Agent Setup

Environment setup for VFPGA-S3 host simulation and development.

Detect the platform with `uname -s` (Linux, Darwin, MINGW/MSYS for Windows).

## 1. Required: C++17 Compiler

| Platform | Install | Verify |
|----------|---------|--------|
| Linux (Debian/Ubuntu) | `sudo apt install build-essential` | `g++ --version` (needs >= 7) |
| Linux (Fedora/RHEL) | `sudo dnf install gcc-c++` | `g++ --version` (needs >= 7) |
| macOS | `xcode-select --install` | `clang++ --version` (needs >= 10) |
| Windows | MSYS2 UCRT64 shell: `pacman -S mingw-w64-ucrt-x86_64-clang` | `clang++ --version` |

Windows gotchas:

- Stock MinGW.org GCC 6.3 is pre-C++17 and fails silently. Use the
  UCRT64 packages above.
- Dynamic links pick up stale `libstdc++-6.dll` copies from
  `C:\WINDOWS\SYSTEM32` (left there by old MinGW installs). Build host
  binaries with `-static` on Windows to sidestep this.

## 2. Optional: GTKWave (waveform viewer)

Only needed to open `.vcd` waveform files. Not required for building or
running anything.

| Platform | Install |
|----------|---------|
| Linux (Debian/Ubuntu) | `sudo apt install gtkwave` |
| Linux (Fedora/RHEL) | `sudo dnf install gtkwave` |
| macOS | `brew install gtkwave` |
| Windows | MSYS2 UCRT64 shell: `pacman -S mingw-w64-ucrt-x86_64-gtkwave` |

On Windows, adding `C:\msys64\ucrt64\bin` to the system PATH is the
durable fix for `gtkwave` discovery.

## 3. Constraint

Never add files under `src/` — `src/CMakeLists.txt` globs `src/*.cpp`,
so any host-only code placed there ships to the ESP32 firmware.
Host-only code lives in `host_test/` or `tools/`.
