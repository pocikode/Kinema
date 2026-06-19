#pragma once

#include "modules/MarkerDetection.h"

#include <glm/vec2.hpp>
#include <unordered_map>
#include <vector>

// Two marker slots sharing one color range: the primary slot owns the range and
// the detector emits its two largest blobs (both with id == primarySlot); the
// follower slot has no range of its own.
struct MarkerPair
{
    int primarySlot = -1;
    int followerSlot = -1;
};

// Association stage between detection and stabilization: relabels the duplicate-id
// blob candidates of each paired range to unique slot ids so two same-color markers
// (e.g. both palms red) keep distinct left/right identities. Tracks each pair's two
// last-known positions and assigns candidates by nearest neighbor, with an x-order
// fallback on cold start and hysteresis so crossing hands don't flip labels.
// Non-paired observations pass through untouched.
class MarkerAssigner
{
  public:
    void SetPairs(std::vector<MarkerPair> pairs); // implies Reset()
    void Assign(std::vector<MarkerObservation> &obs);
    void Reset();

  private:
    struct PairState
    {
        glm::vec2 lastPos[2] = {{0.0f, 0.0f}, {0.0f, 0.0f}}; // [0]=primary, [1]=follower
        bool seeded[2] = {false, false};
    };

    std::vector<MarkerPair> m_pairs;
    std::unordered_map<int, PairState> m_state; // key: primarySlot
};
