#pragma once

#include <algorithm>
#include <cmath>

namespace crt
{

inline float clampF (float v, float lo, float hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float stepF (float edge, float x) noexcept
{
    return x < edge ? 0.0f : 1.0f;
}

inline float mixF (float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

inline float smoothstepF (float e0, float e1, float x) noexcept
{
    const float t = clampF ((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float fractF (float x) noexcept
{
    return x - std::floor (x);
}

// Linear interpolation (cool-retro-term's "lint"), t unclamped like QML.
inline float lint (float x, float y, float t) noexcept
{
    return x + (y - x) * t;
}

// cool-retro-term's distortCoordinates: barrel/pincushion warp shared by the
// static, dynamic and frame passes.
inline void distortCoordinates (float coordsX, float coordsY,
                                float frameSize, float screenCurvature,
                                float& outX, float& outY) noexcept
{
    const float paddedX = coordsX * (1.0f + frameSize * 2.0f) - frameSize;
    const float paddedY = coordsY * (1.0f + frameSize * 2.0f) - frameSize;
    const float ccX = paddedX - 0.5f;
    const float ccY = paddedY - 0.5f;
    const float dist = (ccX * ccX + ccY * ccY) * screenCurvature;
    const float k = (1.0f + dist) * dist;
    outX = paddedX + ccX * k;
    outY = paddedY + ccY * k;
}

// cool-retro-term's rand2: fract (sin (dot (v, (12.9898, 78.233))) * 43758.5453)
inline float rand2 (float x, float y) noexcept
{
    const float s = std::sin (x * 12.9898f + y * 78.233f) * 43758.5453f;
    return fractF (s);
}

// cool-retro-term's rgb2grey: dot (v, (0.21, 0.72, 0.04))
inline float rgb2grey (float r, float g, float b) noexcept
{
    return r * 0.21f + g * 0.72f + b * 0.04f;
}

}
