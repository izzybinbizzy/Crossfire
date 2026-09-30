// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.txt and the notice at the top of main.cpp.
//
// One page in SKSE Menu Framework's Mod Control Panel. The menu draws off the game's main thread, so it works on a
// copy of the settings (MenuCopy) and hands each change over as a task on the main thread (Submit); a change is
// saved to Crossfire.ini when the slider or box is let go. The reaction table is shown, not edited: it lives in
// Crossfire_Rules.ini, which a patch can add to.

#include "Plugin.h"

#include "SKSEMenuFramework.h"

namespace Crossfire
{
	namespace
	{
		// the same warm glow as the RELight - Spell Addon page, so the two sit together
		constexpr ImGuiMCP::ImVec4 kGold{ 1.0f, 0.86f, 0.55f, 1.0f };
		constexpr ImGuiMCP::ImVec4 kDim{ 0.75f, 0.72f, 0.66f, 1.0f };

		class GlowStyle
		{
		public:
			GlowStyle()
			{
				using namespace ImGuiMCP;
				PushStyleColor(ImGuiCol_CheckMark, ImVec4{ 1.0f, 0.80f, 0.42f, 1.0f });
				PushStyleColor(ImGuiCol_SliderGrab, ImVec4{ 1.0f, 0.74f, 0.38f, 0.90f });
				PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4{ 1.0f, 0.86f, 0.55f, 1.0f });
				PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.10f, 0.08f, 0.75f });
				PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{ 0.30f, 0.21f, 0.10f, 0.75f });
				PushStyleColor(ImGuiCol_Separator, ImVec4{ 1.0f, 0.78f, 0.45f, 0.22f });
				PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
			}
			~GlowStyle()
			{
				ImGuiMCP::PopStyleVar(1);
				ImGuiMCP::PopStyleColor(6);
			}
			GlowStyle(const GlowStyle&) = delete;
			GlowStyle& operator=(const GlowStyle&) = delete;
		};

		void Heading(const char* a_text)
		{
			ImGuiMCP::Spacing();
			ImGuiMCP::TextColored(kGold, "%s", a_text);
			ImGuiMCP::Separator();
		}

		// A change goes to the game at once; it is written to the file when the control is let go.
		struct Edit
		{
			Core::Config cfg{ MenuCopy() };
			bool         changed{ false };
			bool         save{ false };

			void After(bool a_changed)
			{
				changed |= a_changed;
				save |= ImGuiMCP::IsItemDeactivatedAfterEdit() || (a_changed && !ImGuiMCP::IsItemActive());
			}
			~Edit()
			{
				if (changed || save) {
					Submit(cfg, save);
				}
			}
		};

		void Check(Edit& a_e, const char* a_label, bool& a_value, const char* a_tip)
		{
			a_e.After(ImGuiMCP::Checkbox(a_label, &a_value));
			ImGuiMCP::SetItemTooltip("%s", a_tip);
		}

		void Slider(Edit& a_e, const char* a_label, const char* a_key, float& a_value, const char* a_format, const char* a_tip)
		{
			const auto r = Core::RangeOf(a_key);
			a_e.After(ImGuiMCP::SliderFloat(a_label, &a_value, r.lo, r.hi, a_format, ImGuiMCP::ImGuiSliderFlags_AlwaysClamp));
			ImGuiMCP::SetItemTooltip("%s", a_tip);
		}

		void SliderI(Edit& a_e, const char* a_label, const char* a_key, int& a_value, const char* a_tip)
		{
			const auto r = Core::RangeOf(a_key);
			a_e.After(ImGuiMCP::SliderInt(a_label, &a_value, static_cast<int>(r.lo), static_cast<int>(r.hi), "%d", ImGuiMCP::ImGuiSliderFlags_AlwaysClamp));
			ImGuiMCP::SetItemTooltip("%s", a_tip);
		}

		void DrawTable(const Core::Config& a_cfg)
		{
			using namespace ImGuiMCP;
			constexpr int kColumns = static_cast<int>(Core::kElements) + 1;
			if (!BeginTable("reactions", kColumns, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				return;
			}
			TableSetupColumn("vs");
			for (std::size_t j = 0; j < Core::kElements; ++j) {
				TableSetupColumn(std::string(Core::ElementName(static_cast<Core::Element>(j))).c_str());
			}
			TableHeadersRow();
			for (std::size_t i = 0; i < Core::kElements; ++i) {
				TableNextRow();
				TableNextColumn();
				TextColored(kGold, "%s", std::string(Core::ElementName(static_cast<Core::Element>(i))).c_str());
				for (std::size_t j = 0; j < Core::kElements; ++j) {
					TableNextColumn();
					const auto action = a_cfg.reactions[i][j];
					const auto name = std::string(Core::ActionName(action));
					if (action == Core::Action::kClash) {
						TextColored(kDim, "%s", name.c_str());
					} else {
						Text("%s", name.c_str());
					}
				}
			}
			EndTable();
		}

		void __stdcall Render()
		{
			const GlowStyle style;
			Edit            e;
			auto&           c = e.cfg;

			Heading("Crossfire");
			Check(e, "Enabled", c.enabled, "Projectiles that meet in the air clash. Off: Crossfire does nothing at all.");
			{
				const char* who[]{ "Everyone's projectiles", "Only when yours are involved" };
				e.After(ImGuiMCP::Combo("Who clashes", &c.who, who, 2));
				ImGuiMCP::SetItemTooltip("%s", "Everyone: two enemy mages' spells can meet too. Yours only: a clash always involves one of your projectiles.");
			}
			Check(e, "Allies pass through", c.ignoreAllies, "Projectiles of people who are not hostile to each other (you and your follower) never clash.");
			Slider(e, "Range", "MaxDistance", c.maxDistance, "%.0f units", "Projectiles further than this from you are left alone. 70 units is about a metre.");

			Heading("Hits");
			Slider(e, "Aim assist", "RadiusBonus", c.radiusBonus, "%.0f units", "Added to every projectile's size, so a shot does not have to be perfect. 0 is the game's own sizes.");
			Slider(e, "Size", "RadiusScale", c.radiusScale, "x%.2f", "Times each projectile's own collision size.");
			Slider(e, "Wall height", "BarrierHeight", c.barrierHeight, "%.0f units", "How tall a wall spell stands, for catching projectiles.");

			Heading("Clash");
			Slider(e, "Overpower", "OverpowerRatio", c.overpowerRatio, "x%.1f", "A projectile this many times stronger than the other destroys it and flies on. Closer than that, both go.");
			Check(e, "The survivor is weakened", c.weakenSurvivor, "The one that flies on loses the strength of what it beat.");
			Check(e, "...and hits softer", c.weakenDamage, "A weakened projectile also does less when it lands.");
			Slider(e, "Arrow strength", "ArrowScale", c.tuning.arrowScale, "x%.1f", "An arrow's strength per point of its damage. A spell's strength is its magicka cost.");
			Slider(e, "Spray strength", "StreamShare", c.tuning.streamShare, "x%.2f", "Each particle of a spray (Flames, Frostbite) as a share of its spell's cost.");
			Slider(e, "Shout strength", "ShoutStrength", c.tuning.shoutStrength, "%.0f", "Every shout's strength; shouts cost no magicka.");

			Heading("Effects");
			Check(e, "Bursts", c.explosions, "A destroyed projectile bursts where it was hit, with its own explosion.");
			Check(e, "Only harmless bursts", c.safeExplosionsOnly, "Skip a burst that would do more than show: damage, an enchantment, something it spawns. Spells' own explosions only show.");
			SliderI(e, "Bursts per frame", "MaxExplosionsPerFrame", c.maxExplosionsPerFrame, "At most this many in one frame.");
			Slider(e, "Burst cooldown", "ExplosionCooldown", c.explosionCooldown, "%.2f s", "Between bursts for the same two shooters. Two sprays meeting clash many times a second.");
			Slider(e, "Skill experience", "SkillXP", c.skillXP, "%.0f", "For each enemy projectile one of yours destroys, to the skill that cast it (Archery for arrows). 0: none.");
			Check(e, "Papyrus event", c.modEvents, "Send the mod event Crossfire_Clash for scripts that listen.");
			Check(e, "Log every clash", c.debugLog, "Write each clash to Crossfire.log (Documents\\My Games\\Skyrim Special Edition\\SKSE).");

			Heading("What meets what");
			ImGuiMCP::TextDisabled("%s", "Row against column. Clash: the stronger wins if it is enough stronger, else both go. Edit Crossfire_Rules.ini to change.");
			DrawTable(c);
			if (ImGuiMCP::Button("Reload the files")) {
				ReloadSoon();
			}
			ImGuiMCP::SetItemTooltip("%s", "Read Crossfire_Rules.ini, the Crossfire folder and Crossfire.ini again.");

			ImGuiMCP::Spacing();
			ImGuiMCP::TextDisabled("%s", "Sprays, held beams and breath that lock together: see the Spell struggles page.");

			Heading("This session");
			auto& s = Counters();
			ImGuiMCP::TextDisabled("%u projectile(s) in range now; %llu clash(es), %llu destroyed, %llu weakened, %llu burst(s), %llu by you",
				s.tracked.load(), static_cast<unsigned long long>(s.clashes.load()), static_cast<unsigned long long>(s.destroyed.load()),
				static_cast<unsigned long long>(s.weakened.load()), static_cast<unsigned long long>(s.explosions.load()),
				static_cast<unsigned long long>(s.yours.load()));
			for (const auto& note : LoadNotes()) {
				ImGuiMCP::TextDisabled("%s", note.c_str());
			}
		}

		// an element's colour, for the struggle bar
		[[nodiscard]] ImGuiMCP::ImVec4 ElementColour(std::uint8_t a_element, float a_alpha)
		{
			switch (static_cast<Core::Element>(a_element)) {
			case Core::Element::kFire:
				return { 1.0f, 0.45f, 0.15f, a_alpha };
			case Core::Element::kFrost:
				return { 0.45f, 0.78f, 1.0f, a_alpha };
			case Core::Element::kShock:
				return { 0.62f, 0.62f, 1.0f, a_alpha };
			case Core::Element::kPoison:
				return { 0.45f, 0.85f, 0.35f, a_alpha };
			case Core::Element::kPhysical:
				return { 0.8f, 0.8f, 0.8f, a_alpha };
			case Core::Element::kForce:
				return { 0.95f, 0.9f, 0.75f, a_alpha };
			default:
				return { 0.8f, 0.5f, 1.0f, a_alpha };
			}
		}

		void __stdcall RenderStruggles()
		{
			const GlowStyle style;
			Edit            e;
			auto&           c = e.cfg;
			auto&           s = c.struggle;

			Heading("Spell struggles");
			Check(e, "Spell struggles", s.enabled,
				"When enemy sprays, held beams or breath meet, they lock together and push. Whoever has the higher magic skill drives the "
				"meeting point back and breaks through. Off: they clash particle by particle, as before.");
			ImGuiMCP::BeginDisabled(!s.enabled);
			Check(e, "Sprays lock", s.sprays, "Flames, Frostbite and other sprays.");
			Check(e, "Beams lock", s.beams, "Sparks, Lightning Storm and other held beams, with sprays and with each other. A one-shot bolt never locks.");
			Check(e, "Breath locks", s.breath, "Fire and Frost Breath, yours or a dragon's, lock with breath, sprays and beams.");
			Check(e, "Fire and frost lock too", s.opposites,
				"Opposites that would cancel each other lock like any other pair. Off: they cancel particle by particle. Pairs set to Pass, Wins "
				"or Loses in the rules never lock.");
			Check(e, "Beams stop where they meet", s.beamsStop, "A locked beam is cut short at the meeting point. Experimental: turn it off if beams flicker or vanish.");
			Check(e, "Between other casters", s.betweenOthers, "Two enemies of each other (your follower and a necromancer) can lock too. Off: only struggles you are part of.");
			Check(e, "Creatures take part", s.creatures,
				"Atronachs, hagravens, draugr and other creatures that cast a spray or beam. A creature's magic skill counts as at least its level.");
			Slider(e, "Lock chance", "StruggleChance", s.chance, "%.0f%%",
				"Chance two casters lock when their streams meet, rolled once each time. A miss: they clash particle by particle until one stops casting.");
			Slider(e, "Dragon lock chance", "DragonChance", s.dragonChance, "%.0f%%", "The same when one side is a dragon. 0: never with dragons.");

			Heading("Who wins");
			Check(e, "Higher magic skill always wins", s.skillAlwaysWins,
				"When both cast from a school of magic, the higher skill always pushes through. Level, magicka, spell, dual casting and enemy "
				"power only change how fast. Breath is a plain contest of power.");
			{
				const char* sources[]{ "The spell's own school", "The best school", "The average of the five" };
				e.After(ImGuiMCP::Combo("Magic skill is", &s.skillSource, sources, 3));
				ImGuiMCP::SetItemTooltip("%s", "Which skill counts as a caster's magic level.");
			}
			Slider(e, "Skill weight", "SkillWeight", s.skillWeight, "%.2f", "How much magic skill counts. At 1, each point over the other is worth 2% more push.");
			Slider(e, "Level weight", "LevelWeight", s.levelWeight, "%.2f", "How much character level counts. At 1, each level over the other is worth 2%.");
			Slider(e, "Spell weight", "SpellWeight", s.spellWeight, "%.2f", "How much a costlier spell counts. 0: only the caster matters.");
			Slider(e, "Magicka weight", "MagickaWeight", s.magickaWeight, "%.2f",
				"A caster low on magicka pushes less. At 1, an empty caster has a quarter of the push. 0: magicka left does not matter.");
			Slider(e, "Two hands or a staff", "DualCastBonus", s.dualBonus, "x%.2f", "Extra push for a dual cast, a staff, or streaming with both hands at once.");
			Slider(e, "Breath strength", "BreathStrength", s.breathStrength, "%.0f", "What a breath counts as: a spell of this magicka cost (Flames is about 14).");
			Slider(e, "Shout word bonus", "ShoutWordBonus", s.wordBonus, "%.2f", "Each word of a breath shout past the first adds this share of push.");
			Slider(e, "Enemy power", "EnemyPower", s.enemyPower, "x%.2f", "How hard whoever struggles against you pushes. Above 1 is harder. Struggles between others are untouched.");
			Slider(e, "Dragon breath power", "DragonPower", s.dragonPower, "x%.2f", "How hard a dragon's breath pushes.");
			{
				// a live example: two casters, all else equal
				Core::Caster a, b;
				a.skill = 60.0f;
				b.skill = 40.0f;
				a.level = b.level = 30.0f;
				a.cost = b.cost = 14.0f;
				const float lead = Core::Advantage(a, b, s);
				const float secs = Core::SecondsToWin(lead, 1.0f, s);
				if (s.maxTime > 0.0f && secs > s.maxTime) {
					ImGuiMCP::TextDisabled("Destruction 60 against 40, all else equal: pushes at %.0f%% of full speed, wins when time runs out.", 100.0f * std::abs(lead));
				} else {
					ImGuiMCP::TextDisabled("Destruction 60 against 40, all else equal: pushes at %.0f%% of full speed, breaks through in about %.1f s.",
						100.0f * std::abs(lead), secs);
				}
			}

			Heading("The push");
			Slider(e, "Push time", "PushTime", s.pushTime, "%.1f s",
				"Seconds for a caster twice as strong to push from where they met to the other's hands. Closer fights take longer. It changes "
				"how long, not who wins.");
			Slider(e, "Time limit", "MaxStruggleTime", s.maxTime, "%.0f s",
				"When time runs out, whoever is ahead breaks through. Dead even: the spells burst and both are thrown off. 0: no limit.");
			Slider(e, "Magicka pressure", "MagickaPressure", s.magickaPressure, "%.2f",
				"The caster being pushed back pays extra magicka, more the further back. A caster who runs dry while losing is overwhelmed. 0: only "
				"the spell's own cost.");
			Check(e, "Last-ditch surge", s.surge,
				"Once per struggle, a caster pushed most of the way back surges for 3 seconds. With 'Higher magic skill always wins' on, a surge "
				"can slow a loss but not turn it.");
			ImGuiMCP::BeginDisabled(!s.surge);
			Slider(e, "Surge power", "SurgePower", s.surgePower, "x%.2f", "How much harder a surging caster pushes.");
			ImGuiMCP::EndDisabled();

			Heading("Breakthrough");
			Check(e, "Break the loser's spell", s.breakCast, "The overwhelmed caster's spell is interrupted, in the hand that lost.");
			Slider(e, "Reeling", "BreakTime", s.breakTime, "%.1f s",
				"How long the overwhelmed caster reels: any spray or beam they cast in that time fizzles, so the winner's stream pours over them.");
			Check(e, "Stagger the overwhelmed", s.stagger, "The overwhelmed caster staggers, with the game's own stagger. Not dragons.");
			ImGuiMCP::BeginDisabled(!s.stagger);
			Check(e, "...you too", s.staggerPlayer, "You also stagger when you are overwhelmed.");
			Slider(e, "Stagger strength", "StaggerStrength", s.staggerStrength, "%.2f", "How hard the stagger is. A clearer win staggers harder.");
			ImGuiMCP::EndDisabled();
			Slider(e, "Breakthrough damage", "OverwhelmDamage", s.overwhelmDamage, "x%.2f",
				"An extra hit on the overwhelmed caster, about two seconds of the winning stream, less their resistance. It counts as the "
				"winner's kill. 0: only the stream's own damage.");
			Check(e, "Finishers", s.finishers, "A breakthrough that kills throws the body away from the winner. Experimental.");
			ImGuiMCP::BeginDisabled(!s.finishers);
			Slider(e, "Finisher force", "FinisherForce", s.finisherForce, "%.1f", "How hard the body is thrown.");
			ImGuiMCP::EndDisabled();
			ImGuiMCP::BeginDisabled(FearSpell() == nullptr);
			Check(e, "Winning scares weaker enemies", s.intimidate,
				"When you break through and kill, nearby enemies of lower level than your victim are frightened, with the game's own Fear spell.");
			ImGuiMCP::EndDisabled();
			if (!FearSpell()) {
				ImGuiMCP::TextDisabled("%s", "(no Fear spell was found in Skyrim.esm, so this cannot work)");
			}
			Slider(e, "Struggle experience", "StruggleXP", s.xp, "%.0f",
				"Skill experience in your spell's school each time you overwhelm someone; doubled if it kills. Breath shouts earn none. 0: none.");
			Check(e, "Messages", s.messages, "A message when you overwhelm someone, they overwhelm you, they give way, or the spells burst between you.");

			Heading("Show");
			Check(e, "Bursts at the lock", s.lockBursts,
				"Bursts where streams lock, now and then while they grind, and where one breaks. Uses the spells' own explosions and follows "
				"Bursts and 'Only harmless bursts'.");
			Slider(e, "Camera shake", "CameraShake", s.cameraShake, "%.2f",
				"A kick when your stream locks, a rumble that grows as the lock nears you, a jolt on a breakthrough. Fights near you shake less. 0: off.");
			Check(e, "Struggle bar", s.bar, "While you are locked, a bar shows who is pushing, in each spell's colour, with both magic skills.");
			ImGuiMCP::BeginDisabled(!s.bar);
			Slider(e, "Bar position", "BarHeight", s.barHeight, "%.0f%%", "How far down the screen the bar sits.");
			Slider(e, "Bar size", "BarScale", s.barScale, "x%.2f", "How big the bar is.");
			Slider(e, "Bar opacity", "BarOpacity", s.barOpacity, "%.2f", "How solid the bar is.");
			ImGuiMCP::EndDisabled();
			ImGuiMCP::EndDisabled();

			Heading("Now");
			const auto v = StruggleNow();
			if (v.active) {
				ImGuiMCP::Text("Locked with %s: %s %d against %d, %.1f s, the lock %s", v.foe, v.school, v.mySkill, v.theirSkill, v.seconds,
					v.balance > 0.02f ? "moving toward them" : (v.balance < -0.02f ? "moving toward you" : "even"));
			} else {
				ImGuiMCP::TextDisabled("%s", "Not locked with anyone.");
			}
			auto& n = Counters();
			ImGuiMCP::TextDisabled("This session: %llu struggle(s), %llu broken through, %llu won and %llu lost by you, %llu gave way, %llu draw(s), %llu particle(s) snuffed",
				static_cast<unsigned long long>(n.struggles.load()), static_cast<unsigned long long>(n.overwhelms.load()),
				static_cast<unsigned long long>(n.won.load()), static_cast<unsigned long long>(n.lost.load()),
				static_cast<unsigned long long>(n.gaveWay.load()), static_cast<unsigned long long>(n.draws.load()),
				static_cast<unsigned long long>(n.cut.load()));
			if (ImGuiMCP::Button("Struggle defaults")) {
				s = {};
				e.changed = e.save = true;
			}
			ImGuiMCP::SetItemTooltip("%s", "Every setting on this page back to how it came.");
		}

		// the struggle bar, on the render thread: it reads only StruggleNow()
		void __stdcall DrawStruggleBar()
		{
			using namespace ImGuiMCP;
			static float alpha = 0.0f;
			const auto   v = StruggleNow();
			auto*        io = GetIO();
			if (!io) {
				return;
			}
			const float dt = std::isfinite(io->DeltaTime) ? std::clamp(io->DeltaTime, 0.0f, 0.25f) : 0.0f;
			alpha = std::clamp(alpha + (v.active && v.bar ? dt : -dt) * 4.0f, 0.0f, 1.0f);
			if (alpha <= 0.0f) {
				return;
			}
			auto* dl = GetForegroundDrawList();
			if (!dl) {
				return;
			}
			const float k = std::clamp(v.barScale, 0.5f, 2.0f);
			const float a = std::clamp(v.barOpacity, 0.1f, 1.0f) * alpha;
			const float w = 360.0f * k, h = 10.0f * k;
			const float cx = io->DisplaySize.x * 0.5f, y = io->DisplaySize.y * std::clamp(v.barHeight, 0.0f, 100.0f) / 100.0f;
			const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
			const float split = x0 + w * std::clamp((1.0f + v.balance) * 0.5f, 0.0f, 1.0f);
			ImDrawListManager::AddRectFilled(dl, ImVec2{ x0 - 2.0f, y - 2.0f }, ImVec2{ x1 + 2.0f, y + h + 2.0f },
				ColorConvertFloat4ToU32(ImVec4{ 0.05f, 0.04f, 0.03f, 0.8f * a }), 3.0f, 0);
			ImDrawListManager::AddRectFilled(dl, ImVec2{ x0, y }, ImVec2{ split, y + h }, ColorConvertFloat4ToU32(ElementColour(v.mine, a)), 2.0f, 0);
			ImDrawListManager::AddRectFilled(dl, ImVec2{ split, y }, ImVec2{ x1, y + h }, ColorConvertFloat4ToU32(ElementColour(v.theirs, a)), 2.0f, 0);
			ImDrawListManager::AddLine(dl, ImVec2{ split, y - 4.0f * k }, ImVec2{ split, y + h + 4.0f * k },
				ColorConvertFloat4ToU32(ImVec4{ 1.0f, 0.97f, 0.85f, a }), 2.0f * k);
			char left[64], right[96];
			std::snprintf(left, sizeof(left), "You - %s %d", v.school, v.mySkill);
			std::snprintf(right, sizeof(right), "%s - %d", v.foe, v.theirSkill);
			const ImU32 text = ColorConvertFloat4ToU32(ImVec4{ 1.0f, 0.94f, 0.8f, a });
			const auto  rs = CalcTextSize(right);
			ImDrawListManager::AddText(dl, ImVec2{ x0, y - 18.0f * k }, text, left);
			ImDrawListManager::AddText(dl, ImVec2{ x1 - rs.x, y - 18.0f * k }, text, right);
		}
	}

	void RegisterMenu()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			SKSE::log::info("SKSE Menu Framework is not installed, so there is no settings page; Crossfire.ini still applies");
			return;
		}
		SKSEMenuFramework::SetSection("Crossfire");
		SKSEMenuFramework::AddSectionItem("Settings", Render);
		SKSEMenuFramework::AddSectionItem("Spell struggles", RenderStruggles);
		SKSEMenuFramework::AddHudElement(DrawStruggleBar);
		SKSE::log::info("settings page added to SKSE Menu Framework {}", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
