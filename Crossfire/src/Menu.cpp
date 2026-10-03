// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.txt and the notice at the top of main.cpp.
//
// Two pages in SKSE Menu Framework's Mod Control Panel: Settings (clashes) and Spell struggles, each with the everyday
// settings only; the finer tuning stays in Crossfire.ini, which the menu writes whole. The menu draws off the game's
// main thread, so it works on a copy of the settings (MenuCopy) and hands each change over as a task on the main
// thread (Submit); a change is saved when the slider or box is let go. The reaction table is shown, not edited: it
// lives in Crossfire_Rules.ini, which a patch can add to. The look is the shared MenuStyle.h in a fiery red-orange, with
// each element in its own colour in the table.

#include "Plugin.h"

#include "SKSEMenuFramework.h"
#include "Translation.h"
#include "MenuStyle.h"

namespace Crossfire
{
	namespace
	{
		constexpr ImGuiMCP::ImVec4 kGold{ 1.0f, 0.86f, 0.55f, 1.0f };
		constexpr ImGuiMCP::ImVec4 kDim{ 0.75f, 0.72f, 0.66f, 1.0f };
		using Translation::T;
#define TR_MARK(x) x  // a line Translation.json carries, though it is not written inside T( ) here
		// the names Core and Struggle.cpp hand the menu and the bar (their own code stays English: the files read them)
		[[maybe_unused]] constexpr const char* kShownNames[] = { TR_MARK("Fire"), TR_MARK("Frost"), TR_MARK("Shock"), TR_MARK("Poison"),
			TR_MARK("Arcane"), TR_MARK("Physical"), TR_MARK("Force"), TR_MARK("Pass"), TR_MARK("Clash"), TR_MARK("Annihilate"),
			TR_MARK("Wins"), TR_MARK("Loses"), TR_MARK("Alteration"), TR_MARK("Conjuration"), TR_MARK("Destruction"),
			TR_MARK("Illusion"), TR_MARK("Restoration"), TR_MARK("Level"), TR_MARK("Thu'um") };
		// T() for a name that is not a string literal: the English is kept here, so the pointer T() hands back (the English
		// itself when there is no translation) outlives the call (render thread only)
		const char* TS(std::string_view a_english)
		{
			static std::unordered_set<std::string> kept;
			return T(kept.emplace(a_english).first->c_str());
		}

		class GlowStyle
		{
		public:
			GlowStyle()
			{
				using namespace ImGuiMCP;
				PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.13f, 0.08f, 0.07f, 0.75f });
				PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{ 0.32f, 0.14f, 0.09f, 0.75f });
				PushStyleColor(ImGuiCol_Separator, ImVec4{ 1.0f, 0.55f, 0.35f, 0.25f });
			}
			~GlowStyle() { ImGuiMCP::PopStyleColor(3); }
			GlowStyle(const GlowStyle&) = delete;
			GlowStyle& operator=(const GlowStyle&) = delete;

		private:
			MenuStyle::Page page;  // the shared accent, rounding and hovers
		};
		namespace Icon = MenuStyle::Icon;

		void Heading(unsigned a_icon, const char* a_text) { MenuStyle::Header(a_icon, T(a_text)); }

		// sliders and choices take half the page, so a paired switch can sit beside its partner at the same column
		class Layout
		{
		public:
			Layout() :
				column(ImGuiMCP::GetContentRegionAvail().x * 0.5f)
			{
				ImGuiMCP::PushItemWidth(column * 0.9f);
			}
			~Layout() { ImGuiMCP::PopItemWidth(); }
			Layout(const Layout&) = delete;
			Layout& operator=(const Layout&) = delete;

			void Beside() const { ImGuiMCP::SameLine(column); }

			const float column;
		};

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
			a_e.After(ImGuiMCP::Checkbox(T(a_label), &a_value));
			ImGuiMCP::SetItemTooltip("%s", T(a_tip));
		}

		void Slider(Edit& a_e, const char* a_label, const char* a_key, float& a_value, const char* a_format, const char* a_tip)
		{
			const auto r = Core::RangeOf(a_key);
			a_e.After(ImGuiMCP::SliderFloat(T(a_label), &a_value, r.lo, r.hi, T(a_format), ImGuiMCP::ImGuiSliderFlags_AlwaysClamp));
			ImGuiMCP::SetItemTooltip("%s", T(a_tip));
		}

		void SliderI(Edit& a_e, const char* a_label, const char* a_key, int& a_value, const char* a_tip)
		{
			const auto r = Core::RangeOf(a_key);
			a_e.After(ImGuiMCP::SliderInt(T(a_label), &a_value, static_cast<int>(r.lo), static_cast<int>(r.hi), "%d", ImGuiMCP::ImGuiSliderFlags_AlwaysClamp));
			ImGuiMCP::SetItemTooltip("%s", T(a_tip));
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

		// a reaction in its colour: a win green, a loss red, both going amber, passing by grey
		ImGuiMCP::ImVec4 ActionColour(Core::Action a_action)
		{
			switch (a_action) {
			case Core::Action::kWins:
				return { 0.5f, 0.88f, 0.5f, 1.0f };
			case Core::Action::kLoses:
				return { 1.0f, 0.5f, 0.45f, 1.0f };
			case Core::Action::kAnnihilate:
				return { 1.0f, 0.7f, 0.35f, 1.0f };
			case Core::Action::kPass:
				return MenuStyle::kMuted;
			default:
				return kDim;
			}
		}

		void DrawTable(const Core::Config& a_cfg)
		{
			using namespace ImGuiMCP;
			constexpr int kColumns = static_cast<int>(Core::kElements) + 1;
			if (!BeginTable("reactions", kColumns, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				return;
			}
			TableSetupColumn(T("vs"));
			for (std::size_t j = 0; j < Core::kElements; ++j) {
				TableSetupColumn(TS(Core::ElementName(static_cast<Core::Element>(j))));
			}
			TableNextRow(ImGuiTableRowFlags_Headers);
			TableNextColumn();
			TextColored(kDim, "%s", T("vs"));
			for (std::size_t j = 0; j < Core::kElements; ++j) {
				TableNextColumn();
				TextColored(ElementColour(static_cast<std::uint8_t>(j), 1.0f), "%s", TS(Core::ElementName(static_cast<Core::Element>(j))));
			}
			for (std::size_t i = 0; i < Core::kElements; ++i) {
				TableNextRow();
				TableNextColumn();
				TextColored(ElementColour(static_cast<std::uint8_t>(i), 1.0f), "%s", TS(Core::ElementName(static_cast<Core::Element>(i))));
				for (std::size_t j = 0; j < Core::kElements; ++j) {
					TableNextColumn();
					const auto action = a_cfg.reactions[i][j];
					TextColored(ActionColour(action), "%s", TS(Core::ActionName(action)));
				}
			}
			EndTable();
		}

		void __stdcall Render()
		{
			const GlowStyle style;
			const Layout    layout;
			Edit            e;
			auto&           c = e.cfg;

			Check(e, "Enabled", c.enabled, "Projectiles that meet in the air clash. Off: Crossfire does nothing at all.");
			ImGuiMCP::BeginDisabled(!c.enabled);

			Heading(Icon::kShield, "Who clashes");
			{
				const char* who[]{ T("Everyone's projectiles"), T("Only when yours are involved") };
				e.After(ImGuiMCP::Combo(T("Projectiles"), &c.who, who, 2));
				ImGuiMCP::SetItemTooltip("%s", T("Everyone: two enemy mages' spells can meet too. Yours only: a clash always involves one of your projectiles."));
			}
			Check(e, "Allies pass through", c.ignoreAllies, "Projectiles of people who are not hostile to each other (you and your follower) never clash.");

			Heading(Icon::kBolt, "Clash");
			Slider(e, "Overpower", "OverpowerRatio", c.overpowerRatio, "x%.1f", "A projectile this many times stronger than the other destroys it and flies on. Closer than that, both go.");
			Slider(e, "Aim assist", "RadiusBonus", c.radiusBonus, "%.0f units", "Added to every projectile's size, so a shot does not have to be perfect. 0 is the game's own sizes.");
			Check(e, "The survivor is weakened", c.weakenSurvivor, "The one that flies on loses the strength of what it beat.");
			layout.Beside();
			Check(e, "Bolts meet", c.boltsMeet, "Two lightning bolts (any one-shot bolt) that cross burst between the casters and both go. A fired bolt still counts for a moment after it fades, so they need not be cast in the same instant.");

			Heading(Icon::kFire, "Effects");
			Check(e, "Bursts", c.explosions, "A destroyed projectile bursts where it was hit, with its own explosion.");
			layout.Beside();
			ImGuiMCP::BeginDisabled(!c.explosions);
			Check(e, "Only harmless bursts", c.safeExplosionsOnly, "Skip a burst that would do more than show: damage, an enchantment, something it spawns. Spells' own explosions only show.");
			Check(e, "Stand-in bursts", c.standInBursts,
				T("A spell with no explosion of its own (Firebolt, Ice Spike, Lightning Bolt) bursts the way its element's spells do, so every clash shows."));
			Slider(e, "Burst size", "BurstScale", c.burstScale, "x%.1f", "How big a clash's burst is.");
			ImGuiMCP::EndDisabled();
			Slider(e, "Skill experience", "SkillXP", c.skillXP, "%.0f", "For each enemy projectile one of yours destroys, to the skill that cast it (Archery for arrows). 0: none.");

			ImGuiMCP::EndDisabled();

			Heading(Icon::kList, "What meets what");
			ImGuiMCP::TextDisabled("%s", T("Row against column. Clash: the stronger wins if it is enough stronger, else both go. Edit Crossfire_Rules.ini to change."));
			DrawTable(c);
			if (ImGuiMCP::Button(T("Reload the files"))) {
				ReloadSoon();
			}
			ImGuiMCP::SetItemTooltip("%s", T("Read Crossfire_Rules.ini, the Crossfire folder and Crossfire.ini again."));

			Heading(Icon::kGauge, "This session");
			Check(e, "Log every clash", c.debugLog, "Write each clash to Crossfire.log (Documents\\My Games\\Skyrim Special Edition\\SKSE).");
			auto& s = Counters();
			ImGuiMCP::TextDisabled(T("%u projectile(s) in range now; %llu clash(es), %llu destroyed, %llu weakened, %llu burst(s), %llu by you"),
				s.tracked.load(), static_cast<unsigned long long>(s.clashes.load()), static_cast<unsigned long long>(s.destroyed.load()),
				static_cast<unsigned long long>(s.weakened.load()), static_cast<unsigned long long>(s.explosions.load()),
				static_cast<unsigned long long>(s.yours.load()));
			for (const auto& note : LoadNotes()) {
				ImGuiMCP::TextDisabled("%s", note.c_str());
			}
			ImGuiMCP::TextDisabled("%s", T("Finer tuning (range, sizes, strengths, burst limits) lives in Crossfire.ini."));
		}

		// when "Preview the bar" was pressed (ImGui time, render thread only); the bar shows a made-up struggle for kPreview seconds
		double            gPreviewFrom = -100.0;
		constexpr double  kPreview = 8.0;
		std::atomic<bool> gPreviewAsked{ false };  // DevBench's way in (another thread): taken on the next frame

		void __stdcall RenderStruggles()
		{
			const GlowStyle style;
			const Layout    layout;
			Edit            e;
			auto&           c = e.cfg;
			auto&           s = c.struggle;

			Check(e, "Spell struggles", s.enabled,
				"When enemy sprays, held beams or breath meet, they lock together and push. Whoever has the higher magic skill drives the "
				"meeting point back and breaks through. Off: they clash particle by particle.");
			ImGuiMCP::BeginDisabled(!s.enabled);

			Heading(Icon::kHand, "What locks");
			Check(e, "Sprays", s.sprays, "Flames, Frostbite and other sprays.");
			ImGuiMCP::SameLine(layout.column * 0.5f);
			Check(e, "Beams", s.beams, "Sparks, Lightning Storm and other held beams. A one-shot bolt never locks.");
			layout.Beside();
			Check(e, "Breath", s.breath, "Fire and Frost Breath, yours or a dragon's.");
			Slider(e, "Minimum distance", "MinDistance", s.minGap, "%.0f units",
				"Casters whose hands are closer than this do not lock. Enemies close right in during a fight, so keep it low. 70 units is about a metre.");
			Slider(e, "Lock chance", "StruggleChance", s.chance, "%.0f%%",
				"Chance two casters lock when their streams meet, rolled once each time. A miss: they clash particle by particle until one stops casting.");
			Slider(e, "Dragon lock chance", "DragonChance", s.dragonChance, "%.0f%%", "The same when one side is a dragon. 0: never with dragons.");

			Heading(Icon::kStar, "Who wins");
			Check(e, "Higher magic skill always wins", s.skillAlwaysWins,
				"When both cast from a school of magic, the higher skill always pushes through. Level, magicka, spell and dual casting only "
				"change how fast. Breath is a plain contest of power.");
			Slider(e, "Enemy power", "EnemyPower", s.enemyPower, "x%.2f", "How hard whoever struggles against you pushes. Above 1 is harder.");
			Slider(e, "Time limit", "MaxStruggleTime", s.maxTime, "%.0f s",
				"When time runs out, whoever is ahead breaks through. Dead even: the spells burst and both are thrown off. 0: no limit.");

			Heading(Icon::kTarget, "Breakthrough");
			Check(e, "Stagger the loser", s.stagger, "The overwhelmed caster staggers, with the game's own stagger. Not dragons.");
			layout.Beside();
			Check(e, "Finishers", s.finishers, "A breakthrough that kills throws the body away from the winner. Experimental.");
			Slider(e, "Breakthrough damage", "OverwhelmDamage", s.overwhelmDamage, "x%.2f",
				"An extra hit on the overwhelmed caster, about two seconds of the winning stream, less their resistance. 0: only the stream's own damage.");

			Heading(Icon::kDisplay, "Show");
			Check(e, "Struggle bar", s.bar, "While you are locked, a bar shows who is pushing, in each spell's colour, with each spell's sigil.");
			layout.Beside();
			Check(e, "Messages", s.messages, "A message when you overwhelm someone, they overwhelm you, they give way, or the spells burst between you.");
			ImGuiMCP::BeginDisabled(!s.bar);
			Check(e, "Show names", s.barNames, "Your name and theirs over the bar.");
			layout.Beside();
			Check(e, "Show skill numbers", s.barSkills, "Both magic skills under the bar.");
			Slider(e, "Bar position", "BarHeight", s.barHeight, "%.0f%%", "How far down the screen the bar sits.");
			Slider(e, "Bar size", "BarScale", s.barScale, "x%.2f", "How big the bar is.");
			if (ImGuiMCP::Button(T("Preview the bar"))) {
				gPreviewFrom = ImGuiMCP::GetTime();
			}
			ImGuiMCP::SetItemTooltip("%s", T("Shows the bar for a few seconds, as a struggle of fire against frost, so you can see its place and size."));
			ImGuiMCP::EndDisabled();

			ImGuiMCP::EndDisabled();

			Heading(Icon::kEye, "Now");
			const auto v = StruggleNow();
			MenuStyle::Status(v.active, v.active ? T("Locked") : T("Free"));
			if (v.active) {
				ImGuiMCP::Text(T("Locked with %s: %s %d against %d, %.1f s, the lock %s"), v.foe, T(v.school), v.mySkill, v.theirSkill, v.seconds,
					v.balance > 0.02f ? T("moving toward them") : (v.balance < -0.02f ? T("moving toward you") : T("even")));
			} else {
				ImGuiMCP::TextDisabled("%s", T("Not locked with anyone."));
			}
			auto& n = Counters();
			ImGuiMCP::TextDisabled(T("This session: %llu struggle(s), %llu broken through, %llu won and %llu lost by you, %llu gave way, %llu draw(s)"),
				static_cast<unsigned long long>(n.struggles.load()), static_cast<unsigned long long>(n.overwhelms.load()),
				static_cast<unsigned long long>(n.won.load()), static_cast<unsigned long long>(n.lost.load()),
				static_cast<unsigned long long>(n.gaveWay.load()), static_cast<unsigned long long>(n.draws.load()));
			if (ImGuiMCP::Button(T("Struggle defaults"))) {
				s = {};
				e.changed = e.save = true;
			}
			ImGuiMCP::SetItemTooltip("%s", T("Every struggle setting back to how it came, the ones in Crossfire.ini too."));
			ImGuiMCP::TextDisabled("%s", T("Finer tuning (skill, level and spell weights, magicka, surges, reeling, camera shake) lives in Crossfire.ini."));
		}

		// a spell's sigil for the struggle bar: a ring in the element's colour with its mark inside
		void DrawSigil(ImGuiMCP::ImDrawList* a_dl, ImGuiMCP::ImVec2 a_c, float a_r, std::uint8_t a_element, float a_alpha)
		{
			using namespace ImGuiMCP;
			const ImU32 c = ColorConvertFloat4ToU32(ElementColour(a_element, a_alpha));
			const ImU32 back = ColorConvertFloat4ToU32(ImVec4{ 0.0f, 0.0f, 0.0f, 0.55f * a_alpha });
			const float t = std::max(1.0f, a_r * 0.16f);
			const auto  at = [&](float x, float y) { return ImVec2{ a_c.x + x * a_r, a_c.y + y * a_r }; };
			ImDrawListManager::AddCircleFilled(a_dl, a_c, a_r, back, 32);
			ImDrawListManager::AddCircle(a_dl, a_c, a_r, c, 32, t);
			switch (static_cast<Core::Element>(a_element)) {
			case Core::Element::kFire:  // a flame: a tongue rising off a round base
				ImDrawListManager::AddCircleFilled(a_dl, at(0.0f, 0.22f), a_r * 0.3f, c, 16);
				ImDrawListManager::AddTriangleFilled(a_dl, at(-0.3f, 0.18f), at(0.08f, -0.62f), at(0.3f, 0.18f), c);
				break;
			case Core::Element::kFrost:  // a snowflake: three crossed strokes
				for (int i = 0; i < 3; ++i) {
					const float ang = 1.5708f + i * 1.0472f;
					ImDrawListManager::AddLine(a_dl, at(std::cos(ang) * 0.6f, -std::sin(ang) * 0.6f), at(-std::cos(ang) * 0.6f, std::sin(ang) * 0.6f), c, t);
				}
				break;
			case Core::Element::kShock: {  // a bolt
				const ImVec2 bolt[]{ at(0.15f, -0.62f), at(-0.25f, 0.05f), at(0.08f, 0.05f), at(-0.15f, 0.62f) };
				ImDrawListManager::AddPolyline(a_dl, bolt, 4, c, 0, t * 1.2f);
				break;
			}
			case Core::Element::kPoison:  // a drop
				ImDrawListManager::AddCircleFilled(a_dl, at(0.0f, 0.2f), a_r * 0.32f, c, 16);
				ImDrawListManager::AddTriangleFilled(a_dl, at(-0.3f, 0.12f), at(0.0f, -0.58f), at(0.3f, 0.12f), c);
				break;
			case Core::Element::kForce:  // a ring inside the ring
				ImDrawListManager::AddCircle(a_dl, a_c, a_r * 0.45f, c, 24, t);
				ImDrawListManager::AddCircleFilled(a_dl, a_c, a_r * 0.15f, c, 12);
				break;
			case Core::Element::kPhysical:  // a blade's point
				ImDrawListManager::AddQuadFilled(a_dl, at(0.0f, -0.6f), at(0.22f, 0.0f), at(0.0f, 0.6f), at(-0.22f, 0.0f), c);
				break;
			default:  // anything else: a four-point star
				ImDrawListManager::AddQuadFilled(a_dl, at(0.0f, -0.6f), at(0.14f, 0.0f), at(0.0f, 0.6f), at(-0.14f, 0.0f), c);
				ImDrawListManager::AddQuadFilled(a_dl, at(-0.6f, 0.0f), at(0.0f, 0.14f), at(0.6f, 0.0f), at(0.0f, -0.14f), c);
				break;
			}
		}

		// the struggle bar, on the render thread: it reads only StruggleNow()
		void __stdcall DrawStruggleBar()
		{
			using namespace ImGuiMCP;
			static float alpha = 0.0f;
			if (gPreviewAsked.exchange(false)) {
				gPreviewFrom = GetTime();
			}
			auto         v = StruggleNow();
			if (const double t = GetTime() - gPreviewFrom; !v.active && t >= 0.0 && t < kPreview) {
				const auto cfg = MenuCopy().struggle;
				v.active = true;
				v.bar = cfg.bar;
				v.barHeight = cfg.barHeight;
				v.barScale = cfg.barScale;
				v.barOpacity = cfg.barOpacity;
				v.barNames = cfg.barNames;
				v.barSkills = cfg.barSkills;
				v.balance = 0.45f * static_cast<float>(std::sin(t * 1.3));
				v.mine = static_cast<std::uint8_t>(Core::Element::kFire);
				v.theirs = static_cast<std::uint8_t>(Core::Element::kFrost);
				v.mySkill = 82;
				v.theirSkill = 64;
				std::snprintf(v.school, sizeof(v.school), "%s", "Destruction");
				std::snprintf(v.theirSchool, sizeof(v.theirSchool), "%s", "Destruction");
				std::snprintf(v.foe, sizeof(v.foe), "%s", T("Bandit Mage"));
			}
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
			const float w = 420.0f * k, h = 6.0f * k;
			const float cx = io->DisplaySize.x * 0.5f, yc = io->DisplaySize.y * std::clamp(v.barHeight, 0.0f, 100.0f) / 100.0f;
			const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
			const float split = x0 + w * std::clamp((1.0f + v.balance) * 0.5f, 0.0f, 1.0f);
			const auto  col = [a](float r, float g, float b, float o) { return ColorConvertFloat4ToU32(ImVec4{ r, g, b, o * a }); };
			const auto  elem = [a](std::uint8_t e, float o) { return ColorConvertFloat4ToU32(ElementColour(e, o * a)); };
			const ImU32 bone = col(0.86f, 0.82f, 0.73f, 0.9f);

			// the track: the game's meter, a dark slot with a faint bone edge
			ImDrawListManager::AddRectFilled(dl, ImVec2{ x0, yc - h * 0.5f - 1.0f }, ImVec2{ x1, yc + h * 0.5f + 1.0f }, col(0.0f, 0.0f, 0.0f, 0.6f), 0.0f, 0);
			ImDrawListManager::AddRect(dl, ImVec2{ x0, yc - h * 0.5f - 1.0f }, ImVec2{ x1, yc + h * 0.5f + 1.0f }, col(0.78f, 0.75f, 0.65f, 0.45f), 0.0f, 0, 1.0f);
			// each side's spell, faint at its own end and brightest where they meet
			ImDrawListManager::AddRectFilledMultiColor(dl, ImVec2{ x0, yc - h * 0.5f }, ImVec2{ split, yc + h * 0.5f },
				elem(v.mine, 0.4f), elem(v.mine, 1.0f), elem(v.mine, 1.0f), elem(v.mine, 0.4f));
			ImDrawListManager::AddRectFilledMultiColor(dl, ImVec2{ split, yc - h * 0.5f }, ImVec2{ x1, yc + h * 0.5f },
				elem(v.theirs, 1.0f), elem(v.theirs, 0.4f), elem(v.theirs, 0.4f), elem(v.theirs, 1.0f));
			// the bracket ends: a tick and a small outward chevron, as on the health and magicka meters
			for (const auto [x, s] : { std::pair{ x0, 1.0f }, std::pair{ x1, -1.0f } }) {
				ImDrawListManager::AddLine(dl, ImVec2{ x, yc - h * 1.6f }, ImVec2{ x, yc + h * 1.6f }, bone, std::max(1.0f, k));
				ImDrawListManager::AddTriangleFilled(dl, ImVec2{ x - s * 10.0f * k, yc }, ImVec2{ x - s * 3.0f * k, yc - h * 0.9f },
					ImVec2{ x - s * 3.0f * k, yc + h * 0.9f }, bone);
			}
			// the glow where the spells meet: soft rings, widest first, breathing a little
			const float pulse = 0.85f + 0.15f * static_cast<float>(std::sin(GetTime() * 4.0));
			for (int i = 0; i < 8; ++i) {
				const float r = (26.0f - i * 2.8f) * k * pulse;
				ImDrawListManager::AddCircleFilled(dl, ImVec2{ split, yc }, r, col(1.0f, 0.92f, 0.75f, 0.05f + i * 0.012f), 24);
			}
			// the meeting point: the compass's diamond
			const float r = 7.0f * k;
			ImDrawListManager::AddQuadFilled(dl, ImVec2{ split, yc - r }, ImVec2{ split + r, yc }, ImVec2{ split, yc + r }, ImVec2{ split - r, yc },
				col(0.96f, 0.93f, 0.82f, 1.0f));
			ImDrawListManager::AddQuad(dl, ImVec2{ split, yc - r }, ImVec2{ split + r, yc }, ImVec2{ split, yc + r }, ImVec2{ split - r, yc },
				col(0.16f, 0.13f, 0.10f, 1.0f), 1.0f);
			// each spell's sigil beyond its end
			DrawSigil(dl, ImVec2{ x0 - 34.0f * k, yc }, 11.0f * k, v.mine, a);
			DrawSigil(dl, ImVec2{ x1 + 34.0f * k, yc }, 11.0f * k, v.theirs, a);

			// names over the bar and skills under it, each only when asked for; capitals, with a drop shadow
			auto*       font = GetFont();
			const float fs = GetFontSize();
			const auto  label = [&](const char* t, float size, float y, bool right, ImU32 c) {
				const float tw = CalcTextSize(t).x * size / fs;
				const float x = right ? x1 - tw : x0;
				ImDrawListManager::AddText(dl, font, size, ImVec2{ x + 1.0f, y + 1.0f }, col(0.0f, 0.0f, 0.0f, 0.8f), t);
				ImDrawListManager::AddText(dl, font, size, ImVec2{ x, y }, c, t);
			};
			if (v.barNames) {
				char foe[64];
				std::snprintf(foe, sizeof(foe), "%s", v.foe);
				for (auto& ch : foe) {
					ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
				}
				const float size = fs * 1.15f * k;
				char you[64];
				std::snprintf(you, sizeof(you), "%s", T("You"));
				for (auto& ch : you) {
					ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
				}
				label(you, size, yc - h * 1.6f - size - 4.0f * k, false, col(0.91f, 0.88f, 0.80f, 1.0f));
				label(foe, size, yc - h * 1.6f - size - 4.0f * k, true, col(0.91f, 0.88f, 0.80f, 1.0f));
			}
			if (v.barSkills) {
				char mine[32], theirs[32];
				std::snprintf(mine, sizeof(mine), "%s %d", T(v.school), v.mySkill);
				std::snprintf(theirs, sizeof(theirs), "%s %d", T(v.theirSchool), v.theirSkill);
				for (auto* t : { mine, theirs }) {
					for (; *t; ++t) {
						*t = static_cast<char>(std::toupper(static_cast<unsigned char>(*t)));
					}
				}
				const float size = fs * 0.85f * k;
				label(mine, size, yc + h * 1.6f + 3.0f * k, false, col(0.75f, 0.71f, 0.63f, 1.0f));
				label(theirs, size, yc + h * 1.6f + 3.0f * k, true, col(0.75f, 0.71f, 0.63f, 1.0f));
			}
		}
	}

	void RequestBarPreview() { gPreviewAsked = true; }

	void RegisterMenu()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			SKSE::log::info("SKSE Menu Framework is not installed, so there is no settings page; Crossfire.ini still applies");
			return;
		}
		MenuStyle::gTheme = MenuStyle::MakeTheme(0xFF7A45);  // fiery red-orange
		SKSEMenuFramework::SetSection(T("Crossfire"));
		SKSEMenuFramework::AddSectionItem(T("Settings"), Render);
		SKSEMenuFramework::AddSectionItem(T("Spell struggles"), RenderStruggles);
		SKSEMenuFramework::AddHudElement(DrawStruggleBar);
		SKSE::log::info("settings page added to SKSE Menu Framework {}", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
