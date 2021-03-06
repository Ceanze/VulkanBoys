#pragma once
#include "Common/RenderingHandler.hpp"
#include "Core/Camera.h"

#include "Vulkan/ImguiVK.h"
#include "Vulkan/VulkanCommon.h"

class BufferVK;
class CommandBufferVK;
class CommandPoolVK;
class FrameBufferVK;
class GBufferVK;
class GraphicsContextVK;
class IGraphicsContext;
class ImageVK;
class ImageViewVK;
class ImguiVK;
class IRenderer;
class IScene;
class MeshRendererVK;
class MeshVK;
class ParticleRendererVK;
class PipelineVK;
class RenderPassVK;
class SceneVK;

class RenderingHandlerVK : public RenderingHandler
{
public:
    RenderingHandlerVK(GraphicsContextVK* pGraphicsContext);
    ~RenderingHandlerVK();

    virtual bool initialize() override;

	virtual void render(IScene* pScene) override;

    virtual void swapBuffers() override;

    virtual void drawProfilerUI() override;

    virtual void setImguiRenderer(IImgui* pImGui) override                                  { m_pImGuiRenderer = reinterpret_cast<ImguiVK*>(pImGui); }
    virtual void setParticleRenderer(IRenderer* pParticleRenderer) override                 { m_pParticleRenderer = reinterpret_cast<ParticleRendererVK*>(pParticleRenderer); }

    virtual void setClearColor(float r, float g, float b) override;
    virtual void setClearColor(const glm::vec3& color) override;
    virtual void setViewport(float width, float height, float minDepth, float maxDepth, float topX, float topY) override;

    virtual void onWindowResize(uint32_t width, uint32_t height) override;
	virtual void onSceneUpdated(IScene* pScene) override;

    FORCEINLINE uint32_t                getCurrentFrameIndex() const            { return m_CurrentFrame; }
    FORCEINLINE FrameBufferVK* const*   getBackBuffers() const                  { return m_ppBackbuffers; }
	FORCEINLINE RenderPassVK*			getGeometryRenderPass() const			{ return m_pGeometryRenderPass; }
    FORCEINLINE RenderPassVK*           getBackBufferRenderPass() const         { return m_pBackBufferRenderPass; }
    FORCEINLINE RenderPassVK*           getParticleRenderPass() const           { return m_pParticleRenderPass; }
    FORCEINLINE BufferVK*               getCameraBufferCompute() const          { return m_pCameraBufferCompute; }
    FORCEINLINE BufferVK*               getCameraBufferGraphics() const         { return m_pCameraBufferGraphics; }
    FORCEINLINE BufferVK*               getLightBufferCompute() const           { return m_pLightBufferCompute; }
    FORCEINLINE BufferVK*               getLightBufferGraphics() const          { return m_pLightBufferGraphics; }
    FORCEINLINE FrameBufferVK*          getCurrentBackBuffer() const            { return m_ppBackbuffers[m_BackBufferIndex]; }
    FORCEINLINE FrameBufferVK*          getCurrentBackBufferWithDepth() const   { return m_ppBackBuffersWithDepth[m_BackBufferIndex]; }
    FORCEINLINE CommandBufferVK*        getCurrentGraphicsCommandBuffer() const { return m_ppGraphicsCommandBuffers[m_CurrentFrame]; }
	FORCEINLINE GBufferVK*				getGBuffer() const						{ return m_pGBuffer; }
    FORCEINLINE virtual IImgui* getImguiRenderer() override                     { return m_pImGuiRenderer; }

private:
    bool createBackBuffers();
    bool createCommandPoolAndBuffers();
    bool createRenderPasses();
    bool createSemaphores();
    bool createBuffers();
	bool createGBuffer();

    void releaseBackBuffers();

    void updateBuffers(SceneVK* pScene, const Camera& camera, const LightSetup& lightSetup);

    void submitParticles();

private:
    CameraBuffer m_CameraBuffer;

    GraphicsContextVK* m_pGraphicsContext;

    ParticleRendererVK*     m_pParticleRenderer;
    ImguiVK*                m_pImGuiRenderer;

    FrameBufferVK*  m_ppBackbuffers[MAX_FRAMES_IN_FLIGHT];
    FrameBufferVK*  m_ppBackBuffersWithDepth[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore     m_pImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore     m_pRenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore     m_ComputeFinishedGraphicsSemaphore;
    VkSemaphore     m_ComputeFinishedTransferSemaphore;
    VkSemaphore     m_GeometryFinishedSemaphore;
    VkSemaphore     m_TransferStartSemaphore;
    VkSemaphore     m_TransferFinishedGraphicsSemaphore;
    VkSemaphore     m_TransferFinishedComputeSemaphore;

    CommandPoolVK*      m_ppGraphicsCommandPools[MAX_FRAMES_IN_FLIGHT];
	CommandBufferVK*    m_ppGraphicsCommandBuffers[MAX_FRAMES_IN_FLIGHT];
    CommandBufferVK*    m_ppGraphicsCommandBuffers2[MAX_FRAMES_IN_FLIGHT];
	CommandPoolVK*      m_ppTransferCommandPools[MAX_FRAMES_IN_FLIGHT];
	CommandBufferVK*    m_ppTransferCommandBuffers[MAX_FRAMES_IN_FLIGHT];
    CommandPoolVK*      m_ppComputeCommandPools[MAX_FRAMES_IN_FLIGHT];
    CommandBufferVK*    m_ppComputeCommandBuffers[MAX_FRAMES_IN_FLIGHT];
    CommandPoolVK*      m_ppCommandPoolsSecondary[MAX_FRAMES_IN_FLIGHT];
	CommandBufferVK*    m_ppCommandBuffersSecondary[MAX_FRAMES_IN_FLIGHT];

	RenderPassVK*   m_pGeometryRenderPass;
	RenderPassVK*   m_pShadowMapRenderPass;
    RenderPassVK*   m_pBackBufferRenderPass;
    RenderPassVK*   m_pParticleRenderPass;
    RenderPassVK*   m_pUIRenderPass;
    PipelineVK*     m_pPipeline;

    uint32_t m_CurrentFrame;
    uint32_t m_BackBufferIndex;

    VkClearValue m_ClearColor;
	VkClearValue m_ClearDepth;

    VkViewport  m_Viewport;
	VkRect2D    m_ScissorRect;

    BufferVK*   m_pCameraBufferGraphics;
    BufferVK*   m_pCameraBufferCompute;
    BufferVK*   m_pLightBufferGraphics;
    BufferVK*   m_pLightBufferCompute;
    GBufferVK*  m_pGBuffer;
};
