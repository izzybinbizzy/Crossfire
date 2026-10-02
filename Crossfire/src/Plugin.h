// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.txt and the notice at the top of main.cpp.
//
// What the files share. The file map is at the top of main.cpp.

#pragma once

#include "Core.h"

namespace Crossfire
{
	// ------------------------------------------------------------------ Settings.cpp: the files
	//
	// Read in this order, each on top of the last: Crossfire_Rules.ini (shipped: elements, reactions, exclusions), every
	// Crossfire\*.ini (patches, alphabetical), Crossfire.ini (the menu's). Live() is the game's main thread's own copy;
	// the menu works on a copy of its own (MenuCopy/Submit), handed over as a task on the main thread.
	void                              LoadSettings();  // main thread: at data load, and "Reload" in the menu
	[[nodiscard]] const Core::Config& Live();          // main thread only
	[[nodiscard]] Core::Config        MenuCopy();      // any thread
	void                              Submit(const Core::Config& a_config, bool a_save);  // any thread; the rules are kept
	void                              ReloadSoon();    // any thread
	[[nodiscard]] std::vector<std::string> LoadNotes();  // any thread: what the last load said (warnings, files read)
	[[nodiscard]] bool                Excluded(const RE::TESForm* a_form);  // main thread

	// ------------------------------------------------------------------ Classify.cpp: what a projectile is to Crossfire
	struct Info
	{
		bool          eligible{ false };  // false: Crossfire never touches it
		Core::Element element{ Core::Element::kArcane };
		Core::Kind    kind{ Core::Kind::kSpell };
		Core::Shape   shape{ Core::Shape::kSphere };
		bool          immune{ false };
		bool          cone{ false };
		Core::Stream  stream{ Core::Stream::kNone };  // what kind of stream it is, if one that can lock in a struggle
		float         cost{ 0.0f };  // the spell's base cost (a spray: per second; a shout's spray: ShoutStrength)
		RE::ActorValue skill{ RE::ActorValue::kNone };  // what the player's skill experience goes to
		bool           staff{ false };   // cast from a staff: pays no magicka, counts as two hands in a struggle
		bool           absorb{ false };  // an Absorb effect: a breakthrough also heals the winner
		float          magnitude{ 0.0f };  // the costliest effect's magnitude
		RE::ActorValue resist{ RE::ActorValue::kNone };  // what resists it
		const RE::MagicItem* spell{ nullptr };
	};
	[[nodiscard]] const Info& InfoOf(RE::Projectile* a_projectile, const RE::BGSProjectile* a_base);  // main thread
	void                      ClearInfo();                                                             // main thread
	[[nodiscard]] RE::BGSExplosion* ExplosionOf(RE::Projectile* a_projectile, const RE::BGSProjectile* a_base);
	[[nodiscard]] bool              SafeExplosion(const RE::BGSExplosion* a_explosion);
	void                            Survey();  // once, at data load: logs how many projectiles there are of each kind
	void                            FindFearSpell();  // once, at data load: the game's own Fear, for Intimidate
	void                            FindStandInBursts();  // once, at data load: a burst per element for spells with none
	[[nodiscard]] RE::BGSExplosion* StandInBurst(Core::Element a_element);  // nullptr: none that only shows
	[[nodiscard]] const char*       StandInModel(Core::Element a_element);  // the model of one that carries harm, or nullptr
	void                            FindClashArt();  // once, at data load: meshes\Crossfire\Clash<Element>.nif that exist
	[[nodiscard]] const char*       ClashArt(Core::Element a_element);  // that mesh (under meshes\), or nullptr
	[[nodiscard]] RE::SpellItem*    FearSpell();      // null when none was found

	// ------------------------------------------------------------------ Clash.cpp: the pass, once a frame
	void Update(float a_delta);  // main thread, from the player's update
	void Reset();                // a save is loading, or a new game: every handle we hold is meaningless now

	struct Stats
	{
		std::atomic<std::uint32_t> tracked{ 0 };  // projectiles looked at in the last frame
		std::atomic<std::uint64_t> clashes{ 0 }, destroyed{ 0 }, weakened{ 0 }, explosions{ 0 }, yours{ 0 };
		// spell struggles: begun, broken through, won and lost by you, given way, drawn, and particles snuffed at a lock
		std::atomic<std::uint64_t> struggles{ 0 }, overwhelms{ 0 }, won{ 0 }, lost{ 0 }, gaveWay{ 0 }, draws{ 0 }, cut{ 0 };
	};
	[[nodiscard]] Stats& Counters();

	// one stream projectile of this frame's pass, for Struggle.cpp; `entry` stays valid until the end of the pass
	struct StreamRef
	{
		std::size_t                   entry{ 0 };
		RE::Projectile*               projectile{ nullptr };
		RE::ProjectileHandle          handle;
		std::uint32_t                 native{ 0 }, shooter{ 0 };
		RE::ObjectRefHandle           shooterHandle;
		RE::Actor*                    actor{ nullptr };
		bool                          player{ false };
		const Info*                   info{ nullptr };
		const RE::BGSProjectile*      base{ nullptr };
		Core::Vec3                    now, dir;  // where it is, and which way it goes (a beam: along itself)
		float                         radius{ 0.0f }, power{ 1.0f }, length{ 0.0f };  // length: a beam's, as drawn
		RE::MagicSystem::CastingSource source{ RE::MagicSystem::CastingSource::kOther };
	};
	void Snuff(std::size_t a_entry);  // kill it quietly: no burst, no event, no experience
	bool BurstAt(std::size_t a_entry, Core::Vec3 a_at, std::uint64_t a_pair, const Core::Config& a_config);  // its own explosion

	// ------------------------------------------------------------------ Struggle.cpp: streams that lock (main thread)
	namespace Struggles
	{
		[[nodiscard]] bool Reeling(std::uint32_t a_shooter);  // it lost a struggle a moment ago: its streams fizzle
		void Frame(std::span<const StreamRef> a_streams, const Core::Config& a_config, float a_delta, std::uint32_t a_frame);
		[[nodiscard]] bool Holds(std::uint32_t a, std::uint32_t b);  // locked together, or one reels from the other
		// two streams of different shooters touched: true when that starts a lock (or one is already on), so the touch
		// is not settled as a clash
		[[nodiscard]] bool TryBegin(const StreamRef& a, const StreamRef& b, Core::Vec3 a_point, const Core::Config& a_config);
		[[nodiscard]] bool Active();
		void Reset(bool a_restoreBeams);  // false on a load: the handles belong to the world that went away
	}

	// what the struggle bar shows; a copy, from any thread
	struct StruggleView
	{
		bool         active{ false };
		float        balance{ 0.0f };  // + : you are pushing it toward them
		float        lead{ 0.0f }, seconds{ 0.0f };
		std::uint8_t mine{ 0 }, theirs{ 0 };  // elements
		int          mySkill{ 0 }, theirSkill{ 0 };
		bool         myBreath{ false }, theirBreath{ false };
		char         school[16]{}, theirSchool[16]{};
		char         foe[64]{};
		bool         bar{ true };
		float        barHeight{ 82.0f }, barScale{ 1.0f }, barOpacity{ 0.9f };
		bool         barNames{ false }, barSkills{ false };
	};
	[[nodiscard]] StruggleView StruggleNow();

	// ------------------------------------------------------------------ Menu.cpp
	void RegisterMenu();
	void RequestBarPreview();  // any thread: the struggle bar shows its preview from the next frame

	// ------------------------------------------------------------------ DevBench.cpp
	void OfferToDevBench();
}
