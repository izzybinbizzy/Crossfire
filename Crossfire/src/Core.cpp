// Crossfire - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.txt and the notice at the top of main.cpp.
//
// The game-free core (see Core.h). No game header here: tests/ builds this file on its own.

#include "Core.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace Crossfire::Core
{
	// ------------------------------------------------------------------ vectors
	float Length(Vec3 a) noexcept { return std::sqrt(Dot(a, a)); }

	bool Finite(Vec3 a) noexcept { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }

	Vec3 Lerp(Vec3 a, Vec3 b, float t) noexcept { return a + (b - a) * t; }

	Vec3 DirectionFromAngles(float a_pitch, float a_heading) noexcept
	{
		const float cp = std::cos(a_pitch);
		return { std::sin(a_heading) * cp, std::cos(a_heading) * cp, -std::sin(a_pitch) };
	}

	namespace
	{
		// the point on segment a-b nearest p
		[[nodiscard]] Vec3 ClosestOnSegment(Vec3 p, Vec3 a, Vec3 b) noexcept
		{
			const Vec3  ab = b - a;
			const float len2 = Dot(ab, ab);
			if (len2 <= 1e-12f) {
				return a;
			}
			const float t = std::clamp(Dot(p - a, ab) / len2, 0.0f, 1.0f);
			return a + ab * t;
		}

		// the point of a barrier's rectangle nearest p
		[[nodiscard]] Vec3 ClosestOnBarrier(Vec3 p, const Body& a_wall) noexcept
		{
			const Vec3  d = p - a_wall.from;
			const float along = std::clamp(Dot(d, a_wall.across), -a_wall.halfWidth, a_wall.halfWidth);
			// the rectangle stands upright: its second axis is world up, whatever way the wall faces
			const float up = std::clamp(d.z - a_wall.across.z * Dot(d, a_wall.across), 0.0f, a_wall.height);
			return a_wall.from + a_wall.across * along + Vec3{ 0.0f, 0.0f, up };
		}

		// the closest points of segments p1-q1 and p2-q2 (Ericson, Real-Time Collision Detection, 5.1.9)
		void ClosestSegmentSegment(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2, Vec3& c1, Vec3& c2) noexcept
		{
			const Vec3  d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
			const float a = Dot(d1, d1), e = Dot(d2, d2), f = Dot(d2, r);
			constexpr float kEps = 1e-12f;
			float           s = 0.0f, t = 0.0f;
			if (a <= kEps && e <= kEps) {
				c1 = p1;
				c2 = p2;
				return;
			}
			if (a <= kEps) {
				t = std::clamp(f / e, 0.0f, 1.0f);
			} else {
				const float c = Dot(d1, r);
				if (e <= kEps) {
					s = std::clamp(-c / a, 0.0f, 1.0f);
				} else {
					const float b = Dot(d1, d2);
					const float denom = a * e - b * b;
					s = denom > kEps ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
					t = (b * s + f) / e;
					if (t < 0.0f) {
						t = 0.0f;
						s = std::clamp(-c / a, 0.0f, 1.0f);
					} else if (t > 1.0f) {
						t = 1.0f;
						s = std::clamp((b - c) / a, 0.0f, 1.0f);
					}
				}
			}
			c1 = p1 + d1 * s;
			c2 = p2 + d2 * t;
		}

		// the first t in [0, 1] where a convex function of t reaches zero or below. Distance from a point moving on a
		// straight line to a convex shape is convex, so a golden-section search finds the nearest approach and a
		// bisection before it finds the first touch.
		template <class F>
		[[nodiscard]] bool FirstRoot(F&& f, float& a_t) noexcept
		{
			const float f0 = f(0.0f);
			if (!std::isfinite(f0)) {
				return false;
			}
			if (f0 <= 0.0f) {
				a_t = 0.0f;
				return true;
			}
			constexpr float kInvPhi = 0.6180339887f;
			float           lo = 0.0f, hi = 1.0f;
			float           x1 = hi - kInvPhi * (hi - lo), x2 = lo + kInvPhi * (hi - lo);
			float           f1 = f(x1), f2 = f(x2);
			for (int i = 0; i < 48 && f1 > 0.0f && f2 > 0.0f; ++i) {
				if (f1 < f2) {
					hi = x2;
					x2 = x1;
					f2 = f1;
					x1 = hi - kInvPhi * (hi - lo);
					f1 = f(x1);
				} else {
					lo = x1;
					x1 = x2;
					f1 = f2;
					x2 = lo + kInvPhi * (hi - lo);
					f2 = f(x2);
				}
			}
			float best = f1 <= f2 ? x1 : x2;
			float fbest = std::min(f1, f2);
			if (const float f1end = f(1.0f); f1end < fbest) {
				best = 1.0f;
				fbest = f1end;
			}
			if (!(fbest <= 0.0f)) {
				return false;
			}
			lo = 0.0f;
			hi = best;
			for (int i = 0; i < 40; ++i) {
				const float mid = 0.5f * (lo + hi);
				if (f(mid) <= 0.0f) {
					hi = mid;
				} else {
					lo = mid;
				}
			}
			a_t = hi;
			return true;
		}

		[[nodiscard]] Vec3 Between(Vec3 a_centre, float a_ra, Vec3 a_other, float a_rb) noexcept
		{
			const float sum = a_ra + a_rb;
			return sum > 0.0f ? a_centre + (a_other - a_centre) * (a_ra / sum) : Lerp(a_centre, a_other, 0.5f);
		}

		[[nodiscard]] bool SphereSphere(const Body& a, const Body& b, Touch& a_out) noexcept
		{
			const Vec3  s = a.from - b.from;
			const Vec3  v = (a.to - a.from) - (b.to - b.from);
			const float r = a.radius + b.radius;
			const float c = Dot(s, s) - r * r;
			float       t = 0.0f;
			if (c > 0.0f) {
				const float vv = Dot(v, v);
				const float sv = Dot(s, v);
				if (vv <= 1e-12f || sv >= 0.0f) {
					return false;  // not moving relative to each other, or moving apart
				}
				const float disc = sv * sv - vv * c;
				if (disc < 0.0f) {
					return false;
				}
				t = (-sv - std::sqrt(disc)) / vv;
				if (!(t >= 0.0f && t <= 1.0f)) {
					return false;
				}
			}
			const Vec3 pa = Lerp(a.from, a.to, t);
			const Vec3 pb = Lerp(b.from, b.to, t);
			a_out = { t, Between(pa, a.radius, pb, b.radius) };
			return true;
		}

		[[nodiscard]] bool SphereBeam(const Body& a_sphere, const Body& a_beam, Touch& a_out) noexcept
		{
			const float r = a_sphere.radius + a_beam.radius;
			float       t = 0.0f;
			const auto  f = [&](float x) { return DistancePointSegment(Lerp(a_sphere.from, a_sphere.to, x), a_beam.from, a_beam.to) - r; };
			if (!FirstRoot(f, t)) {
				return false;
			}
			const Vec3 p = Lerp(a_sphere.from, a_sphere.to, t);
			a_out = { t, Between(p, a_sphere.radius, ClosestOnSegment(p, a_beam.from, a_beam.to), a_beam.radius) };
			return true;
		}

		[[nodiscard]] bool SphereBarrier(const Body& a_sphere, const Body& a_wall, Touch& a_out) noexcept
		{
			const float r = a_sphere.radius + a_wall.radius;
			float       t = 0.0f;
			const auto  f = [&](float x) { return DistancePointBarrier(Lerp(a_sphere.from, a_sphere.to, x), a_wall) - r; };
			if (!FirstRoot(f, t)) {
				return false;
			}
			const Vec3 p = Lerp(a_sphere.from, a_sphere.to, t);
			a_out = { t, Between(p, a_sphere.radius, ClosestOnBarrier(p, a_wall), a_wall.radius) };
			return true;
		}

		[[nodiscard]] bool BeamBeam(const Body& a, const Body& b, Touch& a_out) noexcept
		{
			Vec3 ca, cb;
			ClosestSegmentSegment(a.from, a.to, b.from, b.to, ca, cb);
			if (Length(ca - cb) > a.radius + b.radius) {
				return false;
			}
			a_out = { 0.0f, Between(ca, a.radius, cb, b.radius) };
			return true;
		}

		[[nodiscard]] char Lower(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

		[[nodiscard]] bool SameText(std::string_view a, std::string_view b) noexcept
		{
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return Lower(x) == Lower(y); });
		}

		[[nodiscard]] bool Space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

		[[nodiscard]] std::string_view Trim(std::string_view s) noexcept
		{
			while (!s.empty() && Space(s.front())) {
				s.remove_prefix(1);
			}
			while (!s.empty() && Space(s.back())) {
				s.remove_suffix(1);
			}
			return s;
		}

		[[nodiscard]] float Sanitize(float a_value, float a_fallback) noexcept { return std::isfinite(a_value) ? a_value : a_fallback; }
	}

	float DistancePointSegment(Vec3 p, Vec3 a, Vec3 b) noexcept { return Length(p - ClosestOnSegment(p, a, b)); }

	float DistancePointBarrier(Vec3 p, const Body& a_barrier) noexcept { return Length(p - ClosestOnBarrier(p, a_barrier)); }

	bool Valid(const Body& a) noexcept
	{
		constexpr float kHuge = 1.0e7f;  // well beyond any worldspace
		const auto      sane = [](float v, float hi) { return std::isfinite(v) && v >= 0.0f && v <= hi; };
		if (!Finite(a.from) || !Finite(a.to) || !sane(a.radius, 1.0e5f) || !sane(a.strength, std::numeric_limits<float>::max())) {
			return false;
		}
		for (const Vec3 p : { a.from, a.to }) {
			if (std::abs(p.x) > kHuge || std::abs(p.y) > kHuge || std::abs(p.z) > kHuge) {
				return false;
			}
		}
		if (static_cast<std::size_t>(a.element) >= kElements) {
			return false;
		}
		switch (a.shape) {
		case Shape::kSphere:
		case Shape::kBeam:
			return true;
		case Shape::kBarrier:
			return Finite(a.across) && std::abs(Length(a.across) - 1.0f) < 1e-2f && std::abs(a.across.z) < 1e-2f &&
			       sane(a.halfWidth, 1.0e5f) && a.halfWidth > 0.0f && sane(a.height, 1.0e5f) && a.height > 0.0f;
		}
		return false;
	}

	Box Bounds(const Body& a) noexcept
	{
		Box box{ { std::min(a.from.x, a.to.x), std::min(a.from.y, a.to.y), std::min(a.from.z, a.to.z) },
			{ std::max(a.from.x, a.to.x), std::max(a.from.y, a.to.y), std::max(a.from.z, a.to.z) } };
		if (a.shape == Shape::kBarrier) {
			const Vec3 ends[4]{ a.from + a.across * a.halfWidth, a.from - a.across * a.halfWidth,
				a.from + a.across * a.halfWidth + Vec3{ 0, 0, a.height }, a.from - a.across * a.halfWidth + Vec3{ 0, 0, a.height } };
			box.lo = box.hi = ends[0];
			for (const Vec3 e : ends) {
				box.lo = { std::min(box.lo.x, e.x), std::min(box.lo.y, e.y), std::min(box.lo.z, e.z) };
				box.hi = { std::max(box.hi.x, e.x), std::max(box.hi.y, e.y), std::max(box.hi.z, e.z) };
			}
		}
		const Vec3 r{ a.radius, a.radius, a.radius };
		return { box.lo - r, box.hi + r };
	}

	bool FirstTouch(const Body& a, const Body& b, Touch& a_out) noexcept
	{
		using enum Shape;
		if (a.shape == kSphere && b.shape == kSphere) {
			return SphereSphere(a, b, a_out);
		}
		if (a.shape == kSphere && b.shape == kBeam) {
			return SphereBeam(a, b, a_out);
		}
		if (a.shape == kBeam && b.shape == kSphere) {
			return SphereBeam(b, a, a_out);
		}
		if (a.shape == kSphere && b.shape == kBarrier) {
			return SphereBarrier(a, b, a_out);
		}
		if (a.shape == kBarrier && b.shape == kSphere) {
			return SphereBarrier(b, a, a_out);
		}
		if (a.shape == kBeam && b.shape == kBeam) {
			return BeamBeam(a, b, a_out);
		}
		return false;  // a beam against a wall, or two walls: both are always immune, so nothing would happen
	}

	// ------------------------------------------------------------------ elements and actions
	namespace
	{
		constexpr std::array<std::string_view, kElements> kElementNames{ "Fire", "Frost", "Shock", "Poison", "Arcane", "Physical", "Force" };
	}

	std::string_view ElementName(Element a_element) noexcept
	{
		const auto i = static_cast<std::size_t>(a_element);
		return i < kElements ? kElementNames[i] : std::string_view{ "?" };
	}

	bool ParseElement(std::string_view a_text, Element& a_out) noexcept
	{
		a_text = Trim(a_text);
		for (std::size_t i = 0; i < kElements; ++i) {
			if (SameText(a_text, kElementNames[i])) {
				a_out = static_cast<Element>(i);
				return true;
			}
		}
		return false;
	}

	std::string_view ActionName(Action a_action) noexcept
	{
		switch (a_action) {
		case Action::kPass:
			return "Pass";
		case Action::kClash:
			return "Clash";
		case Action::kAnnihilate:
			return "Annihilate";
		case Action::kWins:
			return "Wins";
		case Action::kLoses:
			return "Loses";
		}
		return "?";
	}

	bool ParseAction(std::string_view a_text, Action& a_out) noexcept
	{
		a_text = Trim(a_text);
		static constexpr std::pair<std::string_view, Action> kNames[]{
			{ "Pass", Action::kPass }, { "Ignore", Action::kPass }, { "None", Action::kPass },
			{ "Clash", Action::kClash },
			{ "Annihilate", Action::kAnnihilate }, { "Cancel", Action::kAnnihilate },
			{ "Wins", Action::kWins }, { "Beats", Action::kWins },
			{ "Loses", Action::kLoses }, { "LosesTo", Action::kLoses },
		};
		for (const auto& [name, action] : kNames) {
			if (SameText(a_text, name)) {
				a_out = action;
				return true;
			}
		}
		return false;
	}

	void SetReaction(Table& a_table, Element a, Element b, Action a_action) noexcept
	{
		const auto i = static_cast<std::size_t>(a), j = static_cast<std::size_t>(b);
		if (i >= kElements || j >= kElements) {
			return;
		}
		if (i == j && (a_action == Action::kWins || a_action == Action::kLoses)) {
			a_action = Action::kClash;  // an element cannot beat itself; the contest decides
		}
		a_table[i][j] = a_action;
		a_table[j][i] = Mirror(a_action);
	}

	Table DefaultTable() noexcept
	{
		Table t{};
		for (auto& row : t) {
			row.fill(Action::kClash);
		}
		SetReaction(t, Element::kFire, Element::kFrost, Action::kAnnihilate);  // steam: opposites cancel whatever their size
		for (std::size_t i = 0; i < kElements; ++i) {
			SetReaction(t, Element::kForce, static_cast<Element>(i), Action::kWins);  // a shout swats projectiles aside
		}
		SetReaction(t, Element::kForce, Element::kForce, Action::kPass);
		return t;
	}

	Outcome Resolve(const Table& a_table, float a_ratio, bool a_weaken, Element ea, float sa, bool immuneA, Element eb,
		float sb, bool immuneB) noexcept
	{
		Outcome out{ false, false, false, sa, sb };
		const auto i = static_cast<std::size_t>(ea), j = static_cast<std::size_t>(eb);
		if ((immuneA && immuneB) || i >= kElements || j >= kElements) {
			return out;
		}
		sa = std::isfinite(sa) ? std::max(sa, 0.0f) : 0.0f;
		sb = std::isfinite(sb) ? std::max(sb, 0.0f) : 0.0f;
		out.strengthA = sa;
		out.strengthB = sb;
		a_ratio = std::isfinite(a_ratio) ? std::clamp(a_ratio, 1.0f, 100.0f) : 2.0f;

		bool loseA = false, loseB = false;
		// who beat whom, and what the winner pays (only in a contest)
		float costA = 0.0f, costB = 0.0f;
		switch (a_table[i][j]) {
		case Action::kPass:
			return out;
		case Action::kAnnihilate:
			loseA = loseB = true;
			break;
		case Action::kWins:
			loseB = true;
			break;
		case Action::kLoses:
			loseA = true;
			break;
		case Action::kClash:
			if (sa >= sb * a_ratio && sa > 0.0f) {
				loseB = true;
				costA = sb;
			} else if (sb >= sa * a_ratio && sb > 0.0f) {
				loseA = true;
				costB = sa;
			} else {
				loseA = loseB = true;
			}
			break;
		}
		if (loseA && !immuneA) {
			out.killA = true;
			out.strengthA = 0.0f;
		}
		if (loseB && !immuneB) {
			out.killB = true;
			out.strengthB = 0.0f;
		}
		// the winner of a contest pays for it; one whose strength runs out goes too
		if (a_weaken && !out.killA && !immuneA && costA > 0.0f) {
			out.strengthA = sa - costA;
			if (out.strengthA <= sa * 1e-4f) {
				out.killA = true;
				out.strengthA = 0.0f;
			}
		}
		if (a_weaken && !out.killB && !immuneB && costB > 0.0f) {
			out.strengthB = sb - costB;
			if (out.strengthB <= sb * 1e-4f) {
				out.killB = true;
				out.strengthB = 0.0f;
			}
		}
		out.happened = out.killA || out.killB || out.strengthA != sa || out.strengthB != sb;
		return out;
	}

	// ------------------------------------------------------------------ strength
	float BaseStrength(Kind a_kind, float a_cost, float a_power, float a_damage, const Tuning& a_tuning) noexcept
	{
		const float power = std::clamp(Sanitize(a_power, 1.0f) > 0.0f ? Sanitize(a_power, 1.0f) : 1.0f, 0.1f, 10.0f);
		const float cost = std::max(Sanitize(a_cost, 0.0f), 0.0f);
		const float floorCost = std::max(cost, std::max(Sanitize(a_tuning.minSpellStrength, 0.0f), 0.0f));
		float       s = 0.0f;
		switch (a_kind) {
		case Kind::kSpell:
		case Kind::kBeam:
		case Kind::kBarrier:
		case Kind::kCone:
			s = floorCost * power;
			break;
		case Kind::kStream:
			s = floorCost * std::clamp(Sanitize(a_tuning.streamShare, 0.15f), 0.0f, 10.0f) * power;
			break;
		case Kind::kArrow:
			s = std::max(Sanitize(a_damage, 0.0f), 1.0f) * std::clamp(Sanitize(a_tuning.arrowScale, 2.0f), 0.0f, 1000.0f);
			break;
		case Kind::kVoice:
			s = std::max(Sanitize(a_tuning.shoutStrength, 200.0f), 0.0f);
			break;
		}
		return std::isfinite(s) ? std::clamp(s, 0.01f, 1.0e9f) : 0.01f;
	}

	float ConeRadius(float a_initial, float a_travelled, float a_tangent) noexcept
	{
		const float r = std::max(Sanitize(a_initial, 0.0f), 0.0f) +
		                std::max(Sanitize(a_travelled, 0.0f), 0.0f) * std::clamp(Sanitize(a_tangent, 0.0f), 0.0f, 10.0f);
		return std::min(r, 1.0e4f);
	}

	// ------------------------------------------------------------------ per-frame decisions
	float Radius(float a_own, const Config& a_config) noexcept
	{
		const float own = std::isfinite(a_own) && a_own > 0.0f ? std::min(a_own, 1.0e5f) : 0.0f;
		const float lo = std::max(Sanitize(a_config.minRadius, 0.0f), 0.0f);
		const float hi = std::max(Sanitize(a_config.maxRadius, lo), lo);
		const float r = own * Sanitize(a_config.radiusScale, 1.0f) + Sanitize(a_config.radiusBonus, 0.0f);
		return std::clamp(std::isfinite(r) ? r : lo, lo, hi);
	}

	Vec3 BarrierAcross(float a_heading) noexcept
	{
		if (!std::isfinite(a_heading)) {
			return { 1.0f, 0.0f, 0.0f };
		}
		// facing is (sin h, cos h, 0); a quarter turn clockwise of it is (cos h, -sin h, 0)
		return { std::cos(a_heading), -std::sin(a_heading), 0.0f };
	}

	Vec3 Backtrack(Vec3 a_now, Vec3 a_velocity, Vec3 a_fallback, float a_delta, float a_travelled) noexcept
	{
		Vec3 v = a_velocity;
		if (!Finite(v) || !(Dot(v, v) > 1.0f)) {
			v = a_fallback;
		}
		const float speed = Length(v);
		if (!Finite(a_now) || !std::isfinite(speed) || speed < 1.0f || !std::isfinite(a_delta) || a_delta <= 0.0f) {
			return a_now;
		}
		float back = speed * a_delta;
		if (std::isfinite(a_travelled) && a_travelled >= 0.0f) {
			back = std::min(back, a_travelled);
		}
		return a_now - v * (back / speed);
	}

	float DamageScale(float a_before, float a_after) noexcept
	{
		if (!std::isfinite(a_before) || !std::isfinite(a_after) || a_before <= 0.0f) {
			return 1.0f;
		}
		return std::clamp(a_after / a_before, 0.05f, 1.0f);
	}

	// ------------------------------------------------------------------ elements of a spell
	Element Classify(const MagicFacts& a_facts, const std::array<std::vector<std::string>, kElements>& a_keywords) noexcept
	{
		for (std::size_t e = 0; e < kElements; ++e) {
			for (const auto& wanted : a_keywords[e]) {
				for (const auto have : a_facts.keywords) {
					if (SameText(have, wanted)) {
						return static_cast<Element>(e);
					}
				}
			}
		}
		static constexpr std::pair<std::string_view, Element> kResists[]{
			{ "ResistFire", Element::kFire }, { "FireResist", Element::kFire },
			{ "ResistFrost", Element::kFrost }, { "FrostResist", Element::kFrost },
			{ "ResistShock", Element::kShock }, { "ElectricResist", Element::kShock }, { "ShockResist", Element::kShock },
			{ "PoisonResist", Element::kPoison }, { "ResistPoison", Element::kPoison },
		};
		for (const auto& [name, element] : kResists) {
			if (SameText(a_facts.resist, name)) {
				return element;
			}
		}
		return a_facts.voice && a_facts.cone ? Element::kForce : Element::kArcane;
	}

	// ------------------------------------------------------------------ limiter
	void Limiter::BeginFrame(double a_now, int a_perFrame, float a_cooldown) noexcept
	{
		_now = a_now;
		_left = std::max(a_perFrame, 0);
		_cooldown = std::isfinite(a_cooldown) ? std::max(a_cooldown, 0.0f) : 0.0f;
		if (a_now < _lastPrune) {
			_last.clear();  // the clock went back (a save was loaded): nothing remembered still means anything
			_lastPrune = a_now;
		}
		if (a_now - _lastPrune > 5.0) {
			_lastPrune = a_now;
			const double keep = static_cast<double>(_cooldown) * 2.0 + 1.0;
			std::erase_if(_last, [&](const auto& kv) { return a_now - kv.second > keep; });
		}
	}

	bool Limiter::Allow(std::uint64_t a_key)
	{
		if (_left <= 0) {
			return false;
		}
		const auto it = _last.find(a_key);
		if (it != _last.end() && _now - it->second < static_cast<double>(_cooldown) && _now >= it->second) {
			return false;
		}
		_last.insert_or_assign(a_key, _now);
		--_left;
		return true;
	}

	// ------------------------------------------------------------------ form references
	bool ParseFormRef(std::string_view a_text, FormRef& a_out)
	{
		a_text = Trim(a_text);
		if (a_text.empty()) {
			return false;
		}
		const auto bar = a_text.find_first_of("|~");
		if (bar == std::string_view::npos) {
			// an editor ID: letters, digits and _ only
			if (!std::ranges::all_of(a_text, [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; })) {
				return false;
			}
			a_out = { {}, 0, std::string(a_text) };
			return true;
		}
		const auto file = Trim(a_text.substr(0, bar));
		auto       id = Trim(a_text.substr(bar + 1));
		if (file.empty() || file.find_first_of("|~") != std::string_view::npos) {
			return false;
		}
		if (id.size() > 2 && id[0] == '0' && (id[1] == 'x' || id[1] == 'X')) {
			id.remove_prefix(2);
		}
		if (id.empty() || id.size() > 8) {
			return false;
		}
		std::uint32_t value = 0;
		const auto [end, ec] = std::from_chars(id.data(), id.data() + id.size(), value, 16);
		if (ec != std::errc{} || end != id.data() + id.size()) {
			return false;
		}
		a_out = { std::string(file), value, {} };
		return true;
	}

	// ------------------------------------------------------------------ the settings files
	namespace
	{
		enum class Type : std::uint8_t
		{
			kBool,
			kInt,
			kFloat
		};

		struct Setting
		{
			std::string_view section, key;
			Type             type;
			float            lo, hi;
			float (*get)(const Config&);
			void (*set)(Config&, float);
			std::string_view note;
		};

#define CF_BOOL(sec, key, field, note) \
	Setting { sec, key, Type::kBool, 0.0f, 1.0f, [](const Config& c) { return c.field ? 1.0f : 0.0f; }, [](Config& c, float v) { c.field = v != 0.0f; }, note }
#define CF_INT(sec, key, field, lo, hi, note) \
	Setting { sec, key, Type::kInt, lo, hi, [](const Config& c) { return static_cast<float>(c.field); }, [](Config& c, float v) { c.field = static_cast<int>(std::lround(v)); }, note }
#define CF_FLOAT(sec, key, field, lo, hi, note) \
	Setting { sec, key, Type::kFloat, lo, hi, [](const Config& c) { return c.field; }, [](Config& c, float v) { c.field = v; }, note }

		const std::array kSettings{
			CF_BOOL("General", "Enabled", enabled, "0 turns Crossfire off"),
			CF_INT("General", "Who", who, 0.0f, 1.0f, "0: every projectile can clash; 1: only a clash that involves one of yours"),
			CF_BOOL("General", "IgnoreAllies", ignoreAllies, "projectiles of shooters who are not hostile to each other pass through each other"),
			CF_FLOAT("General", "MaxDistance", maxDistance, 500.0f, 30000.0f, "projectiles further than this from you are left alone (units; 70 is a metre)"),
			CF_FLOAT("Hits", "RadiusScale", radiusScale, 0.1f, 10.0f, "times each projectile's own collision radius"),
			CF_FLOAT("Hits", "RadiusBonus", radiusBonus, 0.0f, 200.0f, "units added to every projectile's radius, so a shot does not have to be perfect"),
			CF_FLOAT("Hits", "MinRadius", minRadius, 0.0f, 100.0f, "no projectile is smaller than this"),
			CF_FLOAT("Hits", "MaxRadius", maxRadius, 1.0f, 2000.0f, "no projectile is bigger than this (a cone keeps growing as it goes)"),
			CF_FLOAT("Hits", "BarrierHeight", barrierHeight, 10.0f, 1000.0f, "how tall a wall spell stands, for catching projectiles"),
			CF_FLOAT("Clash", "OverpowerRatio", overpowerRatio, 1.0f, 100.0f, "a projectile this many times stronger than the other destroys it and flies on"),
			CF_BOOL("Clash", "WeakenSurvivor", weakenSurvivor, "the one that flies on loses the strength it beat"),
			CF_BOOL("Clash", "WeakenDamage", weakenDamage, "and does that much less when it lands"),
			CF_FLOAT("Clash", "StreamShare", tuning.streamShare, 0.0f, 10.0f, "a spray particle's strength, as a share of its spell's cost"),
			CF_FLOAT("Clash", "ArrowScale", tuning.arrowScale, 0.0f, 100.0f, "an arrow's strength per point of its damage"),
			CF_FLOAT("Clash", "ShoutStrength", tuning.shoutStrength, 0.0f, 100000.0f, "every shout's strength (shouts cost no magicka)"),
			CF_FLOAT("Clash", "MinSpellStrength", tuning.minSpellStrength, 0.0f, 1000.0f, "no spell is weaker than this"),
			CF_BOOL("Effects", "Explosions", explosions, "a destroyed projectile bursts where it was hit, with its own explosion"),
			CF_BOOL("Effects", "SafeExplosionsOnly", safeExplosionsOnly, "skip an explosion that would do more than show (damage, an enchantment, something it spawns)"),
			CF_INT("Effects", "MaxExplosionsPerFrame", maxExplosionsPerFrame, 0.0f, 32.0f, "at most this many bursts in one frame"),
			CF_FLOAT("Effects", "ExplosionCooldown", explosionCooldown, 0.0f, 5.0f, "seconds between bursts for the same two shooters (sprays clash many times a second)"),
			CF_INT("Effects", "MaxContactsPerFrame", maxContactsPerFrame, 1.0f, 512.0f, "at most this many clashes are settled in one frame"),
			CF_FLOAT("Player", "SkillXP", skillXP, 0.0f, 1000.0f, "skill experience for each enemy projectile one of yours destroys (0: none)"),
			CF_BOOL("Events", "ModEvents", modEvents, "send the Papyrus mod event Crossfire_Clash"),
			CF_BOOL("Debug", "Log", debugLog, "write every clash to Crossfire.log"),
		};
#undef CF_BOOL
#undef CF_INT
#undef CF_FLOAT

		constexpr std::array<std::string_view, 7> kSettingSections{ "General", "Hits", "Clash", "Effects", "Player", "Events", "Debug" };

		[[nodiscard]] bool ParseNumber(std::string_view a_text, float& a_out) noexcept
		{
			a_text = Trim(a_text);
			if (!a_text.empty() && a_text.front() == '+') {
				a_text.remove_prefix(1);
			}
			if (a_text.empty()) {
				return false;
			}
			float value = 0.0f;
			const auto [end, ec] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), value);
			if (ec != std::errc{} || end != a_text.data() + a_text.size() || !std::isfinite(value)) {
				return false;
			}
			a_out = value;
			return true;
		}

		[[nodiscard]] bool ParseBool(std::string_view a_text, float& a_out) noexcept
		{
			a_text = Trim(a_text);
			for (const auto yes : { "1", "true", "yes", "on" }) {
				if (SameText(a_text, yes)) {
					a_out = 1.0f;
					return true;
				}
			}
			for (const auto no : { "0", "false", "no", "off" }) {
				if (SameText(a_text, no)) {
					a_out = 0.0f;
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] std::string FormatNumber(float a_value)
		{
			char buf[64];
			const auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), a_value);
			return ec == std::errc{} ? std::string(buf, end) : std::string("0");
		}

		// "a, b ,c" -> {"a","b","c"}, empties dropped
		template <class F>
		void ForEachItem(std::string_view a_list, F&& a_f)
		{
			while (!a_list.empty()) {
				const auto comma = a_list.find(',');
				const auto item = Trim(a_list.substr(0, comma));
				if (!item.empty()) {
					a_f(item);
				}
				if (comma == std::string_view::npos) {
					break;
				}
				a_list.remove_prefix(comma + 1);
			}
		}

		// "Fire.Frost", "Fire vs Frost", "Fire:Frost", "Fire,Frost"; either side may be *
		[[nodiscard]] bool SplitPair(std::string_view a_key, std::string_view& a_left, std::string_view& a_right) noexcept
		{
			a_key = Trim(a_key);
			for (std::size_t i = 0; i + 4 <= a_key.size(); ++i) {
				if (SameText(a_key.substr(i, 4), " vs ")) {
					a_left = Trim(a_key.substr(0, i));
					a_right = Trim(a_key.substr(i + 4));
					return !a_left.empty() && !a_right.empty();
				}
			}
			const auto sep = a_key.find_first_of(".:,");
			if (sep == std::string_view::npos) {
				return false;
			}
			a_left = Trim(a_key.substr(0, sep));
			a_right = Trim(a_key.substr(sep + 1));
			return !a_left.empty() && !a_right.empty() && a_right.find_first_of(".:,") == std::string_view::npos;
		}

		[[nodiscard]] bool ElementsOf(std::string_view a_side, std::vector<Element>& a_out) noexcept
		{
			a_out.clear();
			if (a_side == "*") {
				for (std::size_t i = 0; i < kElements; ++i) {
					a_out.push_back(static_cast<Element>(i));
				}
				return true;
			}
			Element e{};
			if (!ParseElement(a_side, e)) {
				return false;
			}
			a_out.push_back(e);
			return true;
		}
	}

	Config::Config()
	{
		keywords[static_cast<std::size_t>(Element::kFire)] = { "MagicDamageFire" };
		keywords[static_cast<std::size_t>(Element::kFrost)] = { "MagicDamageFrost" };
		keywords[static_cast<std::size_t>(Element::kShock)] = { "MagicDamageShock" };
	}

	Range RangeOf(std::string_view a_key) noexcept
	{
		for (const auto& s : kSettings) {
			if (SameText(s.key, a_key)) {
				return { s.lo, s.hi };
			}
		}
		return { 0.0f, 0.0f };
	}

	void ParseIni(std::string_view a_text, Config& a_config, std::vector<std::string>& a_warnings)
	{
		const auto warn = [&](std::size_t a_line, std::string_view a_what, std::string_view a_detail) {
			if (a_warnings.size() < 200) {  // a garbage file must not flood the log
				a_warnings.push_back("line " + std::to_string(a_line) + ": " + std::string(a_what) + " '" +
									 std::string(a_detail.substr(0, 80)) + "'");
			}
		};
		if (a_text.starts_with("\xEF\xBB\xBF")) {
			a_text.remove_prefix(3);  // a UTF-8 byte order mark, as Notepad writes
		}
		enum class Where : std::uint8_t
		{
			kNone,
			kSettings,
			kElementLists,
			kReactions,
			kExclude,
			kUnknown
		};
		Where                where = Where::kNone;
		std::string          section;
		std::size_t          lineNo = 0;
		std::vector<Element> left, right;
		while (!a_text.empty()) {
			const auto nl = a_text.find_first_of("\r\n");
			auto       line = a_text.substr(0, nl);
			if (nl == std::string_view::npos) {
				a_text = {};
			} else {
				// \r\n is one line end
				const std::size_t skip = (a_text[nl] == '\r' && nl + 1 < a_text.size() && a_text[nl + 1] == '\n') ? 2 : 1;
				a_text.remove_prefix(nl + skip);
			}
			++lineNo;
			line = Trim(line);
			if (line.empty() || line.front() == ';' || line.front() == '#') {
				continue;
			}
			if (line.front() == '[') {
				const auto close = line.find(']');
				if (close == std::string_view::npos) {
					warn(lineNo, "a section with no ]", line);
					where = Where::kUnknown;
					continue;
				}
				section = std::string(Trim(line.substr(1, close - 1)));
				where = Where::kUnknown;
				for (const auto s : kSettingSections) {
					if (SameText(section, s)) {
						where = Where::kSettings;
						section = std::string(s);
					}
				}
				if (SameText(section, "Elements")) {
					where = Where::kElementLists;
				} else if (SameText(section, "Reactions")) {
					where = Where::kReactions;
				} else if (SameText(section, "Exclude")) {
					where = Where::kExclude;
				}
				if (where == Where::kUnknown) {
					warn(lineNo, "an unknown section", section);
				}
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string_view::npos) {
				warn(lineNo, "no = in", line);
				continue;
			}
			const auto key = Trim(line.substr(0, eq));
			auto       value = line.substr(eq + 1);
			if (const auto comment = value.find_first_of(";#"); comment != std::string_view::npos) {
				value = value.substr(0, comment);
			}
			value = Trim(value);
			if (key.empty()) {
				warn(lineNo, "no key before =", line);
				continue;
			}
			switch (where) {
			case Where::kNone:
				warn(lineNo, "a setting before any section", key);
				break;
			case Where::kUnknown:
				break;  // said once, at the section
			case Where::kSettings:
				{
					const Setting* found = nullptr;
					for (const auto& s : kSettings) {
						if (SameText(s.section, section) && SameText(s.key, key)) {
							found = &s;
						}
					}
					if (!found) {
						warn(lineNo, "an unknown setting", key);
						break;
					}
					float v = 0.0f;
					if (!(found->type == Type::kBool ? ParseBool(value, v) : ParseNumber(value, v))) {
						warn(lineNo, found->type == Type::kBool ? "not 0 or 1" : "not a number", value);
						break;
					}
					if (found->type == Type::kInt) {
						v = std::round(v);
					}
					if (v < found->lo || v > found->hi) {
						warn(lineNo, "out of range, clamped", key);
						v = std::clamp(v, found->lo, found->hi);
					}
					found->set(a_config, v);
					break;
				}
			case Where::kElementLists:
				{
					Element e{};
					if (!ParseElement(key, e)) {
						warn(lineNo, "not an element", key);
						break;
					}
					auto& list = a_config.keywords[static_cast<std::size_t>(e)];
					if (!value.empty() && value.front() == '+') {
						value.remove_prefix(1);
					} else {
						list.clear();
					}
					ForEachItem(value, [&](std::string_view a_kw) {
						if (std::ranges::none_of(list, [&](const std::string& s) { return SameText(s, a_kw); })) {
							list.emplace_back(a_kw);
						}
					});
					break;
				}
			case Where::kReactions:
				{
					std::string_view l, r;
					Action           action{};
					if (!SplitPair(key, l, r) || !ElementsOf(l, left) || !ElementsOf(r, right)) {
						warn(lineNo, "not a pair of elements", key);
						break;
					}
					if (!ParseAction(value, action)) {
						warn(lineNo, "not Clash, Annihilate, Pass, Wins or Loses", value);
						break;
					}
					for (const auto a : left) {
						for (const auto b : right) {
							// "Force.*=Wins" must not turn Force against itself into a contest it then loses
							if (a == b && left.size() * right.size() > 1 && (action == Action::kWins || action == Action::kLoses)) {
								continue;
							}
							SetReaction(a_config.reactions, a, b, action);
						}
					}
					break;
				}
			case Where::kExclude:
				ForEachItem(value, [&](std::string_view a_item) {
					FormRef ref;
					if (!ParseFormRef(a_item, ref)) {
						warn(lineNo, "not Plugin.esp|0x123456 or an editor ID", a_item);
					} else if (std::ranges::find(a_config.exclude, ref) == a_config.exclude.end()) {
						a_config.exclude.push_back(std::move(ref));
					}
				});
				break;
			}
		}
	}

	std::string WriteSettings(const Config& a_config)
	{
		std::string out = "; Crossfire - written by its menu (SKSE Menu Framework). Rules (elements, reactions, exclusions)\n"
						  "; are in Crossfire_Rules.ini, which the menu never writes.\n";
		for (const auto sec : kSettingSections) {
			out += "\n[";
			out += sec;
			out += "]\n";
			for (const auto& s : kSettings) {
				if (s.section != sec) {
					continue;
				}
				out += "; ";
				out += s.note;
				out += "\n";
				out += s.key;
				out += "=";
				const float v = s.get(a_config);
				out += s.type == Type::kFloat ? FormatNumber(v) : std::to_string(static_cast<int>(std::lround(v)));
				out += "\n";
			}
		}
		return out;
	}
}
