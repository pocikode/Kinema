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

// Ratio-based color band used by RGBRatioMarkerDetector. A pixel matches when
// its R/G and R/B channel ratios both fall inside the configured windows and it
// is bright enough (max channel > vMin). Red, for example, sits around
// R/G in [1..5] and R/B in [2..4].
struct RGBRatioRange
{
    float rgMin = 1.0f;
    float rgMax = 5.0f;
    float rbMin = 2.0f;
    float rbMax = 4.0f;
    int vMin = 50; // reject dark pixels: max(B,G,R) must exceed this
};
