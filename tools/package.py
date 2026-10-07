"""The release archive: the FOMOD (fomod/) plus everything the game needs under Core/.
  python tools/package.py            -> build/dist/Finishers <version>.zip
Builds build/pack fresh from the sources (DLL, esp, Papyrus, MCM, docs) and refuses a DLL that names this machine.
The version comes from CMakeLists.txt, so the archive, the DLL and the page cannot disagree.
"""
import getpass
import hashlib
import pathlib
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
PACK = ROOT / "build" / "pack"
FOMOD = ROOT / "fomod"
DLL = ROOT / "build-rd" / "Release" / "Finishers.dll"


def main():
    version = re.search(r"VERSION (\d+\.\d+\.\d+)", (ROOT / "CMakeLists.txt").read_text()).group(1)
    if (ROOT / "VERSION").read_text().strip() != version:
        raise SystemExit("VERSION and CMakeLists.txt disagree")
    if getpass.getuser().encode() in DLL.read_bytes():
        raise SystemExit(f"{DLL} names this machine's user")
    if PACK.exists():
        shutil.rmtree(PACK)
    (PACK / "F4SE" / "Plugins").mkdir(parents=True)
    shutil.copy2(DLL, PACK / "F4SE" / "Plugins" / "Finishers.dll")
    shutil.copytree(ROOT / "mcm", PACK / "MCM")
    if "bDetailedLog=0" not in (PACK / "MCM" / "Config" / "Finishers" / "settings.ini").read_text():
        raise SystemExit("the shipped settings.ini must keep the detailed log off")
    subprocess.run([sys.executable, str(ROOT / "tools" / "make_esp.py"), str(PACK / "Finishers.esp")], check=True)
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "scripts" / "build-papyrus.ps1")],
                   check=True)
    docs = PACK / "Docs" / "Finishers"
    docs.mkdir(parents=True)
    for f in ("README.md", "CHANGELOG.md", "LICENSE"):
        shutil.copy2(ROOT / f, docs / f)
    for need in ["F4SE/Plugins/Finishers.dll", "Finishers.esp", "Scripts/Finishers/Debug.pex",
                 "MCM/Config/Finishers/config.json", "MCM/Config/Finishers/settings.ini"]:
        if not (PACK / need).is_file():
            raise SystemExit(f"missing from build/pack: {need}")
    out = ROOT / "build" / "dist" / f"Finishers {version}.zip"
    out.parent.mkdir(parents=True, exist_ok=True)
    if out.exists():
        out.unlink()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in sorted(FOMOD.rglob("*")):
            if f.is_file():
                z.write(f, "fomod/" + f.relative_to(FOMOD).as_posix())
        for f in sorted(PACK.rglob("*")):
            if f.is_file():
                z.write(f, "Core/" + f.relative_to(PACK).as_posix())
    sha = hashlib.sha256(out.read_bytes()).hexdigest()
    dll = hashlib.sha256(DLL.read_bytes()).hexdigest()
    with zipfile.ZipFile(out) as z:
        names = z.namelist()
    print(f"{out} {out.stat().st_size:,} bytes, {len(names)} files, sha256 {sha}\n  Finishers.dll sha256 {dll}")


if __name__ == "__main__":
    main()
