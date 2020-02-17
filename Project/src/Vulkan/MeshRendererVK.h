#pragma once
#include "Common/IRenderer.h"
#include "VulkanCommon.h"

class BufferVK;
class PipelineVK;
class RenderingHandlerVK;
class RenderPassVK;
class FrameBufferVK;
class CommandPoolVK;
class CommandBufferVK;
class DescriptorSetVK;
class DescriptorPoolVK;
class PipelineLayoutVK;
class GraphicsContextVK;
class DescriptorSetLayoutVK;

//Temp
class RayTracingSceneVK;
class RayTracingPipelineVK;
class ShaderBindingTableVK;
class ImageVK;
class ImageViewVK;
class ShaderVK;
class SamplerVK;
class Texture2DVK;
struct TempMaterial;

class MeshRendererVK : public IRenderer
{
public:
	MeshRendererVK(GraphicsContextVK* pContext, RenderingHandlerVK* pRenderingHandler);
	~MeshRendererVK();

	virtual bool init() override;

	virtual void beginFrame(const Camera& camera) override;
	virtual void endFrame() override;

	virtual void beginRayTraceFrame(const Camera& camera) override;
	virtual void endRayTraceFrame() override;
	virtual void traceRays() override;
	
	virtual void setViewport(float width, float height, float minDepth, float maxDepth, float topX, float topY) override;

	void submitMesh(IMesh* pMesh, const glm::vec4& color, const glm::mat4& transform);

private:
	bool createSemaphores();
	bool createCommandPoolAndBuffers();
	bool createPipelines();
	bool createPipelineLayouts();
	bool createRayTracingPipelineLayouts();

	void initRayTracing();

private:
	GraphicsContextVK* m_pContext;
	RenderingHandlerVK* m_pRenderingHandler;
	CommandPoolVK* m_ppCommandPools[MAX_FRAMES_IN_FLIGHT];
	CommandBufferVK* m_ppCommandBuffers[MAX_FRAMES_IN_FLIGHT];

	CommandPoolVK* m_ppComputeCommandPools[MAX_FRAMES_IN_FLIGHT];
	CommandBufferVK* m_ppComputeCommandBuffers[MAX_FRAMES_IN_FLIGHT];

	RenderPassVK* m_pRenderPass;
	FrameBufferVK* m_ppBackbuffers[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore m_ImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore m_RenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];

	// TEMPORARY MOVE TO MATERIAL or SOMETHING
	PipelineVK* m_pPipeline;
	PipelineLayoutVK* m_pPipelineLayout;
	DescriptorSetVK* m_pDescriptorSet;
	DescriptorPoolVK* m_pDescriptorPool;
	DescriptorSetLayoutVK* m_pDescriptorSetLayout;

	VkViewport m_Viewport;
	VkRect2D m_ScissorRect;

	uint64_t m_CurrentFrame;
	uint32_t m_BackBufferIndex;

	//Temp Ray Tracing Stuff
	RayTracingSceneVK* m_pRayTracingScene;
	RayTracingPipelineVK* m_pRayTracingPipeline;
	PipelineLayoutVK* m_pRayTracingPipelineLayout;
	ShaderBindingTableVK* m_pSBT;
	ImageVK* m_pRayTracingStorageImage;
	ImageViewVK* m_pRayTracingStorageImageView;
	
	DescriptorSetVK* m_pRayTracingDescriptorSet;
	DescriptorPoolVK* m_pRayTracingDescriptorPool;
	DescriptorSetLayoutVK* m_pRayTracingDescriptorSetLayout;

	BufferVK* m_pRayTracingUniformBuffer;

	IMesh* m_pMeshCube;
	IMesh* m_pMeshGun;

	glm::mat4 m_Matrix0;
	glm::mat4 m_Matrix1;
	glm::mat4 m_Matrix2;
	glm::mat4 m_Matrix3;

	uint32_t m_InstanceIndex0;
	uint32_t m_InstanceIndex1;
	uint32_t m_InstanceIndex2;
	uint32_t m_InstanceIndex3;

	ShaderVK* m_pRaygenShader;
	ShaderVK* m_pClosestHitShader;
	ShaderVK* m_pMissShader;

	SamplerVK* m_pSampler;
	
	TempMaterial* m_pCubeMaterial;
	TempMaterial* m_pGunMaterial;

	float m_TempTimer;
};
