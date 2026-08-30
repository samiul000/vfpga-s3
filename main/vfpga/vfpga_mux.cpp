#include "vfpga_mux.h"

VSignal VMux::mux2(VSignal a, VSignal b, VSignal sel) {
    return (sel & 1) ? b : a;
}

VSignal VMux::mux4(VSignal a, VSignal b, VSignal c, VSignal d, VSignal sel) {
    VSignal idx = sel & 3;
    switch (idx) {
        case 0: return a;
        case 1: return b;
        case 2: return c;
        case 3: return d;
        default: return 0;
    }
}
