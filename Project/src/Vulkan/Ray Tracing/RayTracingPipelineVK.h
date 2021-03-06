#pragma once

#include "Vulkan/VulkanCommon.h"

class ShaderBindingTableVK;
class GraphicsContextVK;
class PipelineLayoutVK;
class ShaderVK;

struct RaygenGroupParams
{
	ShaderVK* pRaygenShader = nullptr;
};

struct HitGroupParams
{
	ShaderVK* pIntersectShader = nullptr;
	ShaderVK* pAnyHitShader = nullptr;
	ShaderVK* pClosestHitShader = nullptr;
};

struct MissGroupParams
{
	ShaderVK* pMissShader = nullptr;
};

class RayTracingPipelineVK
{
public:
	DECL_NO_COPY(RayTracingPipelineVK);

	RayTracingPipelineVK(GraphicsContextVK* pGraphicsContext);
	~RayTracingPipelineVK();

	void addRaygenShaderGroup(const RaygenGroupParams& params);
	void addMissShaderGroup(const MissGroupParams& params);
	void addHitShaderGroup(const HitGroupParams& params);

	bool finalize(PipelineLayoutVK* pPipelineLayout);
	
	void setMaxRecursionDepth(uint32_t value) { m_MaxRecursionDepth = value; }

	VkPipeline getPipeline()				{ return m_Pipeline; }
	uint32_t getNumRaygenShaderGroups()		{ return (uint32_t)m_RaygenShaderGroups.size(); }
	uint32_t getNumMissShaderGroups()		{ return (uint32_t)m_MissShaderGroups.size(); }
	uint32_t getNumIntersectShaderGroups()	{ return (uint32_t)m_HitShaderGroups.size(); }
	uint32_t getNumTotalShaderGroups()		{ return (uint32_t)m_AllShaderGroups.size(); }
	
	uint32_t getNumShaders()				{ return (uint32_t)m_Shaders.size(); }

	ShaderBindingTableVK* getSBT()			{ return m_pSBT; }

private:
	void createShaderStageInfo(VkPipelineShaderStageCreateInfo& shaderStageInfo, const ShaderVK* pShader);
	
private:
	GraphicsContextVK* m_pGraphicsContext;
	
	std::vector<VkRayTracingShaderGroupCreateInfoNV> m_RaygenShaderGroups;
	std::vector<VkRayTracingShaderGroupCreateInfoNV> m_MissShaderGroups;
	std::vector<VkRayTracingShaderGroupCreateInfoNV> m_HitShaderGroups;
	std::vector<VkRayTracingShaderGroupCreateInfoNV> m_AllShaderGroups; //Redundant

	std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStagesInfos;
	std::vector<ShaderVK*> m_Shaders;

	uint32_t m_MaxRecursionDepth;

	VkPipeline m_Pipeline;

	ShaderBindingTableVK* m_pSBT;
};
