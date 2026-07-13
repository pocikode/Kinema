#include "modules/EvalLogger.h"

EvalLogger::~EvalLogger()
{
    Stop();
}

bool EvalLogger::Start(const std::string &path)
{
    Stop();
    m_file = std::fopen(path.c_str(), "w");
    if (!m_file)
        return false;
    m_time = 0.0;
    m_frame = -1;
    std::fprintf(m_file, "frame,t_sec,fps,slot,name,detected,predicted,cx_norm,cy_norm,cx_px,cy_px,area_px,z_est\n");
    return true;
}

void EvalLogger::Stop()
{
    if (m_file)
    {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

bool EvalLogger::IsActive() const
{
    return m_file != nullptr;
}

void EvalLogger::BeginFrame(float deltaTime, float fps)
{
    if (!m_file)
        return;
    m_time += deltaTime;
    ++m_frame;
    m_fps = fps;
}

void EvalLogger::LogSlot(
    int slot, const char *name, bool predicted, float cxNorm, float cyNorm, float cxPx, float cyPx, float areaPx,
    float zEst
)
{
    if (!m_file)
        return;
    std::fprintf(
        m_file, "%ld,%.4f,%.1f,%d,%s,%d,%d,%.5f,%.5f,%.2f,%.2f,%.1f,%.4f\n", m_frame, m_time, m_fps, slot, name,
        predicted ? 0 : 1, predicted ? 1 : 0, cxNorm, cyNorm, cxPx, cyPx, areaPx, zEst
    );
}

void EvalLogger::LogMissing(int slot, const char *name)
{
    if (!m_file)
        return;
    std::fprintf(m_file, "%ld,%.4f,%.1f,%d,%s,0,0,,,,,,\n", m_frame, m_time, m_fps, slot, name);
}
