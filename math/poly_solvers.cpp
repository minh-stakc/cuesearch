#include "math/poly_solvers.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cue::poly {
namespace {

constexpr double kEps = 1e-12;
constexpr double kPi = 3.14159265358979323846;

double clampUnit(double x) { return x < -1.0 ? -1.0 : (x > 1.0 ? 1.0 : x); }

}  // namespace

std::vector<double> quadratic(double a, double b, double c) {
    std::vector<double> out;
    if (std::fabs(a) < kEps) {                 // degenerate -> linear
        if (std::fabs(b) >= kEps) out.push_back(-c / b);
        return out;
    }
    const double disc = b * b - 4.0 * a * c;
    if (disc < -kEps) return out;
    if (disc <= kEps) {
        out.push_back(-b / (2.0 * a));
        return out;
    }
    // Numerically stable Citardauq form.
    const double s = std::sqrt(disc);
    const double q = -0.5 * (b + std::copysign(s, b));
    out.push_back(q / a);
    if (std::fabs(q) >= kEps) out.push_back(c / q);
    return out;
}

std::vector<double> cubic(double a, double b, double c, double d) {
    if (std::fabs(a) < kEps) return quadratic(b, c, d);

    // Normalize to x^3 + A x^2 + B x + C, then depress with x = t - A/3.
    const double A = b / a, B = c / a, C = d / a;
    const double p = B - A * A / 3.0;
    const double q = 2.0 * A * A * A / 27.0 - A * B / 3.0 + C;
    const double shift = A / 3.0;

    std::vector<double> t;
    const double disc = q * q / 4.0 + p * p * p / 27.0;

    if (disc > kEps) {                          // one real root
        const double s = std::sqrt(disc);
        t.push_back(std::cbrt(-q / 2.0 + s) + std::cbrt(-q / 2.0 - s));
    } else if (disc >= -kEps) {                 // multiple real roots
        if (std::fabs(p) < kEps && std::fabs(q) < kEps) {
            t.push_back(0.0);                   // triple root
        } else {
            const double u = std::cbrt(-q / 2.0);
            t.push_back(2.0 * u);               // double + single
            t.push_back(-u);
        }
    } else {                                    // three distinct real roots
        const double m = 2.0 * std::sqrt(-p / 3.0);
        const double theta =
            std::acos(clampUnit(3.0 * q / (p * m))) / 3.0;
        const double k = 2.0 * kPi / 3.0;
        t.push_back(m * std::cos(theta));
        t.push_back(m * std::cos(theta - k));
        t.push_back(m * std::cos(theta - 2.0 * k));
    }

    std::vector<double> out;
    out.reserve(t.size());
    for (double ti : t) out.push_back(ti - shift);
    return out;
}

std::vector<double> quartic(double a, double b, double c, double d, double e) {
    if (std::fabs(a) < kEps) return cubic(b, c, d, e);

    // Normalize to x^4 + A x^3 + B x^2 + C x + D, depress with x = y - A/4.
    const double A = b / a, B = c / a, C = d / a, D = e / a;
    const double shift = A / 4.0;
    const double p = B - 3.0 * A * A / 8.0;
    const double q = A * A * A / 8.0 - A * B / 2.0 + C;
    const double r =
        -3.0 * A * A * A * A / 256.0 + A * A * B / 16.0 - A * C / 4.0 + D;

    std::vector<double> ys;

    if (std::fabs(r) < kEps) {                   // y * (y^3 + p y + q) = 0
        ys.push_back(0.0);
        for (double y : cubic(1.0, 0.0, p, q)) ys.push_back(y);
    } else {
        // Resolvent cubic: z^3 - (p/2) z^2 - r z + (p r / 2 - q^2 / 8) = 0.
        std::vector<double> zr =
            cubic(1.0, -0.5 * p, -r, 0.5 * r * p - 0.125 * q * q);
        if (zr.empty()) return {};
        const double z = zr.front();

        double u = z * z - r;
        double v = 2.0 * z - p;
        if (std::fabs(u) < kEps) u = 0.0;
        else if (u > 0.0)        u = std::sqrt(u);
        else                     return {};       // no real factorization
        if (std::fabs(v) < kEps) v = 0.0;
        else if (v > 0.0)        v = std::sqrt(v);
        else                     return {};

        // Two quadratics; linear-coeff sign chosen by sign(q) so the
        // factorization reconstructs the original +q term.
        const double sgn = (q < 0.0) ? -1.0 : 1.0;
        for (double y : quadratic(1.0, sgn * v, z - u)) ys.push_back(y);
        for (double y : quadratic(1.0, -sgn * v, z + u)) ys.push_back(y);
    }

    const auto f = [&](double x) {
        return (((a * x + b) * x + c) * x + d) * x + e;
    };
    const auto df = [&](double x) {
        return ((4.0 * a * x + 3.0 * b) * x + 2.0 * c) * x + d;
    };
    const auto polish = [&](double x) {
        for (int i = 0; i < 16; ++i) {
            const double fp = df(x);
            if (std::fabs(fp) < 1e-300) break;
            const double step = f(x) / fp;
            x -= step;
            if (std::fabs(step) <= 1e-15 * (1.0 + std::fabs(x))) break;
        }
        return x;
    };

    // Polish the analytic seeds against the ORIGINAL quartic.
    std::vector<double> seeds;
    seeds.reserve(ys.size());
    for (double y : ys) seeds.push_back(polish(y - shift));
    if (seeds.empty()) return {};

    // Coverage guarantee: analytic seeds can collide (two -> same root),
    // dropping a well-separated root. Take the best real root, synthetically
    // deflate it out, solve the remaining cubic, and polish those against the
    // ORIGINAL polynomial. This recovers any root the seeds missed.
    double rb = seeds.front();
    for (double s : seeds)
        if (std::fabs(f(s)) < std::fabs(f(rb))) rb = s;

    const double q3 = a;
    const double q2 = b + q3 * rb;
    const double q1 = c + q2 * rb;
    const double q0 = d + q1 * rb;

    std::vector<double> out;
    out.push_back(rb);
    for (double r : cubic(q3, q2, q1, q0)) out.push_back(polish(r));
    return out;
}

double smallestRootIn(const std::array<double, 5>& coeff, double lo,
                      double hi) {
    const auto eval = [&](double x) {
        return ((((coeff[0]) * x + coeff[1]) * x + coeff[2]) * x + coeff[3]) *
                   x +
               coeff[4];
    };
    const auto deriv = [&](double x) {
        return (((4.0 * coeff[0]) * x + 3.0 * coeff[1]) * x + 2.0 * coeff[2]) *
                   x +
               coeff[3];
    };

    std::vector<double> roots =
        quartic(coeff[0], coeff[1], coeff[2], coeff[3], coeff[4]);

    double best = std::numeric_limits<double>::quiet_NaN();
    for (double x : roots) {
        // Newton polish (a few steps) against the original polynomial.
        for (int i = 0; i < 8; ++i) {
            const double f = eval(x);
            const double fp = deriv(x);
            if (std::fabs(fp) < 1e-300) break;
            const double step = f / fp;
            x -= step;
            if (std::fabs(step) < 1e-15) break;
        }
        if (x > lo + 1e-12 && x <= hi) {
            if (std::isnan(best) || x < best) best = x;
        }
    }
    return best;
}

}  // namespace cue::poly
