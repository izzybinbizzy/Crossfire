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
	}

	void RegisterMenu()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			SKSE::log::info("SKSE Menu Framework is not installed, so there is no settings page; Crossfire.ini still applies");
			return;
		}
		SKSEMenuFramework::SetSection("Crossfire");
		SKSEMenuFramework::AddSectionItem("Settings", Render);
		SKSE::log::info("settings page added to SKSE Menu Framework {}", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
