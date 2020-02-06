#include "DescriptorSetVK.h"

#include "DescriptorPoolVK.h"
//#include "DeviceVK.h"

DescriptorSetVK::DescriptorSetVK()
    :m_pDescriptorPool(nullptr),
    m_DescriptorSet(VK_NULL_HANDLE),
    m_DescriptorCounts({0})
{}

DescriptorSetVK::~DescriptorSetVK()
{
    if (m_pDescriptorPool != nullptr && m_DescriptorSet != VK_NULL_HANDLE) {
        m_pDescriptorPool->deallocateDescriptorSet(this);
    }
}

void DescriptorSetVK::initialize(DeviceVK* pDevice, DescriptorPoolVK* pDescriptorPool, const DescriptorCounts& descriptorCounts)
{
    m_pDevice = pDevice;
    m_pDescriptorPool = pDescriptorPool;
    m_DescriptorCounts = descriptorCounts;
}

void DescriptorSetVK::writeUniformBufferDescriptor(VkBuffer buffer, uint32_t binding)
{
    writeBufferDescriptor(buffer, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void DescriptorSetVK::writeStorageBufferDescriptor(VkBuffer buffer, uint32_t binding)
{
    writeBufferDescriptor(buffer, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void DescriptorSetVK::writeSampledImageDescriptor(VkImageView imageView, VkSampler sampler, uint32_t binding)
{
    VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageView		= imageView;
	imageInfo.sampler		= sampler;
	imageInfo.imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet descriptorImageWrite = {};
	descriptorImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorImageWrite.dstSet = m_DescriptorSet;
	descriptorImageWrite.dstBinding = binding;
	descriptorImageWrite.dstArrayElement = 0;
	descriptorImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorImageWrite.descriptorCount = 1;
	descriptorImageWrite.pBufferInfo = nullptr;
	descriptorImageWrite.pImageInfo = &imageInfo;
	descriptorImageWrite.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_pVulkanDevice->getDevice(), 1, &descriptorImageWrite, 0, nullptr);
}

void DescriptorSetVK::writeBufferDescriptor(VkBuffer buffer, uint32_t binding, VkDescriptorType bufferType)
{
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer	= buffer;
    bufferInfo.offset	= 0;
    bufferInfo.range	= VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorBufferWrite = {};
    descriptorBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorBufferWrite.dstSet = m_DescriptorSet;
    descriptorBufferWrite.dstBinding = binding;
    descriptorBufferWrite.dstArrayElement = 0;
    descriptorBufferWrite.descriptorType = bufferType;
    descriptorBufferWrite.descriptorCount = 1;
    descriptorBufferWrite.pBufferInfo = &bufferInfo;
    descriptorBufferWrite.pImageInfo = nullptr;
    descriptorBufferWrite.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(m_pVulkanDevice->getDevice(), 1, &descriptorBufferWrite, 0, nullptr);
}
