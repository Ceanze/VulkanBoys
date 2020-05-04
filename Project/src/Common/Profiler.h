#pragma once

#include <stdint.h>

// How many frames to wait until profiling another frame
const uint32_t g_ProfilerCooldownFrames = 5u;

class Profiler
{
public:
    static void newFrame();

protected:
    static bool m_ProfileFrame;

private:
    // Counts down towards the next time profiling will be performed
    static uint32_t m_CooldownFramesRemaining;
};
