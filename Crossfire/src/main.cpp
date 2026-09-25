// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version. See LICENSE.txt.
//
// Projectiles that meet in the air clash: a fireball and an ice spike cancel in a burst of steam, an arrow can shoot
// down a firebolt, a big spell punches through a small one and flies on weaker, a shout swats everything aside.
//
// THE FILES, AND WHAT EACH ONE IS FOR
//   main.cpp        this file - loading, the one hook (the player's update, once a frame), save/load
//   Core.h/.cpp     everything that does not touch the game: shapes, finding touches, the reaction table, the
//                   strength contest, the settings files. Tested on its own: tests/run.sh
//   Classify.cpp    what a projectile is to Crossfire (hostile? which element? how costly?), remembered
//   Clash.cpp       the pass, once a frame: gather, find touches, settle them
//   Settings.cpp    the files: Crossfire_Rules.ini, Crossfire\*.ini, Crossfire.ini
//   Menu.cpp        the settings page, in SKSE Menu Framework's Mod Control Panel
//   CrossfireAPI.h  what other SKSE plugins are told of each clash
//   Plugin.h        what the files share      PCH.h  what they all include

#include "Plugin.h"

namespace
{
	// the player's update runs once a frame on the main thread while the game is not paused; after it, every
	// projectile has moved for this frame
	struct PlayerUpdate
	{
		static void thunk(RE::PlayerCharacter* a_this, float a_delta)
		{
			func(a_this, a_delta);
			Crossfire::Update(a_delta);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::PlayerCharacter::VTABLE[0] };
		PlayerUpdate::func = vtbl.write_vfunc(0xAD, PlayerUpdate::thunk);
		SKSE::log::info("the player's update hooked");
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	SKSE::log::info("{} {} loading", plugin->GetName(), plugin->GetVersion().string());
	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
		if (!a_msg) {
			return;
		}
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			Crossfire::LoadSettings();
			Crossfire::Survey();
			Install();
			Crossfire::RegisterMenu();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Crossfire::Reset();  // every handle we remember belonged to the world that is going away
			break;
		default:
			break;
		}
	});
	return true;
}
