# VFPGA-S3 Agent Setup

Environment setup for VFPGA-S3 host simulation and development.

## Platform Detection

Run this first to determine the target environment:

```bash
uname -s
```

Expected output: `Linux`, `Darwin` (macOS), or `MINGW64_NT-*` / `MSYS_NT-*`
(Windows via Git Bash or MSYS2). In PowerShell, `uname` does not exist —
use `$env:OS` (`Windows_NT`) instead.

Follow the install steps for the detected platform below, then run the
verify commands. If a step fails, retry up to 3 times (transient network
errors are the usual cause). If it still fails, stop, write
`setup_error.log` in the repo root, and report the failure:

```text
[TIMESTAMP]
Platform: <uname -s output>
Step: <step name, e.g. "Install C++17 compiler">
Attempt: <n>/3
Command: <exact command run>
Exit code: <code>
Stdout: <captured output>
Stderr: <captured output>
```

## 1. Required: C++17 Compiler

| Platform | Install | Verify |
|----------|---------|--------|
| Linux (Debian/Ubuntu) | `sudo apt update && sudo apt install -y build-essential` | `g++ --version` (needs >= 7) |
| Linux (Fedora/RHEL) | `sudo dnf install -y gcc-c++` | `g++ --version` (needs >= 7) |
| macOS | `xcode-select --install` | `clang++ --version` (needs >= 10) |
| Windows | MSYS2 UCRT64 shell: `pacman -S --needed mingw-w64-ucrt-x86_64-clang` | `clang++ --version` |

Compiler failure is fatal: nothing in this project builds without C++17.
If the verify command still fails after 3 retries, write the error log
and stop.

Windows gotchas:

- Stock MinGW.org GCC 6.3 is pre-C++17 and fails silently. Use the
  UCRT64 packages above.
- Dynamic links pick up stale `libstdc++-6.dll` copies from
  `C:\WINDOWS\SYSTEM32` (left there by old MinGW installs). Build host
  binaries with `-static` on Windows to sidestep this.

## 2. GTKWave (waveform viewer)

Only needed to open `.vcd` waveform files. Not required for building or
running anything. GTKWave failure is non-fatal: complete the remaining
setup and note the missing viewer in the success message.

| Platform | Install | Verify |
|----------|---------|--------|
| Linux (Debian/Ubuntu) | `sudo apt install -y gtkwave` | `gtkwave --version` |
| Linux (Fedora/RHEL) | `sudo dnf install -y gtkwave` | `gtkwave --version` |
| macOS | `brew install gtkwave` | `gtkwave --version` |
| Windows | MSYS2 UCRT64 shell: `pacman -S --needed mingw-w64-ucrt-x86_64-gtkwave` | `gtkwave --version` |

On Windows, add `C:\msys64\ucrt64\bin` to the system PATH for `gtkwave` discovery.

## 3. Success Message

After all steps pass, print a summary of the configured environment:

```text
VFPGA-S3 environment ready.
Platform: <uname -s output>
Compiler: <compiler name and version from verify step>
GTKWave: <version, or "not installed (waveform viewing unavailable)">
```

## 4. Constraint

Never add files under `src/` — `src/CMakeLists.txt` globs `src/*.cpp`,
so any host-only code placed there ships to the ESP32 firmware.
Host-only code lives in `host_test/` or `tools/`.
