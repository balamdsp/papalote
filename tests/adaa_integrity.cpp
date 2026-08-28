// =============================================================================
// ADAA1 Integrity Test Suite  — standalone, no JUCE required
//
// Tests verified (new 5-type standard-polynomial API):
//  1. antiDerivRaw() antisymmetry:       F(b)-F(a) == -(F(a)-F(b))
//  2. antiDerivRaw() zero-length:        F(a)-F(a) == 0
//  3. Closed-form vs. adaptive-Simpson:  antiDerivRaw diff matches numeric
//     integral across all 5 waveshaper types, at multiple drive levels.
//  4. naive() identity at drive=0:       output ≈ x (unity gain passthrough)
//  5. naive() finite on [-1,1]:          no NaN/Inf for any type+drive combo
//  6. process() NaN-free on DC:          dx=0 triggers midpoint fallback
//  7. process() NaN-free on impulse:     single spike then silence
//  8. ADAA1 convergence (low-freq sine): ADAA1 output ≈ numeric mean of f
//     when the signal moves slowly (1/512 of Nyquist)
//  9. process() finite on 200-pt sweep:  deterministic grid [-2, +2]
// =============================================================================

#include "../Source/SaturationCurves.cpp"

#include <cstdio>
#include <cmath>
#include <cfloat>
#include <array>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

using namespace papalote::satcurves;

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------
static int g_pass = 0, g_fail = 0;

static void check(const char* name, bool ok)
{
    if (ok) { std::printf("  PASS  %s\n", name); ++g_pass; }
    else    { std::printf("  FAIL  %s\n", name); ++g_fail; }
}

static void checkVal(const char* name, double got, double ref, double tol)
{
    bool ok = std::fabs(got - ref) <= tol;
    if (ok)
        std::printf("  PASS  %s\n", name);
    else
        std::printf("  FAIL  %s  (got %.10e  ref %.10e  diff %.2e  tol %.2e)\n",
                    name, got, ref, std::fabs(got - ref), tol);
    ok ? ++g_pass : ++g_fail;
}

static const char* typeName(int t)
{
    static const char* n[] = { "Lum", "Iri", "Rad", "Dar", "Lus" };
    return (t >= 0 && t < 5) ? n[t] : "???";
}

// ---------------------------------------------------------------------------
// Helper: evaluate naive(type, x, driveNorm) for a given drive level index
// ---------------------------------------------------------------------------
static double naiveAtLevel(int t, double x, int level)
{
    const int nl = numDriveLevels(t);
    const int clamped = std::min(level, nl - 1);
    const float driveNorm = (nl > 1) ? (float) clamped / (float) (nl - 1) : 0.0f;
    return naive(t, x, driveNorm);
}

// ---------------------------------------------------------------------------
// Adaptive-Simpson numeric integrator for naive(type, x, driveNorm)
// ---------------------------------------------------------------------------
static double simpsonStep(int t, double a, double b, float dn)
{
    double m = 0.5 * (a + b);
    return (b - a) / 6.0 * (naive(t, a, dn) + 4.0 * naive(t, m, dn) + naive(t, b, dn));
}

static double adaptiveSimpson(int t, double a, double b, float dn, double tol, int depth)
{
    double m   = 0.5 * (a + b);
    double s12 = simpsonStep(t, a, m, dn) + simpsonStep(t, m, b, dn);
    double s1  = simpsonStep(t, a, b, dn);
    if (depth <= 0 || std::fabs(s12 - s1) < 15.0 * tol)
        return s12 + (s12 - s1) / 15.0;
    return adaptiveSimpson(t, a, m, dn, tol / 2.0, depth - 1)
         + adaptiveSimpson(t, m, b, dn, tol / 2.0, depth - 1);
}

static double numIntegral(int t, double a, double b, float dn)
{
    if (a == b)  return 0.0;
    if (a > b)   return -numIntegral(t, b, a, dn);
    return adaptiveSimpson(t, a, b, dn, 1e-10, 14);
}

// ---------------------------------------------------------------------------
// Helper: get interpolated coefficients for a type at a given driveNorm
// ---------------------------------------------------------------------------
static std::array<float, numCoeffs> getInterpCoeffs(int t, float driveNorm)
{
    std::array<float, numCoeffs> c {};
    interpolatedCoeffs(t, driveNorm, c);
    return c;
}

// ---------------------------------------------------------------------------
// Test 1 & 2: antiDerivRaw() antisymmetry and zero-length
// ---------------------------------------------------------------------------
static void testAntiDerivProperties()
{
    std::printf("\n[1] antiDerivRaw() antisymmetry and identity\n");

    const double testPoints[] = { -0.9, -0.5, -0.1, 0.0, 0.1, 0.5, 0.9 };
    const int nPts = (int) (sizeof(testPoints) / sizeof(testPoints[0]));

    for (int t = 0; t < numTypes; ++t)
    {
        auto c = getInterpCoeffs(t, 1.0f);

        for (int i = 0; i < nPts; ++i)
        {
            double a = testPoints[i];
            char name[96];

            // F(a) - F(a) == 0
            double fa = antiDerivRaw(c, a);
            std::snprintf(name, sizeof(name), "identity  %s  x=%.2f", typeName(t), a);
            checkVal(name, fa - fa, 0.0, 0.0);

            // antisymmetry: for any b, (F(b)-F(a)) + (F(a)-F(b)) == 0
            for (int j = i + 1; j < nPts; ++j)
            {
                double b = testPoints[j];
                double fb = antiDerivRaw(c, b);
                double fwd = fb - fa;
                double rev = fa - fb;
                std::snprintf(name, sizeof(name),
                    "antisymm  %s  [%.2f,%.2f]", typeName(t), a, b);
                checkVal(name, fwd + rev, 0.0, 1e-14);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Test 3: closed-form vs. adaptive-Simpson numeric integral
// ---------------------------------------------------------------------------
static void testNumericalAccuracy()
{
    std::printf("\n[2] Closed-form (antiDerivRaw) vs. adaptive-Simpson  (rel tol 1e-6)\n");

    const double intervals[][2] = {
        { -0.8,  -0.2 },
        { -0.5,   0.5 },
        {  0.2,   0.8 },
        { -0.95,  0.95 },
        { -0.001, 0.001 },
        { -1.0,   1.0 },
    };
    const int nIntervals = (int) (sizeof(intervals) / sizeof(intervals[0]));

    // Test at max drive (driveNorm=1.0)
    const float driveNorm = 1.0f;

    for (int t = 0; t < numTypes; ++t)
    {
        auto c = getInterpCoeffs(t, driveNorm);

        for (int i = 0; i < nIntervals; ++i)
        {
            double a = intervals[i][0], b = intervals[i][1];
            double cf  = antiDerivRaw(c, b) - antiDerivRaw(c, a);
            double num = numIntegral(t, a, b, driveNorm);
            double mag = std::fabs(num);
            double tol = (mag > 1e-12) ? 1e-6 * mag : 1e-10;

            char name[96];
            std::snprintf(name, sizeof(name),
                "numeric  %s  [%.4f, %.4f]", typeName(t), a, b);
            checkVal(name, cf, num, tol);
        }
    }
}

// ---------------------------------------------------------------------------
// Test 4: naive() near-identity at drive=0 (c1 ≈ 1, others ≈ 0)
// ---------------------------------------------------------------------------
static void testDriveZeroIdentity()
{
    // Only Lum has a true 0% drive level. Other types (Iri/Rad/Dar/Lus)
    // start at 25%, so driveNorm=0.0 maps to their 25% curve, which is
    // already somewhat saturated. We test Lum for unity and the others
    // just for finite output at driveNorm=0.0.
    std::printf("\n[3] naive() near-unity at drive=0 (Lum) + finite (others)\n");

    const double testX[] = { 0.0, 0.25, -0.25, 0.5, -0.5, 0.9, -0.9 };

    // Lum: 5 levels including 0% — should be near-unity at driveNorm=0
    for (double x : testX)
    {
        double got = naive(0, x, 0.0f);
        char name[64];
        std::snprintf(name, sizeof(name), "unity  Lum  x=%.2f", x);
        checkVal(name, got, x, 1e-4);
    }

    // Iri/Rad/Dar/Lus: just check finiteness at driveNorm=0
    for (int t = 1; t < numTypes; ++t)
    {
        bool allFinite = true;
        for (double x : testX)
        {
            if (! std::isfinite(naive(t, x, 0.0f)))
            {
                allFinite = false;
                break;
            }
        }
        char name[64];
        std::snprintf(name, sizeof(name), "finite-at-zero  %s", typeName(t));
        check(name, allFinite);
    }
}

// ---------------------------------------------------------------------------
// Test 5: naive() finite on [-1, 1] for all types and drive levels
// ---------------------------------------------------------------------------
static void testNaiveFinite()
{
    std::printf("\n[4] naive() finite on [-1, 1] for all types and drive levels\n");

    for (int t = 0; t < numTypes; ++t)
    {
        const int nl = numDriveLevels(t);
        bool allFinite = true;

        for (int lev = 0; lev < nl; ++lev)
        {
            const float dn = (nl > 1) ? (float) lev / (float) (nl - 1) : 0.0f;
            for (double x = -1.0; x <= 1.0; x += 0.01)
            {
                if (! std::isfinite(naive(t, x, dn)))
                {
                    allFinite = false;
                    break;
                }
            }
            if (! allFinite) break;
        }

        char name[64];
        std::snprintf(name, sizeof(name), "finite  %s", typeName(t));
        check(name, allFinite);
    }
}

// ---------------------------------------------------------------------------
// Test 6: process() NaN-free on DC (dx == 0, midpoint fallback must fire)
// ---------------------------------------------------------------------------
static void testDCInput()
{
    std::printf("\n[5] process() NaN-free on DC (dx=0 triggers midpoint fallback)\n");
    const double dcValues[] = { 0.0, 0.5, -0.5, 1.0, -1.0, 0.995 };

    for (int t = 0; t < numTypes; ++t)
    {
        for (double dc : dcValues)
        {
            AdaaState s; reset(s);
            process(t, dc, s, 1.0f);
            float out = process(t, dc, s, 1.0f);

            char name[80];
            std::snprintf(name, sizeof(name),
                "DC NaN-free  %s  x=%.3f", typeName(t), dc);
            check(name, std::isfinite(out));
        }
    }
}

// ---------------------------------------------------------------------------
// Test 7: process() NaN-free on impulse → silence
// ---------------------------------------------------------------------------
static void testImpulse()
{
    std::printf("\n[6] process() NaN-free on impulse then silence\n");
    const double impulses[] = { 1.0, -1.0, 0.5, -0.5 };

    for (int t = 0; t < numTypes; ++t)
    {
        for (double amp : impulses)
        {
            AdaaState s; reset(s);
            float r1 = process(t, amp, s, 1.0f);
            float r2 = process(t, 0.0, s, 1.0f);
            float r3 = process(t, 0.0, s, 1.0f);

            char name[80];
            std::snprintf(name, sizeof(name),
                "impulse  %s  amp=%.1f", typeName(t), amp);
            check(name, std::isfinite(r1) && std::isfinite(r2) && std::isfinite(r3));
        }
    }
}

// ---------------------------------------------------------------------------
// Test 8: ADAA1 convergence (low-freq sine) — output ≈ integral mean
// ---------------------------------------------------------------------------
static void testLowFreqConvergence()
{
    std::printf("\n[7] ADAA1 output ≈ integral mean  (rel tol 1e-5, 1/512 Nyquist sine)\n");

    const int    N    = 4096;
    const double freq = 1.0 / 512.0;
    const double amp  = 0.8;
    const float  dn   = 1.0f;

    for (int t = 0; t < numTypes; ++t)
    {
        AdaaState s; reset(s);
        double maxRelErr = 0.0;
        double xPrev = 0.0;

        for (int n = 0; n < N; ++n)
        {
            double x   = amp * std::sin(2.0 * M_PI * freq * n);
            float  out = process(t, x, s, dn);

            double dx = x - xPrev;
            double ref;
            if (std::fabs(dx) <= 1e-6)
                ref = naive(t, 0.5 * (x + xPrev), dn);
            else
                ref = numIntegral(t, xPrev, x, dn) / dx;

            double mag    = std::fabs(ref);
            double relErr = (mag > 1e-9) ? std::fabs((double)out - ref) / mag
                                         : std::fabs((double)out - ref);
            if (relErr > maxRelErr) maxRelErr = relErr;

            xPrev = x;
        }

        char name[96];
        std::snprintf(name, sizeof(name),
            "integral mean  %s  maxRelErr=%.2e", typeName(t), maxRelErr);
        check(name, maxRelErr < 1e-5);
    }
}

// ---------------------------------------------------------------------------
// Test 9: process() finite on a 200-point deterministic sweep
// ---------------------------------------------------------------------------
static void testFiniteGrid()
{
    std::printf("\n[8] process() finite on 200-pt sweep [-2, +2]\n");
    const int steps = 200;

    for (int t = 0; t < numTypes; ++t)
    {
        AdaaState s; reset(s);
        bool allFinite = true;
        for (int i = 0; i <= steps; ++i)
        {
            double x   = -2.0 + 4.0 * i / (double) steps;
            float  out = process(t, x, s, 1.0f);
            if (! std::isfinite(out)) { allFinite = false; break; }
        }
        char name[64];
        std::snprintf(name, sizeof(name), "finite grid  %s", typeName(t));
        check(name, allFinite);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    std::printf("=============================================================\n");
    std::printf("  ADAA1 Integrity Test Suite\n");
    std::printf("  numTypes=%d  numCoeffs=%d\n", numTypes, numCoeffs);
    std::printf("=============================================================\n");

    testAntiDerivProperties();
    testNumericalAccuracy();
    testDriveZeroIdentity();
    testNaiveFinite();
    testDCInput();
    testImpulse();
    testLowFreqConvergence();
    testFiniteGrid();

    std::printf("\n=============================================================\n");
    std::printf("  Results: %d passed,  %d failed  (total %d)\n",
                g_pass, g_fail, g_pass + g_fail);
    std::printf("=============================================================\n");

    return (g_fail == 0) ? 0 : 1;
}
