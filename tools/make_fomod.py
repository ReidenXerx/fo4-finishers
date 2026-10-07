"""fomod/ModuleConfig.xml per nexus-tools/docs/FOMOD-STANDARD.md (UTF-8 with BOM, schema 5.0, one step per feature).

    python tools/make_fomod.py

Nothing a FOMOD can see is required: Finishers.esp masters only Fallout4.esm, and one DLL runs on 1.10.163, next-gen
and the Anniversary Edition through Runtime Database. F4SE, Runtime Database and MCM are named on the first page.
"""
import html
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent

SETUP = """Fallout 4: 1.10.163, next-gen or the Anniversary Edition, all work (through Runtime Database).
F4SE: check this yourself. Finishers is an F4SE plugin: start the game through f4se_loader.exe (f4se.silverlock.org).
Runtime Database: check this yourself. It finds the game's functions; without its f4rd-runtime.bin the game stops at launch (Nexus 108394).
MCM: check this yourself. Optional: Mod Configuration Menu gives Finishers its settings page and the test fight button; without it everything runs on its defaults.
Finishers.esp: a light plugin that takes no load-order slot and changes no game record."""

STEPS = [
    ("More kill moves", "often.jpg",
     "The game plays a kill move only when it predicts that this exact swing kills, and a raider even tries a grab or "
     "kill move one time in ten, every five seconds. Finishers raises that to at least half the time, every three "
     "seconds, and lets a target at 30% health or less be finished early on half the swings: the victim dies in the "
     "animation. NPCs finish NPCs and you finish them; against you it stays the game's own rule, only a blow that would "
     "kill you anyway. Every number is a slider in MCM."),
    ("Every kill move in turn", "variety.jpg",
     "Each weapon has a list of kill moves, and the game plays the first one whose dice roll passes, so the first in "
     "line won most of them (Far Harbor's throat slash took half of all one-handed blade kill moves). At every game "
     "start Finishers orders each list and evens out the rolls, so every kill move your weapon can do is equally likely: "
     "20 lists, 61 kill moves, creatures included. Paired moves that do not kill keep the game's odds."),
]


def option(name, text, image, flag):
    return f"""    <installStep name="{html.escape(name)}">
      <optionalFileGroups order="Explicit">
        <group name="{html.escape(name)}" type="SelectAll">
          <plugins order="Explicit">
            <plugin name="{html.escape(name)}">
              <description>{html.escape(text)}</description>
              <image path="fomod/images/{image}"/>
              <conditionFlags><flag name="{flag}">seen</flag></conditionFlags>
              <typeDescriptor><type name="Required"/></typeDescriptor>
            </plugin>
          </plugins>
        </group>
      </optionalFileGroups>
    </installStep>
"""


def main():
    steps = option("Checking your setup", SETUP, "thumb.jpg", "fin_page_1")
    for i, (name, image, text) in enumerate(STEPS, 2):
        steps += option(name, text, image, f"fin_page_{i}")
    xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<config xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
        xsi:noNamespaceSchemaLocation="http://qconsulting.ca/fo3/ModConfig5.0.xsd">
  <moduleName>Finishers</moduleName>
  <moduleImage path="fomod/images/thumb.jpg"/>

  <!-- FOMOD standard rule 2: nothing a FOMOD can see is required (tools/make_fomod.py). -->

  <requiredInstallFiles>
    <folder source="Core" destination="" priority="0"/>
  </requiredInstallFiles>

  <installSteps order="Explicit">
{steps}  </installSteps>
</config>
"""
    out = ROOT / "fomod" / "ModuleConfig.xml"
    out.write_text(xml, encoding="utf-8-sig")
    info = """<?xml version="1.0" encoding="UTF-8"?>
<fomod>
  <Name>Finishers - More and Varied Kill Moves</Name>
  <Author>Dudu'sButt</Author>
  <Version>{version}</Version>
  <Website>https://github.com/ReidenXerx/fo4-finishers</Website>
  <Description>More melee kill moves, and every one of them in turn.</Description>
</fomod>
""".format(version=(ROOT / "VERSION").read_text().strip())
    (ROOT / "fomod" / "info.xml").write_text(info, encoding="utf-8-sig")
    print(out, "and info.xml")


if __name__ == "__main__":
    main()
