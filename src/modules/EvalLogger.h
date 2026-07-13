#pragma once

#include <cstdio>
#include <string>

// Per-frame CSV logger for the quantitative evaluation sessions (Subbab 3.2.2
// of the report): centroid position, blob area, estimated depth, and detection
// status per marker slot. One row per configured slot per frame; slots with no
// observation this frame log detected=0 with empty metric fields so detection
// rate can be computed downstream (tools/eval_summary.py).
class EvalLogger
{
  public:
    ~EvalLogger();

    // Opens `path` for writing and emits the CSV header. Returns false if the
    // file cannot be created (path kept so the UI can show what failed).
    bool Start(const std::string &path);
    void Stop();
    bool IsActive() const;

    // Advances the frame counter/clock; call once per app frame before LogSlot.
    void BeginFrame(float deltaTime, float fps);

    void LogSlot(
        int slot, const char *name, bool predicted, float cxNorm, float cyNorm, float cxPx, float cyPx, float areaPx,
        float zEst
    );
    void LogMissing(int slot, const char *name);

  private:
    FILE *m_file = nullptr;
    double m_time = 0.0;
    long m_frame = -1;
    float m_fps = 0.0f;
};
