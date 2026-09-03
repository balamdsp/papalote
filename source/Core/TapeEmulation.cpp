#include "TapeEmulation.h"
#include <algorithm>
#include <cmath>

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


void TapeEmulation::updateToneFilter(float tone, bool extra, double fs)
{
    // Exact-change guard; fabs form avoids -Wfloat-equal on cached params.
    const bool sameTone = ! (std::fabs (tone - lastTone) > 0.0f);
    const bool sameRate = ! (std::fabs (fs - lastSampleRate) > 0.0);
    if ((sameTone && extra == lastToneExtra && sameRate) || fs <= 0.0)
        return;
    lastTone = tone;
    lastToneExtra = extra;
    lastSampleRate = fs;

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

    if (extra)
    {
        loGain *= 3.0;
        hiGain *= 3.0;
    }

    computeLowShelf(toneShelfLo, loFc, loGain, shelfQ, fs);
    computeHighShelf(toneShelfHi, hiFc, hiGain, shelfQ, fs);
}


void TapeEmulation::updateProcessingRate(double processSampleRate)
{
    if (! (std::fabs (processSampleRate - lastProcessSampleRate) > 0.0) || sampleRate <= 0.0)
        return;

    lastProcessSampleRate = processSampleRate;

    effective_high_pass_filter_coefficient = high_pass_filter_coefficient;

    const double rateRatio = sampleRate / processSampleRate;
    effective_low_pass_filter_coefficient =
        (float) (1.0 - std::pow(1.0 - (double) low_pass_filter_coefficient, rateRatio));

    diffCompensation = (float) (1.0 / rateRatio);
}

void TapeEmulation::processAudio(float *input, float *output,
                                 float input_trim,
                                 float process_amount,
                                 int tape_type, float wet_dry_mix,
                                 float tone, bool toneExtra, bool driveExtra,
                                 int numSamples,
                                 double processSampleRate)
{
    if (tape_type != tape_model_type)
        setMode (tape_type);

    float p_dec = process_amount / PROCESS_AMOUNT_MAX;
    float drivePre = 1.0f;
    float driveWet = 1.0f;
    // X-Drive: hotter push that still leaves Drive-knob travel.
    if (driveExtra)
    {
        p_dec = std::min(p_dec * 2.0f, 1.0f);
        drivePre = 2.0f;
        driveWet = 1.25f;
    }

    updateProcessingRate(processSampleRate);
    updateToneFilter(tone, toneExtra, processSampleRate);

    const float hpEff = effective_high_pass_filter_coefficient;
    const float lpEff = effective_low_pass_filter_coefficient;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dry_sample = input[i] * input_trim;

        const float hp = hpEff * dry_sample
                       + diffCompensation * (dry_sample - previous_sample);
        previous_sample = dry_sample;

        const float filtered = hp * filter_coefficient_1 + hp;

        const float pre_sat = (pre_saturation_bypass == 0) ? dry_sample : filtered;

        const float sig1 = filtered * drivePre;
        const float sat1 = papalote::satcurves::process(saturation_type, sig1, adaaStage1, p_dec);

        const float sig2 = (sat1 * saturation_blend_1 + pre_sat) * drivePre;
        const float sat2 = papalote::satcurves::process(saturation_type, sig2, adaaStage2, p_dec);

        low_pass_filter_state += (sat2 - low_pass_filter_state)
                               * lpEff;

        double toneOut = toneShelfLo.process((double) low_pass_filter_state);
        toneOut = toneShelfHi.process(toneOut);

        float wet = p_dec * ((float) toneOut - dry_sample * saturation_blend_2) * driveWet;
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

    lastProcessSampleRate = -1.0;
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
    lastToneExtra = false;
    lastSampleRate = -1.0;
    lastProcessSampleRate = -1.0;
    effective_high_pass_filter_coefficient = 0.0f;
    effective_low_pass_filter_coefficient = 0.0f;
    diffCompensation = 1.0f;
}
