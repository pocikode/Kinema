#pragma once

#include "ui/UIManager.h" // MarkerSlot

#include <string>
#include <vector>

// Persist the marker slot list (names, HSV bands, bindings, IK wiring) to a
// JSON file so a tuned configuration can be reused across sessions. Reciprocal
// to LoadMarkerConfig; both return false on I/O or parse failure.
bool SaveMarkerConfig(const std::string &path, const std::vector<MarkerSlot> &markers);

// Replaces `markers` with the slots read from `path`. On failure `markers` is
// left untouched.
bool LoadMarkerConfig(const std::string &path, std::vector<MarkerSlot> &markers);
