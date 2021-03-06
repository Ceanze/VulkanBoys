#pragma once

#include <vector>

#include "Common/IFrameBuffer.h"
#include "VulkanCommon.h"

class DeviceVK;
class ImageViewVK;
class RenderPassVK;

class FrameBufferVK : public IFrameBuffer
{
public:
	DECL_NO_COPY(FrameBufferVK);

	FrameBufferVK(DeviceVK* pDevice);
	~FrameBufferVK();

	void addColorAttachment(ImageViewVK* pImageView);
	void setDepthStencilAttachment(ImageViewVK* pImageView);

	bool finalize(RenderPassVK* pRenderPass, uint32_t width, uint32_t height);

	VkFramebuffer getFrameBuffer() const { return m_FrameBuffer; }

private:
	DeviceVK* m_pDevice;

	std::vector<ImageViewVK*> m_ColorAttachments;
	ImageViewVK* m_pDepthStencilAttachment;

	VkFramebuffer m_FrameBuffer;
};
