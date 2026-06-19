#pragma once

#include "modules/MarkerDetection.h"

#include <glm/vec2.hpp>
#include <unordered_map>
#include <vector>

// Anti-jitter filter for raw marker observations. The HSV blob detector is very
// sensitive — centroids wobble a few pixels every frame even when the subject holds
// still — so without filtering that wobble becomes bone sway downstream. Filter()
// runs once, right after Detect(), and stabilizes every observation in place so all
// consumers (arm IK, head LookAt, position bones) inherit the smoothed values.
//
// Per marker id, two stages:
//   1. Deadzone (hysteresis): if the raw centroid sits within `deadzone` of the stored
//      value, the stored value is held unchanged — small sway produces no movement.
//   2. EMA: once outside the deadzone, ease toward raw with factor `alpha` so real
//      motion tracks smoothly instead of stepping.
// Blob area gets the same treatment (relative deadzone) to settle depth-from-area (Z).
//
// Occlusion hold: a marker that drops out is extrapolated along its last velocity
// (eased to a stop over `holdSec`, flagged `predicted`) instead of vanishing, so a
// brief occlusion doesn't freeze the bound bone. Past the window the id is pruned
// and re-seeds cleanly on return.
class MarkerStabilizer
{
  public:
    float deadzone = 0.012f; // normalized centroid radius (~1% of frame)
    float alpha = 0.35f;     // EMA factor 0..1 (higher = snappier, less smooth)
    float holdSec = 0.3f;    // extrapolate missing markers this long; 0 = off

    // Smooth centroidNorm + areaPixels of each observation in place; appends a
    // `predicted` observation for each recently lost marker still inside holdSec.
    void Filter(std::vector<MarkerObservation> &obs, float deltaTime);

    // Drop all per-id state (call when slots are reordered/reconfigured).
    void Reset();

  private:
    struct State
    {
        glm::vec2 centroid{0.0f};
        float area = 0.0f;
        glm::vec2 velocity{0.0f};   // normalized units per second
        float timeSinceSeen = 0.0f; // > 0 while the marker is occluded
    };
    std::unordered_map<int, State> m_state;
};
