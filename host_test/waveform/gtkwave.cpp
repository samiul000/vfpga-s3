// GTKWave launcher. See gtkwave.h.
#include "gtkwave.h"

#include <cstdio>
#include <cstdlib>
#include <string>

bool gtkwave_available() {
#ifdef _WIN32
    return system("where gtkwave >NUL 2>&1") == 0;
#else
    return system("which gtkwave >/dev/null 2>&1") == 0;
#endif
}

int open_wave(const char *path) {
    if (!gtkwave_available()) {
        printf("GTKWave was not found.\nInstall GTKWave to visualize %s\n", path);
        return 5;
    }
    std::string cmd;
#ifdef _WIN32
    cmd = std::string("start \"\" gtkwave \"") + path + "\"";
#else
    cmd = std::string("gtkwave \"") + path + "\" &";
#endif
    return system(cmd.c_str());
}
