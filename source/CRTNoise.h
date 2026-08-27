#pragma once

#include <JuceHeader.h>

#include <vector>

#include "CRTMath.h"

namespace crt
{

inline constexpr int crtNoiseTileSize = 512;

namespace detail
{

inline float valueNoiseOctave (const std::vector<float>& lattice, int grid,
                               float fx, float fy) noexcept
{
    const float gx = fx * (float) grid;
    const float gy = fy * (float) grid;
    const int x0 = ((int) std::floor (gx)) % grid;
    const int y0 = ((int) std::floor (gy)) % grid;
    const int x1 = (x0 + 1) % grid;
    const int y1 = (y0 + 1) % grid;
    const float tx = smoothstepF (0.0f, 1.0f, fractF (gx));
    const float ty = smoothstepF (0.0f, 1.0f, fractF (gy));

    const float p00 = lattice[y0 * grid + x0];
    const float p10 = lattice[y0 * grid + x1];
    const float p01 = lattice[y1 * grid + x0];
    const float p11 = lattice[y1 * grid + x1];

    return mixF (mixF (p00, p10, tx), mixF (p01, p11, tx), ty);
}

}

// Procedural noise tile for the cool-retro-term noiseSource sampler
// (512x512 RGBA, tiled with wrap): r = white noise (vertex hsync randval),
// g/b = coherent value noise (vertex brightness/distortion, pixel jitter X),
// a = cubed white speckle (pixel static noise + jitter Y).
inline void buildNoiseTile (Image& tile) noexcept
{
    constexpr int size = crtNoiseTileSize;

    if (tile.getWidth() != size || tile.getHeight() != size)
        tile = Image (Image::ARGB, size, size, true);

    constexpr int gridA = 16;
    constexpr int gridB = 64;
    std::vector<float> latA ((size_t) gridA * gridA);
    std::vector<float> latB ((size_t) gridB * gridB);

    Random rng;
    for (auto& v : latA) v = rng.nextFloat();
    for (auto& v : latB) v = rng.nextFloat();

    Image::BitmapData dst (tile, Image::BitmapData::writeOnly);
    const float invSize = 1.0f / (float) size;

    for (int y = 0; y < size; ++y)
    {
        const float fy = ((float) y + 0.5f) * invSize;
        PixelARGB* row = reinterpret_cast<PixelARGB*> (dst.getLinePointer (y));

        for (int x = 0; x < size; ++x)
        {
            const float fx = ((float) x + 0.5f) * invSize;

            const float nA = detail::valueNoiseOctave (latA, gridA, fx, fy);
            const float nB = detail::valueNoiseOctave (latB, gridB, fx, fy);
            const float coherent = clampF (0.6f * nA + 0.4f * nB, 0.0f, 1.0f);

            float speck = rng.nextFloat();
            speck = speck * speck * speck;

            const uint8 r  = (uint8) roundToInt (rng.nextFloat() * 255.0f);
            const uint8 g  = (uint8) roundToInt (coherent * 255.0f);
            const uint8 b  = (uint8) roundToInt (coherent * 255.0f);
            const uint8 a  = (uint8) roundToInt (speck * 255.0f);

            row[x].setARGB (a, r, g, b);
        }
    }
}

}
