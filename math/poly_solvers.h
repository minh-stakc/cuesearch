#pragma once
// Real-root solvers for the event-time polynomials of the simulator.
// Ball–ball collision time => quartic; ball–cushion => quadratic.
// Analytic (Schwarze / Graphics Gems) + Newton polish for FP robustness.
// Correctness-critical: validated by tests/test_poly_solvers.cpp.
#include <array>
#include <vector>

namespace cue::poly {

// Real roots of a*x^2 + b*x + c.
std::vector<double> quadratic(double a, double b, double c);

// Real roots of a*x^3 + b*x^2 + c*x + d.
std::vector<double> cubic(double a, double b, double c, double d);

// Real roots of a*x^4 + b*x^3 + c*x^2 + d*x + e.
std::vector<double> quartic(double a, double b, double c, double d, double e);

// Smallest real root strictly in (lo, hi] of the quartic whose coefficients
// are coeff = {a4, a3, a2, a1, a0} (lower-degree handled if leading ~0).
// Returns a quiet NaN if there is no such root. Roots are Newton-polished
// against the original polynomial.
double smallestRootIn(const std::array<double, 5>& coeff, double lo, double hi);

}  // namespace cue::poly
