#include "math/poly_solvers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using cue::poly::cubic;
using cue::poly::quadratic;
using cue::poly::quartic;
using cue::poly::smallestRootIn;

namespace {

// Every expected root must be approximated by some returned root (extra
// spurious roots and multiplicity collapse are tolerated; correctness here
// means "no true root is missed").
bool containsAll(std::vector<double> got, std::vector<double> expected,
                 double tol = 1e-6) {
    for (double e : expected) {
        bool found = false;
        for (double g : got)
            if (std::fabs(g - e) < tol) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

}  // namespace

TEST_CASE("quadratic: distinct, double, none") {
    REQUIRE(containsAll(quadratic(1, -3, 2), {1.0, 2.0}));   // (x-1)(x-2)
    REQUIRE(containsAll(quadratic(1, -6, 9), {3.0}));        // (x-3)^2
    REQUIRE(quadratic(1, 0, 1).empty());                     // x^2+1
}

TEST_CASE("cubic: three real and one real") {
    REQUIRE(containsAll(cubic(1, -6, 11, -6), {1.0, 2.0, 3.0}));
    auto r = cubic(1, 0, 1, 1);                               // x^3+x+1
    REQUIRE(r.size() == 1);
    REQUIRE(r[0] == Catch::Approx(-0.6823278).margin(1e-5));
}

// --- The three required quartic cases ---------------------------------------

TEST_CASE("quartic: four distinct real roots") {
    // (x-1)(x-2)(x-3)(x-4) = x^4 -10x^3 +35x^2 -50x +24
    REQUIRE(containsAll(quartic(1, -10, 35, -50, 24),
                        {1.0, 2.0, 3.0, 4.0}));
}

TEST_CASE("quartic: double root") {
    // (x-2)^2 (x-5)(x+3) = x^4 -6x^3 -3x^2 +52x -60
    REQUIRE(containsAll(quartic(1, -6, -3, 52, -60),
                        {2.0, 5.0, -3.0}));
}

TEST_CASE("quartic: complex pair + two real") {
    // (x^2+1)(x-3)(x+4) = x^4 + x^3 -11x^2 + x -12  -> real roots {3,-4}
    REQUIRE(containsAll(quartic(1, 1, -11, 1, -12), {3.0, -4.0}));
    // (x^2+1)(x^2+4) -> no real roots
    REQUIRE(quartic(1, 0, 5, 0, 4).empty());
}

// --- Randomized round-trip property test ------------------------------------

TEST_CASE("quartic: randomized round-trip") {
    std::mt19937 rng(12345);                       // fixed seed: deterministic
    std::uniform_real_distribution<double> dist(-10.0, 10.0);

    const auto P = [](double x, double b, double c, double d, double e) {
        return (((x + b) * x + c) * x + d) * x + e;
    };

    int separatedTested = 0;
    for (int iter = 0; iter < 2000; ++iter) {
        double r[4] = {dist(rng), dist(rng), dist(rng), dist(rng)};
        std::sort(r, r + 4);
        double r1 = r[0], r2 = r[1], r3 = r[2], r4 = r[3];

        // Monic quartic from elementary symmetric polynomials of the roots.
        double b = -(r1 + r2 + r3 + r4);
        double c = r1*r2 + r1*r3 + r1*r4 + r2*r3 + r2*r4 + r3*r4;
        double d = -(r1*r2*r3 + r1*r2*r4 + r1*r3*r4 + r2*r3*r4);
        double e = r1*r2*r3*r4;

        auto got = quartic(1, b, c, d, e);

        // (1) Conditioning-free correctness: every returned root is an
        //     actual root of the polynomial (residual ~ 0). This must hold
        //     unconditionally — it catches genuine solver bugs.
        for (double g : got) {
            double mag = 1.0 + std::fabs(g);
            REQUIRE(std::fabs(P(g, b, c, d, e)) < 1e-6 * mag * mag * mag);
        }

        // (2) Recovery of planted roots, asserted only where the
        //     coefficient->root map is well-conditioned (min gap > 1e-2).
        //     Clustered roots are Wilkinson-ill-conditioned: not recoverable
        //     to 1e-4 in double precision regardless of algorithm. The
        //     degenerate regime is covered by the explicit double-root case.
        double minGap = std::min({r2 - r1, r3 - r2, r4 - r3});
        if (minGap > 1e-2) {
            REQUIRE(containsAll(got, {r1, r2, r3, r4}, 1e-4));
            ++separatedTested;
        }
    }
    REQUIRE(separatedTested > 1500);   // the guard must not gut the test
}

// --- smallestRootIn window semantics ----------------------------------------

TEST_CASE("smallestRootIn picks the first root in the window") {
    // (x-1)(x-2)(x-3)(x-4) -> roots 1,2,3,4
    std::array<double, 5> q{1, -10, 35, -50, 24};

    REQUIRE(smallestRootIn(q, 0.0, 10.0) == Catch::Approx(1.0).margin(1e-9));
    REQUIRE(smallestRootIn(q, 1.5, 10.0) == Catch::Approx(2.0).margin(1e-9));
    REQUIRE(smallestRootIn(q, 3.5, 10.0) == Catch::Approx(4.0).margin(1e-9));
    REQUIRE(std::isnan(smallestRootIn(q, 4.5, 10.0)));   // none left
}

TEST_CASE("smallestRootIn degenerate leading coeff falls back to cubic") {
    // 0*x^4 + (x-1)(x-2)(x-3) = x^3 -6x^2 +11x -6
    std::array<double, 5> q{0, 1, -6, 11, -6};
    REQUIRE(smallestRootIn(q, 0.0, 10.0) == Catch::Approx(1.0).margin(1e-9));
}
