// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.txt and the notice at the top of main.cpp.
//
// What a projectile is to Crossfire, worked out once for each combination of projectile record, spell, effect, ammo
// and enchantment, and remembered.
//
// Only hostile projectiles clash: an arrow, a bolt, or a spell with a hostile or detrimental effect. A spell that
// harms nobody (Magelight, a mod's invisible scripted marker) is never touched, so nothing that relies on its
// projectile arriving can break. Anything named in [Exclude] is never touched either.
//
// The element comes from the effect's keywords (the [Elements] lists), then its resist value, then "a shout's cone
// with no element is Force"; an arrow is Physical unless its enchantment has an element (Core::Classify).

#include "Plugin.h"

namespace Crossfire
{
	namespace
	{
		struct Key
		{
			const void* base;
			const void* spell;
			const void* effect;
			const void* ammo;
			const void* weapon;  // the bow or crossbow: [Exclude] can name one, so two bows sharing an arrow may differ
			const void* enchantment;
			bool        operator==(const Key&) const = default;
		};

		struct KeyHash
		{
			[[nodiscard]] std::size_t operator()(const Key& a_key) const noexcept
			{
				std::size_t h = 0;
				for (const void* p : { a_key.base, a_key.spell, a_key.effect, a_key.ammo, a_key.weapon, a_key.enchantment }) {
					h ^= std::hash<const void*>{}(p) + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
				}
				return h;
			}
		};

		std::unordered_map<Key, Info, KeyHash> gCache;

		[[nodiscard]] std::string_view ResistName(RE::ActorValue a_av) noexcept
		{
			switch (a_av) {
			case RE::ActorValue::kResistFire:
				return "ResistFire";
			case RE::ActorValue::kResistFrost:
				return "ResistFrost";
			case RE::ActorValue::kResistShock:
				return "ResistShock";
			case RE::ActorValue::kPoisonResist:
				return "PoisonResist";
			default:
				return {};
			}
		}

		[[nodiscard]] Core::Element ElementOf(const RE::EffectSetting* a_effect, bool a_voice, bool a_cone)
		{
			std::array<std::string_view, 32> names{};
			std::size_t                      count = 0;
			std::string_view                 resist;
			if (a_effect) {
				for (std::uint32_t i = 0; a_effect->keywords && i < a_effect->numKeywords && count < names.size(); ++i) {
					if (const auto* kw = a_effect->keywords[i]) {
						if (const char* id = kw->GetFormEditorID(); id && *id) {
							names[count++] = id;
						}
					}
				}
				resist = ResistName(a_effect->data.resistVariable);
			}
			const Core::MagicFacts facts{ std::span<const std::string_view>(names.data(), count), resist, a_voice, a_cone };
			return Core::Classify(facts, Live().keywords);
		}

		[[nodiscard]] const RE::EffectSetting* MainEffect(const RE::MagicItem* a_item)
		{
			if (!a_item) {
				return nullptr;
			}
			if (const auto* e = a_item->GetCostliestEffectItem(); e && e->baseEffect) {
				return e->baseEffect;
			}
			for (const auto* e : a_item->effects) {
				if (e && e->baseEffect) {
					return e->baseEffect;
				}
			}
			return nullptr;
		}

		[[nodiscard]] bool Hostile(const RE::MagicItem* a_item, const RE::EffectSetting* a_effect)
		{
			if (a_effect && (a_effect->IsHostile() || a_effect->IsDetrimental())) {
				return true;
			}
			if (a_item) {
				for (const auto* e : a_item->effects) {
					if (e && e->baseEffect && (e->baseEffect->IsHostile() || e->baseEffect->IsDetrimental())) {
						return true;
					}
				}
			}
			return false;
		}

		// the spell's base cost: its override if it has one, otherwise what its effects cost
		[[nodiscard]] float CostOf(const RE::MagicItem* a_item)
		{
			if (!a_item) {
				return 0.0f;
			}
			if (const auto* data = a_item->GetData(); data && (data->flags & 1u) != 0 && data->costOverride > 0) {
				return static_cast<float>(data->costOverride);
			}
			float cost = 0.0f;
			for (const auto* e : a_item->effects) {
				if (e && std::isfinite(e->cost) && e->cost > 0.0f) {
					cost += e->cost;
				}
			}
			return cost;
		}

		[[nodiscard]] Info Work(RE::Projectile* a_projectile, const RE::BGSProjectile* a_base, const Key& a_key)
		{
			Info  info;
			auto& rd = a_projectile->GetProjectileRuntimeData();
			const auto* spell = static_cast<const RE::MagicItem*>(a_key.spell);
			const auto* effect = static_cast<const RE::EffectSetting*>(a_key.effect);
			const auto* enchantment = static_cast<const RE::EnchantmentItem*>(a_key.enchantment);
			const auto  type = a_projectile->GetFormType();

			if (Excluded(a_base) || Excluded(spell) || Excluded(effect) || Excluded(rd.ammoSource) || Excluded(rd.weaponSource) || Excluded(enchantment)) {
				return info;
			}
			const bool arrow = type == RE::FormType::ProjectileArrow && !spell;
			const bool voice = spell && spell->GetSpellType() == RE::MagicSystem::SpellType::kVoicePower;
			info.cone = type == RE::FormType::ProjectileCone;

			if (arrow) {
				info.eligible = true;
				info.kind = Core::Kind::kArrow;
				info.skill = RE::ActorValue::kArchery;
				info.element = Core::Element::kPhysical;
				if (const auto* enchantEffect = MainEffect(enchantment)) {
					if (const auto e = ElementOf(enchantEffect, false, false); e != Core::Element::kArcane) {
						info.element = e;
					}
				}
				return info;
			}
			if (!spell || !Hostile(spell, effect)) {
				return info;  // not an arrow and harms nobody: never ours to touch
			}
			info.eligible = true;
			info.element = ElementOf(effect, voice, info.cone);
			info.cost = CostOf(spell);
			info.skill = voice ? RE::ActorValue::kNone : spell->GetAssociatedSkill();
			switch (type) {
			case RE::FormType::ProjectileBeam:
				info.kind = Core::Kind::kBeam;
				info.shape = Core::Shape::kBeam;
				info.immune = true;
				break;
			case RE::FormType::ProjectileBarrier:
				info.kind = Core::Kind::kBarrier;
				info.shape = Core::Shape::kBarrier;
				info.immune = true;
				break;
			case RE::FormType::ProjectileCone:
				info.kind = voice ? Core::Kind::kVoice : Core::Kind::kCone;
				info.immune = true;
				break;
			case RE::FormType::ProjectileFlame:
				info.kind = Core::Kind::kStream;
				if (voice) {
					info.cost = Live().tuning.shoutStrength;  // a breath shout's spray: each particle a share of a shout
				}
				break;
			default:
				info.kind = voice ? Core::Kind::kVoice : Core::Kind::kSpell;
				break;
			}
			(void)a_base;
			return info;
		}
	}

	const Info& InfoOf(RE::Projectile* a_projectile, const RE::BGSProjectile* a_base)
	{
		auto&             rd = a_projectile->GetProjectileRuntimeData();
		const RE::MagicItem* spell = rd.spell;
		const RE::EffectSetting* effect = rd.avEffect ? rd.avEffect : MainEffect(spell);
		const RE::EnchantmentItem* enchantment = nullptr;
		if (a_projectile->GetFormType() == RE::FormType::ProjectileArrow) {
			enchantment = static_cast<RE::ArrowProjectile*>(a_projectile)->GetArrowRuntimeData().enchantItem;
		}
		const Key key{ a_base, spell, effect, rd.ammoSource, rd.weaponSource, enchantment };
		if (const auto it = gCache.find(key); it != gCache.end()) {
			return it->second;
		}
		if (gCache.size() > 4096) {
			gCache.clear();  // cannot happen in a real game; a bound all the same
		}
		return gCache.emplace(key, Work(a_projectile, a_base, key)).first->second;
	}

	void ClearInfo() { gCache.clear(); }

	RE::BGSExplosion* ExplosionOf(RE::Projectile* a_projectile, const RE::BGSProjectile* a_base)
	{
		if (auto* e = a_projectile->GetProjectileRuntimeData().explosion) {
			return e;
		}
		if (a_base && a_base->data.flags.any(RE::BGSProjectileData::BGSProjectileFlags::kExplosion)) {
			return a_base->data.explosionType;
		}
		return nullptr;
	}

	// an explosion that only shows: no damage of its own, no enchantment, nothing it spawns or places. The ones spells
	// use are like this (a spell's own effect is what hurts, and Crossfire does not deliver it); a mod's may not be.
	bool SafeExplosion(const RE::BGSExplosion* a_explosion)
	{
		return a_explosion && !a_explosion->formEnchanting && !(a_explosion->data.damage > 0.0f) && !a_explosion->data.spawnProjectile &&
		       !a_explosion->data.impactPlacedObject;
	}

	void Survey()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return;
		}
		std::size_t total = 0, withExplosion = 0, safe = 0;
		std::size_t byType[7]{};
		for (const auto* p : data->GetFormArray<RE::BGSProjectile>()) {
			if (!p) {
				continue;
			}
			++total;
			for (std::size_t bit = 0; bit < 7; ++bit) {
				if ((p->data.types.underlying() & (1u << bit)) != 0) {
					++byType[bit];
				}
			}
			if (p->data.flags.any(RE::BGSProjectileData::BGSProjectileFlags::kExplosion) && p->data.explosionType) {
				++withExplosion;
				safe += SafeExplosion(p->data.explosionType) ? 1 : 0;
			}
		}
		SKSE::log::info("{} projectile records: {} missile, {} lobber, {} beam, {} spray, {} cone, {} wall, {} arrow; {} explode, {} of those only show",
			total, byType[0], byType[1], byType[2], byType[3], byType[4], byType[5], byType[6], withExplosion, safe);
	}
}
