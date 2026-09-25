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
		float         cost{ 0.0f };  // the spell's base cost (a spray: per second; a shout's spray: ShoutStrength)
		RE::ActorValue skill{ RE::ActorValue::kNone };  // what the player's skill experience goes to
	};
	[[nodiscard]] const Info& InfoOf(RE::Projectile* a_projectile, const RE::BGSProjectile* a_base);  // main thread
	void                      ClearInfo();                                                             // main thread
	[[nodiscard]] RE::BGSExplosion* ExplosionOf(RE::Projectile* a_projectile, const RE::BGSProjectile* a_base);
	[[nodiscard]] bool              SafeExplosion(const RE::BGSExplosion* a_explosion);
	void                            Survey();  // once, at data load: logs how many projectiles there are of each kind

	// ------------------------------------------------------------------ Clash.cpp: the pass, once a frame
	void Update(float a_delta);  // main thread, from the player's update
	void Reset();                // a save is loading, or a new game: every handle we hold is meaningless now

	struct Stats
	{
		std::atomic<std::uint32_t> tracked{ 0 };  // projectiles looked at in the last frame
		std::atomic<std::uint64_t> clashes{ 0 }, destroyed{ 0 }, weakened{ 0 }, explosions{ 0 }, yours{ 0 };
	};
	[[nodiscard]] Stats& Counters();

	// ------------------------------------------------------------------ Menu.cpp
	void RegisterMenu();
}
