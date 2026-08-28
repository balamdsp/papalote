#include "TapeEmulation.h"
#include <algorithm>
#include <cmath>

// ── Material → waveshaper type mapping ──────────────────────────────────────
static const TapeEmulation::TapeModeParameters s_tapeModes[5] =
    {
        {0.4375f, 0.3125f, 0.75f, 0.3125f, 0.0625f, 1, 0},
        {0.375f, 0.3125f, 0.875f, 0.3125f, 0.0625f, 1, 1},
        {0.45629901f, 0.375f, 0.75f, 0.1875f, 0.0125f, 0, 2},
        {0.45629901f, 0.375f, 0.6875f, 0.27343899f, 0.1171875f, 0, 3},
        {0.45629901f, 0.375f, 0.75f, 0.5625f, 0.0125f, 0, 4}};

TapeEmulation::TapeEmulation()
{
}

TapeEmulation::~TapeEmulation() {}

// ── Audio EQ Cookbook — low shelf ──────────────────────────────────────────

void TapeEmulation::computeLowShelf(Biquad& bq, double fc, double gainDb,
                                    double Q, double fs)
{
    const double A  = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
    const double sn = std::sin(w0);
    const double cs = std::cos(w0);
    const double alpha = sn / (2.0 * Q);
    const double sqrtA = std::sqrt(A);

    const double b0 =      A * ((A + 1.0) - (A - 1.0) * cs + 2.0 * sqrtA * alpha);
    const double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs);
    const double b2 =      A * ((A + 1.0) - (A - 1.0) * cs - 2.0 * sqrtA * alpha);
    const double a0 =            (A + 1.0) + (A - 1.0) * cs + 2.0 * sqrtA * alpha;
    const double a1 =     -2.0 * ((A - 1.0) + (A + 1.0) * cs);
    const double a2 =            (A + 1.0) + (A - 1.0) * cs - 2.0 * sqrtA * alpha;

    bq.b0 = b0 / a0;
    bq.b1 = b1 / a0;
    bq.b2 = b2 / a0;
    bq.a1 = a1 / a0;
    bq.a2 = a2 / a0;
}

// ── Audio EQ Cookbook — high shelf ─────────────────────────────────────────

void TapeEmulation::computeHighShelf(Biquad& bq, double fc, double gainDb,
                                     double Q, double fs)
{
    const double A  = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
    const double sn = std::sin(w0);
    const double cs = std::cos(w0);
    const double alpha = sn / (2.0 * Q);
    const double sqrtA = std::sqrt(A);

    const double b0 =      A * ((A + 1.0) + (A - 1.0) * cs + 2.0 * sqrtA * alpha);
    const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
    const double b2 =      A * ((A + 1.0) + (A - 1.0) * cs - 2.0 * sqrtA * alpha);
    const double a0 =            (A + 1.0) - (A - 1.0) * cs + 2.0 * sqrtA * alpha;
    const double a1 =      2.0 * ((A - 1.0) - (A + 1.0) * cs);
    const double a2 =            (A + 1.0) - (A - 1.0) * cs - 2.0 * sqrtA * alpha;

    bq.b0 = b0 / a0;
    bq.b1 = b1 / a0;
    bq.b2 = b2 / a0;
    bq.a1 = a1 / a0;
    bq.a2 = a2 / a0;
}

// ── Tone filter update ─────────────────────────────────────────────────────

void TapeEmulation::updateToneFilter(float tone)
{
    if (tone == lastTone || sampleRate <= 0.0)
        return;
    lastTone = tone;

    constexpr double loFc = 200.0;
    constexpr double hiFc = 3000.0;
    constexpr double shelfQ = 0.707;

    double loGain, hiGain;

    if (tone <= 0.5f)
    {
        const float t = tone / 0.5f;            // 0 → 1  
        loGain = 1.0 * (1.0 - t) + 0.0 * t;    // +1.0 → 0
        hiGain = -2.5 * (1.0 - t) + 0.0 * t;   // -2.5 → 0
    }
    else
    {
        const float t = (tone - 0.5f) / 0.5f;   // 0 → 1  
        loGain = 0.0 * (1.0 - t) + (-0.5) * t;  // 0 → -0.5
        hiGain = 0.0 * (1.0 - t) + 2.5 * t;     // 0 → +2.5
    }

    computeLowShelf(toneShelfLo, loFc, loGain, shelfQ, sampleRate);
    computeHighShelf(toneShelfHi, hiFc, hiGain, shelfQ, sampleRate);
}

// ── Process ────────────────────────────────────────────────────────────────

void TapeEmulation::processAudio(float *input, float *output,
                                 float input_trim,
                                 float process_amount,
                                 int tape_type, float wet_dry_mix,
                                 float tone, int numSamples)
{
    juce::ignoreUnused(tape_type);

    const float p_dec = process_amount / PROCESS_AMOUNT_MAX;

    updateToneFilter(tone);

    for (int i = 0; i < numSamples; ++i)
    {
        const float dry_sample = input[i] * input_trim;

        const float hp = high_pass_filter_coefficient * dry_sample
                       + (dry_sample - previous_sample);
        previous_sample = dry_sample;

        const float filtered = hp * filter_coefficient_1 + hp;

        const float pre_sat = (pre_saturation_bypass == 0) ? dry_sample : filtered;

        const float sig1 = filtered;
        const float sat1 = papalote::satcurves::process(
            saturation_type, sig1, adaaStage1, p_dec);

        const float sig2 = sat1 * saturation_blend_1 + pre_sat;
        const float sat2 = papalote::satcurves::process(
            saturation_type, sig2, adaaStage2, p_dec);

        low_pass_filter_state += (sat2 - low_pass_filter_state)
                               * low_pass_filter_coefficient;

        double toneOut = toneShelfLo.process((double) low_pass_filter_state);
        toneOut = toneShelfHi.process(toneOut);

        float wet = p_dec * ((float) toneOut - dry_sample * saturation_blend_2);
        if (tape_model_type == 3)
            wet *= 0.5f;

        float out = dry_sample + (wet * wet_dry_mix);

        output[i] = out;
    }
}

void TapeEmulation::setMode(int type)
{
    if (type < 0 || type >= NUM_TYPES)
        return;

    const auto &p = s_tapeModes[type];
    tape_model_type = type;
    high_pass_filter_coefficient = p.high_pass_filter_coefficient;
    low_pass_filter_coefficient = p.low_pass_filter_coefficient;
    filter_coefficient_1 = p.filter_coefficient_1;
    saturation_blend_1 = p.saturation_blend_1;
    saturation_blend_2 = p.saturation_blend_2;
    pre_saturation_bypass = p.pre_saturation_bypass;
    saturation_type = p.saturation_type;
}

void TapeEmulation::resetPreviousState()
{
    previous_sample = 0.0f;
    low_pass_filter_state = 0.0f;

    papalote::satcurves::reset(adaaStage1);
    papalote::satcurves::reset(adaaStage2);

    toneShelfLo.reset();
    toneShelfHi.reset();
    lastTone = -1.0f;
}
