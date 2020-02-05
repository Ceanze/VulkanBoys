#include "DeviceVK.h"
#include "InstanceVK.h"

#include <iostream>
#include <map>
#include <set>

DeviceVK::DeviceVK() :
	m_PhysicalDevice(VK_NULL_HANDLE),
	m_Device(VK_NULL_HANDLE),
	m_GraphicsQueue(VK_NULL_HANDLE),
	m_ComputeQueue(VK_NULL_HANDLE),
	m_TransferQueue(VK_NULL_HANDLE),
	m_PresentQueue(VK_NULL_HANDLE)
{
}

DeviceVK::~DeviceVK()
{
}

bool DeviceVK::finalize(InstanceVK* pInstance)
{
	if (!initPhysicalDevice(pInstance))
		return false;
	
	if (!initLogicalDevice(pInstance))
		return false;
	
	return true;
}

void DeviceVK::release()
{
	if (m_Device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(m_Device);
		vkDestroyDevice(m_Device, nullptr);
		m_Device = VK_NULL_HANDLE;
	}
}

void DeviceVK::addRequiredExtension(const char* extensionName)
{
	m_RequestedRequiredExtensions.push_back(extensionName);
}

void DeviceVK::addOptionalExtension(const char* extensionName)
{
	m_RequestedOptionalExtensions.push_back(extensionName);
}

bool DeviceVK::initPhysicalDevice(InstanceVK* pInstance)
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(pInstance->getInstance() , &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		std::cerr << "Presentation is not supported by the selected physicaldevice" << std::endl;
		return false;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(pInstance->getInstance(), &deviceCount, physicalDevices.data());

	std::multimap<int32_t, VkPhysicalDevice> physicalDeviceCandidates;

	for (const auto& physicalDevice : physicalDevices)
	{
		int32_t score = rateDevice(physicalDevice);
		physicalDeviceCandidates.insert(std::make_pair(score, physicalDevice));
	}

	// Check if the best candidate is suitable at all
	if (physicalDeviceCandidates.rbegin()->first <= 0)
	{
		std::cerr << "Failed to find a suitable GPU!" << std::endl;
		return false;
	}

	m_PhysicalDevice = physicalDeviceCandidates.rbegin()->second;
	setEnabledExtensions();
	m_DeviceQueueFamilyIndices = findQueueFamilies(m_PhysicalDevice);

	return true;
}

bool DeviceVK::initLogicalDevice(InstanceVK* pInstance)
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = 
	{
		m_DeviceQueueFamilyIndices.graphicsFamily.value(),
		m_DeviceQueueFamilyIndices.computeFamily.value(),
		m_DeviceQueueFamilyIndices.transferFamily.value(),
		m_DeviceQueueFamilyIndices.presentFamily.value()
	};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.fillModeNonSolid = true;
	deviceFeatures.vertexPipelineStoresAndAtomics = true;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(m_EnabledExtensions.size());
	createInfo.ppEnabledExtensionNames = m_EnabledExtensions.data();

	if (pInstance->validationLayersEnabled())
	{
		auto& validationLayers = pInstance->getValidationLayers();
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
	{
		std::cerr << "Failed to create logical device!" << std::endl;
		return false;
	}

	vkGetDeviceQueue(m_Device, m_DeviceQueueFamilyIndices.graphicsFamily.value(), 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, m_DeviceQueueFamilyIndices.computeFamily.value(), 0, &m_ComputeQueue);
	vkGetDeviceQueue(m_Device, m_DeviceQueueFamilyIndices.transferFamily.value(), 0, &m_TransferQueue);
	vkGetDeviceQueue(m_Device, m_DeviceQueueFamilyIndices.presentFamily.value(), 0, &m_PresentQueue);

	return true;
}

int32_t DeviceVK::rateDevice(VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	bool requiredExtensionsSupported = false;
	uint32_t numOfOptionalExtensionsSupported = 0;
	checkExtensionsSupport(physicalDevice, requiredExtensionsSupported, numOfOptionalExtensionsSupported);

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	if (!indices.IsComplete() || !requiredExtensionsSupported)
		return 0;

	int score = 1 + numOfOptionalExtensionsSupported;

	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;

	return score;
}

void DeviceVK::checkExtensionsSupport(VkPhysicalDevice physicalDevice, bool& requiredExtensionsSupported, uint32_t& numOfOptionalExtensionsSupported)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(m_RequestedRequiredExtensions.begin(), m_RequestedRequiredExtensions.end());
	std::set<std::string> optionalExtensions(m_RequestedOptionalExtensions.begin(), m_RequestedOptionalExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
		optionalExtensions.erase(extension.extensionName);
	}

	requiredExtensionsSupported = requiredExtensions.empty();
	numOfOptionalExtensionsSupported = optionalExtensions.size();
}

void DeviceVK::setEnabledExtensions()
{
	m_EnabledExtensions = std::vector<const char*>(m_RequestedRequiredExtensions.begin(), m_RequestedRequiredExtensions.end());
	
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> optionalExtensions(m_RequestedOptionalExtensions.begin(), m_RequestedOptionalExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		if (optionalExtensions.erase(extension.extensionName) > 0)
		{
			m_EnabledExtensions.push_back(extension.extensionName);
		}
		else
		{
			std::cerr << "Optional Device Extension : " << extension.extensionName << " not supported!" << std::endl;
		}
	}
}

QueueFamilyIndices DeviceVK::findQueueFamilies(VkPhysicalDevice physicalDevice)
{
	QueueFamilyIndices indices;
	
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	indices.graphicsFamily = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT, queueFamilies);
	indices.computeFamily = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT, queueFamilies);
	indices.transferFamily = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT, queueFamilies);
	indices.presentFamily = m_DeviceQueueFamilyIndices.graphicsFamily; //Assume present support at this stage

	return indices;
}

uint32_t DeviceVK::getQueueFamilyIndex(VkQueueFlagBits queueFlags, const std::vector<VkQueueFamilyProperties>& queueFamilies)
{
	if (queueFlags & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < uint32_t(queueFamilies.size()); i++)
		{
			if ((queueFamilies[i].queueFlags & queueFlags) && ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
				return i;
		}
	}

	if (queueFlags & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < uint32_t(queueFamilies.size()); i++)
		{
			if ((queueFamilies[i].queueFlags & queueFlags) && ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
				return i;
		}
	}

	for (uint32_t i = 0; i < uint32_t(queueFamilies.size()); i++)
	{
		if (queueFamilies[i].queueFlags & queueFlags)
			return i;
	}

	return UINT32_MAX;
}
