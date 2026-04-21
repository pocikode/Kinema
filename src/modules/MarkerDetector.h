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
};
