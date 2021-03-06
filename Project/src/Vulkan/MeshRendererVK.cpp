#include "MeshRendererVK.h"

#include "Core/Camera.h"
#include "Core/LightSetup.h"

#include "BufferVK.h"
#include "CommandBufferVK.h"
#include "CommandPoolVK.h"
#include "DescriptorPoolVK.h"
#include "DescriptorSetVK.h"
#include "ImguiVK.h"
#include "MeshVK.h"
#include "PipelineVK.h"
#include "RenderPassVK.h"
#include "SamplerVK.h"
#include "SwapChainVK.h"
#include "Texture2DVK.h"
#include "ImageVK.h"
#include "ShaderVK.h"
#include "GBufferVK.h"
#include "TextureCubeVK.h"
#include "FrameBufferVK.h"
#include "ImageViewVK.h"
#include "PipelineLayoutVK.h"
#include "SkyboxRendererVK.h"
#include "GraphicsContextVK.h"
#include "RenderingHandlerVK.h"
#include "SceneVK.h"

#include <glm/gtc/type_ptr.hpp>

MeshRendererVK::MeshRendererVK(GraphicsContextVK* pContext, RenderingHandlerVK* pRenderingHandler)
	: m_pContext(pContext),
	m_pRenderingHandler(pRenderingHandler),
	m_ppGeometryPassPools(),
	m_ppGeometryPassBuffers(),
	m_pSkyboxPipeline(nullptr),
	m_pLightDescriptorSet(nullptr),
	m_pGBufferSampler(nullptr),
	m_pRTSampler(nullptr),
	m_pSkyboxPipelineLayout(nullptr),
	m_pDescriptorPool(nullptr),
	m_pLightDescriptorSetLayout(nullptr),
	m_pMaterialParametersBuffer(nullptr),
	m_pTransformsBuffer(nullptr),
	m_pEnvironmentMap(nullptr),
	m_pIntegrationLUT(nullptr),
	m_pGPassProfiler(nullptr),
	m_pLightPassProfiler(nullptr),
	m_ClearColor(),
	m_ClearDepth(),
	m_Viewport(),
	m_ScissorRect(),
	m_CurrentFrame(0)
{
	m_ClearDepth.depthStencil.depth = 1.0f;
	m_ClearDepth.depthStencil.stencil = 0;
}

MeshRendererVK::~MeshRendererVK()
{
	m_pContext->getDevice()->wait();

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		SAFEDELETE(m_ppGeometryPassPools[i]);
		SAFEDELETE(m_ppLightPassPools[i]);
	}

	SAFEDELETE(m_pGPassProfiler);
	SAFEDELETE(m_pLightPassProfiler);

	SAFEDELETE(m_pBRDFSampler);
	SAFEDELETE(m_pIntegrationLUT);
	SAFEDELETE(m_pEnvironmentMap);
	SAFEDELETE(m_pIrradianceMap);
	SAFEDELETE(m_pSkyboxSampler);
	SAFEDELETE(m_pSkyboxPipeline);
	SAFEDELETE(m_pSkyboxPipelineLayout);
	SAFEDELETE(m_pSkyboxDescriptorSetLayout);
	SAFEDELETE(m_pGBufferSampler);
	SAFEDELETE(m_pRTSampler);
	SAFEDELETE(m_pSkyboxPipelineLayout);
	SAFEDELETE(m_pSkyboxPipeline);
	SAFEDELETE(m_pGeometryPipeline);
	SAFEDELETE(m_pLightPipeline);
	SAFEDELETE(m_pLightPipelineLayout);
	SAFEDELETE(m_pDescriptorPool);
	SAFEDELETE(m_pLightDescriptorSetLayout);
	SAFEDELETE(m_pDefaultTexture);
	SAFEDELETE(m_pDefaultNormal);

	m_pContext = nullptr;
}

bool MeshRendererVK::init()
{
	createProfiler();

	if (!createSamplers())
	{
		return false;
	}

	if (!createCommandPoolAndBuffers())
	{
		return false;
	}

	if (!createPipelineLayouts())
	{
		return false;
	}

	if (!createPipelines())
	{
		return false;
	}

	if (!createTextures())
	{
		return false;
	}

	if (!generateBRDFLookUp())
	{
		return false;
	}

	updateGBufferDescriptors();

	const BufferVK* pLightBuffer	= m_pRenderingHandler->getLightBufferGraphics();
	const BufferVK* pCameraBuffer	= m_pRenderingHandler->getCameraBufferGraphics();
	m_pLightDescriptorSet->writeUniformBufferDescriptor(pLightBuffer,	LP_LIGHT_BUFFER_BINDING);
	m_pLightDescriptorSet->writeUniformBufferDescriptor(pCameraBuffer,	CAMERA_BUFFER_BINDING);

	ImageViewVK* pIntegrationLUT = m_pIntegrationLUT->getImageView();
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pIntegrationLUT, &m_pBRDFSampler, 1, LP_BRDF_LUT_BINDING);

	m_pSkyboxDescriptorSet->writeUniformBufferDescriptor(pCameraBuffer, 0);

	return true;
}

void MeshRendererVK::onWindowResize(uint32_t width, uint32_t height)
{
	UNREFERENCED_PARAMETER(width);
	UNREFERENCED_PARAMETER(height);

	updateGBufferDescriptors();
}

void MeshRendererVK::beginFrame(IScene* pScene)
{
	m_pScene = reinterpret_cast<SceneVK*>(pScene);

	m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	m_ppGeometryPassBuffers[m_CurrentFrame]->reset(false);
	m_ppGeometryPassPools[m_CurrentFrame]->reset();

	// Needed to begin a secondary buffer
	RenderPassVK*	pGeometryRenderPass	= m_pRenderingHandler->getGeometryRenderPass();
	FrameBufferVK*	pFramebuffer		= m_pRenderingHandler->getGBuffer()->getFrameBuffer();

	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType		= VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.pNext		= nullptr;
	inheritanceInfo.renderPass	= pGeometryRenderPass->getRenderPass();
	//TODO: Not use subpass zero all the time?
	inheritanceInfo.subpass		= 0;
	inheritanceInfo.framebuffer = pFramebuffer->getFrameBuffer();

	m_ppGeometryPassBuffers[m_CurrentFrame]->begin(&inheritanceInfo, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
	m_pGPassProfiler->beginFrame(m_ppGeometryPassBuffers[m_CurrentFrame]);

	// Begin geometrypass
	m_ppGeometryPassBuffers[m_CurrentFrame]->setViewports(&m_Viewport, 1);
	m_ppGeometryPassBuffers[m_CurrentFrame]->setScissorRects(&m_ScissorRect, 1);
}

void MeshRendererVK::endFrame(IScene* pScene)
{
	UNREFERENCED_PARAMETER(pScene);

	m_pGPassProfiler->endFrame();

	m_ppGeometryPassBuffers[m_CurrentFrame]->bindPipeline(m_pSkyboxPipeline);
	m_ppGeometryPassBuffers[m_CurrentFrame]->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pSkyboxPipelineLayout, 0, 1, &m_pSkyboxDescriptorSet, 0, nullptr);
	m_ppGeometryPassBuffers[m_CurrentFrame]->drawInstanced(36, 1, 0, 0);

	m_ppGeometryPassBuffers[m_CurrentFrame]->end();
}

void MeshRendererVK::renderUI()
{
}

void MeshRendererVK::setViewport(float width, float height, float minDepth, float maxDepth, float topX, float topY)
{
	m_Viewport.x		= topX;
	m_Viewport.y		= topY;
	m_Viewport.width	= width;
	m_Viewport.height	= height;
	m_Viewport.minDepth = minDepth;
	m_Viewport.maxDepth = maxDepth;

	m_ScissorRect.extent.width	= uint32_t(width);
	m_ScissorRect.extent.height = uint32_t(height);
	m_ScissorRect.offset.x		= 0;
	m_ScissorRect.offset.y		= 0;
}

void MeshRendererVK::updateGBufferDescriptors()
{
	GBufferVK* pGBuffer				= m_pRenderingHandler->getGBuffer();
	ImageViewVK* pAttachment0		= pGBuffer->getColorAttachment(0);
	ImageViewVK* pAttachment1		= pGBuffer->getColorAttachment(1);
	ImageViewVK* pAttachment2		= pGBuffer->getColorAttachment(2);
	ImageViewVK* pDepthAttachment	= pGBuffer->getDepthAttachment();
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment0, &m_pGBufferSampler, 1, LP_GBUFFER_ALBEDO_BINDING);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment1, &m_pGBufferSampler, 1, LP_GBUFFER_NORMAL_BINDING);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment2, &m_pGBufferSampler, 1, LP_GBUFFER_VELOCITY_BINDING);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pDepthAttachment, &m_pGBufferSampler, 1, LP_GBUFFER_DEPTH_BINDING);
}

void MeshRendererVK::setClearColor(float r, float g, float b)
{
	setClearColor(glm::vec3(r, g, b));
}

void MeshRendererVK::setClearColor(const glm::vec3& color)
{
	m_ClearColor.color.float32[0] = color.r;
	m_ClearColor.color.float32[1] = color.g;
	m_ClearColor.color.float32[2] = color.b;
	m_ClearColor.color.float32[3] = 1.0f;
}

void MeshRendererVK::setSkybox(TextureCubeVK* pSkybox, TextureCubeVK* pIrradiance, TextureCubeVK* pEnvironmentMap)
{
	m_pSkybox			= pSkybox;
	m_pIrradianceMap	= pIrradiance;
	m_pEnvironmentMap	= pEnvironmentMap;

	ImageViewVK* pSkyboxView = m_pSkybox->getImageView();
	m_pSkyboxDescriptorSet->writeCombinedImageDescriptors(&pSkyboxView, &m_pSkyboxSampler, 1, 1);
	ImageViewVK* pIrradianceMapView = m_pIrradianceMap->getImageView();
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pIrradianceMapView, &m_pSkyboxSampler, 1, LP_IRRADIANCE_BINDING);
	ImageViewVK* pEnvironmentMapView = m_pEnvironmentMap->getImageView();
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pEnvironmentMapView, &m_pSkyboxSampler, 1, LP_ENVIRONMENT_BINDING);
}

void MeshRendererVK::setRayTracingResultImages(ImageViewVK* pRadianceImageView, ImageViewVK* pGlossyImageView)
{
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pRadianceImageView, &m_pRTSampler, 1, LP_RADIANCE_BINDING);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pGlossyImageView, &m_pRTSampler, 1, LP_GLOSSY_BINDING);
}

void MeshRendererVK::submitMesh(const MeshVK* pMesh, const Material* pMaterial, uint32_t materialIndex, uint32_t transformsIndex)
{
	ASSERT(pMesh != nullptr);

	m_ppGeometryPassBuffers[m_CurrentFrame]->bindPipeline(m_pGeometryPipeline);

	PipelineLayoutVK* pGeometryPassLayout = m_pScene->getGeometryPipelineLayout();

	uint32_t pushConstants[2] = { materialIndex, transformsIndex };
	m_ppGeometryPassBuffers[m_CurrentFrame]->pushConstants(pGeometryPassLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t) * 2, &pushConstants);

	BufferVK* pIndexBuffer = reinterpret_cast<BufferVK*>(pMesh->getIndexBuffer());
	m_ppGeometryPassBuffers[m_CurrentFrame]->bindIndexBuffer(pIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

	DescriptorSetVK* pDescriptorSet = m_pScene->getDescriptorSetFromMeshAndMaterial(pMesh, pMaterial);
	m_ppGeometryPassBuffers[m_CurrentFrame]->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, pGeometryPassLayout, 0, 1, &pDescriptorSet, 0, nullptr);

	m_ppGeometryPassBuffers[m_CurrentFrame]->drawIndexInstanced(pMesh->getIndexCount(), 1, 0, 0, 0);
}

void MeshRendererVK::buildLightPass(RenderPassVK* pRenderPass, FrameBufferVK* pFramebuffer)
{
	m_ppLightPassBuffers[m_CurrentFrame]->reset(false);
	m_ppLightPassPools[m_CurrentFrame]->reset();

	// Needed to begin a secondary buffer
	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType		= VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.pNext		= nullptr;
	inheritanceInfo.renderPass	= pRenderPass->getRenderPass();
	//TODO: Not use subpass zero all the time?
	inheritanceInfo.subpass		= 0;
	inheritanceInfo.framebuffer = pFramebuffer->getFrameBuffer();
	m_ppLightPassBuffers[m_CurrentFrame]->begin(&inheritanceInfo, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

	m_pLightPassProfiler->beginFrame(m_ppLightPassBuffers[m_CurrentFrame]);

	m_ppLightPassBuffers[m_CurrentFrame]->bindPipeline(m_pLightPipeline);
	m_ppLightPassBuffers[m_CurrentFrame]->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pLightPipelineLayout, 0, 1, &m_pLightDescriptorSet, 0, nullptr);

	m_ppLightPassBuffers[m_CurrentFrame]->setViewports(&m_Viewport, 1);
	m_ppLightPassBuffers[m_CurrentFrame]->setScissorRects(&m_ScissorRect, 1);

	m_ppLightPassBuffers[m_CurrentFrame]->drawInstanced(3, 1, 0, 0);

	m_pLightPassProfiler->endFrame();

	m_ppLightPassBuffers[m_CurrentFrame]->end();
}

bool MeshRendererVK::generateBRDFLookUp()
{
	constexpr uint32_t size = 512;

	m_pIntegrationLUT = DBG_NEW Texture2DVK(m_pContext->getDevice());
	if (!m_pIntegrationLUT->initFromMemory(nullptr, size, size, ETextureFormat::FORMAT_R16G16B16A16_FLOAT, VK_IMAGE_USAGE_STORAGE_BIT, false))
	{
		return false;
	}

	ImageViewVK* pImageView = m_pIntegrationLUT->getImageView();

	IShader* pComputeShader = m_pContext->createShader();
	pComputeShader->initFromFile(EShader::COMPUTE_SHADER, "main", "assets/shaders/genIntegrationLUTCompute.spv");
	if (!pComputeShader->finalize())
	{
		return false;
	}

	DescriptorSetLayoutVK* pDescriptorLayout = DBG_NEW DescriptorSetLayoutVK(m_pContext->getDevice());
	pDescriptorLayout->addBindingStorageImage(VK_SHADER_STAGE_COMPUTE_BIT, 0, 1);
	if (!pDescriptorLayout->finalize())
	{
		return false;
	}

	DescriptorSetVK* pDescriptorSet = m_pDescriptorPool->allocDescriptorSet(pDescriptorLayout);
	if (pDescriptorSet)
	{
		pDescriptorSet->writeStorageImageDescriptor(pImageView, 0);
	}
	else
	{
		return false;
	}

	std::vector<VkPushConstantRange> pushConstantRanges = {};
	std::vector<const DescriptorSetLayoutVK*> descriptorSetLayouts = { pDescriptorLayout };

	PipelineLayoutVK* pPipelineLayout = DBG_NEW PipelineLayoutVK(m_pContext->getDevice());
	if (!pPipelineLayout->init(descriptorSetLayouts, pushConstantRanges))
	{
		return false;
	}

	PipelineVK* pPipeline = DBG_NEW PipelineVK(m_pContext->getDevice());
	if (!pPipeline->finalizeCompute(pComputeShader, pPipelineLayout))
	{
		return false;
	}

	DeviceVK* pDevice = m_pContext->getDevice();
	CommandPoolVK* pCommandPool = DBG_NEW CommandPoolVK(pDevice, pDevice->getQueueFamilyIndices().ComputeQueues.value().FamilyIndex);
	if (!pCommandPool->init())
	{
		return false;
	}

	CommandBufferVK* pCommandBuffer = pCommandPool->allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	if (!pCommandBuffer)
	{
		return false;
	}

	pCommandBuffer->reset(true);
	pCommandPool->reset();

	pCommandBuffer->begin(nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	pCommandBuffer->transitionImageLayout(m_pIntegrationLUT->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 1, 0, 1);

	pCommandBuffer->bindPipeline(pPipeline);
	pCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, pPipelineLayout, 0, 1, &pDescriptorSet, 0, nullptr);

	pCommandBuffer->dispatch(size, size, 1);

	pCommandBuffer->transitionImageLayout(m_pIntegrationLUT->getImage(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1, 0, 1);
	pCommandBuffer->end();

	pDevice->executeCompute(pCommandBuffer, nullptr, nullptr, 0, nullptr, 0);
	pDevice->wait();

	SAFEDELETE(pCommandPool);
	SAFEDELETE(pPipeline);
	SAFEDELETE(pPipelineLayout);
	SAFEDELETE(pComputeShader);
	SAFEDELETE(pDescriptorLayout);

	m_pDescriptorPool->deallocateDescriptorSet(pDescriptorSet);

	return true;
}

bool MeshRendererVK::createCommandPoolAndBuffers()
{
	DeviceVK* pDevice = m_pContext->getDevice();

	const uint32_t graphicsQueueIndex = pDevice->getQueueFamilyIndices().GraphicsQueues.value().FamilyIndex;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_ppGeometryPassPools[i] = DBG_NEW CommandPoolVK(pDevice, graphicsQueueIndex);
		if (!m_ppGeometryPassPools[i]->init())
		{
			return false;
		}

		m_ppGeometryPassBuffers[i] = m_ppGeometryPassPools[i]->allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		if (m_ppGeometryPassBuffers[i] == nullptr)
		{
			return false;
		}
		std::string name = "GeometryPass CommandBuffer[" + std::to_string(i) + "]";
		m_ppGeometryPassBuffers[i]->setName(name.c_str());

		m_ppLightPassPools[i] = DBG_NEW CommandPoolVK(pDevice, graphicsQueueIndex);
		if (!m_ppLightPassPools[i]->init())
		{
			return false;
		}

		m_ppLightPassBuffers[i] = m_ppLightPassPools[i]->allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		if (m_ppLightPassBuffers[i] == nullptr)
		{
			return false;
		}
		name = "LightPass CommandBuffer[" + std::to_string(i) + "]";
		m_ppLightPassBuffers[i]->setName(name.c_str());
	}

	return true;
}

bool MeshRendererVK::createPipelines()
{
	RenderPassVK* pGeometryRenderPass	= m_pRenderingHandler->getGeometryRenderPass();
	RenderPassVK* pBackbufferRenderPass = m_pRenderingHandler->getBackBufferRenderPass();

	//Geometry Pass
	IShader* pVertexShader = m_pContext->createShader();
	pVertexShader->initFromFile(EShader::VERTEX_SHADER, "main", "assets/shaders/geometryVertex.spv");
	if (!pVertexShader->finalize())
	{
		return false;
	}

	IShader* pPixelShader = m_pContext->createShader();
	pPixelShader->initFromFile(EShader::PIXEL_SHADER, "main", "assets/shaders/geometryFragment.spv");
	if (!pPixelShader->finalize())
	{
		return false;
	}

	m_pGeometryPipeline = DBG_NEW PipelineVK(m_pContext->getDevice());

	VkPipelineColorBlendAttachmentState blendAttachment = {};
	blendAttachment.blendEnable		= VK_FALSE;
	blendAttachment.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	m_pGeometryPipeline->addColorBlendAttachment(blendAttachment);
	m_pGeometryPipeline->addColorBlendAttachment(blendAttachment);
	m_pGeometryPipeline->addColorBlendAttachment(blendAttachment);

	VkPipelineRasterizationStateCreateInfo rasterizerState = {};
	rasterizerState.cullMode				= VK_CULL_MODE_BACK_BIT;
	rasterizerState.frontFace				= VK_FRONT_FACE_CLOCKWISE;
	rasterizerState.polygonMode				= VK_POLYGON_MODE_FILL;
	rasterizerState.rasterizerDiscardEnable = VK_FALSE;
	rasterizerState.lineWidth				= 1.0f;
	m_pGeometryPipeline->setRasterizerState(rasterizerState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.depthTestEnable	= VK_TRUE;
	depthStencilState.depthWriteEnable	= VK_TRUE;
	depthStencilState.depthCompareOp	= VK_COMPARE_OP_LESS;
	depthStencilState.stencilTestEnable = VK_FALSE;
	m_pGeometryPipeline->setDepthStencilState(depthStencilState);

	SceneVK* pScene = reinterpret_cast<SceneVK*>(m_pRenderingHandler->getScene());

	std::vector<const IShader*> shaders = { pVertexShader, pPixelShader };
	if (!m_pGeometryPipeline->finalizeGraphics(shaders, pGeometryRenderPass, pScene->getGeometryPipelineLayout()))
	{
		return false;
	}

	SAFEDELETE(pVertexShader);
	SAFEDELETE(pPixelShader);

	//Light Pass
	pVertexShader = m_pContext->createShader();
	pVertexShader->initFromFile(EShader::VERTEX_SHADER, "main", "assets/shaders/fullscreenVertex.spv");
	if (!pVertexShader->finalize())
	{
		return false;
	}

	pPixelShader = m_pContext->createShader();
	pPixelShader->initFromFile(EShader::PIXEL_SHADER, "main", "assets/shaders/lightFragment.spv");
	if (!pPixelShader->finalize())
	{
		return false;
	}
	reinterpret_cast<ShaderVK*>(pPixelShader)->setSpecializationConstant<uint32_t>(0, m_pContext->isRayTracingEnabled() ? 1 : 0);

	shaders = { pVertexShader, pPixelShader };
	m_pLightPipeline = DBG_NEW PipelineVK(m_pContext->getDevice());

	blendAttachment.blendEnable		= VK_FALSE;
	blendAttachment.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	m_pLightPipeline->addColorBlendAttachment(blendAttachment);

	rasterizerState.cullMode				= VK_CULL_MODE_NONE;
	rasterizerState.lineWidth				= 1.0f;
	rasterizerState.rasterizerDiscardEnable = VK_FALSE;
	m_pLightPipeline->setRasterizerState(rasterizerState);

	depthStencilState.depthTestEnable	= VK_FALSE;
	depthStencilState.depthWriteEnable	= VK_FALSE;
	depthStencilState.depthCompareOp	= VK_COMPARE_OP_LESS;
	depthStencilState.stencilTestEnable = VK_FALSE;
	m_pLightPipeline->setDepthStencilState(depthStencilState);
	if (!m_pLightPipeline->finalizeGraphics(shaders, pBackbufferRenderPass, m_pLightPipelineLayout))
	{
		return false;
	}

	SAFEDELETE(pVertexShader);
	SAFEDELETE(pPixelShader);

	//Skybox
	pVertexShader = m_pContext->createShader();
	pVertexShader->initFromFile(EShader::VERTEX_SHADER, "main", "assets/shaders/skyboxVertex.spv");
	if (!pVertexShader->finalize())
	{
		return false;
	}

	pPixelShader = m_pContext->createShader();
	pPixelShader->initFromFile(EShader::PIXEL_SHADER, "main", "assets/shaders/skyboxFragment.spv");
	if (!pPixelShader->finalize())
	{
		return false;
	}

	shaders = { pVertexShader, pPixelShader };
	m_pSkyboxPipeline = DBG_NEW PipelineVK(m_pContext->getDevice());

	blendAttachment.blendEnable		= VK_FALSE;
	blendAttachment.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	m_pSkyboxPipeline->addColorBlendAttachment(blendAttachment);
	m_pSkyboxPipeline->addColorBlendAttachment(blendAttachment);
	m_pSkyboxPipeline->addColorBlendAttachment(blendAttachment);

	rasterizerState.cullMode				= VK_CULL_MODE_BACK_BIT;
	rasterizerState.frontFace				= VK_FRONT_FACE_CLOCKWISE;
	rasterizerState.polygonMode				= VK_POLYGON_MODE_FILL;
	rasterizerState.lineWidth				= 1.0f;
	rasterizerState.rasterizerDiscardEnable = VK_FALSE;
	m_pSkyboxPipeline->setRasterizerState(rasterizerState);

	depthStencilState.depthTestEnable	= VK_TRUE;
	depthStencilState.depthWriteEnable	= VK_TRUE;
	depthStencilState.depthCompareOp	= VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.stencilTestEnable = VK_FALSE;
	m_pSkyboxPipeline->setDepthStencilState(depthStencilState);

	if (!m_pSkyboxPipeline->finalizeGraphics(shaders, pGeometryRenderPass, m_pSkyboxPipelineLayout))
	{
		return false;
	}

	SAFEDELETE(pVertexShader);
	SAFEDELETE(pPixelShader);

	return true;
}

bool MeshRendererVK::createPipelineLayouts()
{
	//Descriptorpool
	DescriptorCounts descriptorCounts = {};
	descriptorCounts.m_SampledImages	= 4096;
	descriptorCounts.m_StorageImages	= 1024;
	descriptorCounts.m_StorageBuffers	= 2048;
	descriptorCounts.m_UniformBuffers	= 1024;

	m_pDescriptorPool = DBG_NEW DescriptorPoolVK(m_pContext->getDevice());
	if (!m_pDescriptorPool->init(descriptorCounts, 512))
	{
		return false;
	}

	//Lightpass
	m_pLightDescriptorSetLayout = DBG_NEW DescriptorSetLayoutVK(m_pContext->getDevice());
	m_pLightDescriptorSetLayout->addBindingUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, LP_LIGHT_BUFFER_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, CAMERA_BUFFER_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_GBUFFER_ALBEDO_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_GBUFFER_NORMAL_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_GBUFFER_VELOCITY_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_GBUFFER_DEPTH_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_IRRADIANCE_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_ENVIRONMENT_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_BRDF_LUT_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_RADIANCE_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, LP_GLOSSY_BINDING, 1);
	if (!m_pLightDescriptorSetLayout->finalize())
	{
		return false;
	}

	std::vector<VkPushConstantRange> pushConstantRanges = { };
	std::vector<const DescriptorSetLayoutVK*> descriptorSetLayouts = { m_pLightDescriptorSetLayout };

	m_pLightPipelineLayout = DBG_NEW PipelineLayoutVK(m_pContext->getDevice());
	if (!m_pLightPipelineLayout->init(descriptorSetLayouts, pushConstantRanges))
	{
		return false;
	}

	m_pLightDescriptorSet = m_pDescriptorPool->allocDescriptorSet(m_pLightDescriptorSetLayout);
	if (!m_pLightDescriptorSet)
	{
		return false;
	}

	//Skybox
	m_pSkyboxDescriptorSetLayout = DBG_NEW DescriptorSetLayoutVK(m_pContext->getDevice());
	m_pSkyboxDescriptorSetLayout->addBindingUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, 1);
	m_pSkyboxDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, 1, 1);
	if (!m_pSkyboxDescriptorSetLayout->finalize())
	{
		return false;
	}

	pushConstantRanges = { };
	descriptorSetLayouts = { m_pSkyboxDescriptorSetLayout };

	m_pSkyboxPipelineLayout = DBG_NEW PipelineLayoutVK(m_pContext->getDevice());
	if (!m_pSkyboxPipelineLayout->init(descriptorSetLayouts, pushConstantRanges))
	{
		return false;
	}

	m_pSkyboxDescriptorSet = m_pDescriptorPool->allocDescriptorSet(m_pSkyboxDescriptorSetLayout);
	if (!m_pSkyboxDescriptorSet)
	{
		return false;
	}

	return true;
}

bool MeshRendererVK::createTextures()
{
	uint8_t whitePixels[] = { 255, 255, 255, 255 };
	m_pDefaultTexture = DBG_NEW Texture2DVK(m_pContext->getDevice());
	if (!m_pDefaultTexture->initFromMemory(whitePixels, 1, 1, ETextureFormat::FORMAT_R8G8B8A8_UNORM, 0, false))
	{
		return false;
	}

	uint8_t pixels[] = { 127, 127, 255, 255 };
	m_pDefaultNormal = DBG_NEW Texture2DVK(m_pContext->getDevice());
	return m_pDefaultNormal->initFromMemory(pixels, 1, 1, ETextureFormat::FORMAT_R8G8B8A8_UNORM, 0, false);
}

bool MeshRendererVK::createSamplers()
{
	SamplerParams params = {};
	params.MagFilter = VK_FILTER_NEAREST;
	params.MinFilter = VK_FILTER_NEAREST;
	params.WrapModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	params.WrapModeV = params.WrapModeU;
	params.WrapModeW = params.WrapModeU;

	m_pGBufferSampler = DBG_NEW SamplerVK(m_pContext->getDevice());
	if (!m_pGBufferSampler->init(params))
	{
		return false;
	}

	m_pSkyboxSampler = DBG_NEW SamplerVK(m_pContext->getDevice());
	if (!m_pSkyboxSampler->init(params))
	{
		return false;
	}

	m_pBRDFSampler = DBG_NEW SamplerVK(m_pContext->getDevice());
	if (!m_pBRDFSampler->init(params))
	{
		return false;
	}

	SamplerParams rtParams = {};
	rtParams.MagFilter = VK_FILTER_LINEAR;
	rtParams.MinFilter = VK_FILTER_LINEAR;
	rtParams.WrapModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	rtParams.WrapModeV = params.WrapModeU;
	rtParams.WrapModeW = params.WrapModeU;

	m_pRTSampler = DBG_NEW SamplerVK(m_pContext->getDevice());
	if (!m_pRTSampler->init(rtParams))
	{
		return false;
	}

	return true;
}

void MeshRendererVK::createProfiler()
{
	//m_pGPassProfiler		= DBG_NEW ProfilerVK("Mesh Renderer: Geometry Pass", m_pContext->getDevice());
	//m_pLightPassProfiler	= DBG_NEW ProfilerVK("Mesh Renderer: Light Pass", m_pContext->getDevice());
	//m_pGPassProfiler->initTimestamp(&m_TimestampGeometry, "Draw indexed");
}
