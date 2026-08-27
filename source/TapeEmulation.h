#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include "SaturationCurves.h"

static constexpr float PROCESS_AMOUNT_MAX = 100.0f;

static constexpr int NUM_TYPES = 5;

class TapeEmulation
{
public:
    TapeEmulation();
    ~TapeEmulation();

    void processAudio(float *input, float *output,
                      float input_trim,
                      float process_amount,
                      int tape_type, float wet_dry_mix,
                      float tone, int numSamples);

    void resetPreviousState();

    void setMode(int type);

    void setSampleRate(double sr) { sampleRate = sr; }
    void updateToneFilter(float tone);

    struct TapeModeParameters
    {
        float high_pass_filter_coefficient;
        float low_pass_filter_coefficient;
        float filter_coefficient_1;
        float saturation_blend_1;
        float saturation_blend_2;
        int pre_saturation_bypass;
        int saturation_type;
    };

private:
    // One ADAA state per stage: stage 2's input differs from stage 1's.
    papalote::satcurves::AdaaState adaaStage1;
    papalote::satcurves::AdaaState adaaStage2;

    int tape_model_type = 0;

    float high_pass_filter_coefficient = 0.0f;
    float low_pass_filter_coefficient = 0.0f;
    float filter_coefficient_1 = 0.0f;
    float saturation_blend_1 = 0.0f;
    float saturation_blend_2 = 0.0f;
    int pre_saturation_bypass = 0;
    int saturation_type = 0;

    float previous_sample = 0.0f;
    float low_pass_filter_state = 0.0f;

    double sampleRate = 44100.0;

    // ── Tone EQ: two cascaded biquads (low shelf + high shelf) ─────────────
    struct Biquad
    {
        double b0 = 0.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

        void reset() { x1 = x2 = y1 = y2 = 0.0; }

        double process(double x) noexcept
        {
            double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    Biquad toneShelfLo;
    Biquad toneShelfHi;
    float lastTone = -1.0f;

    static void computeLowShelf(Biquad& bq, double fc, double gainDb, double Q, double fs);
    static void computeHighShelf(Biquad& bq, double fc, double gainDb, double Q, double fs);
};
