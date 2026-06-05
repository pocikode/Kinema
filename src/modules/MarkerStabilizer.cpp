#include "modules/MarkerStabilizer.h"

#include <glm/geometric.hpp>
#include <unordered_set>

void MarkerStabilizer::Filter(std::vector<MarkerObservation> &obs)
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

        // Centroid: hold through the deadzone, else EMA toward raw.
        if (glm::distance(o.centroidNorm, s.centroid) > deadzone)
            s.centroid = glm::mix(s.centroid, o.centroidNorm, alpha);
        o.centroidNorm = s.centroid;

        // Area: same gate, deadzone scaled relative to the current value so it tracks
        // across depth ranges instead of being a fixed pixel count.
        float areaBand = deadzone * (s.area + 1.0f);
        if (std::abs(o.areaPixels - s.area) > areaBand)
            s.area = glm::mix(s.area, o.areaPixels, alpha);
        o.areaPixels = s.area;
    }

    // Prune markers that dropped out so they re-seed cleanly on return.
    for (auto it = m_state.begin(); it != m_state.end();)
        it = seen.count(it->first) ? std::next(it) : m_state.erase(it);
}

void MarkerStabilizer::Reset()
{
    m_state.clear();
}
