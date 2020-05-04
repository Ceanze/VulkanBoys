#pragma once
#include <optional>
#include <vector>
#include <unordered_map>

#include "Core/Spinlock.h"

#include "VulkanCommon.h"

class InstanceVK;
class CopyHandlerVK;
class CommandBufferVK;

struct QueueIndices {
	bool operator<(const QueueIndices& other) const {
		return FamilyIndex < other.FamilyIndex;
	}

	uint32_t FamilyIndex;
	uint32_t QueueCount;
};

struct QueueFamilyIndices
{
	std::optional<QueueIndices> GraphicsQueues;
	std::optional<QueueIndices> ComputeQueues;
	std::optional<QueueIndices> TransferQueues;
	std::optional<QueueIndices> PresentQueues;

	bool IsComplete()
	{
		return GraphicsQueues.has_value() && ComputeQueues.has_value() && TransferQueues.has_value() && PresentQueues.has_value();
	}
};

class DeviceVK
{
public:
	DeviceVK();
	~DeviceVK();

	DECL_NO_COPY(DeviceVK);

	bool finalize(InstanceVK* pInstance, bool useMultipleQueues);
	void release();

	void addRequiredExtension(const char* extensionName);
	void addOptionalExtension(const char* extensionName);

	void executeGraphics(CommandBufferVK* pCommandBuffer, const VkSemaphore* pWaitSemaphore, const VkPipelineStageFlags* pWaitStages,
		uint32_t waitSemaphoreCount, const VkSemaphore* pSignalSemaphores, uint32_t signalSemaphoreCount);
	void executeCompute(CommandBufferVK* pCommandBuffer, const VkSemaphore* pWaitSemaphore, const VkPipelineStageFlags* pWaitStages,
		uint32_t waitSemaphoreCount, const VkSemaphore* pSignalSemaphores, uint32_t signalSemaphoreCount);
	void executeTransfer(CommandBufferVK* pCommandBuffer, const VkSemaphore* pWaitSemaphore, const VkPipelineStageFlags* pWaitStages,
		uint32_t waitSemaphoreCount, const VkSemaphore* pSignalSemaphores, uint32_t signalSemaphoreCount);

	void waitGraphics();
	void waitCompute();
	void waitTransfer();
	void wait();

	void setVulkanObjectName(const char* pName, uint64_t objectHandle, VkObjectType type);

	VkPhysicalDevice	getPhysicalDevice()	const	{ return m_PhysicalDevice; };
	VkDevice			getDevice() const			{ return m_Device; }
	VkQueue				getPresentQueue() const		{ return m_PresentQueue; }
	CopyHandlerVK*		getCopyHandler() const		{ return m_pCopyHandler; }

	const QueueFamilyIndices& getQueueFamilyIndices() const { return m_DeviceQueueFamilyIndices; }
	bool hasUniqueQueueFamilyIndices() const;

	void getMaxComputeWorkGroupSize(uint32_t pWorkGroupSize[3]);
	float getTimestampPeriod() const { return m_DeviceLimits.timestampPeriod; };

	const VkPhysicalDeviceRayTracingPropertiesNV& getRayTracingProperties() const { return m_RayTracingProperties; }
	bool supportsRayTracing() const { return m_ExtensionsStatus.at(VK_NV_RAY_TRACING_EXTENSION_NAME); }

private:
	bool initPhysicalDevice();
	bool initLogicalDevice();

	int32_t rateDevice(VkPhysicalDevice physicalDevice);
	void checkExtensionsSupport(VkPhysicalDevice physicalDevice, bool& requiredExtensionsSupported, uint32_t& numOfOptionalExtensionsSupported);
	void setEnabledExtensions();
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice);

	void registerExtensionFunctions();

	void executeCommandBuffer(VkQueue queue, CommandBufferVK* pCommandBuffer, const VkSemaphore* pWaitSemaphore, const VkPipelineStageFlags* pWaitStages,
		uint32_t waitSemaphoreCount, const VkSemaphore* pSignalSemaphores, uint32_t signalSemaphoreCount);

	void getQueues(const char* pObjectName, const QueueIndices& queueIndices, std::vector<VkQueue>& queues);

private:
	static QueueIndices getQueueFamilyIndex(VkQueueFlagBits queueFlags, const std::vector<VkQueueFamilyProperties>& queueFamilies);

private:
	VkPhysicalDevice m_PhysicalDevice;
	VkDevice m_Device;

	QueueFamilyIndices m_DeviceQueueFamilyIndices;

	std::vector<VkQueue> m_GraphicsQueues;
	uint32_t m_NextGraphicsQueue;

	std::vector<VkQueue> m_ComputeQueues;
	uint32_t m_NextComputeQueue;

	std::vector<VkQueue> m_TransferQueues;
	uint32_t m_NextTransferQueue;

	VkQueue m_PresentQueue;

	Spinlock m_ComputeLock;
	Spinlock m_GraphicsLock;
	Spinlock m_TransferLock;

	std::vector<const char*> m_RequestedRequiredExtensions;
	std::vector<const char*> m_RequestedOptionalExtensions;
	std::vector<const char*> m_EnabledExtensions;
	std::unordered_map<std::string, bool> m_ExtensionsStatus;
	std::vector<VkExtensionProperties> m_AvailabeExtensions;

	InstanceVK* m_pInstance;
	CopyHandlerVK* m_pCopyHandler;

	VkPhysicalDeviceLimits m_DeviceLimits;

	//Extensions
	VkPhysicalDeviceRayTracingPropertiesNV m_RayTracingProperties;

	bool m_UseMultipleQueues;

public:
	//Extension Function Pointers
	PFN_vkCreateAccelerationStructureNV					vkCreateAccelerationStructureNV;
	PFN_vkDestroyAccelerationStructureNV				vkDestroyAccelerationStructureNV;
	PFN_vkBindAccelerationStructureMemoryNV				vkBindAccelerationStructureMemoryNV;
	PFN_vkGetAccelerationStructureHandleNV				vkGetAccelerationStructureHandleNV;
	PFN_vkGetAccelerationStructureMemoryRequirementsNV	vkGetAccelerationStructureMemoryRequirementsNV;
	PFN_vkCmdBuildAccelerationStructureNV				vkCmdBuildAccelerationStructureNV;
	PFN_vkCreateRayTracingPipelinesNV					vkCreateRayTracingPipelinesNV;
	PFN_vkGetRayTracingShaderGroupHandlesNV				vkGetRayTracingShaderGroupHandlesNV;
	PFN_vkCmdTraceRaysNV								vkCmdTraceRaysNV;
};

