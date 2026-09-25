// Independent differential test of Crossfire::Core::FirstTouch and Resolve.
// The reference uses its OWN distance functions (not Core's) and samples the frame densely; it shares no code with
// the search it checks. Build with ASan+UBSan:
//   run by tests/run.sh (after the core tests); by hand: tests/fuzz_touch <cases>
#include "Core.h"

#include <cmath>
#include <cstdio>
#include <random>

using namespace Crossfire::Core;

namespace
{
	struct V
	{
		double x, y, z;
	};
	V      D(Vec3 a) { return { a.x, a.y, a.z }; }
	V      operator-(V a, V b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	V      operator+(V a, V b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	V      operator*(V a, double s) { return { a.x * s, a.y * s, a.z * s }; }
	double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	double len(V a) { return std::sqrt(dot(a, a)); }

	double SegDist(V p, V a, V b)
	{
		const V      ab = b - a;
		const double L = dot(ab, ab);
		double       t = L > 0 ? dot(p - a, ab) / L : 0;
		t = t < 0 ? 0 : t > 1 ? 1 : t;
		return len(p - (a + ab * t));
	}
	double RectDist(V p, const Body& w)  // upright rectangle: base centre `from`, `across` * [-hw, hw], z in [0, height]
	{
		const V      o = D(w.from), u = D(w.across);
		const V      d = p - o;
		double       s = dot(d, u), h = d.z;
		s = s < -w.halfWidth ? -w.halfWidth : s > w.halfWidth ? w.halfWidth : s;
		h = h < 0 ? 0 : h > w.height ? w.height : h;
		return len(p - (o + u * s + V{ 0, 0, h }));
	}
	// distance between the two bodies at time t (a moving sphere against anything; two beams are static)
	double Dist(const Body& a, const Body& b, double t)
	{
		auto at = [](const Body& s, double when) { return D(s.from) + (D(s.to) - D(s.from)) * when; };
		if (a.shape == Shape::kSphere && b.shape == Shape::kSphere) {
			return len(at(a, t) - at(b, t));
		}
		const Body& s = a.shape == Shape::kSphere ? a : b;
		const Body& o = a.shape == Shape::kSphere ? b : a;
		if (o.shape == Shape::kBeam) {
			return SegDist(at(s, t), D(o.from), D(o.to));
		}
		return RectDist(at(s, t), o);
	}
}

int main(int argc, char** argv)
{
	const long            iters = argc > 1 ? std::atol(argv[1]) : 200000;
	std::mt19937_64       g(4242);
	std::uniform_real_distribution<double> u(0.0, 1.0);
	long                  touched = 0, fails = 0, checked = 0;
	auto                  report = [&](const char* what, const Body& a, const Body& b, double x, double y) {
        if (++fails <= 8) {
            std::printf("FAIL %s: shapes %d/%d  r %.4g+%.4g  a (%.6g %.6g %.6g)->(%.6g %.6g %.6g)  b (%.6g %.6g %.6g)->(%.6g %.6g %.6g)  [%g %g]\n",
                what, int(a.shape), int(b.shape), a.radius, b.radius, a.from.x, a.from.y, a.from.z, a.to.x, a.to.y, a.to.z,
                b.from.x, b.from.y, b.from.z, b.to.x, b.to.y, b.to.z, x, y);
        }
	};
	for (long i = 0; i < iters; ++i) {
		// a scale per case, from tiny to world-sized, and degenerate cases on purpose
		const double scale = std::pow(10.0, -1.0 + 6.0 * u(g));
		auto         rnd = [&](double s) { return static_cast<float>((u(g) * 2 - 1) * s); };
		auto         pt = [&]() { const float x = rnd(scale), y = rnd(scale), z = rnd(scale); return Vec3{ x, y, z }; };
		Body         a, b;
		a.shape = Shape::kSphere;
		a.from = pt();
		a.to = u(g) < 0.1 ? a.from : a.from + Vec3{ rnd(scale), rnd(scale), rnd(scale) };
		a.radius = u(g) < 0.05 ? 0.0f : static_cast<float>(u(g) * scale * 0.3);
		const double kind = u(g);
		b.radius = u(g) < 0.05 ? 0.0f : static_cast<float>(u(g) * scale * 0.3);
		if (kind < 0.4) {
			b.shape = Shape::kSphere;
			b.from = u(g) < 0.05 ? a.from : pt();
			b.to = u(g) < 0.1 ? b.from : b.from + Vec3{ rnd(scale), rnd(scale), rnd(scale) };
		} else if (kind < 0.7) {
			b.shape = Shape::kBeam;
			b.from = pt();
			b.to = u(g) < 0.05 ? b.from : b.from + Vec3{ rnd(scale * 2), rnd(scale * 2), rnd(scale) };
		} else {
			b.shape = Shape::kBarrier;
			b.from = pt();
			const double ang = u(g) * 6.283185307;
			b.across = { static_cast<float>(std::cos(ang)), static_cast<float>(std::sin(ang)), 0.0f };
			b.halfWidth = static_cast<float>(scale * (0.01 + u(g)));
			b.height = static_cast<float>(scale * (0.01 + u(g)));
			b.to = b.from;
		}
		const bool swap = u(g) < 0.5;
		const Body& A = swap ? b : a;
		const Body& B = swap ? a : b;
		if (!Valid(A) || !Valid(B)) {
			continue;
		}
		++checked;
		Touch      t;
		const bool hit = FirstTouch(A, B, t);
		const double r = double(a.radius) + b.radius;
		const double tol = 2e-4 * (scale + r) + 1e-3;  // float storage of world-sized coordinates
		constexpr int N = 4000;
		double    refFirst = -1, minD = 1e300;
		for (int k = 0; k <= N; ++k) {
			const double d = Dist(A, B, double(k) / N);
			minD = std::min(minD, d);
			if (refFirst < 0 && d <= r - tol) {
				refFirst = double(k) / N;
			}
		}
		if (refFirst >= 0 && !hit) {
			report("reference touches, FirstTouch says no", A, B, refFirst, minD - r);
			continue;
		}
		if (hit) {
			++touched;
			if (!(t.t >= 0.0f && t.t <= 1.0f) || !std::isfinite(t.point.x) || !std::isfinite(t.point.y) || !std::isfinite(t.point.z)) {
				report("touch time or point out of range", A, B, t.t, 0);
				continue;
			}
			const bool staticPair = A.shape != Shape::kSphere && B.shape != Shape::kSphere;
			const double dAt = Dist(A, B, staticPair ? 0.0 : t.t);
			if (dAt > r + tol) {
				report("FirstTouch time is not touching", A, B, t.t, dAt - r);
				continue;
			}
			if (minD > r + tol) {
				report("FirstTouch says yes, reference never gets close", A, B, t.t, minD - r);
				continue;
			}
			if (refFirst >= 0 && t.t > refFirst + 1.0 / N + 1e-4) {
				report("FirstTouch is later than the first touch", A, B, t.t, refFirst);
				continue;
			}
		}
	}
	// Resolve: properties that must hold for any table, strengths and ratio
	std::uniform_int_distribution<int> el(0, int(kElements) - 1), act(0, 4);
	long                               rfails = 0;
	for (long i = 0; i < iters; ++i) {
		Table tb = DefaultTable();
		for (int k = 0; k < 4; ++k) {
			const auto e1 = static_cast<Element>(el(g)), e2 = static_cast<Element>(el(g));
			SetReaction(tb, e1, e2, static_cast<Action>(act(g)));
		}
		const auto  ea = static_cast<Element>(el(g)), eb = static_cast<Element>(el(g));
		const float sa = u(g) < 0.05 ? 0.0f : float(std::pow(10.0, 4 * u(g) - 1)), sb = u(g) < 0.05 ? 0.0f : float(std::pow(10.0, 4 * u(g) - 1));
		const float ratio = float(1 + 20 * u(g));
		const bool  ia = u(g) < 0.2, ib = u(g) < 0.2, weaken = u(g) < 0.5;
		const auto  o = Resolve(tb, ratio, weaken, ea, sa, ia, eb, sb, ib);
		const bool  ok = (!ia || (!o.killA && o.strengthA == sa)) && (!ib || (!o.killB && o.strengthB == sb)) &&
		                std::isfinite(o.strengthA) && std::isfinite(o.strengthB) && o.strengthA >= 0 && o.strengthB >= 0 &&
		                o.strengthA <= sa && o.strengthB <= sb && (o.happened || (!o.killA && !o.killB)) &&
		                (!o.killA || !o.killB || o.happened);
		if (!ok && ++rfails <= 5) {
			std::printf("RESOLVE FAIL: %d(%g%s) vs %d(%g%s) ratio %g weaken %d -> happened %d killA %d killB %d left %g %g\n", int(ea), sa,
				ia ? " immune" : "", int(eb), sb, ib ? " immune" : "", ratio, weaken, o.happened, o.killA, o.killB, o.strengthA, o.strengthB);
		}
	}
	std::printf("%ld valid pairs (%ld touching) checked against the reference, %ld failure(s); %ld Resolve cases, %ld failure(s)\n",
		checked, touched, fails, iters, rfails);
	return fails || rfails ? 1 : 0;
}
