#include "bitparallel.h"

VSignal BitParallel::op_and(VSignal a, VSignal b) { return a & b; }
VSignal BitParallel::op_or(VSignal a, VSignal b) { return a | b; }
VSignal BitParallel::op_xor(VSignal a, VSignal b) { return a ^ b; }
VSignal BitParallel::op_not(VSignal a) { return ~a; }
VSignal BitParallel::op_nand(VSignal a, VSignal b) { return ~(a & b); }
VSignal BitParallel::op_nor(VSignal a, VSignal b) { return ~(a | b); }
VSignal BitParallel::op_xnor(VSignal a, VSignal b) { return ~(a ^ b); }
VSignal BitParallel::op_mux(VSignal a, VSignal b, VSignal sel) { return (sel & 1) ? b : a; }
