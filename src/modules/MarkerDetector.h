#pragma once

// Legacy single-marker API has been retired. Only HSVRange is still shared,
// consumed by HSVMarkerDetector and the UI's per-slot HSV sliders.
struct HSVRange
{
    int hMin = 0;
    int hMax = 179;
    int sMin = 50;
    int sMax = 255;
    int vMin = 50;
    int vMax = 255;

    // Red wraps the hue circle (hue ~0 and ~179), so a single [hMin..hMax]
    // band can't capture it. When dualHue is on, a second hue band
    // [hMin2..hMax2] is OR'd into the mask (same S/V bounds).
    bool dualHue = false;
    int hMin2 = 0;
    int hMax2 = 10;
};

// Chromaticity-based color band used by RGBRatioMarkerDetector. Each pixel is
// normalized into rn = R/(R+G+B), gn = G/(R+G+B); a pixel matches when rn and gn
// both fall inside the configured windows, it is bright enough (mean channel >
// vMin), and it is saturated enough (max deviation from neutral 1/3 >= satMin,
// which rejects gray/washed-out pixels). Red, for example, sits around
// rn in [0.45..1.0] and gn in [0..0.35].
struct RGBRatioRange
{
    float rMin = 0.45f;
    float rMax = 1.0f;
    float gMin = 0.0f;
    float gMax = 0.35f;
    int vMin = 30;        // reject dark pixels: mean(R,G,B) must exceed this
    float satMin = 0.12f; // reject neutral/gray pixels (chromaticity near 1/3,1/3)
};
