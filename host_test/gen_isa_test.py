#!/usr/bin/env python3
"""Generate a full-coverage RV32I test program for the VFPGA-S3 emulator.

Emits: C arrays (program, expected, names) + exact executed-step count.
Every encoding is round-trip decode-checked. Expected values are computed
with 32-bit masking to match the emulator's unsigned wrap semantics.
"""
import sys

prog = []          # emitted words
tests = []         # (name, slot, expected)
SLOT_BASE = 0x1200

def addr():
    return len(prog) * 4

def emit(w):
    assert 0 <= w < 2**32, hex(w)
    prog.append(w)

# ---------------- encoders (with round-trip asserts) ----------------
def encR(f7, rs2, rs1, f3, rd):
    w = (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | 0x33
    assert ((w >> 25) & 0x7F) == f7 and ((w >> 20) & 0x1F) == rs2
    assert ((w >> 15) & 0x1F) == rs1 and ((w >> 12) & 0x7) == f3
    assert ((w >> 7) & 0x1F) == rd and (w & 0x7F) == 0x33
    return w

def encI(imm12, rs1, f3, rd, op=0x13):
    imm = imm12 & 0xFFF
    w = (imm << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op
    assert ((w >> 20) & 0xFFF) == imm and ((w >> 15) & 0x1F) == rs1
    assert ((w >> 12) & 0x7) == f3 and ((w >> 7) & 0x1F) == rd
    return w

def encS(imm12, rs2, rs1, f3):
    imm = imm12 & 0xFFF
    w = (((imm >> 5) & 0x7F) << 25) | (rs2 << 20) | (rs1 << 15) | \
        (f3 << 12) | ((imm & 0x1F) << 7) | 0x23
    assert ((((w >> 25) & 0x7F) << 5) | ((w >> 7) & 0x1F)) == imm
    return w

def encB(offset, rs2, rs1, f3):
    assert offset % 2 == 0 and -4096 <= offset < 4096
    imm = offset & 0x1FFF
    w = ((((imm >> 12) & 1) << 31) | ((((imm >> 5) & 0x3F) << 25)) |
         (rs2 << 20) | (rs1 << 15) | (f3 << 12) |
         ((((imm >> 1) & 0xF) << 8)) | ((((imm >> 11) & 1) << 7)) | 0x63)
    # round-trip
    got = ((((w >> 31) & 1) << 12) | (((w >> 25) & 0x3F) << 5) |
           (((w >> 8) & 0xF) << 1) | (((w >> 7) & 1) << 11))
    s = got if not (got & 0x1000) else got - 0x2000
    assert s == offset, (hex(w), s, offset)
    return w

def encU(imm20, rd, op):
    w = ((imm20 & 0xFFFFF) << 12) | (rd << 7) | op
    assert ((w >> 12) & 0xFFFFF) == (imm20 & 0xFFFFF)
    return w

def encJ(offset, rd):
    assert offset % 2 == 0 and -(1 << 20) <= offset < (1 << 20)
    imm = offset & 0x1FFFFF
    w = ((((imm >> 20) & 1) << 31) | ((((imm >> 1) & 0x3FF) << 21)) |
         ((((imm >> 11) & 1) << 20)) | ((((imm >> 12) & 0xFF) << 12)) |
         (rd << 7) | 0x6F)
    got = ((((w >> 31) & 1) << 20) | (((w >> 21) & 0x3FF) << 1) |
           (((w >> 20) & 1) << 11) | (((w >> 12) & 0xFF) << 12))
    s = got if not (got & 0x100000) else got - 0x200000
    assert s == offset, (hex(w), s, offset)
    return w

def M32(v):
    return v & 0xFFFFFFFF

def sext(v, bits):
    v &= (1 << bits) - 1
    return v - (1 << bits) if v & (1 << (bits - 1)) else v

# ---------------- helpers ----------------
slot = 0

def op_store_result(reg=7):
    """sw x7 -> next result slot. Returns slot index."""
    global slot
    s = slot
    emit(encS(s * 4, reg, 6, 0x2))   # sw, x6 = 0x1200 base
    slot += 1
    return s

def test_r(name, f7, rs2, rs1, f3, a, b, fn):
    emit(encR(f7, rs2, rs1, f3, 7))
    s = op_store_result()
    tests.append((name, s, M32(fn(a, b))))

def test_i(name, imm, rs1, f3, rd, a, fn):
    emit(encI(imm, rs1, f3, rd))
    s = op_store_result(rd)
    tests.append((name, s, M32(fn(a, sext(imm, 12)))))

# ---------------- setup ----------------
X1, X2, X3, X4, MEM, RES = 1, 2, 3, 4, 5, 6
emit(encI(12, 0, 0, X1))        # x1 = 12
emit(encI(10, 0, 0, X2))        # x2 = 10
emit(encI(-1, 0, 0, X3))        # x3 = 0xFFFFFFFF
emit(encU(0x80000, X4, 0x37))   # x4 = 0x80000000
emit(encU(0x1, MEM, 0x37))      # x5 = 0x1000 (data scratch)
emit(encU(0x1, RES, 0x37))      # x6 = 0x1000 ...
emit(encI(0x200, RES, 0, RES))  # ... + 0x200 = 0x1200 (results table)

A1, A2, A3, A4 = 12, 10, 0xFFFFFFFF, 0x80000000

# ---------------- R-type (10) ----------------
test_r("ADD",  0x00, X2, X1, 0, A1, A2, lambda a, b: a + b)          # 22
test_r("SUB",  0x20, X2, X1, 0, A1, A2, lambda a, b: a - b)          # 2
test_r("SLL",  0x00, X2, X1, 1, A1, A2, lambda a, b: (a & 0xFFFFFFFF) << (b & 31))
test_r("SLT",  0x00, X1, X3, 2, A3, A1, lambda a, b: 1 if sext(a, 32) < sext(b, 32) else 0)
test_r("SLTU", 0x00, X3, X1, 3, A1, A3, lambda a, b: 1 if (a & 0xFFFFFFFF) < (b & 0xFFFFFFFF) else 0)
test_r("XOR",  0x00, X2, X1, 4, A1, A2, lambda a, b: a ^ b)
test_r("SRL",  0x00, X2, X3, 5, A3, A2, lambda a, b: (a & 0xFFFFFFFF) >> (b & 31))
test_r("SRA",  0x20, X2, X4, 5, A4, A2, lambda a, b: sext(a, 32) >> (b & 31))
test_r("OR",   0x00, X2, X1, 6, A1, A2, lambda a, b: a | b)
test_r("AND",  0x00, X2, X1, 7, A1, A2, lambda a, b: a & b)

# ---------------- I-type (9) ----------------
test_i("ADDI",  5,    X1, 0, 7, A1, lambda a, i: a + i)
test_i("SLLI",  3,    X1, 1, 7, A1, lambda a, i: (a & 0xFFFFFFFF) << (i & 31))
test_i("SLTI",  20,   X1, 2, 7, A1, lambda a, i: 1 if sext(a, 32) < i else 0)
test_i("SLTIU", 20,   X3, 3, 7, A3, lambda a, i: 1 if (a & 0xFFFFFFFF) < (i & 0xFFFFFFFF) else 0)
test_i("XORI",  0xFF, X1, 4, 7, A1, lambda a, i: a ^ (i & 0xFFFFFFFF))
test_i("SRLI",  4,    X3, 5, 7, A3, lambda a, i: (a & 0xFFFFFFFF) >> (i & 31))
test_i("SRAI",  0x404, X3, 5, 7, A3, lambda a, i: sext(a, 32) >> (i & 31))
test_i("ORI",   0xFF, X1, 6, 7, A1, lambda a, i: a | (i & 0xFFFFFFFF))
test_i("ANDI",  0x0F, X1, 7, 7, A1, lambda a, i: a & (i & 0xFFFFFFFF))

# ---------------- stores + loads ----------------
for val, off in ((0xDD, 0), (0xCC, 1), (0xBB, 2), (0xAA, 3)):
    emit(encI(val, 0, 0, 7))
    emit(encS(off, 7, MEM, 0x0))          # sb -> 0x1000+off
emit(encI(0, MEM, 0x0, 7, 0x03))          # lb
s = op_store_result(); tests.append(("LB", s, 0xFFFFFFDD))
emit(encI(0, MEM, 0x4, 7, 0x03))          # lbu
s = op_store_result(); tests.append(("LBU", s, 0xDD))
emit(encI(2, MEM, 0x1, 7, 0x03))          # lh @0x1002 = 0xAABB
s = op_store_result(); tests.append(("LH", s, 0xFFFFAABB))
emit(encI(2, MEM, 0x5, 7, 0x03))          # lhu
s = op_store_result(); tests.append(("LHU", s, 0xAABB))
emit(encI(0, MEM, 0x2, 7, 0x03))          # lw = 0xAABBCCDD
s = op_store_result(); tests.append(("LW", s, 0xAABBCCDD))
emit(encU(0x1, 7, 0x37))                  # x7 = 0x1000
emit(encI(0x234, 7, 0, 7))                # x7 = 0x1234
emit(encS(0x10, 7, MEM, 0x1))             # sh 0x1234 @0x1010
emit(encI(0x10, MEM, 0x1, 7, 0x03))       # lh back
s = op_store_result(); tests.append(("SH", s, 0x1234))

# ---------------- branches: taken x6 + 1 not-taken ----------------
def branch_taken(name, rs2, rs1, f3):
    emit(encI(0, 0, 0, 7))
    s = op_store_result()                 # sentinel 0
    emit(encB(8, rs2, rs1, f3))            # taken: skip next
    emit(encI(99, 0, 0, 7))                # trap (skipped)
    emit(encS(s * 4, 7, RES, 0x2))        # store sentinel (stays 0)
    tests.append((name, s, 0))

branch_taken("BEQ",  X1, X1, 0x0)
branch_taken("BNE",  X2, X1, 0x1)
branch_taken("BLT",  X1, X3, 0x4)
branch_taken("BGE",  X3, X1, 0x5)
branch_taken("BLTU", X3, X1, 0x6)
branch_taken("BGEU", X1, X3, 0x7)
# not-taken sanity: BNE x1,x1 falls through, stores 77
emit(encB(8, X1, X1, 0x1))
emit(encI(77, 0, 0, 7))
s = op_store_result(); tests.append(("BNE-nt", s, 77))

# ---------------- jumps ----------------
jal_at = addr()
emit(encJ(8, 7))                          # rd = pc+4, skip trap
emit(encI(99, 0, 0, 7))                   # trap (skipped)
s = op_store_result(); tests.append(("JAL", s, jal_at + 4))
jalr_at = addr()
landing = jalr_at + 8
emit(encI(landing, 0, 0, 7, 0x67))        # jalr x7, landing(x0): rd=pc+4
emit(encI(99, 0, 0, 7))                   # trap (skipped)
assert addr() == landing
s = op_store_result(); tests.append(("JALR", s, jalr_at + 4))

# ---------------- upper immediates ----------------
auipc_at = addr()
emit(encU(0x0, 7, 0x17))                  # auipc x7, 0 -> pc
s = op_store_result(); tests.append(("AUIPC", s, auipc_at))
emit(encU(0x12345, 7, 0x37))              # lui -> 0x12345000
s = op_store_result(); tests.append(("LUI", s, 0x12345000))

# ---------------- step count ----------------
skipped = 6 + 1 + 1      # 6 taken branches + JAL + JALR
executed = len(prog) - skipped

# ---------------- emit header ----------------
import os
out_path = os.environ.get("ISA_OUT",
    r"D:\A_Robotics\vfpga_s3\src\isa_test_vectors.h")
lines = []
lines.append("#pragma once")
lines.append("// Generated by host_test/gen_isa_test.py - do not hand-edit.")
lines.append("// %d words, %d executed steps, %d result checks" %
             (len(prog), executed, len(tests)))
lines.append("static const uint32_t ISA_PROGRAM[] = {")
for i, w in enumerate(prog):
    lines.append("    0x%08X, // [%d] @0x%03X" % (w, i, i * 4))
lines.append("};")
lines.append("static const uint32_t ISA_EXPECTED[] = {")
for name, s, exp in tests:
    lines.append("    0x%08X, // slot %2d %-7s" % (exp, s, name))
lines.append("};")
lines.append("static const char *ISA_NAMES[] = {")
for name, s, exp in tests:
    lines.append('    "%s",' % name)
lines.append("};")
lines.append("static const int ISA_STEPS = %d;" % executed)
lines.append("static const int ISA_CHECKS = %d;" % len(tests))
with open(out_path, "w") as f:
    f.write("\n".join(lines) + "\n")
print("wrote %s" % out_path)

# sanity: slot indices must be dense 0..N-1
assert [s for _, s, _ in tests] == list(range(len(tests))), "slots not dense!"
# sanity: distinct RV32I mnemonics covered (stores counted, SH via sh+lh)
mn = [n for n, _, _ in tests]
assert len(mn) == 36
print("// OK: 36 result slots, 37 distinct instructions "
      "(ADD SUB SLL SLT SLTU XOR SRL SRA OR AND ADDI SLLI SLTI SLTIU XORI "
      "SRLI SRAI ORI ANDI LB LH LW LBU LHU SB SH SW BEQ BNE BLT BGE BLTU "
      "BGEU JAL JALR LUI AUIPC)", file=sys.stderr)
