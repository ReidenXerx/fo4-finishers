"""Strings -> who references them (RIP-relative lea/mov in .text), and callers of a function.

usage: python re_xref.py str <text>      every code site that loads the string's address
       python re_xref.py call <rva hex>   every direct E8 call to rva
       python re_xref.py rip <rva hex>    every RIP-relative operand that points at rva (globals)
       python re_xref.py func <rva hex>   the function start containing rva (walks back to CC padding)
"""
import struct
import sys

from re_shouldattackkill import base, find_all, ids, img, pe

text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
t0, t1 = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize


def rip_refs(target):
    """4-byte displacements d at offset o such that o + 4 + d (+ small instruction tail) == target."""
    out = []
    tb = img[t0:t1]
    for tail in (0, 1, 4):
        for o in range(0, len(tb) - 4):
            pass
    return out


def rip_scan(target):
    hits = []
    mv = memoryview(img)
    for o in range(t0, t1 - 4):
        d = struct.unpack_from("<i", mv, o)[0]
        # displacement is relative to the END of the instruction; the disp is last (tail 0) or followed by imm8/imm32
        for tail in (0, 1, 4):
            if o + 4 + tail + d == target:
                hits.append((o, tail))
    return hits


def func_start(rva):
    r = rva
    while r > t0:
        if img[r - 1] == 0xCC and img[r - 2] == 0xCC:
            return r
        r -= 1
    return None


if __name__ == "__main__":
    mode, arg = sys.argv[1], sys.argv[2]
    if mode == "str":
        for s in find_all(img, arg.encode() + b"\0"):
            if img[s - 1] != 0:
                continue
            print(f"string at {s:#x}")
            for p in find_all(img, struct.pack("<Q", base + s)):
                print(f"  pointer slot at {p:#x}")
            for o, tail in rip_scan(s):
                f = func_start(o)
                print(f"  code ref at {o:#x} in function {f:#x} id {ids.get(f)}")
    elif mode == "call":
        target = int(arg, 16)
        for o in find_all(img[t0:t1], b"\xE8"):
            o += t0
            d = struct.unpack_from("<i", img, o + 1)[0]
            if o + 5 + d == target:
                f = func_start(o)
                print(f"call at {o:#x} in function {f:#x} id {ids.get(f)}")
    elif mode == "rip":
        target = int(arg, 16)
        for o, tail in rip_scan(target):
            f = func_start(o)
            print(f"ref at {o:#x} in function {f:#x} id {ids.get(f)}")
    elif mode == "func":
        f = func_start(int(arg, 16))
        print(f"{f:#x} id {ids.get(f)}")
