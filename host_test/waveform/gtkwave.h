#pragma once
// Host-only GTKWave launcher (Stage 7). GTKWave is visualization-only:
// absence never fails verification (§52, exit code 5).
#include <string>

// True if a `gtkwave` binary is on PATH.
bool gtkwave_available();

// Open path in GTKWave (detached). Returns 0 on launch, 5 if GTKWave is
// missing (prints install hint, does not treat as verification failure).
int open_wave(const char *path);
