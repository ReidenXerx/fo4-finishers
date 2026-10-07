"""Finishers.esp: an empty light plugin (master Fallout4.esm, no records). It only carries the MCM page: everything
the mod does is in the F4SE plugin (src/main.cpp), so it takes no load-order slot and overrides nothing.

    python tools/make_esp.py [out.esp]
"""
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def field(sig, data):
    return sig.encode("ascii") + struct.pack("<H", len(data)) + data


def record(sig, fid, body, flags=0):
    return sig.encode() + struct.pack("<III", len(body), flags, fid) + struct.pack("<IHH", 0, 131, 0) + body


def main():
    out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build" / "pack" / "Finishers.esp")
    header = field("HEDR", struct.pack("<fiI", 1.0, 0, 0x800))
    header += field("CNAM", b"Dudu'sButt\0") + field("SNAM", b"Finishers - More and Varied Kill Moves\0")
    header += field("MAST", b"Fallout4.esm\0") + field("DATA", struct.pack("<Q", 0))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(record("TES4", 0, header, flags=0x200))
    print(f"{out}: {out.stat().st_size} bytes, light, no records")


if __name__ == "__main__":
    main()
