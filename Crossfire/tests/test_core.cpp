// Crossfire - tests for the game-free core (src/Core.*). Built and run by tests/run.sh, natively, with sanitizers.
// GPL-3.0-or-later; see LICENSE.txt.

#include "Core.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace Crossfire::Core;

namespace
{
	int gChecks = 0, gFailed = 0;

	void Check(bool a_ok, const char* a_what, const char* a_file, int a_line)
	{
		++gChecks;
		if (!a_ok) {
			++gFailed;
			std::printf("FAIL %s:%d: %s\n", a_file, a_line, a_what);
		}
	}
#define CHECK(x) Check(static_cast<bool>(x), #x, __FILE__, __LINE__)
#define NEAR(a, b, eps) Check(std::abs(static_cast<double>(a) - static_cast<double>(b)) <= (eps), #a " ~= " #b, __FILE__, __LINE__)

	constexpr float kPi = 3.14159265358979f;

	Body Sphere(std::uint32_t id, Vec3 from, Vec3 to, float r, Element e = Element::kFire, float s = 10.0f, std::uint32_t shooter = 0)
	{
		Body b;
		b.id = id;
		b.shooter = shooter ? shooter : id + 1000;
		b.shape = Shape::kSphere;
		b.from = from;
		b.to = to;
		b.radius = r;
		b.element = e;
		b.strength = s;
		return b;
	}

	Body Beam(std::uint32_t id, Vec3 from, Vec3 to, float r)
	{
		Body b = Sphere(id, from, to, r, Element::kShock);
		b.shape = Shape::kBeam;
		b.immune = true;
		return b;
	}

	Body Wall(std::uint32_t id, Vec3 base, Vec3 across, float halfWidth, float height, float r = 0.0f)
	{
		Body b = Sphere(id, base, base, r, Element::kFrost);
		b.shape = Shape::kBarrier;
		b.immune = true;
		b.across = across;
		b.halfWidth = halfWidth;
		b.height = height;
		return b;
	}

	void TestVectors()
	{
		const Vec3 north = DirectionFromAngles(0.0f, 0.0f);
		NEAR(north.x, 0.0f, 1e-6);
		NEAR(north.y, 1.0f, 1e-6);
		NEAR(north.z, 0.0f, 1e-6);
		const Vec3 east = DirectionFromAngles(0.0f, kPi / 2);
		NEAR(east.x, 1.0f, 1e-6);
		NEAR(east.y, 0.0f, 1e-6);
		const Vec3 down = DirectionFromAngles(kPi / 2, 1.234f);
		NEAR(down.z, -1.0f, 1e-6);
		NEAR(Length(down), 1.0f, 1e-6);
		for (int i = 0; i < 100; ++i) {
			NEAR(Length(DirectionFromAngles(static_cast<float>(i) * 0.37f, static_cast<float>(i) * 0.91f)), 1.0f, 1e-5);
		}
		NEAR(DistancePointSegment({ 0, 5, 0 }, { -10, 0, 0 }, { 10, 0, 0 }), 5.0f, 1e-6);
		NEAR(DistancePointSegment({ 20, 0, 0 }, { -10, 0, 0 }, { 10, 0, 0 }), 10.0f, 1e-6);
		NEAR(DistancePointSegment({ 3, 4, 0 }, { 0, 0, 0 }, { 0, 0, 0 }), 5.0f, 1e-6);  // degenerate segment
		CHECK(Finite({ 1, 2, 3 }));
		CHECK(!Finite({ NAN, 0, 0 }));
		CHECK(!Finite({ 0, INFINITY, 0 }));
	}

	void TestSphereSphere()
	{
		Touch t;
		// head on, each covering 200 units in the frame: without a swept test they would pass through each other
		CHECK(FirstTouch(Sphere(1, { 0, -100, 0 }, { 0, 100, 0 }, 5), Sphere(2, { 0, 100, 0 }, { 0, -100, 0 }, 5), t));
		NEAR(t.t, 0.475f, 1e-5);
		NEAR(t.point.y, 0.0f, 1e-3);
		// unequal radii: the point sits where the surfaces meet
		CHECK(FirstTouch(Sphere(1, { 0, 0, 0 }, { 0, 100, 0 }, 10), Sphere(2, { 0, 100, 0 }, { 0, 100, 0 }, 30), t));
		NEAR(t.t, 0.6f, 1e-5);
		NEAR(t.point.y, 70.0f, 1e-3);
		// grazing: centres pass 10.01 apart with radii 5 + 5
		CHECK(!FirstTouch(Sphere(1, { 10.01f, -100, 0 }, { 10.01f, 100, 0 }, 5), Sphere(2, { 0, 100, 0 }, { 0, -100, 0 }, 5), t));
		CHECK(FirstTouch(Sphere(1, { 9.99f, -100, 0 }, { 9.99f, 100, 0 }, 5), Sphere(2, { 0, 100, 0 }, { 0, -100, 0 }, 5), t));
		// flying side by side at the same speed never meet
		CHECK(!FirstTouch(Sphere(1, { 0, 0, 0 }, { 0, 500, 0 }, 5), Sphere(2, { 20, 0, 0 }, { 20, 500, 0 }, 5), t));
		// already touching at the start of the frame
		CHECK(FirstTouch(Sphere(1, { 0, 0, 0 }, { 0, 500, 0 }, 5), Sphere(2, { 8, 0, 0 }, { 8, 500, 0 }, 5), t));
		NEAR(t.t, 0.0f, 0);
		// moving apart
		CHECK(!FirstTouch(Sphere(1, { 0, 0, 0 }, { 0, -100, 0 }, 5), Sphere(2, { 0, 20, 0 }, { 0, 120, 0 }, 5), t));
		// both still, apart / overlapping
		CHECK(!FirstTouch(Sphere(1, { 0, 0, 0 }, { 0, 0, 0 }, 5), Sphere(2, { 0, 20, 0 }, { 0, 20, 0 }, 5), t));
		CHECK(FirstTouch(Sphere(1, { 0, 0, 0 }, { 0, 0, 0 }, 5), Sphere(2, { 0, 9, 0 }, { 0, 9, 0 }, 5), t));
		// would touch only after the frame ends
		CHECK(!FirstTouch(Sphere(1, { 0, 0, 0 }, { 0, 10, 0 }, 1), Sphere(2, { 0, 100, 0 }, { 0, 90, 0 }, 1), t));
		// zero radii crossing exactly
		CHECK(FirstTouch(Sphere(1, { -1, 0, 0 }, { 1, 0, 0 }, 0), Sphere(2, { 1, 0, 0 }, { -1, 0, 0 }, 0), t));
		NEAR(t.t, 0.5f, 1e-6);
		// order does not matter
		Touch u;
		const Body a = Sphere(1, { 3, -50, 7 }, { -2, 60, 1 }, 6), b = Sphere(2, { 0, 70, 3 }, { 1, -40, 2 }, 4);
		CHECK(FirstTouch(a, b, t) && FirstTouch(b, a, u));
		NEAR(t.t, u.t, 1e-6);
		NEAR(t.point.y, u.point.y, 1e-2);
	}

	void TestSphereBeam()
	{
		Touch      t;
		const Body beam = Beam(9, { -1000, 0, 0 }, { 1000, 0, 0 }, 2);
		// a missile crossing the bolt from y=-100 to y=100: first touch where y = -(5 + 2)
		CHECK(FirstTouch(Sphere(1, { 0, -100, 0 }, { 0, 100, 0 }, 5), beam, t));
		NEAR(t.t, (100.0f - 7.0f) / 200.0f, 1e-4);
		NEAR(t.point.y, -2.0f, 1e-2);
		CHECK(FirstTouch(beam, Sphere(1, { 0, -100, 0 }, { 0, 100, 0 }, 5), t));  // either order
		// passing beyond the end of the bolt
		CHECK(!FirstTouch(Sphere(1, { 1010, -100, 0 }, { 1010, 100, 0 }, 5), beam, t));
		// over it
		CHECK(!FirstTouch(Sphere(1, { 0, -100, 8 }, { 0, 100, 8 }, 5), beam, t));
		CHECK(FirstTouch(Sphere(1, { 0, -100, 6.9f }, { 0, 100, 6.9f }, 5), beam, t));
		// flying along it, already inside
		CHECK(FirstTouch(Sphere(1, { -500, 0, 0 }, { 500, 0, 0 }, 5), beam, t));
		NEAR(t.t, 0.0f, 0);
	}

	void TestSphereBarrier()
	{
		Touch      t;
		const Body wall = Wall(9, { 0, 0, 0 }, { 1, 0, 0 }, 100, 150);
		CHECK(FirstTouch(Sphere(1, { 0, -100, 50 }, { 0, 100, 50 }, 5), wall, t));
		NEAR(t.t, 95.0f / 200.0f, 1e-4);
		NEAR(t.point.y, 0.0f, 1e-2);
		NEAR(t.point.z, 50.0f, 1e-2);
		CHECK(FirstTouch(wall, Sphere(1, { 0, -100, 50 }, { 0, 100, 50 }, 5), t));
		CHECK(!FirstTouch(Sphere(1, { 0, -100, 160 }, { 0, 100, 160 }, 5), wall, t));  // over the top
		CHECK(FirstTouch(Sphere(1, { 0, -100, 154 }, { 0, 100, 154 }, 5), wall, t));   // clipping the top
		CHECK(!FirstTouch(Sphere(1, { 120, -100, 50 }, { 120, 100, 50 }, 5), wall, t));  // beside it
		CHECK(!FirstTouch(Sphere(1, { 0, -100, -10 }, { 0, 100, -10 }, 5), wall, t));    // under it (below its base)
		// a wall turned 45 degrees
		const float s = std::sqrt(0.5f);
		const Body  turned = Wall(9, { 0, 0, 0 }, { s, s, 0 }, 100, 150);
		CHECK(FirstTouch(Sphere(1, { 50, -50, 50 }, { -50, 50, 50 }, 1), turned, t));
		NEAR(t.point.x, 0.0f, 1.0);
		NEAR(DistancePointBarrier({ 0, 10, 50 }, wall), 10.0f, 1e-4);
		NEAR(DistancePointBarrier({ 0, 0, 200 }, wall), 50.0f, 1e-4);
		NEAR(DistancePointBarrier({ 103, 4, 50 }, wall), 5.0f, 1e-4);
	}

	void TestBeamBeamAndOthers()
	{
		Touch t;
		CHECK(FirstTouch(Beam(1, { -10, 0, 0 }, { 10, 0, 0 }, 1), Beam(2, { 0, -10, 1.5f }, { 0, 10, 1.5f }, 1), t));
		CHECK(!FirstTouch(Beam(1, { -10, 0, 0 }, { 10, 0, 0 }, 1), Beam(2, { 0, -10, 2.5f }, { 0, 10, 2.5f }, 1), t));
		CHECK(!FirstTouch(Beam(1, { -10, 0, 0 }, { 10, 0, 0 }, 1), Wall(2, { 0, 0, 0 }, { 0, 1, 0 }, 10, 10), t));
		CHECK(!FirstTouch(Wall(1, { 0, 0, 0 }, { 1, 0, 0 }, 10, 10), Wall(2, { 0, 0, 0 }, { 0, 1, 0 }, 10, 10), t));
	}

	void TestValid()
	{
		CHECK(Valid(Sphere(1, { 0, 0, 0 }, { 1, 1, 1 }, 5)));
		CHECK(!Valid(Sphere(1, { NAN, 0, 0 }, { 1, 1, 1 }, 5)));
		CHECK(!Valid(Sphere(1, { 0, 0, 0 }, { 1, INFINITY, 1 }, 5)));
		CHECK(!Valid(Sphere(1, { 0, 0, 0 }, { 1, 1, 1 }, -1)));
		CHECK(!Valid(Sphere(1, { 0, 0, 0 }, { 1, 1, 1 }, NAN)));
		CHECK(!Valid(Sphere(1, { 2e7f, 0, 0 }, { 1, 1, 1 }, 5)));
		Body b = Sphere(1, { 0, 0, 0 }, { 1, 1, 1 }, 5);
		b.strength = NAN;
		CHECK(!Valid(b));
		b.strength = -1;
		CHECK(!Valid(b));
		b = Sphere(1, { 0, 0, 0 }, { 1, 1, 1 }, 5);
		b.element = static_cast<Element>(99);
		CHECK(!Valid(b));
		b = Sphere(1, { 0, 0, 0 }, { 1, 1, 1 }, 5);
		b.shape = static_cast<Shape>(7);
		CHECK(!Valid(b));
		CHECK(Valid(Wall(1, { 0, 0, 0 }, { 1, 0, 0 }, 10, 10)));
		CHECK(!Valid(Wall(1, { 0, 0, 0 }, { 2, 0, 0 }, 10, 10)));       // not a unit axis
		CHECK(!Valid(Wall(1, { 0, 0, 0 }, { 0, 0, 1 }, 10, 10)));       // not horizontal
		CHECK(!Valid(Wall(1, { 0, 0, 0 }, { 1, 0, 0 }, 0, 10)));        // no width
		CHECK(!Valid(Wall(1, { 0, 0, 0 }, { 1, 0, 0 }, 10, -1)));       // no height
		CHECK(!Valid(Wall(1, { 0, 0, 0 }, { NAN, 0, 0 }, 10, 10)));
		const Box box = Bounds(Wall(1, { 0, 0, 0 }, { 1, 0, 0 }, 10, 20, 1));
		NEAR(box.lo.x, -11, 1e-6);
		NEAR(box.hi.x, 11, 1e-6);
		NEAR(box.hi.z, 21, 1e-6);
		NEAR(box.lo.y, -1, 1e-6);
	}

	// random bodies for the property tests
	Body RandomBody(std::mt19937& g, std::uint32_t id, float span)
	{
		std::uniform_real_distribution<float> pos(-span, span), step(-300.0f, 300.0f), rad(0.0f, 40.0f), unit(0.0f, 1.0f);
		const float                           kind = unit(g);
		const Vec3                            p{ pos(g), pos(g), pos(g) * 0.2f };
		if (kind < 0.75f) {
			Body b = Sphere(id, p, p + Vec3{ step(g), step(g), step(g) * 0.3f }, rad(g), static_cast<Element>(id % kElements), 1.0f + unit(g) * 100.0f,
				1 + id % 7);
			b.immune = unit(g) < 0.1f;  // a cone
			return b;
		}
		if (kind < 0.9f) {
			Body b = Beam(id, p, p + Vec3{ step(g) * 3, step(g) * 3, step(g) }, rad(g) * 0.2f);
			b.shooter = 1 + id % 7;
			return b;
		}
		const float angle = unit(g) * 2 * kPi;
		Body        b = Wall(id, p, { std::cos(angle), std::sin(angle), 0 }, 20 + rad(g) * 5, 50 + rad(g) * 5, rad(g) * 0.1f);
		b.shooter = 1 + id % 7;
		return b;
	}

	void TestFindContactsAgainstBruteForce()
	{
		std::mt19937 g(12345);
		std::size_t  total = 0;
		for (int round = 0; round < 300; ++round) {
			const std::size_t n = 2 + static_cast<std::size_t>(round % 60);
			std::vector<Body> bodies;
			for (std::size_t i = 0; i < n; ++i) {
				bodies.push_back(RandomBody(g, static_cast<std::uint32_t>(i), 200.0f + static_cast<float>(round % 5) * 300.0f));
			}
			if (round % 7 == 0) {
				bodies[0].from.x = NAN;  // an invalid body is skipped, not tested
			}
			const auto may = [&](std::size_t i, std::size_t j) { return bodies[i].shooter != bodies[j].shooter; };
			std::vector<Contact> fast;
			FindContacts(bodies, may, 100000, fast);
			std::set<std::pair<std::size_t, std::size_t>> slow;
			for (std::size_t i = 0; i < n; ++i) {
				for (std::size_t j = i + 1; j < n; ++j) {
					Touch t;
					if (Valid(bodies[i]) && Valid(bodies[j]) && !(bodies[i].immune && bodies[j].immune) && may(i, j) &&
						FirstTouch(bodies[i], bodies[j], t)) {
						slow.insert({ i, j });
					}
				}
			}
			std::set<std::pair<std::size_t, std::size_t>> got;
			for (std::size_t k = 0; k < fast.size(); ++k) {
				got.insert({ fast[k].a, fast[k].b });
				CHECK(fast[k].a < fast[k].b);
				if (k) {
					CHECK(fast[k - 1].touch.t <= fast[k].touch.t);
				}
				CHECK(fast[k].touch.t >= 0.0f && fast[k].touch.t <= 1.0f);
				CHECK(Finite(fast[k].touch.point));
			}
			CHECK(got == slow);
			CHECK(got.size() == fast.size());  // no pair twice
			total += fast.size();
		}
		CHECK(total > 200);  // the random worlds really did collide
		std::printf("  brute force agreed on %zu contacts in 300 random worlds\n", total);
	}

	void TestFindContactsRules()
	{
		std::vector<Body> bodies{
			Sphere(1, { 0, -100, 0 }, { 0, 100, 0 }, 5, Element::kFire, 10, 1),
			Sphere(2, { 0, 100, 0 }, { 0, -100, 0 }, 5, Element::kFrost, 10, 2),
			Sphere(3, { 0, -100, 0 }, { 0, 100, 0 }, 5, Element::kFire, 10, 1),  // same shooter as 1, same path
		};
		std::size_t          asked = 0;
		std::vector<Contact> out;
		FindContacts(bodies, [&](std::size_t i, std::size_t j) { ++asked; return bodies[i].shooter != bodies[j].shooter; }, 10, out);
		CHECK(out.size() == 2);
		CHECK(asked == 3);
		FindContacts(bodies, [&](std::size_t, std::size_t) { return true; }, 1, out);
		CHECK(out.size() == 1);
		NEAR(out[0].touch.t, 0.0f, 0);  // 1 and 3 overlap from the start: the earliest is kept
		FindContacts(bodies, [&](std::size_t, std::size_t) { return true; }, 0, out);
		CHECK(out.empty());
		FindContacts(std::span<const Body>{}, [&](std::size_t, std::size_t) { return true; }, 10, out);
		CHECK(out.empty());
		// two immune bodies are never offered
		std::vector<Body> walls{ Wall(1, { 0, 0, 0 }, { 1, 0, 0 }, 50, 50), Beam(2, { 0, -50, 10 }, { 0, 50, 10 }, 2) };
		asked = 0;
		FindContacts(walls, [&](std::size_t, std::size_t) { ++asked; return true; }, 10, out);
		CHECK(asked == 0 && out.empty());
		// far apart in x: the sweep stops early and never asks
		std::vector<Body> apart{ Sphere(1, { 0, 0, 0 }, { 1, 0, 0 }, 5), Sphere(2, { 5000, 0, 0 }, { 5001, 0, 0 }, 5) };
		asked = 0;
		FindContacts(apart, [&](std::size_t, std::size_t) { ++asked; return true; }, 10, out);
		CHECK(asked == 0);
		// same x band, apart in y
		std::vector<Body> apartY{ Sphere(1, { 0, 0, 0 }, { 1, 0, 0 }, 5), Sphere(2, { 0, 5000, 0 }, { 1, 5000, 0 }, 5) };
		asked = 0;
		FindContacts(apartY, [&](std::size_t, std::size_t) { ++asked; return true; }, 10, out);
		CHECK(asked == 0);
	}

	void TestTable()
	{
		const Table t = DefaultTable();
		const auto  at = [&](Element a, Element b) { return t[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)]; };
		CHECK(at(Element::kFire, Element::kFrost) == Action::kAnnihilate);
		CHECK(at(Element::kFrost, Element::kFire) == Action::kAnnihilate);
		CHECK(at(Element::kForce, Element::kPhysical) == Action::kWins);
		CHECK(at(Element::kPhysical, Element::kForce) == Action::kLoses);
		CHECK(at(Element::kForce, Element::kForce) == Action::kPass);
		CHECK(at(Element::kFire, Element::kFire) == Action::kClash);
		CHECK(at(Element::kShock, Element::kPhysical) == Action::kClash);
		for (std::size_t i = 0; i < kElements; ++i) {
			for (std::size_t j = 0; j < kElements; ++j) {
				CHECK(t[i][j] == Mirror(t[j][i]));  // the table is always consistent with its mirror
			}
		}
		Table u = DefaultTable();
		SetReaction(u, Element::kFire, Element::kFire, Action::kWins);
		CHECK(u[0][0] == Action::kClash);
		SetReaction(u, static_cast<Element>(50), Element::kFire, Action::kPass);  // ignored, no crash
		CHECK(Mirror(Action::kWins) == Action::kLoses && Mirror(Action::kLoses) == Action::kWins && Mirror(Action::kClash) == Action::kClash);
		Element e{};
		CHECK(ParseElement(" frost ", e) && e == Element::kFrost);
		CHECK(ParseElement("PHYSICAL", e) && e == Element::kPhysical);
		CHECK(!ParseElement("Frosty", e));
		CHECK(!ParseElement("", e));
		Action a{};
		CHECK(ParseAction("beats", a) && a == Action::kWins);
		CHECK(ParseAction("Cancel", a) && a == Action::kAnnihilate);
		CHECK(!ParseAction("Explode", a));
		for (std::size_t i = 0; i < kElements; ++i) {
			Element back{};
			CHECK(ParseElement(ElementName(static_cast<Element>(i)), back) && back == static_cast<Element>(i));
		}
		CHECK(ElementName(static_cast<Element>(99)) == "?");
	}

	void TestResolve()
	{
		const Table t = DefaultTable();
		using enum Element;
		// the contest
		auto o = Resolve(t, 2.0f, true, kFire, 100, false, kShock, 40, false);
		CHECK(o.happened && !o.killA && o.killB);
		NEAR(o.strengthA, 60.0f, 1e-4);
		o = Resolve(t, 2.0f, false, kFire, 100, false, kShock, 40, false);
		CHECK(!o.killA && o.killB);
		NEAR(o.strengthA, 100.0f, 0);
		o = Resolve(t, 2.0f, true, kFire, 30, false, kShock, 40, false);
		CHECK(o.killA && o.killB);
		o = Resolve(t, 2.0f, true, kShock, 40, false, kFire, 100, false);
		CHECK(o.killA && !o.killB);
		NEAR(o.strengthB, 60.0f, 1e-4);
		// opposites cancel whatever their size
		o = Resolve(t, 2.0f, true, kFire, 1000, false, kFrost, 1, false);
		CHECK(o.killA && o.killB);
		// force wins
		o = Resolve(t, 2.0f, true, kForce, 1, true, kFire, 1000, false);
		CHECK(!o.killA && o.killB);
		o = Resolve(t, 2.0f, true, kFire, 1000, false, kForce, 1, true);
		CHECK(o.killA && !o.killB);
		o = Resolve(t, 2.0f, true, kForce, 1, true, kForce, 1, false);
		CHECK(!o.happened);
		// immune sides
		o = Resolve(t, 2.0f, true, kShock, 10, true, kFire, 1000, false);  // a weak bolt against a big fireball
		CHECK(!o.killA && !o.killB && o.happened);
		NEAR(o.strengthB, 990.0f, 1e-3);
		NEAR(o.strengthA, 10.0f, 0);  // an immune side is never weakened
		o = Resolve(t, 2.0f, true, kShock, 10, true, kFire, 12, false);  // even: only the mortal one goes
		CHECK(!o.killA && o.killB);
		o = Resolve(t, 2.0f, true, kShock, 10, true, kFire, 12, true);
		CHECK(!o.happened);
		o = Resolve(t, 2.0f, true, kFrost, 10, true, kFire, 1000, false);  // a frost wall stops any fire
		CHECK(!o.killA && o.killB);
		// ratio 1: equals both go; exactly beaten goes to zero and dies
		o = Resolve(t, 1.0f, true, kFire, 50, false, kShock, 50, false);
		CHECK(o.killA && o.killB);
		o = Resolve(t, 1.0f, true, kFire, 51, false, kShock, 50, false);
		CHECK(!o.killA && o.killB);
		NEAR(o.strengthA, 1.0f, 1e-4);
		// bad input is survived
		o = Resolve(t, NAN, true, kFire, NAN, false, kShock, 50, false);
		CHECK(o.killA);  // a NaN strength counts as none
		o = Resolve(t, 0.5f, true, kFire, 100, false, kShock, 60, false);  // a ratio below 1 is taken as 1
		CHECK(!o.killA && o.killB);
		o = Resolve(t, 2.0f, true, static_cast<Element>(42), 1, false, kFire, 1, false);
		CHECK(!o.happened);
		o = Resolve(t, 2.0f, true, kFire, 0, false, kShock, 0, false);
		CHECK(o.killA && o.killB);
		// pass
		Table p = DefaultTable();
		SetReaction(p, kFire, kShock, Action::kPass);
		o = Resolve(p, 2.0f, true, kFire, 100, false, kShock, 1, false);
		CHECK(!o.happened && !o.killA && !o.killB);
		// symmetry: swapping the sides swaps the outcome, for any table and strengths
		std::mt19937                          g(7);
		std::uniform_real_distribution<float> s(0.0f, 200.0f);
		std::uniform_int_distribution<int>    act(0, 4), el(0, static_cast<int>(kElements) - 1), coin(0, 3);
		for (int i = 0; i < 20000; ++i) {
			Table r = DefaultTable();
			for (int k = 0; k < 6; ++k) {
				SetReaction(r, static_cast<Element>(el(g)), static_cast<Element>(el(g)), static_cast<Action>(act(g)));
			}
			const Element ea = static_cast<Element>(el(g)), eb = static_cast<Element>(el(g));
			const float   sa = s(g), sb = s(g), ratio = 1.0f + s(g) / 50.0f;
			const bool    ia = coin(g) == 0, ib = coin(g) == 0, weaken = coin(g) < 2;
			const auto    x = Resolve(r, ratio, weaken, ea, sa, ia, eb, sb, ib);
			const auto    y = Resolve(r, ratio, weaken, eb, sb, ib, ea, sa, ia);
			CHECK(x.killA == y.killB && x.killB == y.killA && x.happened == y.happened);
			NEAR(x.strengthA, y.strengthB, 1e-4);
			if (ia) {
				CHECK(!x.killA && x.strengthA == sa);
			}
			CHECK(x.strengthA >= 0.0f && x.strengthB >= 0.0f && x.strengthA <= sa && x.strengthB <= sb);
			CHECK(!(x.killA && x.strengthA != 0.0f));
		}
	}

	void TestStrength()
	{
		Tuning t;
		NEAR(BaseStrength(Kind::kSpell, 41, 1, 0, t), 41.0f, 1e-4);
		NEAR(BaseStrength(Kind::kSpell, 41, 2.2f, 0, t), 90.2f, 1e-3);          // dual cast
		NEAR(BaseStrength(Kind::kSpell, 1, 1, 0, t), 5.0f, 1e-4);              // the floor
		NEAR(BaseStrength(Kind::kStream, 20, 1, 0, t), 3.0f, 1e-4);
		NEAR(BaseStrength(Kind::kArrow, 0, 1, 18, t), 36.0f, 1e-4);
		NEAR(BaseStrength(Kind::kArrow, 0, 1, 0, t), 2.0f, 1e-4);              // at least one point of damage
		NEAR(BaseStrength(Kind::kVoice, 0, 1, 0, t), 200.0f, 1e-4);
		NEAR(BaseStrength(Kind::kCone, 50, 1, 0, t), 50.0f, 1e-4);
		// nonsense in, something sane out
		for (const float bad : { NAN, INFINITY, -INFINITY, -5.0f, 0.0f, 1e30f }) {
			for (const Kind k : { Kind::kSpell, Kind::kStream, Kind::kArrow, Kind::kVoice, Kind::kBeam, Kind::kBarrier, Kind::kCone }) {
				const float s = BaseStrength(k, bad, bad, bad, t);
				CHECK(std::isfinite(s) && s >= 0.01f && s <= 1e9f);
			}
		}
		Tuning weird{ NAN, -1, INFINITY, -3 };
		CHECK(std::isfinite(BaseStrength(Kind::kStream, 10, 1, 0, weird)));
		CHECK(std::isfinite(BaseStrength(Kind::kVoice, 10, 1, 0, weird)));
		NEAR(ConeRadius(10, 100, 0.5f), 60.0f, 1e-4);
		NEAR(ConeRadius(-10, -100, 0.5f), 0.0f, 0);
		NEAR(ConeRadius(NAN, 100, NAN), 0.0f, 0);
		NEAR(ConeRadius(10, 1e20f, 10), 1.0e4f, 0);
	}

	void TestClassify()
	{
		std::array<std::vector<std::string>, kElements> kw;
		kw[0] = { "MagicDamageFire" };
		kw[1] = { "MagicDamageFrost" };
		kw[2] = { "MagicDamageShock", "MyShock" };
		const std::string_view fireFrost[]{ "MagicDamageFrost", "magicdamagefire" };
		CHECK(Classify({ fireFrost, "", false, false }, kw) == Element::kFire);  // the first element's list wins
		const std::string_view mine[]{ "Unrelated", "MYSHOCK" };
		CHECK(Classify({ mine, "ResistFire", false, false }, kw) == Element::kShock);  // keywords before the resist
		CHECK(Classify({ {}, "ResistFrost", false, false }, kw) == Element::kFrost);
		CHECK(Classify({ {}, "ElectricResist", false, false }, kw) == Element::kShock);
		CHECK(Classify({ {}, "PoisonResist", false, false }, kw) == Element::kPoison);
		CHECK(Classify({ {}, "", true, true }, kw) == Element::kForce);
		CHECK(Classify({ {}, "", true, false }, kw) == Element::kArcane);
		CHECK(Classify({ {}, "", false, true }, kw) == Element::kArcane);
		CHECK(Classify({ {}, "MagicResist", false, false }, kw) == Element::kArcane);
		std::array<std::vector<std::string>, kElements> none;
		CHECK(Classify({ fireFrost, "", false, false }, none) == Element::kArcane);
	}

	void TestLimiter()
	{
		Limiter l;
		l.BeginFrame(10.0, 2, 0.5f);
		CHECK(l.Allow(1));
		CHECK(!l.Allow(1));  // cooldown
		CHECK(l.Allow(2));
		CHECK(!l.Allow(3));  // the frame's cap
		l.BeginFrame(10.2, 2, 0.5f);
		CHECK(!l.Allow(1));
		CHECK(l.Allow(3));
		l.BeginFrame(10.6, 2, 0.5f);
		CHECK(l.Allow(1));
		l.BeginFrame(1.0, 2, 0.5f);  // a save was loaded: the clock went back
		CHECK(l.Allow(1));
		l.BeginFrame(2.0, 0, 0.0f);
		CHECK(!l.Allow(99));
		l.BeginFrame(3.0, -5, NAN);
		CHECK(!l.Allow(99));
		for (std::uint64_t k = 0; k < 1000; ++k) {
			l.BeginFrame(4.0 + static_cast<double>(k) * 0.001, 1, 0.1f);
			CHECK(l.Allow(k));
		}
		l.BeginFrame(100.0, 1, 0.1f);  // long after: the old keys are forgotten
		CHECK(l.Remembered() == 0);
	}

	void TestFormRef()
	{
		FormRef r;
		CHECK(ParseFormRef("Skyrim.esm|0x012EB7", r) && r.file == "Skyrim.esm" && r.id == 0x12EB7 && r.editorID.empty());
		CHECK(ParseFormRef(" My Mod.esp | 0XABC ", r) && r.file == "My Mod.esp" && r.id == 0xABC);
		CHECK(ParseFormRef("Dawnguard.esm~00F1A2", r) && r.file == "Dawnguard.esm" && r.id == 0xF1A2);
		CHECK(ParseFormRef("Update.esm|FFFFFFFF", r) && r.id == 0xFFFFFFFF);
		CHECK(ParseFormRef("MyProjectile_01", r) && r.file.empty() && r.editorID == "MyProjectile_01");
		for (const char* bad : { "", "   ", "|0x1", "Skyrim.esm|", "Skyrim.esm|0x", "Skyrim.esm|0xZZ", "Skyrim.esm|0x123456789", "a|b|c",
				 "has space", "Skyrim.esm|-1", "Skyrim.esm|+1", "Skyrim.esm|12 34", "Edit-ID", "a~b|1" }) {
			CHECK(!ParseFormRef(bad, r));
		}
	}

	void TestIni()
	{
		{
			Config                   c;
			std::vector<std::string> w;
			ParseIni("\xEF\xBB\xBF; a comment\r\n[General]\r\nEnabled = off\r\nWho=1 ; inline\r\nMaxDistance=+7000\r\n"
					 "[hits]\rRadiusBonus=20\rMinRadius=-5\n[Clash]\nOverpowerRatio=3.5\nWeakenSurvivor=no\nShoutStrength=1e3\n"
					 "[Effects]\nMaxExplosionsPerFrame=2.6\nExplosionCooldown=nan\n[Debug]\nLog=yes\n",
				c, w);
			CHECK(!c.enabled);
			CHECK(c.who == 1);
			NEAR(c.maxDistance, 7000.0f, 0);
			NEAR(c.radiusBonus, 20.0f, 0);
			NEAR(c.minRadius, 0.0f, 0);  // clamped
			NEAR(c.overpowerRatio, 3.5f, 0);
			CHECK(!c.weakenSurvivor);
			NEAR(c.tuning.shoutStrength, 1000.0f, 0);
			CHECK(c.maxExplosionsPerFrame == 3);  // an int is rounded
			NEAR(c.explosionCooldown, 0.12f, 0);  // nan refused, the default kept
			CHECK(c.debugLog);
			CHECK(w.size() == 2);  // MinRadius clamped, nan refused
		}
		{
			Config                   c;
			std::vector<std::string> w;
			ParseIni("Enabled=0\n[Nope]\nx=1\ny=2\n[General]\nNoSuchThing=1\nWho\n=5\nWho=abc\nWho=12\n[General\nEnabled=0\n", c, w);
			CHECK(c.enabled);  // the first line had no section; the last is under a broken one
			CHECK(c.who == 1);  // 12 clamped to 1
			CHECK(w.size() == 8);
			for (const auto& s : w) {
				CHECK(s.starts_with("line "));
			}
		}
		{
			Config                   c;
			std::vector<std::string> w;
			ParseIni("[Elements]\nFire=A, B ,,b\nFrost=+X\nFrost=+x, Y\nShock=\nWater=Z\n", c, w);
			CHECK((c.keywords[0] == std::vector<std::string>{ "A", "B" }));  // replaced, empties and case-duplicates dropped
			CHECK((c.keywords[1] == std::vector<std::string>{ "MagicDamageFrost", "X", "Y" }));  // appended
			CHECK(c.keywords[2].empty());
			CHECK(w.size() == 1);
		}
		{
			Config                   c;
			std::vector<std::string> w;
			ParseIni("[Reactions]\nFire.Shock=Pass\nArcane vs Physical=Wins\n*.Poison=Annihilate\nPhysical.*=Loses\nFire.Nope=Pass\n"
					 "Fire=Pass\nFire.Frost=Boom\nA.B.C=Pass\n",
				c, w);
			const auto at = [&](Element a, Element b) { return c.reactions[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)]; };
			CHECK(at(Element::kFire, Element::kShock) == Action::kPass && at(Element::kShock, Element::kFire) == Action::kPass);
			CHECK(at(Element::kPoison, Element::kFire) == Action::kAnnihilate);
			CHECK(at(Element::kPhysical, Element::kArcane) == Action::kLoses && at(Element::kArcane, Element::kPhysical) == Action::kWins);
			CHECK(at(Element::kPhysical, Element::kPhysical) == Action::kClash);  // a wildcard never sets an element against itself to Wins/Loses
			CHECK(at(Element::kPhysical, Element::kPoison) == Action::kLoses);  // later lines win
			CHECK(at(Element::kFire, Element::kFrost) == Action::kAnnihilate);
			CHECK(w.size() == 4);
			for (std::size_t i = 0; i < kElements; ++i) {
				for (std::size_t j = 0; j < kElements; ++j) {
					CHECK(c.reactions[i][j] == Mirror(c.reactions[j][i]));
				}
			}
		}
		{
			Config                   c;
			std::vector<std::string> w;
			ParseIni("[Exclude]\nForms=Skyrim.esm|0x10F7ED, Bad Thing, MyProj\nMore=Skyrim.esm|10F7ED,Dawnguard.esm~1\n", c, w);
			CHECK(c.exclude.size() == 3);  // the duplicate is kept once
			CHECK(w.size() == 1);
		}
		{
			// a flood of bad lines is capped in the warnings
			std::string text = "[General]\n";
			for (int i = 0; i < 1000; ++i) {
				text += "Bad" + std::to_string(i) + "=1\n";
			}
			Config                   c;
			std::vector<std::string> w;
			ParseIni(text, c, w);
			CHECK(w.size() == 200);
		}
		// what the menu writes reads back exactly
		std::mt19937                          g(99);
		std::uniform_real_distribution<float> u(0.0f, 1.0f);
		for (int round = 0; round < 500; ++round) {
			Config                   c;
			std::vector<std::string> w;
			c.enabled = u(g) < 0.5f;
			c.who = u(g) < 0.5f ? 1 : 0;
			c.ignoreAllies = u(g) < 0.5f;
			c.maxDistance = 500 + u(g) * 29500;
			c.radiusScale = 0.1f + u(g) * 9.9f;
			c.radiusBonus = u(g) * 200;
			c.minRadius = u(g) * 100;
			c.maxRadius = 1 + u(g) * 1999;
			c.barrierHeight = 10 + u(g) * 990;
			c.overpowerRatio = 1 + u(g) * 99;
			c.weakenSurvivor = u(g) < 0.5f;
			c.weakenDamage = u(g) < 0.5f;
			c.tuning.streamShare = u(g) * 10;
			c.tuning.arrowScale = u(g) * 100;
			c.tuning.shoutStrength = u(g) * 100000;
			c.tuning.minSpellStrength = u(g) * 1000;
			c.explosions = u(g) < 0.5f;
			c.safeExplosionsOnly = u(g) < 0.5f;
			c.maxExplosionsPerFrame = static_cast<int>(u(g) * 32);
			c.explosionCooldown = u(g) * 5;
			c.maxContactsPerFrame = 1 + static_cast<int>(u(g) * 511);
			c.skillXP = u(g) * 1000;
			c.modEvents = u(g) < 0.5f;
			c.debugLog = u(g) < 0.5f;
			Config back;
			ParseIni(WriteSettings(c), back, w);
			CHECK(w.empty());
			CHECK(back.enabled == c.enabled && back.who == c.who && back.ignoreAllies == c.ignoreAllies && back.maxDistance == c.maxDistance &&
				  back.radiusScale == c.radiusScale && back.radiusBonus == c.radiusBonus && back.minRadius == c.minRadius &&
				  back.maxRadius == c.maxRadius && back.barrierHeight == c.barrierHeight && back.overpowerRatio == c.overpowerRatio &&
				  back.weakenSurvivor == c.weakenSurvivor && back.weakenDamage == c.weakenDamage &&
				  back.tuning.streamShare == c.tuning.streamShare && back.tuning.arrowScale == c.tuning.arrowScale &&
				  back.tuning.shoutStrength == c.tuning.shoutStrength && back.tuning.minSpellStrength == c.tuning.minSpellStrength &&
				  back.explosions == c.explosions && back.safeExplosionsOnly == c.safeExplosionsOnly &&
				  back.maxExplosionsPerFrame == c.maxExplosionsPerFrame && back.explosionCooldown == c.explosionCooldown &&
				  back.maxContactsPerFrame == c.maxContactsPerFrame && back.skillXP == c.skillXP && back.modEvents == c.modEvents &&
				  back.debugLog == c.debugLog);
		}
		// the shipped defaults are what a fresh Config holds
		Config                   fresh, reread;
		std::vector<std::string> w;
		ParseIni(WriteSettings(fresh), reread, w);
		CHECK(w.empty());
		CHECK(WriteSettings(fresh) == WriteSettings(reread));
		const Range r = RangeOf("radiusbonus");
		CHECK(r.lo == 0.0f && r.hi == 200.0f);
		const Range none = RangeOf("nothing");
		CHECK(none.lo == 0.0f && none.hi == 0.0f);
	}

	void TestFrameDecisions()
	{
		Config c;  // bonus 12, scale 1, min 4, max 256
		NEAR(Radius(10, c), 22.0f, 1e-5);
		NEAR(Radius(0, c), 12.0f, 1e-5);
		NEAR(Radius(-5, c), 12.0f, 1e-5);
		NEAR(Radius(NAN, c), 12.0f, 1e-5);
		NEAR(Radius(1e9f, c), 256.0f, 0);
		c.radiusBonus = 0;
		NEAR(Radius(1, c), 4.0f, 0);  // the minimum
		c.maxRadius = 2;              // a maximum below the minimum: the minimum wins
		NEAR(Radius(100, c), 4.0f, 0);
		c.radiusScale = NAN;
		c.maxRadius = 256;
		NEAR(Radius(10, c), 10.0f, 1e-5);
		for (int i = 0; i < 64; ++i) {
			const float h = static_cast<float>(i) * 0.3f - 6.0f;
			const Vec3  across = BarrierAcross(h);
			NEAR(Length(across), 1.0f, 1e-5);
			NEAR(across.z, 0.0f, 0);
			NEAR(Dot(across, DirectionFromAngles(0.0f, h)), 0.0f, 1e-5);  // square to the way it faces
			Body wall = Wall(1, { 0, 0, 0 }, across, 10, 10);
			CHECK(Valid(wall));
		}
		CHECK(Valid(Wall(1, { 0, 0, 0 }, BarrierAcross(NAN), 10, 10)));

		// first sight: back along the velocity, never further than flown
		Vec3 p = Backtrack({ 100, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 0 }, 0.05f, 1000.0f);
		NEAR(p.x, 50.0f, 1e-3);
		p = Backtrack({ 100, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 0 }, 0.05f, 20.0f);
		NEAR(p.x, 80.0f, 1e-3);
		p = Backtrack({ 100, 0, 0 }, { 0, 0, 0 }, { 0, 2000, 0 }, 0.05f, NAN);  // the fallback velocity, no limit
		NEAR(p.y, -100.0f, 1e-3);
		p = Backtrack({ 100, 0, 0 }, { NAN, 0, 0 }, { 0, 0, 0 }, 0.05f, 10.0f);
		CHECK(p == (Vec3{ 100, 0, 0 }));
		p = Backtrack({ 100, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 0 }, NAN, 10.0f);
		CHECK(p == (Vec3{ 100, 0, 0 }));
		p = Backtrack({ 100, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 0 }, 0.05f, -1.0f);  // a negative distance is ignored
		NEAR(p.x, 50.0f, 1e-3);
		p = Backtrack({ 100, 0, 0 }, { 0.5f, 0, 0 }, { 0, 0, 0 }, 0.05f, 10.0f);  // barely moving: where it is
		CHECK(p == (Vec3{ 100, 0, 0 }));

		// who may clash: every combination against the rule written out plainly
		for (int bits = 0; bits < 256; ++bits) {
			Shooters s;
			s.a = (bits & 1) ? 7u : 0u;
			s.b = (bits & 2) ? 7u : ((bits & 4) ? 9u : 0u);
			s.playerA = (bits & 8) != 0;
			s.playerB = (bits & 16) != 0;
			s.actorA = (bits & 32) != 0;
			s.actorB = (bits & 64) != 0;
			const bool hostile = (bits & 128) != 0;
			for (int who = 0; who < 2; ++who) {
				for (int allies = 0; allies < 2; ++allies) {
					bool asked = false;
					const bool got = MayInteract(s, who, allies != 0, [&]() { asked = true; return hostile; });
					bool want = s.a != s.b && (who == 0 || s.playerA || s.playerB);
					if (want && allies && s.actorA && s.actorB) {
						want = hostile;
					}
					CHECK(got == want);
					CHECK(!asked || (allies && s.actorA && s.actorB));  // hostility is only asked when it matters
				}
			}
		}

		NEAR(DamageScale(100, 40), 0.4f, 1e-6);
		NEAR(DamageScale(100, 1), 0.05f, 0);   // never below a twentieth
		NEAR(DamageScale(100, 150), 1.0f, 0);  // never stronger
		NEAR(DamageScale(0, 5), 1.0f, 0);
		NEAR(DamageScale(NAN, 5), 1.0f, 0);
		NEAR(DamageScale(10, NAN), 1.0f, 0);
	}

	// the rules file the mod ships reads without a warning and gives exactly the built-in defaults
	void TestShippedRules()
	{
		const char* path = CROSSFIRE_DATA "/SKSE/Plugins/Crossfire_Rules.ini";
		std::FILE*  f = std::fopen(path, "rb");
		CHECK(f != nullptr);
		if (!f) {
			return;
		}
		std::string text;
		char        buf[4096];
		for (std::size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) {
			text.append(buf, n);
		}
		std::fclose(f);
		Config                   c;
		std::vector<std::string> w;
		ParseIni(text, c, w);
		for (const auto& s : w) {
			std::printf("  shipped rules: %s\n", s.c_str());
		}
		CHECK(w.empty());
		CHECK(c.reactions == DefaultTable());
		CHECK(c.keywords == Config{}.keywords);
		CHECK(c.exclude.empty());
	}

	void TestGarbageNeverCrashes()
	{
		std::mt19937                       g(2026);
		std::uniform_int_distribution<int> len(0, 400), byte(0, 255), pick(0, 9);
		const char*                        pieces[]{ "[General]", "[Elements]", "[Reactions]", "[Exclude]", "=", "\n", "\r", ",", "*.", "|0x" };
		for (int i = 0; i < 20000; ++i) {
			std::string s;
			const int   n = len(g);
			for (int k = 0; k < n; ++k) {
				if (pick(g) < 3) {
					s += pieces[pick(g)];
				} else {
					s += static_cast<char>(byte(g));
				}
			}
			Config                   c;
			std::vector<std::string> w;
			ParseIni(s, c, w);
			FormRef r;
			(void)ParseFormRef(s, r);
			CHECK(w.size() <= 200);
		}
	}

	void Bench()
	{
		std::mt19937      g(5);
		std::vector<Body> bodies;
		for (std::uint32_t i = 0; i < 400; ++i) {
			bodies.push_back(RandomBody(g, i, 4000.0f));
		}
		std::vector<Contact> out;
		const auto           may = [&](std::size_t i, std::size_t j) { return bodies[i].shooter != bodies[j].shooter; };
		const auto           start = std::chrono::steady_clock::now();
		constexpr int        kRuns = 2000;
		std::size_t          found = 0;
		for (int i = 0; i < kRuns; ++i) {
			FindContacts(bodies, may, 64, out);
			found += out.size();
		}
		const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / kRuns;
		std::printf("  bench: 400 projectiles, %.1f microseconds a frame (%zu contacts)\n", us, found / kRuns);
		CHECK(us < 2000.0);  // generous: sanitizer builds are slow; an optimised build is far below this
	}
}

int main()
{
	TestVectors();
	TestSphereSphere();
	TestSphereBeam();
	TestSphereBarrier();
	TestBeamBeamAndOthers();
	TestValid();
	TestFindContactsAgainstBruteForce();
	TestFindContactsRules();
	TestTable();
	TestResolve();
	TestStrength();
	TestClassify();
	TestLimiter();
	TestFormRef();
	TestIni();
	TestFrameDecisions();
	TestShippedRules();
	TestGarbageNeverCrashes();
	Bench();
	std::printf("%d checks, %d failed\n", gChecks, gFailed);
	return gFailed == 0 ? 0 : 1;
}
