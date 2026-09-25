// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.txt and the notice at the top of main.cpp.
//
// The pass, once a frame, on the game's main thread (from the player's update, main.cpp):
//   1. copy the projectile manager's handles (under its lock), and for each live, hostile projectile near the player
//      make a Core::Body: where it was last frame and where it is now, how big, what element, how strong
//   2. Core::FindContacts: which of them touched during the frame, earliest first
//   3. settle each touch with Core::Resolve: the loser is killed (after its own explosion is placed where they met),
//      a winner that pays for it is weakened, the player may earn skill experience, and listeners are told
// Everything the game gives us is checked before it is used; anything odd is simply left out of the frame.

#include "Plugin.h"

#include "CrossfireAPI.h"

namespace Crossfire
{
	namespace
	{
		using Core::Vec3;

		[[nodiscard]] Vec3         V(const RE::NiPoint3& a_p) noexcept { return { a_p.x, a_p.y, a_p.z }; }
		[[nodiscard]] RE::NiPoint3 N(Vec3 a_v) noexcept { return { a_v.x, a_v.y, a_v.z }; }

		// what we remember of a projectile between frames, by its handle
		struct Track
		{
			Vec3          last;
			float         strength{ 0.0f };
			std::uint32_t frame{ 0 };
		};

		// one projectile in this frame's pass
		struct Entry
		{
			RE::NiPointer<RE::Projectile>    ref;
			RE::NiPointer<RE::TESObjectREFR> shooterRef;
			const RE::BGSProjectile*         base{ nullptr };
			Info                             info;  // a copy: the cache it came from may change
			Track*                           track{ nullptr };  // a node of gTracks: stable until the prune at the end
			Vec3                             now;
			std::uint32_t                    handle{ 0 };
			std::uint32_t                    shooter{ 0 };  // the shooter's handle, 0 for none
			RE::Actor*                       actor{ nullptr };
			bool                             player{ false };
			bool                             killed{ false };
		};

		Stats gStats;

		std::unordered_map<std::uint32_t, Track>  gTracks;
		std::unordered_map<std::uint32_t, double> gKilled;  // handles we killed, and when: never touched again
		std::unordered_map<std::uint64_t, bool>   gHostile;  // this frame's answers, by pair of shooters
		std::unordered_set<std::uint64_t>         gSettled;  // pairs of projectiles already settled: a bolt or a wall that
		                                                     // lasts several frames meets a projectile once, not every frame
		std::vector<RE::ProjectileHandle>          gHandles;
		std::vector<Entry>                         gEntries;
		std::vector<Core::Body>                    gBodies;
		std::vector<Core::Contact>                 gContacts;
		Core::Limiter                              gBursts, gXP, gEvents;
		std::uint32_t                              gFrame = 0;
		double                                     gClock = 0.0;  // game seconds since the game started; never goes back

		[[nodiscard]] std::uint64_t PairKey(std::uint32_t a, std::uint32_t b) noexcept
		{
			return (static_cast<std::uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
		}

		void Gather()
		{
			gHandles.clear();
			auto* manager = RE::Projectile::Manager::GetSingleton();
			if (!manager) {
				return;
			}
			RE::BSSpinLockGuard guard(manager->projectileLock);
			gHandles.reserve(manager->limited.size() + manager->unlimited.size());
			for (const auto& h : manager->limited) {
				gHandles.push_back(h);
			}
			for (const auto& h : manager->unlimited) {
				gHandles.push_back(h);
			}
		}

		[[nodiscard]] float StrengthOf(const Info& a_info, const RE::Projectile::PROJECTILE_RUNTIME_DATA& a_rd, const Core::Tuning& a_tuning)
		{
			float damage = 0.0f;
			if (a_info.kind == Core::Kind::kArrow) {
				damage = a_rd.weaponDamage;
				if (!(std::isfinite(damage) && damage > 0.0f)) {
					damage = 0.0f;
					if (a_rd.ammoSource) {
						damage += a_rd.ammoSource->GetRuntimeData().data.damage;
					}
					if (a_rd.weaponSource) {
						damage += static_cast<float>(a_rd.weaponSource->GetAttackDamage());
					}
				}
			}
			return Core::BaseStrength(a_info.kind, a_info.cost, a_rd.power, damage, a_tuning);
		}

		// not const: CommonLib's BSSimpleList cannot be walked through a const reference
		[[nodiscard]] float BeamLength(RE::Projectile::PROJECTILE_RUNTIME_DATA& a_rd, const RE::BGSProjectile* a_base, Vec3 a_from)
		{
			float length = a_rd.range > 0.0f ? a_rd.range : a_base->data.range;
			if (!(std::isfinite(length) && length > 0.0f)) {
				length = 3000.0f;
			}
			// the bolt ends where it hit
			for (const auto* impact : a_rd.impacts) {
				if (impact) {
					const float d = Core::Length(V(impact->desiredTargetLoc) - a_from);
					if (std::isfinite(d) && d > 1.0f) {
						length = std::min(length, d);
					}
					break;
				}
			}
			return std::clamp(length, 1.0f, 20000.0f);
		}

		[[nodiscard]] bool Hostile(const Entry& a, const Entry& b)
		{
			const auto key = PairKey(a.shooter, b.shooter);
			if (const auto it = gHostile.find(key); it != gHostile.end()) {
				return it->second;
			}
			const bool hostile = a.actor->IsHostileToActor(b.actor) || b.actor->IsHostileToActor(a.actor);
			gHostile.emplace(key, hostile);
			return hostile;
		}

		// the projectile's own explosion, placed where the two met - before it is killed, while its cell is sure
		void Burst(RE::Projectile* a_projectile, RE::BGSExplosion* a_explosion, Vec3 a_at)
		{
			auto* data = RE::TESDataHandler::GetSingleton();
			auto* cell = a_projectile->GetParentCell();
			if (!data || !cell || !Core::Finite(a_at)) {
				return;
			}
			const auto handle = data->CreateReferenceAtLocation(a_explosion, N(a_at), a_projectile->GetAngle(), cell,
				a_projectile->GetWorldspace(), nullptr, nullptr, RE::ObjectRefHandle(), false, true);
			if (handle) {
				gStats.explosions.fetch_add(1, std::memory_order_relaxed);
			}
		}

		void Settle(Entry& a_e, bool a_kill, float a_before, float a_after, Vec3 a_at, std::uint64_t a_pair, const Core::Config& a_cfg)
		{
			auto* p = a_e.ref.get();
			if (a_kill) {
				if (a_cfg.explosions) {
					if (auto* explosion = ExplosionOf(p, a_e.base); explosion && (!a_cfg.safeExplosionsOnly || SafeExplosion(explosion)) &&
																	  gBursts.Allow(a_pair)) {
						Burst(p, explosion, a_at);
					}
				}
				p->Kill();
				a_e.killed = true;
				gKilled.insert_or_assign(a_e.handle, gClock);
				gStats.destroyed.fetch_add(1, std::memory_order_relaxed);
				return;
			}
			if (a_after < a_before) {
				a_e.track->strength = a_after;
				gStats.weakened.fetch_add(1, std::memory_order_relaxed);
				if (a_cfg.weakenDamage && a_before > 0.0f) {
					// what it does when it lands: a spell's scales with its power, an arrow's with its damage. Never an
					// arrow's power: with no spell, power is its speed (Projectile::GetPowerSpeedMult).
					const float k = Core::DamageScale(a_before, a_after);
					auto&       rd = p->GetProjectileRuntimeData();
					if (a_e.info.kind == Core::Kind::kArrow) {
						if (std::isfinite(rd.weaponDamage)) {
							rd.weaponDamage *= k;
						}
					} else if (rd.spell && std::isfinite(rd.power)) {
						rd.power *= k;
					}
				}
			}
		}

		void Reward(const Entry& a_mine, const Entry& a_theirs, bool a_theirsKilled, const Core::Config& a_cfg)
		{
			if (!a_mine.player || a_theirs.player || !a_theirsKilled) {
				return;
			}
			gStats.yours.fetch_add(1, std::memory_order_relaxed);
			if (a_cfg.skillXP > 0.0f && a_mine.info.skill != RE::ActorValue::kNone && gXP.Allow(0)) {
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					player->AddSkillExperience(a_mine.info.skill, a_cfg.skillXP);
				}
			}
		}

		void Tell(const Entry& a, const Entry& b, const Core::Outcome& a_o, Vec3 a_at, std::uint64_t a_pair, const Core::Config& a_cfg)
		{
			CrossfireAPI::Clash msg;
			msg.x = a_at.x;
			msg.y = a_at.y;
			msg.z = a_at.z;
			msg.elementA = static_cast<std::uint8_t>(a.info.element);
			msg.elementB = static_cast<std::uint8_t>(b.info.element);
			msg.flags = static_cast<std::uint8_t>((a_o.killA ? CrossfireAPI::kDestroyedA : 0) | (a_o.killB ? CrossfireAPI::kDestroyedB : 0) |
												  (!a_o.killA && a_o.strengthA < a.track->strength ? CrossfireAPI::kWeakenedA : 0) |
												  (!a_o.killB && a_o.strengthB < b.track->strength ? CrossfireAPI::kWeakenedB : 0));
			msg.shooterA = a.shooterRef ? a.shooterRef->GetFormID() : 0;
			msg.shooterB = b.shooterRef ? b.shooterRef->GetFormID() : 0;
			msg.projectileA = a.base->GetFormID();
			msg.projectileB = b.base->GetFormID();
			if (auto* messaging = SKSE::GetMessagingInterface()) {
				messaging->Dispatch(CrossfireAPI::kClash, &msg, sizeof(msg), nullptr);
			}
			if (a_cfg.modEvents && gEvents.Allow(a_pair)) {
				if (auto* source = SKSE::GetModCallbackEventSource()) {
					// strArg "Fire,Frost"; numArg: 1 the first was destroyed, 2 the second, 3 both; sender: the first's shooter
					SKSE::ModCallbackEvent event{ "Crossfire_Clash",
						std::format("{},{}", Core::ElementName(a.info.element), Core::ElementName(b.info.element)),
						static_cast<float>((a_o.killA ? 1 : 0) + (a_o.killB ? 2 : 0)), a.shooterRef.get() };
					source->SendEvent(&event);
				}
			}
		}
	}

	Stats& Counters() { return gStats; }

	void Reset()
	{
		gTracks.clear();
		gKilled.clear();
		gHostile.clear();
		gSettled.clear();
		gHandles.clear();
		gEntries.clear();
		gBodies.clear();
		gContacts.clear();
		gBursts = {};
		gXP = {};
		gEvents = {};
		gStats.tracked.store(0, std::memory_order_relaxed);
	}

	void Update(float a_delta)
	{
		const auto& cfg = Live();
		if (!cfg.enabled || !std::isfinite(a_delta) || a_delta <= 0.0f) {
			if (!gTracks.empty()) {
				Reset();  // off, or time stood still: whatever we remembered is stale when it moves again
			}
			return;
		}
		a_delta = std::min(a_delta, 0.25f);
		++gFrame;
		gClock += static_cast<double>(a_delta);

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* myCell = player ? player->GetParentCell() : nullptr;
		if (!myCell) {
			return;
		}
		const bool  interior = myCell->IsInteriorCell();
		const auto* myWorld = player->GetWorldspace();
		const Vec3  me = V(player->GetPosition());
		const float maxDistance2 = cfg.maxDistance * cfg.maxDistance;
		const auto  playerHandle = player->GetHandle().native_handle();
		const auto  radius = [&](float a_own) { return Core::Radius(a_own, cfg); };

		Gather();
		gEntries.clear();
		gBodies.clear();
		gHostile.clear();
		std::erase_if(gKilled, [](const auto& a_kv) { return gClock - a_kv.second > 2.0; });

		for (const auto& h : gHandles) {
			const std::uint32_t native = h.native_handle();
			if (!native || gKilled.contains(native)) {
				continue;
			}
			auto ref = h.get();
			auto* p = ref.get();
			if (!p || p->IsDeleted() || p->IsDisabled()) {
				continue;
			}
			auto& rd = p->GetProjectileRuntimeData();
			if (rd.flags.any(RE::Projectile::Flags::kDestroyed, RE::Projectile::Flags::kIsTracer)) {
				continue;
			}
			auto* cell = p->GetParentCell();
			if (!cell || (interior ? cell != myCell : p->GetWorldspace() != myWorld)) {
				continue;
			}
			const Vec3 now = V(p->GetPosition());
			const Vec3 off = now - me;
			if (!Core::Finite(now) || Core::Dot(off, off) > maxDistance2) {
				continue;
			}
			const auto* base = p->GetProjectileBase();
			if (!base) {
				continue;
			}
			const Info& info = InfoOf(p, base);
			if (!info.eligible) {
				continue;
			}

			auto [it, fresh] = gTracks.try_emplace(native);
			Track&     track = it->second;
			const bool continuous = !fresh && track.frame + 1 == gFrame;
			if (fresh) {
				track.strength = StrengthOf(info, rd, cfg.tuning);
			}
			const Vec3 from = continuous ? track.last : Core::Backtrack(now, V(rd.linearVelocity), V(rd.velocity), a_delta, rd.distanceMoved);
			track.last = now;
			track.frame = gFrame;

			Core::Body b;
			b.id = native;
			b.shape = info.shape;
			b.element = info.element;
			b.immune = info.immune;
			b.strength = track.strength;
			switch (info.shape) {
			case Core::Shape::kSphere:
				{
					// one that has already hit something, or lies still (an arrow stuck in a wall, a lobbed spell resting
					// on the ground), is done flying. Not moving is the test that cannot be wrong; the impact list is only
					// a quicker answer for one that hit this frame. (A missile's very first frame, before it has moved,
					// is skipped too: it is looked at from the next.)
					if (!info.cone && (!rd.impacts.empty() || Core::Length(now - from) < 0.5f)) {
						continue;
					}
					b.from = from;
					b.to = now;
					if (info.cone) {
						const auto& cone = static_cast<RE::ConeProjectile*>(p)->GetConeRuntimeData();
						b.radius = radius(Core::ConeRadius(cone.initialCollisionSphereRadius, Core::Length(now - V(cone.origin)), cone.coneAngleTangent));
					} else {
						b.radius = radius(base->data.collisionRadius);
					}
					break;
				}
			case Core::Shape::kBeam:
				{
					const auto angle = p->GetAngle();
					b.from = now;
					b.to = now + Core::DirectionFromAngles(angle.x, angle.z) * BeamLength(rd, base, now);
					b.radius = radius(base->data.collisionRadius);
					break;
				}
			case Core::Shape::kBarrier:
				{
					const float width = static_cast<RE::BarrierProjectile*>(p)->GetBarrierRuntimeData().width;
					const float heading = p->GetAngleZ();
					if (!(std::isfinite(width) && width > 1.0f) || !std::isfinite(heading)) {
						continue;
					}
					b.from = b.to = now;
					b.across = Core::BarrierAcross(heading);
					b.halfWidth = 0.5f * width;
					b.height = cfg.barrierHeight;
					b.radius = std::min(radius(base->data.collisionRadius), 64.0f);
					break;
				}
			}
			if (!Core::Valid(b)) {
				continue;
			}

			Entry e;
			e.shooterRef = rd.shooter.get();
			e.shooter = rd.shooter.native_handle();
			e.actor = e.shooterRef ? e.shooterRef->As<RE::Actor>() : nullptr;
			e.player = e.shooter != 0 && e.shooter == playerHandle;
			e.ref = std::move(ref);
			e.base = base;
			e.info = info;
			e.track = &track;
			e.now = now;
			e.handle = native;
			b.shooter = e.shooter;
			gEntries.push_back(std::move(e));
			gBodies.push_back(b);
		}
		gStats.tracked.store(static_cast<std::uint32_t>(gEntries.size()), std::memory_order_relaxed);

		if (gEntries.size() >= 2) {
			const auto may = [&](std::size_t i, std::size_t j) {
				const Entry& a = gEntries[i];
				const Entry& b = gEntries[j];
				const Core::Shooters s{ a.shooter, b.shooter, a.player, b.player, a.actor != nullptr, b.actor != nullptr };
				return Core::MayInteract(s, cfg.who, cfg.ignoreAllies, [&]() { return Hostile(a, b); });
			};
			Core::FindContacts(gBodies, may, static_cast<std::size_t>(cfg.maxContactsPerFrame), gContacts);
		} else {
			gContacts.clear();
		}

		gBursts.BeginFrame(gClock, cfg.maxExplosionsPerFrame, cfg.explosionCooldown);
		gXP.BeginFrame(gClock, 1, 0.5f);
		gEvents.BeginFrame(gClock, 8, 0.25f);
		for (const auto& c : gContacts) {
			Entry& a = gEntries[c.a];
			Entry& b = gEntries[c.b];
			if (a.killed || b.killed) {
				continue;  // it went earlier in this frame
			}
			const auto meeting = PairKey(a.handle, b.handle);
			if (gSettled.contains(meeting)) {
				continue;
			}
			const float sa = a.track->strength, sb = b.track->strength;
			const auto  o = Core::Resolve(cfg.reactions, cfg.overpowerRatio, cfg.weakenSurvivor, a.info.element, sa, a.info.immune,
                b.info.element, sb, b.info.immune);
			if (!o.happened) {
				continue;
			}
			gStats.clashes.fetch_add(1, std::memory_order_relaxed);
			gSettled.insert(meeting);
			const auto pair = PairKey(a.shooter, b.shooter);
			if (cfg.debugLog) {
				SKSE::log::info("clash: {:08X} {} {:.0f}{} vs {:08X} {} {:.0f}{} at ({:.0f}, {:.0f}, {:.0f}), t {:.2f}: {} / {}",
					a.base->GetFormID(), Core::ElementName(a.info.element), sa, a.player ? " (yours)" : "", b.base->GetFormID(),
					Core::ElementName(b.info.element), sb, b.player ? " (yours)" : "", c.touch.point.x, c.touch.point.y, c.touch.point.z,
					c.touch.t, o.killA ? "destroyed" : (o.strengthA < sa ? std::format("left {:.0f}", o.strengthA) : "untouched"),
					o.killB ? "destroyed" : (o.strengthB < sb ? std::format("left {:.0f}", o.strengthB) : "untouched"));
			}
			Tell(a, b, o, c.touch.point, pair, cfg);  // before Settle: it reads the strengths as they were
			Settle(a, o.killA, sa, o.strengthA, c.touch.point, pair, cfg);
			Settle(b, o.killB, sb, o.strengthB, c.touch.point, pair, cfg);
			Reward(a, b, o.killB, cfg);
			Reward(b, a, o.killA, cfg);
		}

		// forget what was not seen this frame, and let go of every projectile
		std::erase_if(gTracks, [](const auto& a_kv) { return a_kv.second.frame != gFrame; });
		for (const auto& e : gEntries) {
			if (e.killed) {
				gTracks.erase(e.handle);
			}
		}
		std::erase_if(gSettled, [](std::uint64_t a_pair) {
			return !gTracks.contains(static_cast<std::uint32_t>(a_pair >> 32)) || !gTracks.contains(static_cast<std::uint32_t>(a_pair));
		});
		gEntries.clear();
		gHandles.clear();
	}
}
