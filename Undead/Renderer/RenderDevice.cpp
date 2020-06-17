#include "RenderDevice.h"


#include "../Core/MemoryWrapper.h"
#include "../Core/SettingsDefines.h"
#include "../Core/Assertions.h"

#include "../GPU/GpuMemoryManager.h"


UD_NAMESPACE_BEGIN


RenderDevice::RenderDeviceAllocator* RenderDevice::GetAllocator()
{
	static eos::HeapAreaR memoryArea(SettingsDefines::Engine::kRenderQueueFamilyAllocatorSize);
	static RenderDeviceAllocator memoryAllocator(memoryArea, "RenderDeviceAllocator");

	return &memoryAllocator;
}

RenderDevice::RenderDevice()
	: m_enableValidationLayers(false)
	, m_instance(VK_NULL_HANDLE)
	, m_surface(VK_NULL_HANDLE)
	, m_enabledDeviceFeatures({})
	, m_device(VK_NULL_HANDLE)
	, m_physicalDevice(VK_NULL_HANDLE)
	, m_graphicsQueue(VK_NULL_HANDLE)
	, m_presentQueue(VK_NULL_HANDLE)
	, m_computeQueue(VK_NULL_HANDLE)
{
#if _DEBUG
	m_enableValidationLayers = true;
#else
	m_enableValidationLayers = false;
#endif
}

RenderDevice::~RenderDevice()
{
	Destroy();
}

bool RenderDevice::Create(const VkInstance& _instance, const VkSurfaceKHR& _surface, const VkPhysicalDeviceFeatures& _enabledFeatures /*= {}*/)
{
	m_instance = _instance;
	m_surface = _surface;
	m_enabledDeviceFeatures = _enabledFeatures;

	uint32 deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	udAssertReturnValue(deviceCount > 0, false, "Failed to find GPU with supporting Vulkan API");

	eos::Vector<VkPhysicalDevice, RenderDeviceAllocator, GetAllocator> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	for (VkPhysicalDevice device : devices)
	{
		if (IsDeviceSuitable(device))
		{
			m_physicalDevice = device;

			eos::Vector<VkDeviceQueueCreateInfo, RenderDeviceAllocator, GetAllocator> queueCreateInfos;
			eos::Set<int32, RenderDeviceAllocator, GetAllocator> uniqueQueueFamilies =
			{
				m_queueFamily.GetGraphicsFamily(),
				m_queueFamily.GetComputeFamily(),
				m_queueFamily.GetPresentFamily()
			};

			float queuePriority = 1.0f;
			for (int32 queueFamily : uniqueQueueFamilies)
			{
				VkDeviceQueueCreateInfo queueCreateInfo = {};
				queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriority;
				queueCreateInfos.push_back(queueCreateInfo);
			}

			VkDeviceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.pQueueCreateInfos = queueCreateInfos.data();
			createInfo.queueCreateInfoCount = static_cast<uint32>(queueCreateInfos.size());
			createInfo.pEnabledFeatures = &m_enabledDeviceFeatures;

			const char* const extensions[1] = { { VK_KHR_SWAPCHAIN_EXTENSION_NAME } };
			createInfo.enabledExtensionCount = static_cast<uint32>(1);
			createInfo.ppEnabledExtensionNames = extensions;

			if (m_enableValidationLayers)
			{
				const char* const validation[1] = { { "VK_LAYER_LUNARG_standard_validation" } };
				createInfo.enabledLayerCount = static_cast<uint32>(1);
				createInfo.ppEnabledLayerNames = validation;
			}
			else
			{
				createInfo.enabledLayerCount = 0;
			}

			VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, GpuMemoryManager::Instance().GetVK(), &m_device);
			udAssertReturnValue(result == VK_SUCCESS, false, "Cannot create logical device");

			vkGetDeviceQueue(m_device, m_queueFamily.GetGraphicsFamily(), 0, &m_graphicsQueue);
			vkGetDeviceQueue(m_device, m_queueFamily.GetComputeFamily(), 0, &m_presentQueue);
			vkGetDeviceQueue(m_device, m_queueFamily.GetPresentFamily(), 0, &m_computeQueue);

			vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);

			return true;
		}
	}

	udAssertReturnValue(false, false, "No suitable device found");
}

void RenderDevice::Destroy()
{
	m_queueFamily.Destroy();
}

bool RenderDevice::CheckDeviceExtensionSupport(VkPhysicalDevice _device, const eos::Vector<const char *, RenderDeviceAllocator, GetAllocator>& _deviceExtensions)
{
	uint32 extensionCount;
	vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, nullptr);

	eos::Vector<VkExtensionProperties, RenderDeviceAllocator, GetAllocator> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, availableExtensions.data());

	eos::Set<std::string, RenderDeviceAllocator, GetAllocator> requiredExtensions(_deviceExtensions.begin(), _deviceExtensions.end());

	for (const VkExtensionProperties& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

bool RenderDevice::IsDeviceSuitable(VkPhysicalDevice _physicalDevice)
{
	m_queueFamily.Create(_physicalDevice, m_surface);
	m_queueFamily.Clear();
	m_queueFamily.SetPhysicalDevice(_physicalDevice);
	m_queueFamily.FindQueueFamilies(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

	bool extensionsSupported = CheckDeviceExtensionSupport(_physicalDevice, { VK_KHR_SWAPCHAIN_EXTENSION_NAME });

	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		VkSurfaceCapabilitiesKHR capabilities;
		eos::Vector<VkSurfaceFormatKHR, RenderDeviceAllocator, GetAllocator> formats;
		eos::Vector<VkPresentModeKHR, RenderDeviceAllocator, GetAllocator> presentModes;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, m_surface, &capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, m_surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, m_surface, &formatCount, formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, m_surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, m_surface, &presentModeCount, presentModes.data());
		}

		swapChainAdequate = !formats.empty() && !presentModes.empty();
	}

	bool isValidFamily = m_queueFamily.GetGraphicsFamily() >= 0 && m_queueFamily.GetPresentFamily() >= 0 && m_queueFamily.GetComputeFamily() >= 0;

	return isValidFamily && extensionsSupported && swapChainAdequate;
}

UD_NAMESPACE_END