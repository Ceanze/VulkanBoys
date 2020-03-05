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

#include <glm/gtc/type_ptr.hpp>

MeshRendererVK::MeshRendererVK(GraphicsContextVK* pContext, RenderingHandlerVK* pRenderingHandler)
	: m_pContext(pContext),
	m_pRenderingHandler(pRenderingHandler),
	m_ppGeometryPassPools(),
	m_ppGeometryPassBuffers(),
	m_pRenderPass(nullptr),
	m_pSkyboxPipeline(nullptr),
	m_pLightDescriptorSet(nullptr),
	m_pGBufferSampler(nullptr),
	m_pSkyboxPipelineLayout(nullptr),
	m_pGBuffer(nullptr),
	m_pDescriptorPool(nullptr),
	m_pLightDescriptorSetLayout(nullptr),
	m_pCameraBuffer(nullptr),
	m_pLightBuffer(nullptr),
	m_pEnvironmentMap(nullptr),
	m_pIntegrationLUT(nullptr),
	m_pProfiler(nullptr),
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

	SAFEDELETE(m_pProfiler);
	SAFEDELETE(m_pBRDFSampler);
	SAFEDELETE(m_pIntegrationLUT);
	SAFEDELETE(m_pEnvironmentMap);
	SAFEDELETE(m_pIrradianceMap);
	SAFEDELETE(m_pSkyboxSampler);
	SAFEDELETE(m_pSkyboxPipeline);
	SAFEDELETE(m_pSkyboxPipelineLayout);
	SAFEDELETE(m_pSkyboxDescriptorSetLayout);
	SAFEDELETE(m_pGBuffer);
	SAFEDELETE(m_pGBufferSampler);
	SAFEDELETE(m_pRenderPass);
	SAFEDELETE(m_pBackBufferRenderPass);
	SAFEDELETE(m_pSkyboxPipelineLayout);
	SAFEDELETE(m_pSkyboxPipeline);
	SAFEDELETE(m_pGeometryPipeline);
	SAFEDELETE(m_pGeometryPipelineLayout);
	SAFEDELETE(m_pGeometryDescriptorSetLayout);
	SAFEDELETE(m_pLightPipeline);
	SAFEDELETE(m_pLightPipelineLayout);
	SAFEDELETE(m_pDescriptorPool);
	SAFEDELETE(m_pLightDescriptorSetLayout);
	SAFEDELETE(m_pCameraBuffer);
	SAFEDELETE(m_pLightBuffer);
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

	if (!createRenderPass())
	{
		return false;
	}

	if (!createGBuffer())
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
	
	if (!createBuffersAndTextures())
	{
		return false;
	}

	if (!generateBRDFLookUp())
	{
		return false;
	}

	ImageViewVK* pAttachment0 = m_pGBuffer->getColorAttachment(0);
	ImageViewVK* pAttachment1 = m_pGBuffer->getColorAttachment(1);
	ImageViewVK* pAttachment2 = m_pGBuffer->getColorAttachment(2);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment0, &m_pGBufferSampler, 1, GBUFFER_ALBEDO_BINDING);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment1, &m_pGBufferSampler, 1, GBUFFER_NORMAL_BINDING);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment2, &m_pGBufferSampler, 1, GBUFFER_POSITION_BINDING);
	m_pLightDescriptorSet->writeUniformBufferDescriptor(m_pLightBuffer, LIGHT_BUFFER_BINDING);
	m_pLightDescriptorSet->writeUniformBufferDescriptor(m_pCameraBuffer, CAMERA_BUFFER_BINDING);

	ImageViewVK* pIntegrationLUT = m_pIntegrationLUT->getImageView();
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pIntegrationLUT, &m_pBRDFSampler, 1, BRDF_LUT_BINDING);

	m_pSkyboxDescriptorSet->writeUniformBufferDescriptor(m_pCameraBuffer, 0);

	return true;
}

void MeshRendererVK::onWindowResize(uint32_t width, uint32_t height)
{
	m_pGBuffer->resize(width, height);

	ImageViewVK* pAttachment0 = m_pGBuffer->getColorAttachment(0);
	ImageViewVK* pAttachment1 = m_pGBuffer->getColorAttachment(1);
	ImageViewVK* pAttachment2 = m_pGBuffer->getColorAttachment(2);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment0, &m_pGBufferSampler, 1, GBUFFER_ALBEDO_BINDING);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment1, &m_pGBufferSampler, 1, GBUFFER_NORMAL_BINDING);
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pAttachment2, &m_pGBufferSampler, 1, GBUFFER_POSITION_BINDING);
}

void MeshRendererVK::beginFrame(const Camera& camera, const LightSetup& lightSetup)
{
	UNREFERENCED_PARAMETER(camera);
	UNREFERENCED_PARAMETER(lightSetup);

	m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	m_ppGeometryPassBuffers[m_CurrentFrame]->reset(false);
	m_ppGeometryPassPools[m_CurrentFrame]->reset();

	// Needed to begin a secondary buffer
	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType		= VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.pNext		= nullptr;
	inheritanceInfo.renderPass	= m_pRenderPass->getRenderPass();
	//TODO: Not use subpass zero all the time?
	inheritanceInfo.subpass		= 0;
	inheritanceInfo.framebuffer = m_pGBuffer->getFrameBuffer()->getFrameBuffer();

	m_ppGeometryPassBuffers[m_CurrentFrame]->begin(&inheritanceInfo, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
	m_pProfiler->beginFrame(m_ppGeometryPassBuffers[m_CurrentFrame]);

	// Begin geometrypass
	m_ppGeometryPassBuffers[m_CurrentFrame]->setViewports(&m_Viewport, 1);
	m_ppGeometryPassBuffers[m_CurrentFrame]->setScissorRects(&m_ScissorRect, 1);
}

void MeshRendererVK::endFrame()
{
	m_pProfiler->endFrame();

	m_ppGeometryPassBuffers[m_CurrentFrame]->bindPipeline(m_pSkyboxPipeline);
	m_ppGeometryPassBuffers[m_CurrentFrame]->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pSkyboxPipelineLayout, 0, 1, &m_pSkyboxDescriptorSet, 0, nullptr);
	m_ppGeometryPassBuffers[m_CurrentFrame]->drawInstanced(36, 1, 0, 0);

	m_ppGeometryPassBuffers[m_CurrentFrame]->end();
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

void MeshRendererVK::setupFrame(CommandBufferVK* pPrimaryBuffer, const Camera& camera, const LightSetup& lightsetup)
{
	m_pProfiler->reset(m_CurrentFrame, pPrimaryBuffer);

	updateBuffers(pPrimaryBuffer, camera, lightsetup);

	//Start renderpass
	VkClearValue clearValues[] = { m_ClearColor, m_ClearColor, m_ClearColor, m_ClearDepth };
	pPrimaryBuffer->beginRenderPass(m_pRenderPass, m_pGBuffer->getFrameBuffer(), (uint32_t)m_Viewport.width, (uint32_t)m_Viewport.height, clearValues, 4, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}

void MeshRendererVK::finalizeFrame(CommandBufferVK* pPrimaryBuffer)
{
	pPrimaryBuffer->executeSecondary(m_ppGeometryPassBuffers[m_CurrentFrame]);
	pPrimaryBuffer->endRenderPass();
}

void MeshRendererVK::updateBuffers(CommandBufferVK* pPrimaryBuffer, const Camera& camera, const LightSetup& lightSetup)
{
	//Update camera
	CameraBuffer cameraBuffer = {};
	cameraBuffer.Projection		= camera.getProjectionMat();
	cameraBuffer.View			= camera.getViewMat();
	cameraBuffer.Position		= glm::vec4(camera.getPosition(), 1.0f);
	pPrimaryBuffer->updateBuffer(m_pCameraBuffer, 0, (const void*)&cameraBuffer, sizeof(CameraBuffer));

	//Update lights
	const uint32_t numPointLights = lightSetup.getPointLightCount();
	pPrimaryBuffer->updateBuffer(m_pLightBuffer, 0, (const void*)lightSetup.getPointLights(), sizeof(PointLight) * numPointLights);
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
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pIrradianceMapView, &m_pSkyboxSampler, 1, IRRADIANCE_BINDING);
	ImageViewVK* pEnvironmentMapView = m_pEnvironmentMap->getImageView();
	m_pLightDescriptorSet->writeCombinedImageDescriptors(&pEnvironmentMapView, &m_pSkyboxSampler, 1, ENVIRONMENT_BINDING);
}

void MeshRendererVK::submitMesh(const MeshVK* pMesh, const Material* pMaterial, const glm::vec3& materialProperties, const glm::mat4& transform)
{
	ASSERT(pMesh != nullptr);

	m_ppGeometryPassBuffers[m_CurrentFrame]->bindPipeline(m_pGeometryPipeline);

	m_ppGeometryPassBuffers[m_CurrentFrame]->pushConstants(m_pGeometryPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4), (const void*)glm::value_ptr(transform));
	m_ppGeometryPassBuffers[m_CurrentFrame]->pushConstants(m_pGeometryPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec4), (const void*)glm::value_ptr(pMaterial->getAlbedo()));
	m_ppGeometryPassBuffers[m_CurrentFrame]->pushConstants(m_pGeometryPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), sizeof(glm::vec3), (const void*)glm::value_ptr(materialProperties));

	BufferVK* pIndexBuffer = reinterpret_cast<BufferVK*>(pMesh->getIndexBuffer());
	m_ppGeometryPassBuffers[m_CurrentFrame]->bindIndexBuffer(pIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

	DescriptorSetVK* pDescriptorSet = getDescriptorSetFromMeshAndMaterial(pMesh, pMaterial);
	m_ppGeometryPassBuffers[m_CurrentFrame]->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pGeometryPipelineLayout, 0, 1, &pDescriptorSet, 0, nullptr);

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

	m_ppLightPassBuffers[m_CurrentFrame]->bindPipeline(m_pLightPipeline);
	m_ppLightPassBuffers[m_CurrentFrame]->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pLightPipelineLayout, 0, 1, &m_pLightDescriptorSet, 0, nullptr);

	m_ppLightPassBuffers[m_CurrentFrame]->setViewports(&m_Viewport, 1);
	m_ppLightPassBuffers[m_CurrentFrame]->setScissorRects(&m_ScissorRect, 1);

	m_ppLightPassBuffers[m_CurrentFrame]->drawInstanced(3, 1, 0, 0);
	m_ppLightPassBuffers[m_CurrentFrame]->end();
}

bool MeshRendererVK::generateBRDFLookUp()
{
	constexpr uint32_t size = 512;

	m_pIntegrationLUT = DBG_NEW Texture2DVK(m_pContext->getDevice());
	if (!m_pIntegrationLUT->initFromMemory(nullptr, size, size, ETextureFormat::FORMAT_R16G16B16A16_FLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false))
	{
		return false;
	}

	RenderPassVK* pRenderPass = DBG_NEW RenderPassVK(m_pContext->getDevice());

	VkAttachmentDescription attachment = {};
	attachment.flags			= 0;
	attachment.format			= VK_FORMAT_R16G16B16A16_SFLOAT;
	attachment.samples			= VK_SAMPLE_COUNT_1_BIT;
	attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	pRenderPass->addAttachment(attachment);

	VkAttachmentReference attachmentRef = {};
	attachmentRef.attachment	= 0;
	attachmentRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	pRenderPass->addSubpass(&attachmentRef, 1, nullptr);

	VkSubpassDependency dependency = {};
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;
	dependency.srcAccessMask	= 0;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	pRenderPass->addSubpassDependency(dependency);
	if (!pRenderPass->finalize())
	{
		return false;
	}

	ImageViewParams imageViewParams = {};
	imageViewParams.Type			= VK_IMAGE_VIEW_TYPE_2D;
	imageViewParams.FirstLayer		= 0;
	imageViewParams.LayerCount		= 1;
	imageViewParams.FirstMipLevel	= 0;
	imageViewParams.MipLevels		= 1;
	imageViewParams.AspectFlags		= VK_IMAGE_ASPECT_COLOR_BIT;

	ImageViewVK* pImageView = DBG_NEW ImageViewVK(m_pContext->getDevice(), m_pIntegrationLUT->getImage());
	if (!pImageView->init(imageViewParams))
	{
		return false;
	}

	FrameBufferVK* pFrameBuffer = DBG_NEW FrameBufferVK(m_pContext->getDevice());
	pFrameBuffer->addColorAttachment(pImageView);
	if (!pFrameBuffer->finalize(pRenderPass, size, size))
	{
		return false;
	}

	IShader* pVertexShader = m_pContext->createShader();
	pVertexShader->initFromFile(EShader::VERTEX_SHADER, "main", "assets/shaders/fullscreenVertex.spv");
	if (!pVertexShader->finalize())
	{
		return false;
	}

	IShader* pPixelShader = m_pContext->createShader();
	pPixelShader->initFromFile(EShader::PIXEL_SHADER, "main", "assets/shaders/genIntegrationMap.spv");
	if (!pPixelShader->finalize())
	{
		return false;
	}

	std::vector<VkPushConstantRange> pushConstantRanges = {};
	std::vector<const DescriptorSetLayoutVK*> descriptorSetLayouts = {};

	PipelineLayoutVK* pPipelineLayout = DBG_NEW PipelineLayoutVK(m_pContext->getDevice());
	if (!pPipelineLayout->init(descriptorSetLayouts, pushConstantRanges))
	{
		return false;
	}

	PipelineVK* pPipeline = DBG_NEW PipelineVK(m_pContext->getDevice());

	VkPipelineColorBlendAttachmentState blendstate = {};
	blendstate.blendEnable		= VK_FALSE;
	blendstate.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	pPipeline->addColorBlendAttachment(blendstate);

	VkPipelineDepthStencilStateCreateInfo depthState = {};
	depthState.depthTestEnable		= VK_FALSE;
	depthState.depthWriteEnable		= VK_FALSE;
	depthState.stencilTestEnable	= VK_FALSE;
	pPipeline->setDepthStencilState(depthState);

	VkPipelineRasterizationStateCreateInfo rasterizerState = {};
	rasterizerState.cullMode	= VK_CULL_MODE_NONE;
	rasterizerState.lineWidth	= 1.0f;
	rasterizerState.frontFace	= VK_FRONT_FACE_CLOCKWISE;
	pPipeline->setRasterizerState(rasterizerState);

	std::vector<const IShader*> shaders = { pVertexShader, pPixelShader };
	if (!pPipeline->finalizeGraphics(shaders, pRenderPass, pPipelineLayout))
	{
		return false;
	}

	DeviceVK* pDevice = m_pContext->getDevice();
	CommandPoolVK* pCommandPool = DBG_NEW CommandPoolVK(pDevice, pDevice->getQueueFamilyIndices().graphicsFamily.value());
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
	pCommandBuffer->transitionImageLayout(m_pIntegrationLUT->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1, 0, 1);

	pCommandBuffer->beginRenderPass(pRenderPass, pFrameBuffer, size, size, nullptr, 0, VK_SUBPASS_CONTENTS_INLINE);

	pCommandBuffer->bindPipeline(pPipeline);

	VkViewport viewport = {};
	viewport.width		= float(size);
	viewport.height		= float(size);
	viewport.minDepth	= 0.0f;
	viewport.maxDepth	= 1.0f;
	viewport.x			= 0.0f;
	viewport.y			= 0.0f;
	pCommandBuffer->setViewports(&viewport, 1);

	VkRect2D scissorRect = {};
	scissorRect.offset = { 0, 0 };
	scissorRect.extent = { size, size };
	pCommandBuffer->setScissorRects(&scissorRect, 1);

	pCommandBuffer->drawInstanced(3, 1, 0, 0);

	pCommandBuffer->endRenderPass();

	pCommandBuffer->transitionImageLayout(m_pIntegrationLUT->getImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1, 0, 1);
	pCommandBuffer->end();

	pDevice->executeCommandBuffer(pDevice->getGraphicsQueue(), pCommandBuffer, nullptr, nullptr, 0, nullptr, 0);
	pDevice->wait();

	SAFEDELETE(pCommandPool);
	SAFEDELETE(pPipeline);
	SAFEDELETE(pPipeline);
	SAFEDELETE(pPipelineLayout);
	SAFEDELETE(pVertexShader);
	SAFEDELETE(pPixelShader);
	SAFEDELETE(pRenderPass);
	SAFEDELETE(pImageView);
	SAFEDELETE(pFrameBuffer);

	return true;
}

bool MeshRendererVK::createGBuffer()
{
	VkExtent2D extent = m_pContext->getSwapChain()->getExtent();

	m_pGBuffer = DBG_NEW GBufferVK(m_pContext->getDevice());
	m_pGBuffer->addColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM);
	m_pGBuffer->addColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
	m_pGBuffer->addColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
	m_pGBuffer->setDepthAttachmentFormat(VK_FORMAT_D24_UNORM_S8_UINT);
	return m_pGBuffer->finalize(m_pRenderPass, extent.width, extent.height);
}

bool MeshRendererVK::createCommandPoolAndBuffers()
{
	DeviceVK* pDevice = m_pContext->getDevice();

	const uint32_t graphicsQueueIndex = pDevice->getQueueFamilyIndices().graphicsFamily.value();
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
	}

	return true;
}

bool MeshRendererVK::createRenderPass()
{
	//Create renderpass
	m_pBackBufferRenderPass = DBG_NEW RenderPassVK(m_pContext->getDevice());

	VkAttachmentDescription description = {};
	description.format			= VK_FORMAT_B8G8R8A8_UNORM;
	description.samples			= VK_SAMPLE_COUNT_1_BIT;
	description.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	description.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	description.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	m_pBackBufferRenderPass->addAttachment(description);

	description.format			= VK_FORMAT_D24_UNORM_S8_UINT;
	description.samples			= VK_SAMPLE_COUNT_1_BIT;
	description.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	description.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	description.finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	m_pBackBufferRenderPass->addAttachment(description);

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment	= 0;
	colorAttachmentRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthStencilAttachmentRef = {};
	depthStencilAttachmentRef.attachment	= 1;
	depthStencilAttachmentRef.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	m_pBackBufferRenderPass->addSubpass(&colorAttachmentRef, 1, &depthStencilAttachmentRef);

	VkSubpassDependency dependency = {};
	dependency.dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask	= 0;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	m_pBackBufferRenderPass->addSubpassDependency(dependency);
	if (!m_pBackBufferRenderPass->finalize())
	{
		return false;
	}

	//Create renderpass
	m_pRenderPass = DBG_NEW RenderPassVK(m_pContext->getDevice());

	//Albedo
	description.format			= VK_FORMAT_R8G8B8A8_UNORM;
	description.samples			= VK_SAMPLE_COUNT_1_BIT;
	description.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	description.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	description.finalLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_pRenderPass->addAttachment(description);

	//Normals
	description.format			= VK_FORMAT_R16G16B16A16_SFLOAT;
	description.samples			= VK_SAMPLE_COUNT_1_BIT;
	description.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	description.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	description.finalLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_pRenderPass->addAttachment(description);

	//World position
	description.format			= VK_FORMAT_R16G16B16A16_SFLOAT;
	description.samples			= VK_SAMPLE_COUNT_1_BIT;
	description.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	description.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	description.finalLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_pRenderPass->addAttachment(description);

	//Depth
	description.format			= VK_FORMAT_D24_UNORM_S8_UINT;
	description.samples			= VK_SAMPLE_COUNT_1_BIT;
	description.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	description.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	description.finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	m_pRenderPass->addAttachment(description);

	VkAttachmentReference colorAttachmentRefs[3];
	//Albedo
	colorAttachmentRefs[0].attachment	= 0;
	colorAttachmentRefs[0].layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//Normals
	colorAttachmentRefs[1].attachment	= 1;
	colorAttachmentRefs[1].layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//Positions
	colorAttachmentRefs[2].attachment	= 2;
	colorAttachmentRefs[2].layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthStencilAttachmentRef = {};
	depthStencilAttachmentRef.attachment	= 3;
	depthStencilAttachmentRef.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	m_pRenderPass->addSubpass(colorAttachmentRefs, 3, &depthStencilAttachmentRef);

	dependency.dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	m_pRenderPass->addSubpassDependency(dependency);

	dependency.srcSubpass		= 0;
	dependency.dstSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependency.srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	m_pRenderPass->addSubpassDependency(dependency);

	return m_pRenderPass->finalize();
}

bool MeshRendererVK::createPipelines()
{
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
	rasterizerState.cullMode		= VK_CULL_MODE_BACK_BIT;
	rasterizerState.frontFace		= VK_FRONT_FACE_CLOCKWISE;
	rasterizerState.polygonMode		= VK_POLYGON_MODE_FILL;
	rasterizerState.lineWidth		= 1.0f;
	m_pGeometryPipeline->setRasterizerState(rasterizerState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.depthTestEnable	= VK_TRUE;
	depthStencilState.depthWriteEnable	= VK_TRUE;
	depthStencilState.depthCompareOp	= VK_COMPARE_OP_LESS;
	depthStencilState.stencilTestEnable = VK_FALSE;
	m_pGeometryPipeline->setDepthStencilState(depthStencilState);

	std::vector<const IShader*> shaders = { pVertexShader, pPixelShader };
	if (!m_pGeometryPipeline->finalizeGraphics(shaders, m_pRenderPass, m_pGeometryPipelineLayout))
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

	shaders = { pVertexShader, pPixelShader };
	m_pLightPipeline = DBG_NEW PipelineVK(m_pContext->getDevice());

	blendAttachment.blendEnable		= VK_FALSE;
	blendAttachment.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	m_pLightPipeline->addColorBlendAttachment(blendAttachment);

	rasterizerState.cullMode	= VK_CULL_MODE_NONE;
	rasterizerState.lineWidth	= 1.0f;
	m_pLightPipeline->setRasterizerState(rasterizerState);

	depthStencilState.depthTestEnable	= VK_TRUE;
	depthStencilState.depthWriteEnable	= VK_TRUE;
	depthStencilState.depthCompareOp	= VK_COMPARE_OP_LESS;
	depthStencilState.stencilTestEnable = VK_FALSE;
	m_pLightPipeline->setDepthStencilState(depthStencilState);
	if (!m_pLightPipeline->finalizeGraphics(shaders, m_pBackBufferRenderPass, m_pLightPipelineLayout))
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

	blendAttachment.blendEnable = VK_FALSE;
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	m_pSkyboxPipeline->addColorBlendAttachment(blendAttachment);
	m_pSkyboxPipeline->addColorBlendAttachment(blendAttachment);
	m_pSkyboxPipeline->addColorBlendAttachment(blendAttachment);

	rasterizerState.cullMode	= VK_CULL_MODE_BACK_BIT;
	rasterizerState.frontFace	= VK_FRONT_FACE_CLOCKWISE;
	rasterizerState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerState.lineWidth	= 1.0f;
	m_pSkyboxPipeline->setRasterizerState(rasterizerState);

	depthStencilState.depthTestEnable	= VK_TRUE;
	depthStencilState.depthWriteEnable	= VK_TRUE;
	depthStencilState.depthCompareOp	= VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.stencilTestEnable = VK_FALSE;
	m_pSkyboxPipeline->setDepthStencilState(depthStencilState);
	if (!m_pSkyboxPipeline->finalizeGraphics(shaders, m_pRenderPass, m_pSkyboxPipelineLayout))
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
	descriptorCounts.m_SampledImages = 128;
	descriptorCounts.m_StorageBuffers = 128;
	descriptorCounts.m_UniformBuffers = 128;

	m_pDescriptorPool = DBG_NEW DescriptorPoolVK(m_pContext->getDevice());
	if (!m_pDescriptorPool->init(descriptorCounts, 16))
	{
		return false;
	}

	//GeometryPass
	m_pGeometryDescriptorSetLayout = DBG_NEW DescriptorSetLayoutVK(m_pContext->getDevice());
	m_pGeometryDescriptorSetLayout->addBindingUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, CAMERA_BUFFER_BINDING, 1);
	m_pGeometryDescriptorSetLayout->addBindingStorageBuffer(VK_SHADER_STAGE_VERTEX_BIT, VERTEX_BUFFER_BINDING, 1);
	m_pGeometryDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, ALBEDO_MAP_BINDING, 1);
	m_pGeometryDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, NORMAL_MAP_BINDING, 1);
	m_pGeometryDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, AO_MAP_BINDING, 1);
	m_pGeometryDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, METALLIC_MAP_BINDING, 1);
	m_pGeometryDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, ROUGHNESS_MAP_BINDING, 1);

	if (!m_pGeometryDescriptorSetLayout->finalize())
	{
		return false;
	}

	//Transform and Color
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.size = sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(glm::vec3);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	std::vector<VkPushConstantRange> pushConstantRanges = { pushConstantRange };
	std::vector<const DescriptorSetLayoutVK*> descriptorSetLayouts = { m_pGeometryDescriptorSetLayout };

	m_pGeometryPipelineLayout = DBG_NEW PipelineLayoutVK(m_pContext->getDevice());
	if (!m_pGeometryPipelineLayout->init(descriptorSetLayouts, pushConstantRanges))
	{
		return false;
	}

	//Lightpass
	m_pLightDescriptorSetLayout = DBG_NEW DescriptorSetLayoutVK(m_pContext->getDevice());
	m_pLightDescriptorSetLayout->addBindingUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, LIGHT_BUFFER_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, CAMERA_BUFFER_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, GBUFFER_ALBEDO_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, GBUFFER_NORMAL_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, GBUFFER_POSITION_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, IRRADIANCE_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, ENVIRONMENT_BINDING, 1);
	m_pLightDescriptorSetLayout->addBindingCombinedImage(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, BRDF_LUT_BINDING, 1);
	if (!m_pLightDescriptorSetLayout->finalize())
	{
		return false;
	}

	pushConstantRanges = { };
	descriptorSetLayouts = { m_pLightDescriptorSetLayout };

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

bool MeshRendererVK::createBuffersAndTextures()
{
	BufferParams cameraBufferParams = {};
	cameraBufferParams.Usage			= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	cameraBufferParams.SizeInBytes		= sizeof(CameraBuffer);
	cameraBufferParams.MemoryProperty	= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	m_pCameraBuffer = DBG_NEW BufferVK(m_pContext->getDevice());
	if (!m_pCameraBuffer->init(cameraBufferParams))
	{
		return false;
	}

	BufferParams lightBufferParams = {};
	lightBufferParams.Usage				= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	lightBufferParams.SizeInBytes		= sizeof(PointLight) * MAX_POINTLIGHTS;
	lightBufferParams.MemoryProperty	= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	m_pLightBuffer = DBG_NEW BufferVK(m_pContext->getDevice());
	if (!m_pLightBuffer->init(lightBufferParams))
	{
		return false;
	}

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

	return true;
}

DescriptorSetVK* MeshRendererVK::getDescriptorSetFromMeshAndMaterial(const MeshVK* pMesh, const Material* pMaterial)
{
	MeshFilter filter = {};
	filter.pMesh		= pMesh;
	filter.pMaterial	= pMaterial;

	if (m_MeshTable.count(filter) == 0)
	{
		DescriptorSetVK* pDescriptorSet = m_pDescriptorPool->allocDescriptorSet(m_pGeometryDescriptorSetLayout);
		pDescriptorSet->writeUniformBufferDescriptor(m_pCameraBuffer, CAMERA_BUFFER_BINDING);

		BufferVK* pVertBuffer = reinterpret_cast<BufferVK*>(pMesh->getVertexBuffer());
		pDescriptorSet->writeStorageBufferDescriptor(pVertBuffer, VERTEX_BUFFER_BINDING);

		SamplerVK* pSampler = reinterpret_cast<SamplerVK*>(pMaterial->getSampler());

		Texture2DVK* pAlbedo = nullptr;
		if (pMaterial->hasAlbedoMap())
		{
			pAlbedo = reinterpret_cast<Texture2DVK*>(pMaterial->getAlbedoMap());
		}
		else
		{
			pAlbedo = m_pDefaultTexture;
		}
		ImageViewVK* pAlbedoView = pAlbedo->getImageView();
		pDescriptorSet->writeCombinedImageDescriptors(&pAlbedoView, &pSampler, 1, ALBEDO_MAP_BINDING);

		Texture2DVK* pNormal = nullptr;
		if (pMaterial->hasNormalMap())
		{
			pNormal = reinterpret_cast<Texture2DVK*>(pMaterial->getNormalMap());
		}
		else
		{
			pNormal = m_pDefaultNormal;
		}

		ImageViewVK* pNormalView = pNormal->getImageView();
		pDescriptorSet->writeCombinedImageDescriptors(&pNormalView, &pSampler, 1, NORMAL_MAP_BINDING);

		Texture2DVK* pAO = nullptr;
		if (pMaterial->hasAmbientOcclusionMap())
		{
			pAO = reinterpret_cast<Texture2DVK*>(pMaterial->getAmbientOcclusionMap());
		}
		else
		{
			pAO = m_pDefaultTexture;
		}

		ImageViewVK* pAOView = pAO->getImageView();
		pDescriptorSet->writeCombinedImageDescriptors(&pAOView, &pSampler, 1, AO_MAP_BINDING);

		Texture2DVK* pMetallic = nullptr;
		if (pMaterial->hasMetallicMap())
		{
			pMetallic = reinterpret_cast<Texture2DVK*>(pMaterial->getMetallicMap());
		}
		else
		{
			pMetallic = m_pDefaultTexture;
		}

		ImageViewVK* pMetallicView = pMetallic->getImageView();
		pDescriptorSet->writeCombinedImageDescriptors(&pMetallicView, &pSampler, 1, METALLIC_MAP_BINDING);

		Texture2DVK* pRoughness = nullptr;
		if (pMaterial->hasRoughnessMap())
		{
			pRoughness = reinterpret_cast<Texture2DVK*>(pMaterial->getRoughnessMap());
		}
		else
		{
			pRoughness = m_pDefaultTexture;
		}

		ImageViewVK* pRoughnessView = pRoughness->getImageView();
		pDescriptorSet->writeCombinedImageDescriptors(&pRoughnessView, &pSampler, 1, ROUGHNESS_MAP_BINDING);

		MeshPipeline meshPipeline = {};
		meshPipeline.pDescriptorSets = pDescriptorSet;

		m_MeshTable.insert(std::make_pair(filter, meshPipeline));
		return pDescriptorSet;
	}

	MeshPipeline meshPipeline = m_MeshTable[filter];
	return meshPipeline.pDescriptorSets;
}

void MeshRendererVK::createProfiler()
{
	m_pProfiler = DBG_NEW ProfilerVK("Mesh Renderer", m_pContext->getDevice());
	m_pProfiler->initTimestamp(&m_TimestampDrawIndexed, "Draw indexed");
}