#include "RendererVK.h"
#include "ContextVK.h"
#include "PipelineVK.h"
#include "SwapChainVK.h"
#include "RenderPassVK.h"
#include "CommandPoolVK.h"
#include "FrameBufferVK.h"
#include "CommandBufferVK.h"
#include "PipelineLayoutVK.h"

RendererVK::RendererVK(ContextVK* pContext)
	: m_pContext(pContext),
	m_ppCommandPools(),
	m_ppCommandBuffers(),
	m_pRenderPass(nullptr),
	m_ppBackbuffers(),
	m_pPipeline(nullptr),
	m_ClearColor(),
	m_ClearDepth(),
	m_Viewport(),
	m_ScissorRect(),
	m_CurrentFrame(0),
	m_BackBufferIndex(0)
{
	m_ClearDepth.depthStencil.depth = 1.0f;
	m_ClearDepth.depthStencil.stencil = 0;
}

RendererVK::~RendererVK()
{
	m_pContext->getDevice()->wait();

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		SAFEDELETE(m_ppCommandPools[i]);
		SAFEDELETE(m_ppBackbuffers[i]);
	}

	SAFEDELETE(m_pRenderPass);
	SAFEDELETE(m_pPipeline);
	m_pContext = nullptr;
}

bool RendererVK::init()
{
	DeviceVK* pDevice = m_pContext->getDevice();
	SwapChainVK* pSwapChain = m_pContext->getSwapChain();

	//Create renderpass
	m_pRenderPass = new RenderPassVK(pDevice);
	VkAttachmentDescription description = {};
	description.format	= VK_FORMAT_B8G8R8A8_UNORM;
	description.samples = VK_SAMPLE_COUNT_1_BIT;
	description.loadOp	= VK_ATTACHMENT_LOAD_OP_CLEAR;				//Clear Before Rendering
	description.storeOp	= VK_ATTACHMENT_STORE_OP_STORE;				//Store After Rendering
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;	//Dont care about stencil before
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;	//Still dont care about stencil
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;			//Dont care about what the initial layout of the image is (We do not need to save this memory)
	description.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		//The image will be used for presenting
	m_pRenderPass->addAttachment(description);

	description.format	= VK_FORMAT_D24_UNORM_S8_UINT;
	description.samples = VK_SAMPLE_COUNT_1_BIT;
	description.loadOp	= VK_ATTACHMENT_LOAD_OP_CLEAR;				//Clear Before Rendering
	description.storeOp	= VK_ATTACHMENT_STORE_OP_STORE;				//Store After Rendering
	description.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;	//Dont care about stencil before (If we need the stencil this needs to change)
	description.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;	//Still dont care about stencil	 (If we need the stencil this needs to change)
	description.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;							//Dont care about what the initial layout of the image is (We do not need to save this memory)
	description.finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;		//The image will be used as depthstencil
	m_pRenderPass->addAttachment(description);

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthStencilAttachmentRef = {};
	depthStencilAttachmentRef.attachment = 1;
	depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	m_pRenderPass->addSubpass(&colorAttachmentRef, 1, &depthStencilAttachmentRef);
	m_pRenderPass->addSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	m_pRenderPass->finalize();

	//Create commandbuffers, -pools and framebuffers
	const uint32_t queueFamilyIndex = pDevice->getQueueFamilyIndices().graphicsFamily.value();

	VkExtent2D extent = pSwapChain->getExtent();
	ImageViewVK* pDepthStencilView = pSwapChain->getDepthStencilView();
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_ppCommandPools[i]	= new CommandPoolVK(pDevice, queueFamilyIndex);
		m_ppCommandPools[i]->init();
		m_ppCommandBuffers[i]	= m_ppCommandPools[i]->allocateCommandBuffer();

		m_ppBackbuffers[i] = new FrameBufferVK(pDevice);
		m_ppBackbuffers[i]->addColorAttachment(pSwapChain->getImageView(i));
		m_ppBackbuffers[i]->setDepthStencilAttachment(pDepthStencilView);
		m_ppBackbuffers[i]->finalize(m_pRenderPass, extent.width, extent.height);
	}

	//Create pipelinestate
	IShader* pVertexShader = m_pContext->createShader();
	pVertexShader->loadFromFile(EShader::VERTEX_SHADER, "main", "assets/shaders/vertex.spv");
	pVertexShader->finalize();

	IShader* pPixelShader = m_pContext->createShader();
	pPixelShader->loadFromFile(EShader::PIXEL_SHADER, "main", "assets/shaders/fragment.spv");
	pPixelShader->finalize();

	DescriptorSetLayoutVK* pDescriptorSetLayout = new DescriptorSetLayoutVK(pDevice);
	pDescriptorSetLayout->finalizeLayout();

	std::vector<const DescriptorSetLayoutVK*> descriptorSetLayouts = { pDescriptorSetLayout };
	m_pPipelineLayout = new PipelineLayoutVK();
	m_pPipelineLayout->createPipelineLayout(pDevice, descriptorSetLayouts);

	std::vector<IShader*> shaders = { pVertexShader, pPixelShader };
	m_pPipeline = new PipelineVK();
	m_pPipeline->create(shaders, m_pRenderPass, m_pPipelineLayout, pDevice);

	SAFEDELETE(pVertexShader);
	SAFEDELETE(pPixelShader);

	//Create semaphores
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.flags = 0;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VK_CHECK_RESULT(vkCreateSemaphore(m_pContext->getDevice()->getDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]), "Failed to create semaphores for a Frame");
		VK_CHECK_RESULT(vkCreateSemaphore(m_pContext->getDevice()->getDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]), "Failed to create semaphores for a Frame");
	}

	return true;
}

void RendererVK::beginFrame()
{
	m_pContext->getSwapChain()->acquireNextImage(m_ImageAvailableSemaphores[m_CurrentFrame]);
	uint32_t backBufferIndex = m_pContext->getSwapChain()->getImageIndex();

	m_ppCommandBuffers[m_CurrentFrame]->reset();
	m_ppCommandPools[m_CurrentFrame]->reset();

	m_ppCommandBuffers[m_CurrentFrame]->begin();

	VkClearValue clearValues[] = { m_ClearColor, m_ClearDepth };
	m_ppCommandBuffers[m_CurrentFrame]->beginRenderPass(m_pRenderPass, m_ppBackbuffers[backBufferIndex], m_Viewport.width, m_Viewport.height, clearValues, 2);

	m_ppCommandBuffers[m_CurrentFrame]->setViewports(&m_Viewport, 1);
	m_ppCommandBuffers[m_CurrentFrame]->setScissorRects(&m_ScissorRect, 1);

}

void RendererVK::endFrame()
{
	m_ppCommandBuffers[m_CurrentFrame]->endRenderPass();
	m_ppCommandBuffers[m_CurrentFrame]->end();

	//Submit
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;

	VkSemaphore waitSemaphores[]	= { m_ImageAvailableSemaphores[m_CurrentFrame] };
	submitInfo.waitSemaphoreCount	= 1;
	submitInfo.pWaitSemaphores = waitSemaphores;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.pWaitDstStageMask = waitStages;

	VkCommandBuffer commandBuffers[] = { m_ppCommandBuffers[m_CurrentFrame]->getCommandBuffer() };
	submitInfo.pCommandBuffers		= commandBuffers;
	submitInfo.commandBufferCount	= 1;

	VkSemaphore signalSemaphores[]	= { m_RenderFinishedSemaphores[m_CurrentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores	= signalSemaphores;

	VK_CHECK_RESULT(vkQueueSubmit(m_pContext->getDevice()->getGraphicsQueue(), 1, &submitInfo, m_ppCommandBuffers[m_CurrentFrame]->getFence()), "vkQueueSubmit failed");
}

void RendererVK::setClearColor(float r, float g, float b)
{
	m_ClearColor.color.float32[0] = r;
	m_ClearColor.color.float32[1] = g;
	m_ClearColor.color.float32[2] = b;
	m_ClearColor.color.float32[3] = 1.0f;
}

void RendererVK::setViewport(float width, float height, float minDepth, float maxDepth, float topX, float topY)
{
	m_Viewport.x = topX;
	m_Viewport.y = topY;
	m_Viewport.width	= width;
	m_Viewport.height	= height;
	m_Viewport.minDepth = minDepth;
	m_Viewport.maxDepth = maxDepth;

	m_ScissorRect.extent.width = width;
	m_ScissorRect.extent.height = height;
	m_ScissorRect.offset.x = 0;
	m_ScissorRect.offset.y = 0;
}

void RendererVK::swapBuffers()
{
	m_pContext->swapBuffers(m_RenderFinishedSemaphores[m_CurrentFrame]);
	m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void RendererVK::drawTriangle()
{
	m_ppCommandBuffers[m_CurrentFrame]->bindGraphicsPipeline(m_pPipeline);
	m_ppCommandBuffers[m_CurrentFrame]->drawInstanced(3, 1, 0, 0);
}