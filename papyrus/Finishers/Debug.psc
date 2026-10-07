Scriptname Finishers:Debug Hidden
{Finishers' test button (MCM > Testing > "Start a fight here"): two melee raiders appear in front of the player and
 fight each other, and only each other -- they leave every faction and join one of the plugin's two enemy factions,
 so they ignore the player. Watch for kill moves; the log (Detailed log on) says what Finishers allowed.}

Function SpawnFight() Global
	Actor player = Game.GetPlayer()
	ActorBase raider = Game.GetFormFromFile(0x019562, "Fallout4.esm") as ActorBase    ; LvlRaiderMelee
	Faction sideA = Game.GetFormFromFile(0x000800, "Finishers.esp") as Faction
	Faction sideB = Game.GetFormFromFile(0x000801, "Finishers.esp") as Faction
	Form marker = Game.GetFormFromFile(0x00003B, "Fallout4.esm")                      ; XMarker
	If !raider || !sideA || !sideB || !marker
		Debug.Notification("Finishers: the test fight needs Finishers.esp enabled.")
		Return
	EndIf
	If player.IsInCombat()
		Debug.Notification("Finishers: not during your own fight.")
		Return
	EndIf
	Float heading = player.GetAngleZ()
	Float x = player.GetPositionX() + 450.0 * Math.Sin(heading)
	Float y = player.GetPositionY() + 450.0 * Math.Cos(heading)
	Float z = player.GetPositionZ()
	; across the player's view, 160 apart
	Float sx = 80.0 * Math.Cos(heading)
	Float sy = -80.0 * Math.Sin(heading)
	Actor a = Spawn(player, raider, marker, x - sx, y - sy, z)
	Actor b = Spawn(player, raider, marker, x + sx, y + sy, z)
	If !a || !b
		Debug.Notification("Finishers: could not place the raiders here.")
		Return
	EndIf
	a.RemoveFromAllFactions()
	b.RemoveFromAllFactions()
	a.AddToFaction(sideA)
	b.AddToFaction(sideB)
	a.StartCombat(b, False)
	b.StartCombat(a, False)
	Debug.Notification("Finishers: two raiders fight in front of you.")
EndFunction

Actor Function Spawn(Actor akPlayer, ActorBase akBase, Form akMarker, Float afX, Float afY, Float afZ) Global
	ObjectReference spot = akPlayer.PlaceAtMe(akMarker, 1, False, False, True)
	spot.SetPosition(afX, afY, afZ)
	Actor who = spot.PlaceActorAtMe(akBase, 4, None)
	spot.Disable(False)
	spot.Delete()
	Return who
EndFunction
