#pragma once

#include <array>

// JUCE-free ADAA1 (anti-derivative anti-aliasing, 1st order) engine for the
// polynomial saturation curves. Each waveshaper type stores a multi-level
// drive table of 8-coefficient polynomials (standard form: c0 + c1*x + ...
// + c7*x^7). The Drive knob interpolates between levels; ADAA evaluates the
// antiderivative in closed form. All math runs in double; results are cast
// to float on return.

namespace papalote
{
namespace satcurves
{

inline constexpr int numTypes = 5;
inline constexpr int numCoeffs = 8;
inline constexpr int maxDriveLevels = 5;

struct AdaaState
{
    double xPrev = 0.0;
};

inline void reset(AdaaState& s) noexcept { s.xPrev = 0.0; }

int numDriveLevels(int type) noexcept;

const std::array<float, numCoeffs>& driveCoeffs(int type, int level) noexcept;

double naive(int type, double x, float driveNorm) noexcept;

float process(int type, double x, AdaaState& s, float driveNorm) noexcept;

}
}
