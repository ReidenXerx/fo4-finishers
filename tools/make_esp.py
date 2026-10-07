"""Finishers.esp: a light plugin (master Fallout4.esm) that overrides nothing. It carries the MCM page, and two
factions for the MCM test button "Start a fight here" (papyrus/Finishers/Debug.psc): two raiders spawned there are
put one in each, enemies of each other and of nobody else. Everything the mod does is in the F4SE plugin.

    python tools/make_esp.py [out.esp]

Form ids are PERMANENT: FinishersFightA 0x800, FinishersFightB 0x801 (Debug.psc reads them by id).
"""
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FIGHT_A, FIGHT_B = 0x800, 0x801


def field(sig, data):
    return sig.encode("ascii") + struct.pack("<H", len(data)) + data


def record(sig, fid, body, flags=0):
    return sig.encode() + struct.pack("<III", len(body), flags, fid) + struct.pack("<IHH", 0, 131, 0) + body


def group(label, body):
    return b"GRUP" + struct.pack("<I", 24 + len(body)) + label.encode() + struct.pack("<IIHH", 0, 0, 131, 0) + body


def faction(fid, edid, enemy):
    own = 0x01000000 | fid
    body = field("EDID", edid.encode() + b"\0")
    body += field("XNAM", struct.pack("<IiI", own, 0, 2))                    # its own members: allies
    body += field("XNAM", struct.pack("<IiI", 0x01000000 | enemy, 0, 1))     # the other side: enemies
    body += field("DATA", struct.pack("<I", 0))
    body += field("CRVA", bytes(20)) + field("VENV", bytes(12))
    return record("FACT", own, body)


def main():
    out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build" / "pack" / "Finishers.esp")
    facts = faction(FIGHT_A, "FinishersFightA", FIGHT_B) + faction(FIGHT_B, "FinishersFightB", FIGHT_A)
    header = field("HEDR", struct.pack("<fiI", 1.0, 2, 0x802))
    header += field("CNAM", b"Dudu'sButt\0") + field("SNAM", b"Finishers - More and Varied Kill Moves\0")
    header += field("MAST", b"Fallout4.esm\0") + field("DATA", struct.pack("<Q", 0))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(record("TES4", 0, header, flags=0x200) + group("FACT", facts))
    print(f"{out}: {out.stat().st_size} bytes, light, 2 factions")


if __name__ == "__main__":
    main()
