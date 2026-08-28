#include "SaturationCurves.h"

#include <algorithm>
#include <cmath>

namespace papalote
{
namespace satcurves
{

namespace
{

constexpr std::array<std::array<float, numCoeffs>, 5> LumTable = {{
    { 0.000000f, 1.000035f, -0.000000f, -0.000000f, 0.000000f, -0.000000f, -0.000000f, 0.000000f },
    { 0.000000f, 1.139243f, 0.000017f, -0.144589f, -0.000006f, 0.029211f, 0.000000f, 0.001178f },
    { 0.000000f, 1.292887f, 0.000039f, -0.348116f, -0.000023f, 0.114284f, 0.000005f, -0.012771f },
    { 0.000000f, 1.462151f, 0.000066f, -0.618405f, -0.000051f, 0.267662f, 0.000014f, -0.047627f },
    { 0.000000f, 1.645529f, 0.000098f, -0.959458f, -0.000092f, 0.500261f, 0.000030f, -0.109095f }
}};

constexpr std::array<std::array<float, numCoeffs>, 4> IriTable = {{
    { -0.000000f, 1.127533f, 0.000014f, -0.111741f, -0.000005f, 0.018535f, 0.000000f, 0.000981f },
    { -0.000000f, 1.268255f, 0.000033f, -0.268812f, -0.000017f, 0.072853f, 0.000003f, -0.005936f },
    { -0.000000f, 1.423334f, 0.000056f, -0.477712f, -0.000038f, 0.172041f, 0.000009f, -0.024520f },
    { -0.000000f, 1.591453f, 0.000084f, -0.742114f, -0.000068f, 0.324487f, 0.000020f, -0.058677f }
}};

constexpr std::array<std::array<float, numCoeffs>, 4> RadTable = {{
    { -0.000001f, 1.150743f, 0.000019f, -0.045944f, 0.000016f, -0.428443f, -0.000029f, 0.324271f },
    { -0.000002f, 1.313470f, 0.000050f, -0.169135f, -0.000002f, -0.753648f, -0.000035f, 0.610960f },
    { -0.000004f, 1.490019f, 0.000093f, -0.379063f, -0.000054f, -0.958440f, -0.000018f, 0.850554f },
    { -0.000006f, 1.679314f, 0.000145f, -0.681950f, -0.000136f, -1.021807f, 0.000020f, 1.029260f }
}};

constexpr std::array<std::array<float, numCoeffs>, 4> DarTable = {{
    { -0.000004f, 1.235713f, 0.000126f, -0.243462f, -0.000436f, -0.339512f, 0.000436f, 0.349528f },
    { 0.000005f, 1.547768f, -0.000067f, -1.058449f, 0.000355f, 0.229530f, -0.000241f, 0.286550f },
    { 0.000056f, 1.937158f, -0.001244f, -2.555473f, 0.004213f, 2.033713f, -0.002991f, -0.408818f },
    { 0.000081f, 2.382488f, -0.002208f, -4.672112f, 0.008542f, 5.084883f, -0.006535f, -1.792806f }
}};

constexpr std::array<std::array<float, numCoeffs>, 4> LusTable = {{
    { -0.000002f, 1.266302f, 0.000044f, -0.087298f, 0.000008f, -0.824493f, -0.000040f, 0.634762f },
    { -0.000008f, 1.637986f, 0.000196f, -0.890570f, -0.000299f, -0.661759f, 0.000136f, 0.898743f },
    { 0.000000f, 2.228301f, -0.000056f, -3.587114f, 0.001038f, 2.973352f, -0.000983f, -0.642225f },
    { -0.000009f, 2.935392f, 0.000457f, -7.812387f, -0.000909f, 9.919794f, 0.000496f, -4.104297f }
}};

// ── Per-type table accessors ───────────────────────────────────────────────

struct TypeInfo
{
    const float* ptr;
    int levels;
};

TypeInfo typeInfo(int type) noexcept
{
    switch (type)
    {
        case 0:  return { LumTable[0].data(), 5 };
        case 1:  return { IriTable[0].data(), 4 };
        case 2:  return { RadTable[0].data(), 4 };
        case 3:  return { DarTable[0].data(), 4 };
        case 4:  return { LusTable[0].data(), 4 };
        default: return { LumTable[0].data(), 5 };
    }
}

// Stride between consecutive drive levels in a table (numCoeffs floats).
inline constexpr int kStride = numCoeffs;

} // anonymous namespace

// ── Public accessors ───────────────────────────────────────────────────────

int numDriveLevels(int type) noexcept
{
    return typeInfo(type).levels;
}

const std::array<float, numCoeffs>& driveCoeffs(int type, int level) noexcept
{
    const auto info = typeInfo(type);
    const int clamped = std::clamp(level, 0, info.levels - 1);
    return *reinterpret_cast<const std::array<float, numCoeffs>*>(
        info.ptr + clamped * kStride);
}

// ── Polynomial evaluation (standard form) ──────────────────────────────────
// P(x) = c0 + c1*x + c2*x^2 + ... + c7*x^7

double polyRaw(const std::array<float, numCoeffs>& c, double x) noexcept
{
    double a = 0.0;
    for (int i = numCoeffs; i-- > 0;)
        a = a * x + (double) c[(size_t) i];
    return a;
}

// ── Antiderivative F(x) = c0*x + c1*x^2/2 + ... + c7*x^8/8 ──────────────

double antiDerivRaw(const std::array<float, numCoeffs>& c, double x) noexcept
{
    // Horner for the inner sum, then multiply by x:
    // inner = c7*x^6 + c6*x^5 + ... + c1
    // F(x) = inner * x^2 + c0*x = (c7*x^7 + c6*x^6 + ... + c0) * x
    double a = 0.0;
    for (int i = numCoeffs; i-- > 0;)
        a = a * x + (double) c[(size_t) i] / (double) (i + 1);
    return a * x;
}

// ── Interpolated coefficient lookup ────────────────────────────────────────

void interpolatedCoeffs(int type, float driveNorm,
                        std::array<float, numCoeffs>& out) noexcept
{
    const auto info = typeInfo(type);
    const int n = info.levels;

    const float idx = driveNorm * (float) (n - 1);
    const int lo = (int) idx;
    const int hi = std::min(lo + 1, n - 1);
    const float frac = idx - (float) lo;

    const auto& cLo = driveCoeffs(type, lo);
    const auto& cHi = driveCoeffs(type, hi);

    for (int i = 0; i < numCoeffs; ++i)
        out[(size_t) i] = cLo[(size_t) i]
                        + frac * (cHi[(size_t) i] - cLo[(size_t) i]);
}

// ── Naive (non-ADAA) evaluation with drive interpolation ──────────────────

double naive(int type, double x, float driveNorm) noexcept
{
    x = std::clamp(x, -1.0, 1.0);
    std::array<float, numCoeffs> c {};
    interpolatedCoeffs(type, driveNorm, c);
    return polyRaw(c, x);
}

// ── ADAA1 process ──────────────────────────────────────────────────────────

float process(int type, double x, AdaaState& s, float driveNorm) noexcept
{
    x = std::clamp(x, -1.0, 1.0);

    const double xp = s.xPrev;
    s.xPrev = x;

    const double dx = x - xp;

    // Build the interpolated polynomial
    std::array<float, numCoeffs> c {};
    interpolatedCoeffs(type, driveNorm, c);

    // Below ~1e-6 the quotient is dominated by rounding noise; fall back to
    // the naive midpoint.
    if (! (std::fabs(dx) > 1e-6))
        return (float) polyRaw(c, 0.5 * (x + xp));

    const double Fx  = antiDerivRaw(c, x);
    const double Fxp = antiDerivRaw(c, xp);

    return (float) ((Fx - Fxp) / dx);
}

}
}
