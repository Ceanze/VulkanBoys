#include "Profiler.h"

#include <cmath>

bool Profiler::m_ProfileFrame = true;
uint32_t Profiler::m_CooldownFramesRemaining = 0u;

void Profiler::newFrame()
{
    m_ProfileFrame = false;

    if (m_CooldownFramesRemaining == 0) {
        // The last frame was profiled, skip this one
        m_CooldownFramesRemaining = g_ProfilerCooldownFrames;
    } else {
        m_CooldownFramesRemaining -= 1;
        if (m_CooldownFramesRemaining == 0) {
            m_ProfileFrame = true;
        }
    }
}
