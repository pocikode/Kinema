#include "modules/MarkerAssigner.h"

#include <glm/glm.hpp>

void MarkerAssigner::SetPairs(std::vector<MarkerPair> pairs)
{
    m_pairs = std::move(pairs);
    Reset();
}

void MarkerAssigner::Reset()
{
    m_state.clear();
}

void MarkerAssigner::Assign(std::vector<MarkerObservation> &obs)
{
    auto dist2 = [](const glm::vec2 &a, const glm::vec2 &b) {
        glm::vec2 d = a - b;
        return glm::dot(d, d);
    };

    for (const auto &pair : m_pairs)
    {
        // Candidates arrive largest-area first (detector emit order).
        MarkerObservation *cand[2] = {nullptr, nullptr};
        int count = 0;
        for (auto &o : obs)
        {
            if (o.id == pair.primarySlot && count < 2)
            {
                cand[count++] = &o;
            }
        }
        if (count == 0)
        {
            continue;
        }

        auto &st = m_state[pair.primarySlot];

        if (count == 2)
        {
            int firstTrack; // track index (0=primary, 1=follower) taking cand[0]
            if (st.seeded[0] && st.seeded[1])
            {
                // Cost of the two labelings: cand[0]->primary vs cand[0]->follower.
                float costKeep =
                    dist2(cand[0]->centroidNorm, st.lastPos[0]) + dist2(cand[1]->centroidNorm, st.lastPos[1]);
                float costSwap =
                    dist2(cand[0]->centroidNorm, st.lastPos[1]) + dist2(cand[1]->centroidNorm, st.lastPos[0]);
                if (costKeep < costSwap * 0.9f)
                {
                    firstTrack = 0;
                }
                else if (costSwap < costKeep * 0.9f)
                {
                    firstTrack = 1;
                }
                else
                {
                    // Ambiguous (hands touching/crossing): keep the labeling that
                    // preserves the tracks' current x-order to avoid flip-flop.
                    bool tracksLR = st.lastPos[0].x <= st.lastPos[1].x;
                    bool candsLR = cand[0]->centroidNorm.x <= cand[1]->centroidNorm.x;
                    firstTrack = (tracksLR == candsLR) ? 0 : 1;
                }
            }
            else if (st.seeded[0] || st.seeded[1])
            {
                // The seeded track grabs its nearest candidate; the other seeds fresh.
                int s = st.seeded[0] ? 0 : 1;
                float d0 = dist2(cand[0]->centroidNorm, st.lastPos[s]);
                float d1 = dist2(cand[1]->centroidNorm, st.lastPos[s]);
                int nearest = (d0 <= d1) ? 0 : 1;
                firstTrack = (nearest == 0) ? s : 1 - s;
            }
            else
            {
                // Cold start: leftmost blob in the image goes to the primary slot.
                firstTrack = (cand[0]->centroidNorm.x <= cand[1]->centroidNorm.x) ? 0 : 1;
            }

            cand[0]->id = (firstTrack == 0) ? pair.primarySlot : pair.followerSlot;
            cand[1]->id = (firstTrack == 0) ? pair.followerSlot : pair.primarySlot;
            st.lastPos[firstTrack] = cand[0]->centroidNorm;
            st.lastPos[1 - firstTrack] = cand[1]->centroidNorm;
            st.seeded[0] = st.seeded[1] = true;
        }
        else // count == 1
        {
            // Single blob: nearest seeded track claims it; the other track's lastPos
            // is left untouched so identities snap back after a merge/occlusion.
            int track = 0;
            if (st.seeded[0] && st.seeded[1])
            {
                float d0 = dist2(cand[0]->centroidNorm, st.lastPos[0]);
                float d1 = dist2(cand[0]->centroidNorm, st.lastPos[1]);
                track = (d0 <= d1) ? 0 : 1;
            }
            else if (st.seeded[1])
            {
                track = 1;
            }
            cand[0]->id = (track == 0) ? pair.primarySlot : pair.followerSlot;
            st.lastPos[track] = cand[0]->centroidNorm;
            st.seeded[track] = true;
        }
    }
}
