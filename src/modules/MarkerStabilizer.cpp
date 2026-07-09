#include "modules/MarkerStabilizer.h"

#include <glm/geometric.hpp>
#include <unordered_set>

void MarkerStabilizer::Filter(std::vector<MarkerObservation> &obs, float deltaTime)
{
    std::unordered_set<int> seen;
    seen.reserve(obs.size());

    for (auto &o : obs)
    {
        seen.insert(o.id);
        auto it = m_state.find(o.id);
        if (it == m_state.end())
        {
            // First sighting: seed state to raw so there is no lag spike.
            m_state.emplace(o.id, State{o.centroidNorm, o.areaPixels});
            continue;
        }

        State &s = it->second;
        glm::vec2 prevCentroid = s.centroid;

        // Anti-jitter deadzone + EMA:
        // gerakan < deadzone ditahan (dianggap noise); di luar itu dihaluskan
        // dengan Exponential Moving Average: s = (1 - alpha) * s + alpha * raw.
        // Centroid: hold through the deadzone, else EMA toward raw.
        if (glm::distance(o.centroidNorm, s.centroid) > deadzone)
            s.centroid = glm::mix(s.centroid, o.centroidNorm, alpha);
        o.centroidNorm = s.centroid;

        // Track velocity of the filtered centroid (EMA-smoothed like the position)
        // so an occluded marker can coast along its last motion.
        if (deltaTime > 0.0f)
            s.velocity = glm::mix(s.velocity, (s.centroid - prevCentroid) / deltaTime, alpha);
        s.timeSinceSeen = 0.0f;

        // Area: same gate, deadzone scaled relative to the current value so it tracks
        // across depth ranges instead of being a fixed pixel count.
        float areaBand = deadzone * (s.area + 1.0f);
        if (std::abs(o.areaPixels - s.area) > areaBand)
            s.area = glm::mix(s.area, o.areaPixels, alpha);
        o.areaPixels = s.area;
    }

    // Markers that dropped out: extrapolate along the last velocity, easing to a
    // stop at holdSec (confidence ramp), then prune so they re-seed cleanly on return.
    for (auto it = m_state.begin(); it != m_state.end();)
    {
        if (seen.count(it->first))
        {
            ++it;
            continue;
        }
        State &s = it->second;
        s.timeSinceSeen += deltaTime;
        if (holdSec <= 0.0f || s.timeSinceSeen > holdSec)
        {
            it = m_state.erase(it);
            continue;
        }

        float confidence = 1.0f - s.timeSinceSeen / holdSec;
        s.centroid += s.velocity * deltaTime * confidence;

        MarkerObservation held;
        held.id = it->first;
        held.predicted = true;
        held.centroidNorm = s.centroid;
        held.areaPixels = s.area; // hold area: extrapolating it would add depth noise
        obs.push_back(held);
        ++it;
    }
}

void MarkerStabilizer::Reset()
{
    m_state.clear();
}
