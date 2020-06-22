#include "Settings.h"

#include "SettingsDefines.h"

#include "../Core/Assertions.h"

#include "../Dependencies/json.hpp"


NW_NAMESPACE_BEGIN

static constexpr const char* kMemorySettingsFileName = "MemorySettings.json";

Settings& Settings::Instance()
{
	static Settings instance;
	return instance;
}

Settings::Settings()
{
}

Settings::~Settings()
{
}

void Settings::LoadMemorySettings()
{
	// if is no file, it creates and fill with basic information and default values.
	std::ifstream is(kMemorySettingsFileName);
	if (!is.is_open())
	{
		// create the file with the default values and read again
		nlohmann::json nj = {
			{"Engine", {
				{"Vulkan", SettingsDefines::Engine::kVulkanAllocatorSize},
				{"GPU", SettingsDefines::Engine::kGPUAllocatorSize},
				{"StagingBuffer", SettingsDefines::Engine::kStagingBufferSize},
				{"DeviceLocal", SettingsDefines::Engine::kGpuDeviceLocalSize},
				{"HostVisible", SettingsDefines::Engine::kGpuHostVisibleSize},
				{"Instance", SettingsDefines::Engine::kInstanceAllocatorSize},
				{"QueueFamily", SettingsDefines::Engine::kQueueFamilyAllocatorSize},
				{"Device", SettingsDefines::Engine::kDeviceAllocatorSize},
				{"DescriptorPool", SettingsDefines::Engine::kDescriptorPoolAllocatorSize},
			}},
			{"Generic", {
				{"Common", SettingsDefines::Generic::kCommonAllocatorSize},
				{"String", SettingsDefines::Generic::kStringAllocatorSize},
				{"ClientOptions", SettingsDefines::Generic::kClientOptionAllocatorSize},
			}}
		};

 		std::ofstream os(kMemorySettingsFileName);
		os << nj;
		os.close();

		is.open(kMemorySettingsFileName);
		nwAssertReturnVoid(is.is_open(), "Cannot open nor create the %s file", kMemorySettingsFileName);
	}

	nlohmann::json json;
	is >> json;
	is.close();

	// actual settings - Engine
	SettingsDefines::Engine::kVulkanAllocatorSize = json["Engine"]["Vulkan"];
	SettingsDefines::Engine::kGPUAllocatorSize = json["Engine"]["GPU"];
	SettingsDefines::Engine::kVulkanAllocatorSize = json["Engine"]["StagingBuffer"];
	SettingsDefines::Engine::kGPUAllocatorSize = json["Engine"]["DeviceLocal"];
	SettingsDefines::Engine::kGPUAllocatorSize = json["Engine"]["HostVisible"];
	SettingsDefines::Engine::kInstanceAllocatorSize = json["Engine"]["Instance"];
	SettingsDefines::Engine::kQueueFamilyAllocatorSize = json["Engine"]["QueueFamily"];
	SettingsDefines::Engine::kDeviceAllocatorSize = json["Engine"]["Device"];
	SettingsDefines::Engine::kDescriptorPoolAllocatorSize = json["Engine"]["DescriptorPool"];

	// actual settings - Generic
	SettingsDefines::Generic::kCommonAllocatorSize = json["Generic"]["Common"];
	SettingsDefines::Generic::kStringAllocatorSize = json["Generic"]["String"];
	SettingsDefines::Generic::kClientOptionAllocatorSize = json["Generic"]["ClientOptions"];
}

NW_NAMESPACE_END