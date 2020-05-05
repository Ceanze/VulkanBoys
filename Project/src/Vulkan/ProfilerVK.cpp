#include "ProfilerVK.h"

#include "imgui/imgui.h"
#include "vulkan/vulkan.h"

#include "Core/Core.h"
#include "Vulkan/CommandBufferVK.h"
#include "Vulkan/DeviceVK.h"

#include <fstream>

double ProfilerVK::m_TimestampToMillisec = 0.0;
const uint32_t ProfilerVK::m_DashesPerRecurse = 2;
uint32_t ProfilerVK::m_MaxTextWidth = 0;

ProfilerVK::ProfilerVK(const std::string& name, DeviceVK* pDevice, uint32_t queriesPerFrame)
    :m_Name(name),
    m_Time(0),
    m_pDevice(pDevice),
    m_pProfiledCommandBuffer(nullptr),
    m_RecurseDepth(0),
    m_CurrentFrame(0),
    m_OutOfQueries(false)
{
    findWidestText();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_ppQueryPools[i] = DBG_NEW QueryPoolVK(pDevice);

        if (!m_ppQueryPools[i]->init(VK_QUERY_TYPE_TIMESTAMP, queriesPerFrame)) {
            LOG("Profiler %s: failed to initialize query pool", m_Name.c_str());
            return;
        }

        m_pNextQuery[i] = 0u;
    }

    // From Vulkan spec: "The number of nanoseconds it takes for a timestamp value to be incremented by 1"
    double nanoSecPerTime = (double)pDevice->getTimestampPeriod();
    const double nanoSecond = std::pow(10.0, -9.0);
    const double milliSecondInv = 1000.0;

    m_TimestampToMillisec = nanoSecPerTime * nanoSecond * milliSecondInv;

    // Clear the results file
    std::ofstream file;
    file.open("results.txt", std::ios::out | std::ios::trunc | std::ios::binary);
    file.write((char*)(&m_TimestampToMillisec), sizeof(double));
    file.close();
}

ProfilerVK::~ProfilerVK()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        SAFEDELETE(m_ppQueryPools[i]);
    }
}

void ProfilerVK::setParentProfiler(ProfilerVK* pParentProfiler)
{
    m_RecurseDepth = pParentProfiler->getRecurseDepth() + 1;
    findWidestText();
}

void ProfilerVK::reset(uint32_t currentFrame, CommandBufferVK* pResetCmdBuffer)
{
    UNREFERENCED_PARAMETER(pResetCmdBuffer);

    if (m_OutOfQueries) {
        // Expand the previous query pool
        expandQueryPools(pResetCmdBuffer);
        m_OutOfQueries = false;
    }

    m_CurrentFrame = currentFrame;
}

void ProfilerVK::beginFrame(CommandBufferVK* pProfiledCmdBuffer)
{
    m_pProfiledCommandBuffer        = pProfiledCmdBuffer;
    QueryPoolVK* pCurrentQueryPool  = m_ppQueryPools[m_CurrentFrame];

    if (m_pNextQuery[m_CurrentFrame] == 0) {
        vkCmdResetQueryPool(pProfiledCmdBuffer->getCommandBuffer(), pCurrentQueryPool->getQueryPool(), 0, pCurrentQueryPool->getQueryCount());
    }

    // Write a timestamp to measure the time elapsed for the entire scope of the profiler
    vkCmdWriteTimestamp(m_pProfiledCommandBuffer->getCommandBuffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pCurrentQueryPool->getQueryPool(), m_pNextQuery[m_CurrentFrame]++);
}

void ProfilerVK::writeResults()
{
    uint32_t totalQueryCount = 0u;
    for (uint32_t queryCount : m_pNextQuery) {
        totalQueryCount += queryCount;
    }

    m_TimeResults.resize(totalQueryCount);
    void* pTimestampsPtr = (void*)m_TimeResults.data();

    for (uint32_t frameIdx = 0; frameIdx < MAX_FRAMES_IN_FLIGHT; frameIdx++) {
        VkQueryPool currentQueryPool = m_ppQueryPools[m_CurrentFrame]->getQueryPool();
        uint32_t queryCount = m_pNextQuery[frameIdx];

        if (vkGetQueryPoolResults(
            m_pDevice->getDevice(), currentQueryPool,
            0, queryCount,                  // First query, query count
            queryCount * sizeof(uint64_t),  // Data size
            pTimestampsPtr,                 // Data pointer
            sizeof(uint64_t),               // Stride
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)
            != VK_SUCCESS)
        {
            LOG("Profiler %s: failed to get query pool results", m_Name.c_str());
            return;
        }

        uint32_t ptrOffset = uint32_t(sizeof(uint64_t)) * queryCount;
        pTimestampsPtr = (void*)((char*)pTimestampsPtr + ptrOffset);
    }
}

void ProfilerVK::endFrame()
{
    VkQueryPool currentQueryPool = m_ppQueryPools[m_CurrentFrame]->getQueryPool();
    vkCmdWriteTimestamp(m_pProfiledCommandBuffer->getCommandBuffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, currentQueryPool, m_pNextQuery[m_CurrentFrame]++);
}

void ProfilerVK::drawResults()
{
    std::string indent('-', size_t(m_RecurseDepth));

    // Align the number across all timestamps and profilers by filling with whitespaces
    uint32_t fillLength = m_MaxTextWidth - (m_RecurseDepth * m_DashesPerRecurse + (uint32_t)m_Name.size());
    std::string whitespaceFill(fillLength, ' ');

    // Convert time to milliseconds
    double timeMs = m_Time * m_TimestampToMillisec;
    ImGui::Text("%s%s:\t%s%f ms", indent.c_str(), m_Name.c_str(), whitespaceFill.c_str(), timeMs);

    // Print timestamps
    uint32_t timestampPrefixWidth = (m_RecurseDepth + 1) * m_DashesPerRecurse;

    for (Timestamp* pTimestamp : m_Timestamps) {
        fillLength = m_MaxTextWidth - (timestampPrefixWidth + (uint32_t)pTimestamp->name.size());
        whitespaceFill = std::string(fillLength, ' ');

        timeMs = pTimestamp->time * m_TimestampToMillisec;
        ImGui::Text("--%s%s:\t%s%f ms", indent.c_str(), pTimestamp->name.c_str(), whitespaceFill.c_str(), timeMs);
    }

    // Draw the child profilers' results
    for (ProfilerVK* pChild : m_Children) {
        pChild->drawResults();
    }
}

void ProfilerVK::addChildProfiler(ProfilerVK* pChildProfiler)
{
    m_Children.push_back(pChildProfiler);
}

void ProfilerVK::initTimestamp(Timestamp* pTimestamp, const std::string name)
{
    pTimestamp->name = name;
    pTimestamp->time = 0;
    pTimestamp->queries.clear();
    m_Timestamps.push_back(pTimestamp);
}

void ProfilerVK::beginTimestamp(Timestamp* pTimestamp)
{
    QueryPoolVK* pCurrentQueryPool = m_ppQueryPools[m_CurrentFrame];

    if (m_pNextQuery[m_CurrentFrame] > pCurrentQueryPool->getQueryCount() - 2) {
        m_OutOfQueries = true;
        return;
    }

    pTimestamp->queries.push_back(m_pNextQuery[m_CurrentFrame]);
    vkCmdWriteTimestamp(m_pProfiledCommandBuffer->getCommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pCurrentQueryPool->getQueryPool(), m_pNextQuery[m_CurrentFrame]);

    // Advance by two, as the next query index will be used by the ending timestamp
    m_pNextQuery[m_CurrentFrame] += 2;
}

void ProfilerVK::endTimestamp(Timestamp* pTimestamp)
{
    QueryPoolVK* pCurrentQueryPool = m_ppQueryPools[m_CurrentFrame];

    vkCmdWriteTimestamp(m_pProfiledCommandBuffer->getCommandBuffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pCurrentQueryPool->getQueryPool(), pTimestamp->queries.back() + 1);
}

void ProfilerVK::findWidestText()
{
    // Widest string among the profiler and its timestamps, includes the width of the dash-prefix and the name of the profiler/timestamp
    uint32_t maxTextWidthLocal = m_RecurseDepth * m_DashesPerRecurse + (uint32_t)m_Name.size();
    uint32_t timestampPrefixWidth = (m_RecurseDepth + 1) * m_DashesPerRecurse;

    for (Timestamp* pTimestamp : m_Timestamps) {
        maxTextWidthLocal = std::max(timestampPrefixWidth + (uint32_t)pTimestamp->name.size(), maxTextWidthLocal);
    }

    m_MaxTextWidth = std::max(m_MaxTextWidth, maxTextWidthLocal);
}

void ProfilerVK::expandQueryPools(CommandBufferVK* pCommandBuffer)
{
    UNREFERENCED_PARAMETER(pCommandBuffer);

    uint32_t newQueryCount = m_ppQueryPools[m_CurrentFrame]->getQueryCount() * 2;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        QueryPoolVK* pNewQueryPool = DBG_NEW QueryPoolVK(m_pDevice);

        if (!pNewQueryPool->init(VK_QUERY_TYPE_TIMESTAMP, newQueryCount)) {
            LOG("Profiler %s: failed to initialize query pool", m_Name.c_str());
            return;
        }

        SAFEDELETE(m_ppQueryPools[i]);
        m_ppQueryPools[i] = pNewQueryPool;
    }

    D_LOG("Profiler %s: expanded query pools to %d queries", m_Name.c_str(), newQueryCount);
}
