# Finishers - More and Varied Kill Moves

A Fallout 4 F4SE plugin: melee kill moves happen far more often, and every kill move a weapon can do comes up in turn.

## What it does

**More kill moves.** The game plays a kill move only when it predicts that this exact swing kills
(`ShouldAttackKill`: a predicted hit with the fatal flag), and a fighter only tries a grab or kill move through its
combat style's special attack (raiders: one try in ten, every five seconds). Finishers:

- raises the least chance of such a try to 50%, every 3 seconds (`fCombatSpecialAttackChanceMin`,
  `fCombatSpecialAttackDelayTime`; a style already above keeps its own);
- lets a target at 30% health or less be finished on half the swings. The victim is left at 1 health when its kill
  move starts, so the paired blow finishes it, and is killed if it still stands when the move ends;
- never finishes the player early: against you it stays the game's own rule.

**Every kill move in turn.** Under each weapon group the game plays the first kill move whose `GetRandomPercent` roll
passes, so the first in line wins most (Far Harbor's throat slash took half of all one-handed blade kill moves). At
game start Finishers orders each kill-move list (the most specific moves first) and sets each roll to
1 / (1 + the later moves that can play whenever it can), so every move a weapon can reach is equally likely. Lists of
paired moves that do not kill are left alone, so they never crowd the kill moves out.

Every number is a slider in MCM (Data/MCM/Settings/Finishers.ini). MCM > Finishers > Testing > "Start a fight here"
spawns two melee raiders that fight only each other.

## Requirements

- F4SE
- Runtime Database (f4rd-runtime.bin): one DLL for 1.10.163, next-gen and the Anniversary Edition
- MCM (optional: the settings page and the test button)

Install with Vortex or MO2; manual installs are not supported.

## Building

CMake + vcpkg (`x64-windows-static-md`), CommonLibF4RD as a submodule:

    cmake -B build-rd -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
    cmake --build build-rd --config Release
    python tools/make_fomod.py
    python tools/package.py

`tools/re_shouldattackkill.py` and `tools/re_xref.py` are the disassembly probes the gate was read with.

## License

MIT
