// Finishers: melee kill moves more often, and every one of them in turn.
//
// THE GATE (measured 2026-10-07 on 1.11.240, fo4-melee-research/re_shouldattackkill.py). Every human kill move hangs
// under the idle MeleeRightSync_KillRoot (Fallout4.esm 100AEB), whose first condition is ShouldAttackKill == 1 (script
// function 678). Its condition function (AE 0x5AA0B0) calls Actor::ShouldAttackKill (AE 0xCCFA20), which:
//   - refuses an essential target, a protected one unless the player swings, and an invulnerable one (0xC5B410);
//   - builds the attacker's hit on the target with HitData::kPredictDamage and answers kFatal: "this swing kills".
// So a kill move is only ever the exact killing blow, and most killing blows come from a gun or a second attacker.
// MORE OFTEN: the condition function is wrapped (its entry in the script function table, found by name, so one
// build serves every runtime). When the game says no, a target at or under fHealth percent of its health becomes a
// kill-move target on an fChance roll -- never the player (owner 10-07: the mod never kills the player early).
// That swing need not be lethal, so the victim is made sure to die: when its kill move starts, it is left at 1
// health for the paired blow to finish, and killed outright if it is still standing when the move ends.
//
// THE CHOICE (Nexus mod 22489's reading, the data agrees): under each weapon group the game walks the kill moves
// first to last and plays the first whose GetRandomPercent roll passes, so the first ones win (Far Harbor's 50%
// throat slash before everything else). VARIETY: at game start, every paired group (siblings whose animation events
// start with "pa_") is put in order, the most specific moves first (more conditions, e.g. "needs a stabbing weapon"),
// and each one gets the roll that makes every move it can reach equally likely: 1 / (1 + the later moves that can
// play whenever it can). The order changes by moving the contents (conditions, animation) between the group's own
// forms; no form is added or removed. A group with an unusual roll (a global, ">=") is left as it is.

#include "PCH.h"

namespace
{
	// ---- settings (Data/MCM/Settings/Finishers.ini over Data/MCM/Config/Finishers/settings.ini) ----------------------

	struct Settings
	{
		bool  more{ true };          // kill moves before the last blow
		float health{ 30.0f };       // percent of the target's health at or under which a swing may become one
		float chance{ 50.0f };       // percent
		bool  player{ true };        // the player's own swings too
		bool  variety{ true };       // every kill move in turn (applied at game start)
		bool  detailedLog{ false };
		float attempts{ 50.0f };     // percent: the least chance a melee fighter tries a special move (grab, paired move, kill move)
		float attemptDelay{ 3.0f };  // seconds between such tries
	};

	std::atomic_bool                   g_attemptsDirty{ true };
	std::mutex                         g_settingsLock;
	Settings                           g_settings;
	std::filesystem::file_time_type    g_settingsStamp{};
	std::chrono::steady_clock::time_point g_settingsChecked{};
	bool                               g_settingsRead{ false };

	void ReadIni(const std::filesystem::path& a_path, Settings& a_out)
	{
		std::ifstream in(a_path);
		std::string   line;
		while (std::getline(in, line)) {
			std::erase_if(line, [](char c) { return c == '\r' || c == ' ' || c == '\t'; });
			const auto eq = line.find('=');
			if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[' || eq == std::string::npos) {
				continue;
			}
			const auto  k = line.substr(0, eq);
			const int   value = std::atoi(line.c_str() + eq + 1);
			const float real = static_cast<float>(std::atof(line.c_str() + eq + 1));
			auto        is = [&](const char* a_name) { return _stricmp(k.c_str(), a_name) == 0; };
			if (is("bMore")) {
				a_out.more = value != 0;
			} else if (is("fHealth")) {
				a_out.health = std::clamp(real, 0.0f, 100.0f);
			} else if (is("fChance")) {
				a_out.chance = std::clamp(real, 0.0f, 100.0f);
			} else if (is("bPlayer")) {
				a_out.player = value != 0;
			} else if (is("bVariety")) {
				a_out.variety = value != 0;
			} else if (is("fAttempts")) {
				a_out.attempts = std::clamp(real, 0.0f, 100.0f);
			} else if (is("fAttemptDelay")) {
				a_out.attemptDelay = std::clamp(real, 0.5f, 30.0f);
			} else if (is("bDetailedLog")) {
				a_out.detailedLog = value != 0;
			}
		}
	}

	// Re-read at most once a second, and only when the player's file changed.
	Settings CurrentSettings()
	{
		std::scoped_lock lock{ g_settingsLock };
		const auto       now = std::chrono::steady_clock::now();
		if (g_settingsRead && now - g_settingsChecked < std::chrono::seconds(1)) {
			return g_settings;
		}
		g_settingsChecked = now;
		const std::filesystem::path user = "Data/MCM/Settings/Finishers.ini";
		std::error_code             ec;
		const auto                  stamp = std::filesystem::last_write_time(user, ec);
		if (g_settingsRead && (ec ? g_settingsStamp == std::filesystem::file_time_type{} : stamp == g_settingsStamp)) {
			return g_settings;
		}
		Settings fresh;
		ReadIni("Data/MCM/Config/Finishers/settings.ini", fresh);
		if (!ec) {
			ReadIni(user, fresh);
			g_settingsStamp = stamp;
		} else {
			g_settingsStamp = {};
		}
		g_settingsRead = true;
		logger::info("settings: more kill moves {} (at or under {:.0f}% health, {:.0f}% of swings, the player's swings {}), "
					 "special moves tried at least {:.0f}% every {:.1f} s, variety {}, detailed log {}",
			fresh.more, fresh.health, fresh.chance, fresh.player, fresh.attempts, fresh.attemptDelay, fresh.variety, fresh.detailedLog);
		g_settings = fresh;
		g_attemptsDirty = true;
		return g_settings;
	}

	// ---- how often a fighter tries ----------------------------------------------------------------------------------------
	//
	// A paired move (kill move or not) is only ever tried from the combat behaviour "special attack" (AE 0x101CA60, read
	// 10-08): its chance is lerp(fCombatSpecialAttackChanceMin, fCombatSpecialAttackChanceMax, the combat style's Special
	// Attack Mult), one try per fCombatSpecialAttackDelayTime. The game's settings are 0, 1 and 5 s, and raiders' styles
	// carry 0.1: one try in ten, every five seconds -- the first test fight asked for a kill move twice in twenty minutes.
	// The floor is raised (fAttempts) and the wait shortened (fAttemptDelay); a style already above the floor keeps its own.

	struct Tuned
	{
		RE::Setting* setting{ nullptr };
		float        vanilla{ 0.0f };
	};
	Tuned g_chanceMin;
	Tuned g_delay;

	RE::Setting* GameSetting(std::string_view a_name)
	{
		auto* gsc = RE::GameSettingCollection::GetSingleton();
		if (!gsc) {
			return nullptr;
		}
		for (auto& entry : gsc->settings) {
			if (auto* setting = entry.second; setting && _stricmp(std::string(setting->GetKey()).c_str(), std::string(a_name).c_str()) == 0) {
				return setting;
			}
		}
		return nullptr;
	}

	void ApplyAttempts(const Settings& a_settings)
	{
		if (!g_attemptsDirty.exchange(false)) {
			return;
		}
		auto find = [](Tuned& a_t, std::string_view a_name) {
			if (!a_t.setting) {
				a_t.setting = GameSetting(a_name);
				if (a_t.setting) {
					a_t.vanilla = a_t.setting->GetFloat();
				}
			}
			return a_t.setting != nullptr;
		};
		if (!find(g_chanceMin, "fCombatSpecialAttackChanceMin") || !find(g_delay, "fCombatSpecialAttackDelayTime")) {
			logger::warn("the special attack settings are missing - fighters try paired moves as often as in the game");
			return;
		}
		const auto& s = a_settings;
		const float floor = s.more ? std::max(g_chanceMin.vanilla, s.attempts / 100.0f) : g_chanceMin.vanilla;
		const float wait = s.more ? s.attemptDelay : g_delay.vanilla;
		g_chanceMin.setting->SetFloat(floor);
		g_delay.setting->SetFloat(wait);
		logger::info("special moves: tried at least {:.0f}% of the time (game {:.0f}%), every {:.1f} s (game {:.1f} s)", floor * 100.0f,
			g_chanceMin.vanilla * 100.0f, wait, g_delay.vanilla);
	}

	// ---- the gate ------------------------------------------------------------------------------------------------------

	constexpr std::uint32_t kEssential = 1u << 18;   // Actor::BOOL_FLAGS, +0x43C (read by the game's own check, 0xC5B410)
	constexpr std::uint32_t kProtected = 1u << 19;
	constexpr std::uint32_t kInKillMove = 1u << 14;

	using ConditionFn = RE::SCRIPT_FUNCTION::ConditionFunction_t;
	ConditionFn* g_vanilla = nullptr;

	std::mt19937 g_rng{ std::random_device{}() };

	// One answer per attacker and target for two seconds: the idle tree asks again at every node of one attack.
	struct Verdict
	{
		bool                                  yes;
		std::chrono::steady_clock::time_point until;
	};
	std::mutex                                  g_lock;
	std::unordered_map<std::uint64_t, Verdict>  g_verdicts;

	// A kill move we allowed early: its victim must die.
	struct Pending
	{
		RE::ObjectRefHandle                   attacker;
		RE::ObjectRefHandle                   target;
		std::uint32_t                         attackerID;
		std::uint32_t                         targetID;
		std::chrono::steady_clock::time_point until;
		bool                                  started{ false };
		bool                                  lowered{ false };
	};
	std::vector<Pending> g_pending;
	std::atomic_bool     g_watching{ false };
	std::atomic_int      g_logged{ 0 };

	float Health(RE::Actor* a_actor, bool a_max)
	{
		auto* av = RE::ActorValue::GetSingleton();
		if (!av || !av->health) {
			return 0.0f;
		}
		return a_max ? a_actor->GetPermanentActorValue(*av->health) : a_actor->GetActorValue(*av->health);
	}

	bool InKillMove(const RE::Actor* a_actor)
	{
		return a_actor && (a_actor->boolFlags.underlying() & kInKillMove) != 0;
	}

	void Watch();

	// Every 50 ms, on the game's own thread: a victim whose kill move started is left at 1 health (the paired blow
	// finishes it, as a vanilla lethal blow would); still standing when the move is over -- killed by its attacker.
	void CheckPending()
	{
		std::vector<Pending> keep;
		std::vector<Pending> now;
		{
			std::scoped_lock lock{ g_lock };
			now.swap(g_pending);
		}
		const auto clock = std::chrono::steady_clock::now();
		const bool detailed = CurrentSettings().detailedLog;
		for (auto& p : now) {
			auto targetPtr = p.target.get();
			auto attackerPtr = p.attacker.get();
			auto* target = targetPtr ? targetPtr->As<RE::Actor>() : nullptr;
			auto* attacker = attackerPtr ? attackerPtr->As<RE::Actor>() : nullptr;
			if (!target || target->IsDead(false)) {
				if (detailed && p.started) {
					logger::info("kill move {:08X} on {:08X}: the victim died", p.attackerID, p.targetID);
				}
				continue;
			}
			const bool moving = InKillMove(target) || InKillMove(attacker);
			if (!p.started && moving) {
				p.started = true;
				p.until = clock + std::chrono::seconds(12);
				if (detailed) {
					logger::info("kill move {:08X} on {:08X}: started (victim flag {}, attacker flag {}), victim at {:.0f} of {:.0f}",
						p.attackerID, p.targetID, InKillMove(target), InKillMove(attacker), Health(target, false), Health(target, true));
				}
			}
			if (p.started && !p.lowered) {
				if (auto* av = RE::ActorValue::GetSingleton(); av && av->health) {
					const float hp = target->GetActorValue(*av->health);
					if (hp > 1.0f) {
						target->ModActorValue(RE::ACTOR_VALUE_MODIFIER::Damage, *av->health, -(hp - 1.0f));
					}
				}
				p.lowered = true;
			}
			if (p.started && !moving) {
				// Over, and the paired blow did not finish it.
				target->KillImpl(attacker, 1000.0f, true, false);
				if (detailed) {
					logger::info("kill move {:08X} on {:08X}: over with the victim standing - killed", p.attackerID, p.targetID);
				}
				continue;
			}
			if (clock > p.until) {
				if (p.started) {
					target->KillImpl(attacker, 1000.0f, true, false);
					logger::info("kill move {:08X} on {:08X}: still running after 12 s - the victim killed", p.attackerID, p.targetID);
				} else if (detailed) {
					logger::info("kill move {:08X} on {:08X}: allowed, but no kill move started (another move or a plain swing)",
						p.attackerID, p.targetID);
				}
				continue;
			}
			keep.push_back(p);
		}
		std::scoped_lock lock{ g_lock };
		g_pending.insert(g_pending.end(), keep.begin(), keep.end());
	}

	void Watch()
	{
		if (g_watching.exchange(true)) {
			return;
		}
		std::thread([] {
			for (;;) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				{
					std::scoped_lock lock{ g_lock };
					if (g_pending.empty()) {
						g_watching = false;
						return;
					}
				}
				if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddTask([] { CheckPending(); });
				}
			}
		}).detach();
	}

	bool ShouldAttackKill(RE::ConditionCheckParams& a_data, void* a_param2, void* a_param1, float& a_result)
	{
		const bool ok = g_vanilla(a_data, a_param2, a_param1, a_result);
		if (a_result > 0.5f) {
			return ok;
		}
		const auto s = CurrentSettings();
		ApplyAttempts(s);
		if (!s.more || s.chance <= 0.0f || s.health <= 0.0f) {
			return ok;
		}
		auto* attacker = a_data.actionRef ? a_data.actionRef->As<RE::Actor>() : nullptr;
		auto* param = static_cast<RE::TESForm*>(a_param2);
		RE::Actor* target = param && param->GetFormType() == RE::ENUM_FORM_ID::kACHR ? static_cast<RE::Actor*>(param) : nullptr;
		if (!target && a_data.targetRef) {
			target = a_data.targetRef->As<RE::Actor>();
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (s.detailedLog) {
			logger::info("ShouldAttackKill asked: subject {:08X}, parameter {:08X}, target ref {:08X} - the game says no",
				attacker ? attacker->GetFormID() : 0, param ? param->GetFormID() : 0, a_data.targetRef ? a_data.targetRef->GetFormID() : 0);
		}
		auto why = [&](const char* a_reason) {
			if (s.detailedLog) {
				logger::info("  {:08X} on {:08X}: no early kill move - {}", attacker ? attacker->GetFormID() : 0, target ? target->GetFormID() : 0, a_reason);
			}
			return ok;
		};
		if (!attacker || !target || attacker == target) {
			return why("no attacker or target");
		}
		if (target == player) {
			return why("the target is the player");
		}
		if (attacker->IsDead(false) || target->IsDead(false)) {
			return why("one of them is dead");
		}
		if (attacker == player && !s.player) {
			return why("the player's swings are off in MCM");
		}
		const auto flags = target->boolFlags.underlying();
		if ((flags & kEssential) || ((flags & kProtected) && attacker != player)) {
			return why("the target is essential or protected");
		}
		if (InKillMove(target)) {
			return why("the target is in a kill move already");
		}
		const float max = Health(target, true);
		const float hp = Health(target, false);
		if (max <= 0.0f || hp <= 0.0f || hp * 100.0f > s.health * max) {
			if (s.detailedLog) {
				logger::info("  {:08X} on {:08X}: no early kill move - health {:.0f} of {:.0f} is over {:.0f}%", attacker->GetFormID(),
					target->GetFormID(), hp, max, s.health);
			}
			return ok;
		}
		const std::uint64_t key = (std::uint64_t{ attacker->GetFormID() } << 32) | target->GetFormID();
		const auto          clock = std::chrono::steady_clock::now();
		bool                yes = false;
		bool                fresh = false;
		{
			std::scoped_lock lock{ g_lock };
			if (g_verdicts.size() > 512) {
				std::erase_if(g_verdicts, [&](const auto& e) { return e.second.until < clock; });
			}
			if (const auto it = g_verdicts.find(key); it != g_verdicts.end() && it->second.until > clock) {
				yes = it->second.yes;
			} else {
				yes = std::uniform_real_distribution<float>(0.0f, 100.0f)(g_rng) < s.chance;
				g_verdicts[key] = { yes, clock + std::chrono::seconds(2) };
				fresh = true;
				if (yes) {
					g_pending.push_back({ attacker->GetHandle(), target->GetHandle(), attacker->GetFormID(), target->GetFormID(),
						clock + std::chrono::seconds(4) });
				}
			}
		}
		if (fresh && s.detailedLog) {
			logger::info("{:08X} on {:08X} at {:.0f} of {:.0f} health: {}", attacker->GetFormID(), target->GetFormID(), hp, max,
				yes ? "a kill move allowed" : "the roll said no");
		}
		if (!yes) {
			return ok;
		}
		if (fresh) {
			Watch();
		}
		a_result = 1.0f;
		return true;
	}

	// A name pointer inside the game's own image (the table's entries point at its .rdata), checked before it is read.
	bool InModule(const void* a_ptr)
	{
		const auto base = REL::Module::get().base();
		const auto p = reinterpret_cast<std::uintptr_t>(a_ptr);
		return p > base && p < base + 0x8000000;
	}

	void InstallGate()
	{
		RE::SCRIPT_FUNCTION* entry = nullptr;
		for (auto& f : RE::SCRIPT_FUNCTION::GetScriptFunctions()) {
			if (f.functionName && InModule(f.functionName) && _stricmp(f.functionName, "ShouldAttackKill") == 0) {
				entry = &f;
				break;
			}
		}
		if (!entry || !entry->conditionFunction) {
			logger::warn("the ShouldAttackKill condition is missing on this game version - kill moves stay vanilla in number");
			return;
		}
		g_vanilla = entry->conditionFunction;
		entry->conditionFunction = &ShouldAttackKill;
		logger::info("ShouldAttackKill wrapped (vanilla at +{:X})",
			reinterpret_cast<std::uintptr_t>(g_vanilla) - REL::Module::get().base());
	}

	// ---- variety -------------------------------------------------------------------------------------------------------

	constexpr std::uint16_t kGetRandomPercent = 77;
	constexpr std::uint16_t kShouldAttackKill = 678;

	bool HasFunction(const RE::TESIdleForm* a_idle, std::uint16_t a_fn)
	{
		for (auto* c = a_idle->conditions.head; c; c = c->next) {
			if ((static_cast<std::uint16_t>(c->data.functionData.function.underlying()) & 0x0FFF) == a_fn) {
				return true;
			}
		}
		return false;
	}

	// Under a kill branch: the group itself or a parent asks ShouldAttackKill. Only those are evened out: a group of
	// paired moves that do NOT kill (shoves, tackles) can fall through to the kill moves after it when every roll
	// fails, and a 100% last move there would starve them (10-08).
	bool UnderKill(void* a_parent, const std::vector<RE::TESIdleForm*>& a_list)
	{
		if (std::ranges::any_of(a_list, [](auto* i) { return HasFunction(i, kShouldAttackKill); })) {
			return true;
		}
		auto* form = static_cast<RE::TESForm*>(a_parent);
		for (int depth = 0; form && form->GetFormType() == RE::ENUM_FORM_ID::kIDLE && depth < 12; ++depth) {
			auto* idle = static_cast<RE::TESIdleForm*>(form);
			if (HasFunction(idle, kShouldAttackKill)) {
				return true;
			}
			form = idle->parentIdle;
		}
		return false;
	}

	bool IsRandom(const RE::TESConditionItem* a_item)
	{
		return (static_cast<std::uint16_t>(a_item->data.functionData.function.underlying()) & 0x0FFF) == kGetRandomPercent;
	}

	// A condition item as a comparable value, for "can this move play whenever that one can".
	struct Cond
	{
		std::uint16_t fn;
		void*         p0;
		void*         p1;
		std::uint32_t op;
		float         value;
		std::uint32_t object;
		std::uint32_t runOn;
		bool operator==(const Cond&) const = default;
	};

	struct Member
	{
		RE::TESIdleForm*  form;
		std::vector<Cond> conds;   // without the roll
		bool              hasOr{ false };
		int               rolls{ 0 };
		bool              oddRoll{ false };
	};

	Member Describe(RE::TESIdleForm* a_idle)
	{
		Member m{ a_idle };
		for (auto* c = a_idle->conditions.head; c; c = c->next) {
			if (c->data.compareOr) {
				m.hasOr = true;
			}
			const auto op = static_cast<std::uint32_t>(c->data.condition);
			if (IsRandom(c)) {
				++m.rolls;
				if (c->data.valueIsGlobal || (op != 4 && op != 5)) {   // only "<" and "<=" are reordered
					m.oddRoll = true;
				}
				continue;
			}
			m.conds.push_back({ static_cast<std::uint16_t>(c->data.functionData.function.underlying()), c->data.functionData.param[0],
				c->data.functionData.param[1], op, c->data.valueIsGlobal ? 0.0f : c->data.value,
				static_cast<std::uint32_t>(c->data.object.underlying()), c->data.runOnRef.native_handle() });
		}
		return m;
	}

	// b can play whenever a can: b's conditions are a subset of a's (OR-chains only when identical).
	bool Covers(const Member& a_b, const Member& a_a)
	{
		if (a_b.hasOr || a_a.hasOr) {
			return a_b.conds == a_a.conds;
		}
		return std::ranges::all_of(a_b.conds, [&](const Cond& c) { return std::ranges::find(a_a.conds, c) != a_a.conds.end(); });
	}

	bool StartsWithPa(const RE::BSFixedString& a_s)
	{
		const char* c = a_s.c_str();
		return c && (c[0] == 'p' || c[0] == 'P') && (c[1] == 'a' || c[1] == 'A') && c[2] == '_';
	}

	// The roll item of an idle, made if it has none (a "no roll" last move that now shares).
	RE::TESConditionItem* RollOf(RE::TESIdleForm* a_idle)
	{
		for (auto* c = a_idle->conditions.head; c; c = c->next) {
			if (IsRandom(c)) {
				return c;
			}
		}
		auto* item = static_cast<RE::TESConditionItem*>(RE::malloc(sizeof(RE::TESConditionItem)));
		if (!item) {
			return nullptr;
		}
		std::memset(item, 0, sizeof(RE::TESConditionItem));
		item->data.functionData.function = static_cast<RE::SCRIPT_OUTPUT>(kGetRandomPercent);
		item->data.condition = RE::ENUM_COMPARISON_CONDITION::kLessThan;
		item->data.object = RE::CONDITIONITEMOBJECT::kSelf;
		item->data.value = 100.0f;
		// Appended at the end: an AND with everything before it (the last item's OR flag must stay clear for that).
		RE::TESConditionItem** tail = &a_idle->conditions.head;
		while (*tail) {
			tail = &(*tail)->next;
		}
		*tail = item;
		return item;
	}

	// The contents of two idle forms trade places (the forms, and so the group's order, stay).
	void SwapContents(RE::TESIdleForm* a_a, RE::TESIdleForm* a_b)
	{
		std::swap(a_a->conditions.head, a_b->conditions.head);
		std::swap(a_a->data, a_b->data);
		std::swap(a_a->childIdles, a_b->childIdles);
		auto raw = [](RE::BSFixedString& a_s) -> void*& { return *reinterpret_cast<void**>(&a_s); };
		std::swap(raw(a_a->behaviorGraphName), raw(a_b->behaviorGraphName));
		std::swap(raw(a_a->animEventName), raw(a_b->animEventName));
		std::swap(raw(a_a->animFileName), raw(a_b->animFileName));
	}

	void ApplyVariety()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return;
		}
		std::unordered_map<void*, std::vector<RE::TESIdleForm*>> groups;
		int total = 0, withParent = 0, paired = 0, pairedLeaves = 0;
		// Idles are not in the data handler's form arrays (that list came back empty, 10-07): every form, filtered.
		std::vector<RE::TESIdleForm*> idles;
		{
			const auto& [map, lock] = RE::TESForm::GetAllForms();
			RE::BSAutoReadLock l{ lock };
			if (map) {
				for (const auto& [id, form] : *map) {
					if (form && form->GetFormType() == RE::ENUM_FORM_ID::kIDLE) {
						idles.push_back(static_cast<RE::TESIdleForm*>(form));
					}
				}
			}
		}
		for (auto* idle : idles) {
			if (!idle) {
				continue;
			}
			++total;
			paired += StartsWithPa(idle->animEventName) ? 1 : 0;
			pairedLeaves += StartsWithPa(idle->animEventName) && !idle->childIdles ? 1 : 0;
			if (idle->parentIdle) {
				++withParent;
				groups[idle->parentIdle].push_back(idle);
			}
		}
		logger::info("variety: {} idles, {} with a parent, {} paired ('pa_' event), {} of them leaves, {} sibling groups", total,
			withParent, paired, pairedLeaves, groups.size());
		// One known kill move as the game holds it (PairedKill1HMStabNeck, Fallout4.esm 0C73B8): what the reading sees.
		if (auto* probe = RE::TESForm::GetFormByID<RE::TESIdleForm>(0x0C73B8)) {
			logger::info("variety probe 0C73B8: event '{}', parent {:08X}, previous {:08X}, children {}, first condition fn {}",
				probe->animEventName.c_str() ? probe->animEventName.c_str() : "", probe->parentIdle ? probe->parentIdle->GetFormID() : 0,
				probe->prevIdle ? probe->prevIdle->GetFormID() : 0, static_cast<const void*>(probe->childIdles),
				probe->conditions.head ? static_cast<int>(probe->conditions.head->data.functionData.function.underlying()) : -1);
		}
		int groupsDone = 0;
		int moves = 0;
		int skipped = 0;
		int mixed = 0;
		int notKill = 0;
		const bool detailed = CurrentSettings().detailedLog;
		for (auto& [parent, list] : groups) {
			const auto pa = std::ranges::count_if(list, [](auto* i) { return StartsWithPa(i->animEventName); });
			if (list.size() < 2 || pa == 0) {
				continue;
			}
			if (!UnderKill(parent, list)) {
				++notKill;
				continue;
			}
			if (pa != static_cast<std::ptrdiff_t>(list.size()) || !std::ranges::all_of(list, [](auto* i) { return !i->childIdles; })) {
				++mixed;
				if (detailed) {
					logger::info("variety: group under {:08X} skipped - {} of {} paired, children {}", static_cast<RE::TESForm*>(parent)->GetFormID(),
						pa, list.size(), std::ranges::count_if(list, [](auto* i) { return i->childIdles != nullptr; }));
				}
				continue;
			}
			// The group's order: first the one whose previous is not in the group, then each one's follower.
			std::unordered_map<RE::TESIdleForm*, RE::TESIdleForm*> next;
			RE::TESIdleForm*                                      first = nullptr;
			for (auto* i : list) {
				if (i->prevIdle && std::ranges::find(list, i->prevIdle) != list.end()) {
					next[i->prevIdle] = i;
				} else if (!first) {
					first = i;
				} else {
					first = nullptr;   // two heads: not a chain we understand
					break;
				}
			}
			std::vector<RE::TESIdleForm*> order;
			for (auto* i = first; i && order.size() <= list.size(); i = next.contains(i) ? next[i] : nullptr) {
				order.push_back(i);
			}
			if (order.size() != list.size()) {
				++skipped;
				continue;
			}
			std::vector<Member> members;
			for (auto* i : order) {
				members.push_back(Describe(i));
			}
			if (std::ranges::count_if(members, [](const Member& m) { return m.rolls > 0; }) < 1 ||
				std::ranges::any_of(members, [](const Member& m) { return m.oddRoll || m.rolls > 1; })) {
				++skipped;
				continue;
			}
			// The most specific first, by how many conditions a move carries (stable: equal ones keep the game's order).
			// A move that needs a stabbing weapon then comes before one any blade can do, so the broad one cannot cut
			// the list short for a knife.
			std::vector<std::size_t> want(members.size());
			for (std::size_t k = 0; k < want.size(); ++k) {
				want[k] = k;
			}
			std::ranges::stable_sort(want, [&](std::size_t x, std::size_t y) { return members[x].conds.size() > members[y].conds.size(); });
			// Contents into place by swaps (positions are the forms in `order`).
			std::vector<std::size_t> at(members.size());   // at[k]: the position holding original member k
			std::vector<std::size_t> who(members.size());  // who[p]: the original member at position p
			for (std::size_t k = 0; k < members.size(); ++k) {
				at[k] = who[k] = k;
			}
			for (std::size_t p = 0; p < want.size(); ++p) {
				const std::size_t k = want[p];
				const std::size_t q = at[k];
				if (q != p) {
					SwapContents(order[p], order[q]);
					std::swap(who[p], who[q]);
					at[who[p]] = p;
					at[who[q]] = q;
				}
			}
			// Each one's roll: 1 / (1 + the later moves that can play whenever it can).
			for (std::size_t p = 0; p < want.size(); ++p) {
				const auto& me = members[want[p]];
				int         later = 0;
				for (std::size_t r = p + 1; r < want.size(); ++r) {
					later += Covers(members[want[r]], me) ? 1 : 0;
				}
				auto* roll = RollOf(order[p]);
				if (!roll) {
					continue;
				}
				const float percent = 100.0f / static_cast<float>(later + 1);
				roll->data.condition = RE::ENUM_COMPARISON_CONDITION::kLessThan;   // GetRandomPercent 0-99: "< N" is N%
				roll->data.value = later == 0 ? 100.0f : std::round(percent);
				++moves;
				if (detailed) {
					logger::info("  {} -> position {} of {}: {:.0f}%", order[p]->animEventName.c_str(), p + 1, want.size(), roll->data.value);
				}
			}
			++groupsDone;
			if (detailed) {
				logger::info("variety: group under {:08X} done ({} moves)", static_cast<RE::TESForm*>(parent)->GetFormID(), want.size());
			}
		}
		logger::info("variety: {} kill-move groups evened out ({} moves), {} left as they are, {} mixed, {} paired groups that do not kill left alone", groupsDone, moves, skipped, mixed, notKill);
	}

	// ---- plumbing ------------------------------------------------------------------------------------------------------

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (a_msg && (a_msg->type == F4SE::MessagingInterface::kPostLoadGame || a_msg->type == F4SE::MessagingInterface::kNewGame)) {
			ApplyAttempts(CurrentSettings());
		}
		if (a_msg && a_msg->type == F4SE::MessagingInterface::kGameDataReady) {
			const auto s = CurrentSettings();
			ApplyAttempts(s);
			if (s.variety) {
				ApplyVariety();
			} else {
				logger::info("variety: off in MCM - the game's own order and rolls");
			}
		}
	}

	class FileSink final : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit FileSink(const std::filesystem::path& a_path) :
			_out(a_path, std::ios::binary | std::ios::trunc)
		{}

	protected:
		void sink_it_(const spdlog::details::log_msg& a_msg) override
		{
			spdlog::memory_buf_t formatted;
			formatter_->format(a_msg, formatted);
			_out.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
		}

		void flush_() override { _out.flush(); }

	private:
		std::ofstream _out;
	};

	void InitLogging()
	{
		auto path = logger::log_directory();
		if (!path) {
			return;
		}
		*path /= FIN_PROJECT_NAME ".log"sv;
		// Opened by its wide path, never path::string(): a user name outside the ANSI code page throws, and a throw in
		// F4SEPlugin_Load disables the plugin with no log (Rapport, 2026-09-24).
		auto sink = std::make_shared<FileSink>(*path);
		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S] %v"s);
	}

	constexpr F4SE::PluginVersionData MakeVersionData() noexcept
	{
		F4SE::PluginVersionData data{};
		data.pluginVersion = (FIN_VERSION_MAJOR << 24) | (FIN_VERSION_MINOR << 16) | (FIN_VERSION_PATCH << 4);
		constexpr std::string_view name = FIN_PROJECT_NAME;
		for (std::size_t i = 0; i < name.size() && i < std::size(data.name) - 1; ++i) {
			data.name[i] = name[i];
		}
		data.addressIndependence = F4SE::PluginVersionData::kAddressIndependence_Signatures;
		data.structureIndependence = F4SE::PluginVersionData::kStructureIndependence_1_10_980Layout |
		                             F4SE::PluginVersionData::kStructureIndependence_1_11_137Layout;
		return data;
	}
}

// OG's F4SE (0.6.23) loads a plugin by Query; NG's and AE's read F4SEPlugin_Version.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = FIN_PROJECT_NAME;
	a_info->version = FIN_VERSION_MAJOR * 10000 + FIN_VERSION_MINOR * 100 + FIN_VERSION_PATCH;
	return !a_f4se->IsEditor();
}

extern "C" DLLEXPORT constinit F4SE::PluginVersionData F4SEPlugin_Version = MakeVersionData();

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	try {
		InitLogging();
	} catch (...) {
	}
	logger::info("{} {} on runtime {}", FIN_PROJECT_NAME, FIN_VERSION_STRING, a_f4se->RuntimeVersion().string());
	CurrentSettings();
	InstallGate();

	// The default listener only (F4SE's own messages). A NAMED sender killed AE's F4SE 0.7.9 silently (Rapport, 2026-09-26).
	const auto* messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		logger::warn("could not listen for game data - variety off");
	}
	return true;
}
