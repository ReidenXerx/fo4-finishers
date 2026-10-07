"""Find the ShouldAttackKill condition function in the AE exe and disassemble it.

Script command entries (F4SE ObScriptCommand): longName +0, shortName +8, opcode +0x10, helpText +0x18,
needsParent +0x20, numParams +0x22, params +0x28, execute +0x30, parse +0x38, eval +0x40, flags +0x48.
"""
import struct
import sys

import capstone
import pefile

EXE = r"D:\SteamFreeGames\Fallout 4 AE\Fallout4.exe"
BIN = r"D:\SteamFreeGames\Fallout 4 AE\Data\F4SE\Plugins\version-1-11-240-0.bin"

pe = pefile.PE(EXE, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
img = pe.get_memory_mapped_image()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = False

ids = {}
d = open(BIN, "rb").read()
cnt = struct.unpack_from("<Q", d, 0)[0]
for k in range(cnt):
    i, o = struct.unpack_from("<QQ", d, 8 + 16 * k)
    ids.setdefault(o, i)


def find_all(blob, needle):
    out, start = [], 0
    while True:
        j = blob.find(needle, start)
        if j < 0:
            return out
        out.append(j)
        start = j + 1


def entry_for(name):
    hits = [r for r in find_all(img, name.encode() + b"\0") if img[r - 1] == 0]
    for srva in hits:
        ptr = struct.pack("<Q", base + srva)
        for prva in find_all(img, ptr):
            eval_va = struct.unpack_from("<Q", img, prva + 0x40)[0]
            exec_va = struct.unpack_from("<Q", img, prva + 0x30)[0]
            opcode = struct.unpack_from("<I", img, prva + 0x10)[0]
            nparams = struct.unpack_from("<H", img, prva + 0x22)[0]
            yield prva, opcode, nparams, exec_va, eval_va


def dis(rva, n=400, label=""):
    code = img[rva:rva + n]
    idn = ids.get(rva)
    print(f"---- {label} rva {rva:#x} id {idn}")
    calls = []
    for ins in md.disasm(code, base + rva):
        print(f"  {ins.address - base:#x}: {ins.mnemonic} {ins.op_str}")
        if ins.mnemonic == "call" and ins.op_str.startswith("0x"):
            calls.append(int(ins.op_str, 16) - base)
        if ins.mnemonic in ("ret", "int3") or ins.mnemonic == "jmp" and ins.op_str.startswith("0x") and False:
            break
    return calls


if __name__ == "__main__":
    name = sys.argv[1] if len(sys.argv) > 1 else "ShouldAttackKill"
    depth = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    for prva, op, npar, ex, ev in entry_for(name):
        print(f"entry rva {prva:#x} opcode {op:#x} params {npar} execute {ex - base:#x} eval {ev - base:#x}")
        seen = set()
        frontier = [ev - base]
        for level in range(depth + 1):
            nxt = []
            for f in frontier:
                if f in seen or not (0 < f < len(img)):
                    continue
                seen.add(f)
                nxt += dis(f, 600, f"level {level}")
            frontier = nxt
