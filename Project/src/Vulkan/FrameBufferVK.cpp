#include "FrameBufferVK.h"

#include "DeviceVK.h"
#include "ImageViewVK.h"
#include "RenderPassVK.h"

FrameBufferVK::FrameBufferVK(DeviceVK* pDevice) :
	m_pDevice(pDevice),
	m_pDepthStencilAttachment(nullptr),
	m_FrameBuffer(VK_NULL_HANDLE)
{
}

FrameBufferVK::~FrameBufferVK()
{
	if (m_FrameBuffer != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(m_pDevice->getDevice(), m_FrameBuffer, nullptr);
		m_FrameBuffer = VK_NULL_HANDLE;
	}
}

void FrameBufferVK::addColorAttachment(ImageViewVK* pImageView)
{
	m_ColorAttachments.push_back(pImageView);
}

void FrameBufferVK::setDepthStencilAttachment(ImageViewVK* pImageView)
{
	m_pDepthStencilAttachment = pImageView;
}

bool FrameBufferVK::finalize(RenderPassVK* pRenderPass, uint32_t width, uint32_t height)
{
	uint32_t attachmentCount = uint32_t(m_ColorAttachments.size()) + (m_pDepthStencilAttachment != nullptr ? 1u : 0u);

	std::vector<VkImageView> attachments(attachmentCount);
	for (uint32_t i = 0; i < m_ColorAttachments.size(); i++)
		attachments[i] = m_ColorAttachments[i]->getImageView();

	if (m_pDepthStencilAttachment != nullptr)
		attachments[attachmentCount - 1] = m_pDepthStencilAttachment->getImageView();
	
	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.pNext = nullptr;
	framebufferInfo.flags = 0;
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.attachmentCount = attachmentCount;
	framebufferInfo.pAttachments	= attachments.data();
	framebufferInfo.renderPass = pRenderPass->getRenderPass();
	framebufferInfo.layers = 1;

	VK_CHECK_RESULT_RETURN_FALSE(vkCreateFramebuffer(m_pDevice->getDevice(), &framebufferInfo, nullptr, &m_FrameBuffer), "--- FrameBufferVK: vkCreateFramebuffer failed");
	return true;
}
