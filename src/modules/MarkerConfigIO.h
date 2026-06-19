#pragma once

#include "ui/UIManager.h" // MarkerSlot

#include <string>
#include <vector>

// Persist the marker slot list (names, HSV bands, bindings, IK wiring) plus the
// 2D→3D depth reference to a JSON file so a tuned configuration can be reused
// across sessions. Reciprocal to LoadMarkerConfig; both return false on I/O or
// parse failure.
bool SaveMarkerConfig(const std::string &path, const std::vector<MarkerSlot> &markers, float depthRefDist,
                      float depthRefArea);

// Replaces `markers` (and the depth reference, when present in the file) with
// the values read from `path`. On failure all outputs are left untouched.
bool LoadMarkerConfig(const std::string &path, std::vector<MarkerSlot> &markers, float &depthRefDist,
                      float &depthRefArea);
