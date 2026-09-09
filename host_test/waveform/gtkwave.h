#pragma once

#include <string>

// True if a `gtkwave` binary is on PATH.
bool gtkwave_available();

int open_wave(const char *path);
