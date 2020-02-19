#include "SkyboxRendererVK.h"
#include "PipelineVK.h"
#include "PipelineLayoutVK.h"
#include "DeviceVK.h"
#include "CommandPoolVK.h"
#include "CommandBufferVK.h"
#include "DescriptorPoolVK.h"
#include "DescriptorSetLayoutVK.h"
#include "DescriptorSetVK.h"
#include "SamplerVK.h"
#include "ShaderVK.h"
#include "RenderPassVK.h"
#include "Texture2DVK.h"
#include "FrameBufferVK.h"
#include "TextureCubeVK.h"
#include "ReflectionProbeVK.h"

#include <glm/gtc/type_ptr.hpp>

SkyboxRendererVK::SkyboxRendererVK(DeviceVK* pDevice)
	: m_pDevice(pDevice),
	m_pPanoramaPipeline(nullptr),
	m_pPanoramaPipelineLayout(nullptr),
	m_pIrradiancePipeline(nullptr),
	m_pPanoramaDescriptorSet(nullptr),
	m_pPanoramaDescriptorSetLayout(nullptr),
	m_pDescriptorPool(nullptr),
	m_pPanoramaSampler(nullptr),
	m_pPanoramaRenderpass(nullptr),
	m_ppCommandPools(),
	m_ppCommandBuffers(),
	m_CurrentFrame(0)
{
}

SkyboxRendererVK::~SkyboxRendererVK()
{
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		SAFEDELETE(m_ppCommandPools[i]);
	}
	
	SAFEDELETE(m_pPanoramaSampler);
	SAFEDELETE(m_pPanoramaRenderpass);
	SAFEDELETE(m_pDescriptorPool);

	SAFEDELETE(m_pPanoramaPipeline);
	SAFEDELETE(m_pPanoramaPipelineLayout);
	SAFEDELETE(m_pPanoramaDescriptorSetLayout);

	SAFEDELETE(m_pIrradiancePipeline);
}

bool SkyboxRendererVK::init()
{
	SamplerParams samplerParams = {};
	samplerParams.MagFilter = VK_FILTER_LINEAR;
	samplerParams.MinFilter = VK_FILTER_LINEAR;
	samplerParams.WrapModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerParams.WrapModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerParams.WrapModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	m_pPanoramaSampler = DBG_NEW SamplerVK(m_pDevice);
	if (!m_pPanoramaSampler->init(samplerParams))
	{
		return false;
	}

	if (!createCommandpoolsAndBuffers())
	{
		return false;
	}

	if (!createPipelineLayouts())
	{
		return false;
	}

	if (!createRenderPasses())
	{
		return false;
	}

	if (!createPipelines())
	{
		return false;
	}

	return true;
}

void SkyboxRendererVK::generateCubemapFromPanorama(TextureCubeVK* pCubemap, Texture2DVK* pPanorama)
{
	ReflectionProbeVK* pReflectionProbe = DBG_NEW ReflectionProbeVK(m_pDevice);
	if (!pReflectionProbe->initFromTextureCube(pCubemap, m_pPanoramaRenderpass))
	{
		return;
	}

	//Setup
	uint32_t width = pCubemap->getWidth();

	VkViewport viewport = {};
	viewport.width		= float(width);
	viewport.height		= viewport.width;
	viewport.x			= 0.0f;
	viewport.y			= 0.0f;
	viewport.minDepth	= 0.0f;
	viewport.maxDepth	= 1.0f;

	VkRect2D scissorRect = {};
	scissorRect.offset	= { 0, 0 };
	scissorRect.extent	= { width, width };

	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] =
	{
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	//Setup panorama image
	m_pPanoramaDescriptorSet->writeCombinedImageDescriptor(pPanorama->getImageView(), m_pPanoramaSampler, 0);

	//Draw
	m_ppCommandBuffers[m_CurrentFrame]->reset();
	m_ppCommandPools[m_CurrentFrame]->reset();

	m_ppCommandBuffers[m_CurrentFrame]->begin();

	m_ppCommandBuffers[m_CurrentFrame]->pushConstants(m_pPanoramaPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), (const void*)glm::value_ptr(captureProjection));

	m_ppCommandBuffers[m_CurrentFrame]->setViewports(&viewport, 1);
	m_ppCommandBuffers[m_CurrentFrame]->setScissorRects(&scissorRect, 1);

	m_ppCommandBuffers[m_CurrentFrame]->bindGraphicsPipeline(m_pPanoramaPipeline);
	m_ppCommandBuffers[m_CurrentFrame]->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pPanoramaPipelineLayout, 0, 1, &m_pPanoramaDescriptorSet, 0, nullptr);

	m_ppCommandBuffers[m_CurrentFrame]->transitionImageLayout(pCubemap->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, pCubemap->getMiplevels(), 0, 6);

	VkClearValue clearValue = { 1.0f, 1.0f, 1.0f, 1.0f };
	for (uint32_t i = 0; i < 6; i++)
	{
		FrameBufferVK* pFramebuffer = pReflectionProbe->getFrameBuffer(i);

		m_ppCommandBuffers[m_CurrentFrame]->beginRenderPass(m_pPanoramaRenderpass, pFramebuffer, width, width, &clearValue, 1);

		m_ppCommandBuffers[m_CurrentFrame]->pushConstants(m_pPanoramaPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::mat4), (const void*)glm::value_ptr(captureViews[i]));
		m_ppCommandBuffers[m_CurrentFrame]->drawInstanced(36, 1, 0, 0);

		m_ppCommandBuffers[m_CurrentFrame]->endRenderPass();
	}
	
	m_ppCommandBuffers[m_CurrentFrame]->transitionImageLayout(pCubemap->getImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, pCubemap->getMiplevels(), 0, 6);
	m_ppCommandBuffers[m_CurrentFrame]->end();

	m_pDevice->executeCommandBuffer(m_pDevice->getGraphicsQueue(), m_ppCommandBuffers[m_CurrentFrame], nullptr, 0, 0, nullptr, 0);
	m_pDevice->wait();

	SAFEDELETE(pReflectionProbe);
}

bool SkyboxRendererVK::createCommandpoolsAndBuffers()
{
	const uint32_t queueFamilyIndex = m_pDevice->getQueueFamilyIndices().graphicsFamily.value();
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_ppCommandPools[i] = DBG_NEW CommandPoolVK(m_pDevice, queueFamilyIndex);

		if (!m_ppCommandPools[i]->init())
		{
			return false;
		}

		m_ppCommandBuffers[i] = m_ppCommandPools[i]->allocateCommandBuffer();
		if (m_ppCommandBuffers[i] == nullptr)
		{
			return false;
		}
	}

	return true;
}

bool SkyboxRendererVK::createPipelineLayouts()
{
	DescriptorCounts descriptorCounts = {};
	descriptorCounts.m_SampledImages	= 128;
	descriptorCounts.m_StorageBuffers	= 128;
	descriptorCounts.m_UniformBuffers	= 128;

	m_pDescriptorPool = DBG_NEW DescriptorPoolVK(m_pDevice);
	if (!m_pDescriptorPool->init(descriptorCounts, 16))
	{
		return false;
	}

	//PANORAMA TO CUBEMAP
	m_pPanoramaDescriptorSetLayout = DBG_NEW DescriptorSetLayoutVK(m_pDevice);
	m_pPanoramaDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, 0, 1);
	if (!m_pPanoramaDescriptorSetLayout->finalize())
	{
		return false;
	}

	m_pPanoramaDescriptorSet = m_pDescriptorPool->allocDescriptorSet(m_pPanoramaDescriptorSetLayout);
	if (!m_pPanoramaDescriptorSet)
	{
		return false;
	}

	//Matrices
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.size			= sizeof(glm::mat4) * 2;
	pushConstantRange.stageFlags	= VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset		= 0;

	std::vector<VkPushConstantRange> pushConstantRanges = { pushConstantRange };
	std::vector<const DescriptorSetLayoutVK*> descriptorSetLayouts = { m_pPanoramaDescriptorSetLayout };

	m_pPanoramaPipelineLayout = DBG_NEW PipelineLayoutVK(m_pDevice);
	if (!m_pPanoramaPipelineLayout->init(descriptorSetLayouts, pushConstantRanges))
	{
		return false;
	}

	return true;
}

bool SkyboxRendererVK::createPipelines()
{
	//Geometry Pass
	ShaderVK* pVertexShader = DBG_NEW ShaderVK(m_pDevice);
	pVertexShader->initFromFile(EShader::VERTEX_SHADER, "main", "assets/shaders/panoramaCubemapVertex.spv");
	if (!pVertexShader->finalize())
	{
		return false;
	}

	ShaderVK* pPixelShader = DBG_NEW ShaderVK(m_pDevice);
	pPixelShader->initFromFile(EShader::PIXEL_SHADER, "main", "assets/shaders/panoramaCubemapFragment.spv");
	if (!pPixelShader->finalize())
	{
		return false;
	}

	m_pPanoramaPipeline = DBG_NEW PipelineVK(m_pDevice);

	VkPipelineColorBlendAttachmentState blendAttachment = {};
	blendAttachment.blendEnable		= VK_FALSE;
	blendAttachment.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	m_pPanoramaPipeline->addColorBlendAttachment(blendAttachment);

	VkPipelineRasterizationStateCreateInfo rasterizerState = {};
	rasterizerState.cullMode	= VK_CULL_MODE_BACK_BIT;
	rasterizerState.frontFace	= VK_FRONT_FACE_CLOCKWISE;
	rasterizerState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerState.lineWidth	= 1.0f;
	m_pPanoramaPipeline->setRasterizerState(rasterizerState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.depthTestEnable	= VK_FALSE;
	depthStencilState.depthWriteEnable	= VK_FALSE;
	depthStencilState.depthCompareOp	= VK_COMPARE_OP_LESS;
	depthStencilState.stencilTestEnable = VK_FALSE;
	m_pPanoramaPipeline->setDepthStencilState(depthStencilState);

	std::vector<IShader*> shaders = { pVertexShader, pPixelShader };
	if (!m_pPanoramaPipeline->finalize(shaders, m_pPanoramaRenderpass, m_pPanoramaPipelineLayout))
	{
		return false;
	}

	SAFEDELETE(pVertexShader);
	SAFEDELETE(pPixelShader);

	return true;
}

bool SkyboxRendererVK::createRenderPasses()
{
	//Create renderpass for panorama
	m_pPanoramaRenderpass = DBG_NEW RenderPassVK(m_pDevice);
	VkAttachmentDescription description = {};
	description.format			= VK_FORMAT_R16G16B16A16_SFLOAT;
	description.samples			= VK_SAMPLE_COUNT_1_BIT;
	description.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	description.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	description.finalLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	m_pPanoramaRenderpass->addAttachment(description);

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment	= 0;
	colorAttachmentRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	m_pPanoramaRenderpass->addSubpass(&colorAttachmentRef, 1, nullptr);

	VkSubpassDependency dependency = {};
	dependency.dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask	= 0;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	m_pPanoramaRenderpass->addSubpassDependency(dependency);
	return m_pPanoramaRenderpass->finalize();
}