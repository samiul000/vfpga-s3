// GTKWave launcher. See gtkwave.h.
#include "gtkwave.h"

#include <cstdio>
#include <cstdlib>
#include <string>

bool gtkwave_available() {
#ifdef _WIN32
    if (system("where gtkwave >NUL 2>&1") == 0) return true;
    // Fall back to well-known install locations (MSYS2 envs, official installer).
    static const char *cands[] = {
        "C:\\msys64\\ucrt64\\bin\\gtkwave.exe",
        "C:\\msys64\\mingw64\\bin\\gtkwave.exe",
        "C:\\msys64\\mingw32\\bin\\gtkwave.exe",
        "C:\\Program Files\\gtkwave\\bin\\gtkwave.exe",
        nullptr
    };
    for (int i = 0; cands[i]; ++i) {
        FILE *f = fopen(cands[i], "rb");
        if (f) { fclose(f); return true; }
    }
    return false;
#else
    return system("which gtkwave >/dev/null 2>&1") == 0;
#endif
}

static std::string gtkwave_cmd() {
#ifdef _WIN32
    static const char *cands[] = {
        "C:\\msys64\\ucrt64\\bin\\gtkwave.exe",
        "C:\\msys64\\mingw64\\bin\\gtkwave.exe",
        "C:\\msys64\\mingw32\\bin\\gtkwave.exe",
        "C:\\Program Files\\gtkwave\\bin\\gtkwave.exe",
        nullptr
    };
    for (int i = 0; cands[i]; ++i) {
        FILE *f = fopen(cands[i], "rb");
        if (f) {
            fclose(f);
            return std::string("\"") + cands[i] + "\"";
        }
    }
#endif
    return "gtkwave";
}

int open_wave(const char *path) {
    if (!gtkwave_available()) {
        printf("GTKWave was not found.\nInstall GTKWave to visualize %s\n", path);
        return 5;
    }
    std::string cmd;
#ifdef _WIN32
    cmd = std::string("start \"\" ") + gtkwave_cmd() + " \"" + path + "\"";
#else
    cmd = std::string("gtkwave \"") + path + "\" &";
#endif
    return system(cmd.c_str());
}
