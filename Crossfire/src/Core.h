// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.txt and the notice at the top of main.cpp.
//
// The part of Crossfire that knows nothing about the game: the shapes a projectile is tested as, finding which ones
// touched during a frame, what a touch does (the reaction table and the strength contest), and the settings files.
// It includes no game header, so tests/ builds and runs it on its own (see tests/run.sh) - everything here is tested
// there, the game-facing files only feed it.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Crossfire::Core
{
	// ------------------------------------------------------------------ vectors (game units; z is up)
	struct Vec3
	{
		float x{ 0.0f }, y{ 0.0f }, z{ 0.0f };

		friend constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
		friend constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
		friend constexpr Vec3 operator*(Vec3 a, float s) noexcept { return { a.x * s, a.y * s, a.z * s }; }
		friend constexpr Vec3 operator*(float s, Vec3 a) noexcept { return a * s; }
		friend constexpr bool operator==(const Vec3&, const Vec3&) = default;
	};

	[[nodiscard]] constexpr float Dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
	[[nodiscard]] float           Length(Vec3 a) noexcept;
	[[nodiscard]] bool            Finite(Vec3 a) noexcept;
	[[nodiscard]] Vec3            Lerp(Vec3 a, Vec3 b, float t) noexcept;

	// the direction a reference faces from its rotation (radians): x is pitch, positive looking down; z is heading,
	// clockwise from north (+y). Heading 0 pitch 0 is (0, 1, 0).
	[[nodiscard]] Vec3 DirectionFromAngles(float a_pitch, float a_heading) noexcept;

	// distance from a point to the segment a-b (a == b is a point)
	[[nodiscard]] float DistancePointSegment(Vec3 p, Vec3 a, Vec3 b) noexcept;

	// ------------------------------------------------------------------ what a projectile is, for Crossfire
	enum class Element : std::uint8_t
	{
		kFire,
		kFrost,
		kShock,
		kPoison,
		kArcane,    // any other hostile magic
		kPhysical,  // arrows, bolts, thrown ammo without an elemental enchantment
		kForce,     // a shout's cone with no element of its own (Unrelenting Force and its like)
		kTotal
	};
	inline constexpr std::size_t kElements = static_cast<std::size_t>(Element::kTotal);

	[[nodiscard]] std::string_view ElementName(Element a_element) noexcept;
	[[nodiscard]] bool             ParseElement(std::string_view a_text, Element& a_out) noexcept;  // case-insensitive

	// the shape a projectile is tested as. A sphere moves during the frame (from its position last frame to this one);
	// the others are where they are this frame. A cone is a sphere whose radius grows with the distance it has gone.
	enum class Shape : std::uint8_t
	{
		kSphere,   // missiles, arrows, lobbed grenades, each particle of a spray, a cone
		kBeam,     // a lightning bolt: a capsule from its origin to where it ends
		kBarrier,  // a wall spell: an upright rectangle, its base on the ground
	};

	struct Body
	{
		std::uint32_t id{ 0 };       // the projectile's handle; unique among the bodies of a frame
		std::uint32_t shooter{ 0 };  // who fired it (0: nobody - a trap)
		Shape         shape{ Shape::kSphere };
		Element       element{ Element::kArcane };
		bool          immune{ false };  // beams, walls and cones are never destroyed by a clash
		float         radius{ 0.0f };
		float         strength{ 0.0f };
		Vec3          from, to;         // sphere: last frame -> this frame; beam: start -> end; barrier: `from` is the base centre
		Vec3          across;           // barrier only: unit, horizontal, along the wall
		float         halfWidth{ 0.0f };  // barrier only
		float         height{ 0.0f };     // barrier only
	};

	[[nodiscard]] bool Valid(const Body& a_body) noexcept;  // finite, sane sizes; anything else is left out of the frame

	struct Box
	{
		Vec3 lo, hi;
	};
	[[nodiscard]] Box Bounds(const Body& a_body) noexcept;

	// When during the frame (0..1) two bodies first touch, and where. False: they did not.
	struct Touch
	{
		float t{ 0.0f };
		Vec3  point;
	};
	[[nodiscard]] bool FirstTouch(const Body& a, const Body& b, Touch& a_out) noexcept;

	// distance from a point to a barrier's rectangle
	[[nodiscard]] float DistancePointBarrier(Vec3 p, const Body& a_barrier) noexcept;

	struct Contact
	{
		std::size_t a{ 0 }, b{ 0 };  // indices into the bodies passed in; a < b
		Touch       touch;
	};

	// Every pair that touched this frame and may interact, earliest first (ties by index, so a frame is reproducible).
	// `a_mayInteract(i, j)` is asked only for pairs whose boxes overlap; two immune bodies are never paired. Bodies that
	// are not Valid() are skipped. `a_out` is cleared first and at most `a_max` contacts are kept (the earliest).
	template <class F>
	void FindContacts(std::span<const Body> a_bodies, F&& a_mayInteract, std::size_t a_max, std::vector<Contact>& a_out);

	// ------------------------------------------------------------------ what a touch does
	enum class Action : std::uint8_t
	{
		kPass,        // nothing
		kClash,       // the strength contest (below)
		kAnnihilate,  // both go, whatever their strength
		kWins,        // the first element destroys the second and goes on untouched
		kLoses,       // the other way round
	};
	[[nodiscard]] std::string_view ActionName(Action a_action) noexcept;
	[[nodiscard]] bool             ParseAction(std::string_view a_text, Action& a_out) noexcept;
	[[nodiscard]] constexpr Action Mirror(Action a) noexcept
	{
		return a == Action::kWins ? Action::kLoses : a == Action::kLoses ? Action::kWins : a;
	}

	using Table = std::array<std::array<Action, kElements>, kElements>;
	[[nodiscard]] Table DefaultTable() noexcept;
	void                SetReaction(Table& a_table, Element a, Element b, Action a_action) noexcept;  // and the mirror

	struct Outcome
	{
		bool  happened{ false };        // anything changed
		bool  killA{ false }, killB{ false };
		float strengthA{ 0.0f }, strengthB{ 0.0f };  // what each has left (unchanged unless weakened)
	};

	// The clash: when one side is at least `a_ratio` times the other it destroys it and goes on, less the loser's
	// strength when `a_weaken`; otherwise both go. An immune side is never destroyed or weakened; it can still destroy.
	[[nodiscard]] Outcome Resolve(const Table& a_table, float a_ratio, bool a_weaken, Element ea, float sa, bool immuneA,
		Element eb, float sb, bool immuneB) noexcept;

	// ------------------------------------------------------------------ strength
	enum class Kind : std::uint8_t
	{
		kSpell,    // a missile or lobbed spell, staff or scroll
		kStream,   // one particle of a spray (Flames, Frostbite, Sparks is a beam)
		kArrow,
		kVoice,    // a shout's own projectile
		kBeam,
		kBarrier,
		kCone,
	};

	struct Tuning
	{
		float streamShare{ 0.15f };     // a spray particle's share of its spell's cost
		float arrowScale{ 2.0f };       // strength per point of an arrow's damage
		float shoutStrength{ 200.0f };  // every shout projectile, whatever it costs (shouts cost no magicka)
		float minSpellStrength{ 5.0f };
	};

	// a_cost: the spell's base magicka cost; a_power: the projectile's power (dual casting raises it); a_damage: an
	// arrow's damage. Never below 0.01 and always finite.
	[[nodiscard]] float BaseStrength(Kind a_kind, float a_cost, float a_power, float a_damage, const Tuning& a_tuning) noexcept;

	// how big a cone is once it has gone `a_travelled` units
	[[nodiscard]] float ConeRadius(float a_initial, float a_travelled, float a_tangent) noexcept;

	// ------------------------------------------------------------------ the per-frame decisions Clash.cpp makes
	struct Config;

	// a projectile's size for Crossfire: its own collision radius scaled, plus the aim assist, within the limits
	[[nodiscard]] float Radius(float a_own, const Config& a_config) noexcept;

	// along a wall spell facing `a_heading` (radians): horizontal, unit, square to the way it faces
	[[nodiscard]] Vec3 BarrierAcross(float a_heading) noexcept;

	// where a projectile seen for the first time was a frame ago: back along its velocity (`a_velocity`, or
	// `a_fallback` when that is nothing), never further back than it has flown (`a_travelled`, ignored if not finite)
	[[nodiscard]] Vec3 Backtrack(Vec3 a_now, Vec3 a_velocity, Vec3 a_fallback, float a_delta, float a_travelled) noexcept;

	// may two projectiles clash at all? `a_hostile` is only asked when both shooters are actors and allies are ignored
	struct Shooters
	{
		std::uint32_t a{ 0 }, b{ 0 };  // handles; 0 is nobody
		bool          playerA{ false }, playerB{ false };
		bool          actorA{ false }, actorB{ false };
	};
	template <class F>
	[[nodiscard]] bool MayInteract(const Shooters& a_s, int a_who, bool a_ignoreAllies, F&& a_hostile)
	{
		if (a_s.a == a_s.b) {
			return false;  // one shooter's own projectiles, or two with nobody behind them (traps)
		}
		if (a_who == 1 && !a_s.playerA && !a_s.playerB) {
			return false;
		}
		if (a_ignoreAllies && a_s.actorA && a_s.actorB) {
			return a_hostile();
		}
		return true;
	}

	// what a weakened projectile's power (and an arrow's damage) is multiplied by
	[[nodiscard]] float DamageScale(float a_before, float a_after) noexcept;

	// ------------------------------------------------------------------ which element a spell is
	struct MagicFacts
	{
		std::span<const std::string_view> keywords;  // the effect's keywords' editor IDs
		std::string_view                  resist;    // the effect's resist value: "ResistFire", "ResistFrost", ...
		bool                              voice{ false };
		bool                              cone{ false };
	};
	// First the keyword lists, element by element in order, then the resist value, then a voice cone is Force;
	// anything else is Arcane.
	[[nodiscard]] Element Classify(const MagicFacts& a_facts, const std::array<std::vector<std::string>, kElements>& a_keywords) noexcept;

	// ------------------------------------------------------------------ limits on explosions
	// At most `a_perFrame` explosions a frame, and none for the same key (a pair of shooters) within `a_cooldown`
	// seconds of the last, so two sprays meeting do not spawn one explosion per particle.
	class Limiter
	{
	public:
		void BeginFrame(double a_now, int a_perFrame, float a_cooldown) noexcept;
		[[nodiscard]] bool Allow(std::uint64_t a_key);
		[[nodiscard]] std::size_t Remembered() const noexcept { return _last.size(); }

	private:
		std::unordered_map<std::uint64_t, double> _last;
		double                                    _now{ 0.0 };
		double                                    _lastPrune{ 0.0 };
		float                                     _cooldown{ 0.0f };
		int                                       _left{ 0 };
	};

	// ------------------------------------------------------------------ the settings files
	// A form named in a file: "Skyrim.esm|0x012EB7" (file and the id within it) or an editor ID.
	struct FormRef
	{
		std::string   file;  // empty: `editorID` names it
		std::uint32_t id{ 0 };
		std::string   editorID;
		friend bool   operator==(const FormRef&, const FormRef&) = default;
	};
	[[nodiscard]] bool ParseFormRef(std::string_view a_text, FormRef& a_out);

	struct Config
	{
		// [General]
		bool  enabled{ true };
		int   who{ 0 };  // 0 everyone's projectiles; 1 only a clash with one of the player's
		bool  ignoreAllies{ true };
		float maxDistance{ 6000.0f };  // from the player; projectiles further away are left alone
		// [Hits]
		float radiusScale{ 1.0f };
		float radiusBonus{ 12.0f };  // added to every projectile's own collision radius: aiming at a fireball is hard
		float minRadius{ 4.0f };
		float maxRadius{ 256.0f };
		float barrierHeight{ 160.0f };
		// [Clash]
		float  overpowerRatio{ 2.0f };
		bool   weakenSurvivor{ true };
		bool   weakenDamage{ true };  // a weakened projectile also does less when it lands
		Tuning tuning;
		// [Effects]
		bool  explosions{ true };
		bool  safeExplosionsOnly{ true };  // skip an explosion that would do anything but show (damage, enchantment, spawns)
		int   maxExplosionsPerFrame{ 4 };
		float explosionCooldown{ 0.12f };
		int   maxContactsPerFrame{ 32 };
		// [Player]
		float skillXP{ 6.0f };  // skill experience for each enemy projectile one of the player's destroys
		// [Events]
		bool modEvents{ true };  // "Crossfire_Clash" for Papyrus
		// [Debug]
		bool debugLog{ false };

		// rules (Crossfire_Rules.ini and Crossfire\*.ini)
		std::array<std::vector<std::string>, kElements> keywords;
		Table                                          reactions{ DefaultTable() };
		std::vector<FormRef>                           exclude;

		Config();
	};

	// Reads an ini's text into `a_config` - only what the text sets; a line it cannot use is skipped and said in
	// `a_warnings` (with its line number). Out-of-range numbers are clamped (and said). Never throws on bad text.
	void ParseIni(std::string_view a_text, Config& a_config, std::vector<std::string>& a_warnings);

	// the settings file the menu writes: [General] .. [Debug], every value, nothing from the rules
	[[nodiscard]] std::string WriteSettings(const Config& a_config);

	// settings that can be changed from the menu are clamped to these
	struct Range
	{
		float lo, hi;
	};
	[[nodiscard]] Range RangeOf(std::string_view a_key) noexcept;  // "RadiusBonus", ...; {0,0} for an unknown key
}

// ------------------------------------------------------------------ template definitions
#include <algorithm>
#include <numeric>

namespace Crossfire::Core
{
	template <class F>
	void FindContacts(std::span<const Body> a_bodies, F&& a_mayInteract, std::size_t a_max, std::vector<Contact>& a_out)
	{
		a_out.clear();
		if (a_max == 0 || a_bodies.size() < 2) {
			return;
		}
		// sort and sweep along x: only bodies whose boxes overlap in x are looked at any closer
		thread_local std::vector<std::size_t> order;
		thread_local std::vector<Box>         boxes;
		order.clear();
		boxes.resize(a_bodies.size());
		for (std::size_t i = 0; i < a_bodies.size(); ++i) {
			if (Valid(a_bodies[i])) {
				boxes[i] = Bounds(a_bodies[i]);
				order.push_back(i);
			}
		}
		std::ranges::sort(order, [](std::size_t l, std::size_t r) { return boxes[l].lo.x < boxes[r].lo.x || (boxes[l].lo.x == boxes[r].lo.x && l < r); });
		for (std::size_t oi = 0; oi < order.size(); ++oi) {
			const std::size_t i = order[oi];
			const Box&        bi = boxes[i];
			for (std::size_t oj = oi + 1; oj < order.size(); ++oj) {
				const std::size_t j = order[oj];
				const Box&        bj = boxes[j];
				if (bj.lo.x > bi.hi.x) {
					break;
				}
				if (bj.lo.y > bi.hi.y || bi.lo.y > bj.hi.y || bj.lo.z > bi.hi.z || bi.lo.z > bj.hi.z) {
					continue;
				}
				const Body& x = a_bodies[i];
				const Body& y = a_bodies[j];
				if (x.immune && y.immune) {
					continue;
				}
				const std::size_t lo = std::min(i, j), hi = std::max(i, j);
				if (!a_mayInteract(lo, hi)) {
					continue;
				}
				Touch touch;
				if (FirstTouch(a_bodies[lo], a_bodies[hi], touch)) {
					a_out.push_back({ lo, hi, touch });
				}
			}
		}
		std::ranges::sort(a_out, [](const Contact& l, const Contact& r) {
			if (l.touch.t != r.touch.t) {
				return l.touch.t < r.touch.t;
			}
			return l.a != r.a ? l.a < r.a : l.b < r.b;
		});
		if (a_out.size() > a_max) {
			a_out.resize(a_max);
		}
	}
}
