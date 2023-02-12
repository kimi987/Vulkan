#pragma once
#include "config.h"
#include "logging.h"
#include "queue_families.h"

namespace vkInit {

	void log_device_properties(const vk::PhysicalDevice& device) {
		vk::PhysicalDeviceProperties properties = device.getProperties();
		std::cout << "Device name: " << properties.deviceName << std::endl;
		std::cout << "Device type: ";
		switch (properties.deviceType)
		{
		case (vk::PhysicalDeviceType::eCpu):
			std::cout << "CPU" << std::endl;
			break;
		case (vk::PhysicalDeviceType::eDiscreteGpu):
			std::cout << "Discrete GPU" << std::endl;
			break;
		case (vk::PhysicalDeviceType::eIntegratedGpu):
			std::cout << "Integrated GPU " << std::endl;
			break;
		case (vk::PhysicalDeviceType::eVirtualGpu):
			std::cout << "Virtual  GPU " << std::endl;
			break;
		default:
			std::cout << "Other " << std::endl;
			break;
		}
	}

	bool checkDeviceExtensionSupport(
		const vk::PhysicalDevice& device,
		const std::vector<const char*>& requestedExtensions,
		const bool& debug
	) {
		std::set<std::string> requiredExtensions(requestedExtensions.begin(), requestedExtensions.end());

		if (debug) {
			std::cout << "Device can support extensions:\n";
		}

		for (vk::ExtensionProperties& extension : device.enumerateDeviceExtensionProperties()) 
		{
			if (debug) {
				std::cout << "\t\"" << extension.extensionName << "\"\n";
			}
			//remove this from the list of required extensions (set checks for equality automatically)
			requiredExtensions.erase(extension.extensionName);
		}
	
		//if the set is empty the all requirements have been satisfied
		return requiredExtensions.empty();
	}

	/**
		Check whether the given physical device is suitable for the system.
		\param device the physical device to check.
		\debug whether the system is running in debug mode.
		\returns whether the device is suitable.
	*/

	bool isSuitable(const vk::PhysicalDevice& device, const bool debug) {
		if (debug) {
			std::cout << "Checking if device is suitable\n";
		}

		const std::vector<const char*> requestedExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		if (debug) {
			std::cout << "We are requesting device extensions:\n";

			for (const char* extension : requestedExtensions) {
				std::cout << "\t\"" << extension << "\"\n";
			}
		}

		if (bool extensionSupported = checkDeviceExtensionSupport(device, requestedExtensions, debug))
		{
			if (debug) {
				std::cout << "Device can support the requested extensions!\n";
			}
		}
		else {
			if (debug) {
				std::cout << "Device can't support the requested extensions!\n";
			}
			return false;
		}
		return true;
	}

	vk::PhysicalDevice choose_physical_device(const vk::Instance& instance, const bool debug) {
		if (debug) {
			std::cout << "Choosing Physical Device\n";
		}

		std::vector<vk::PhysicalDevice> availableDevices = instance.enumeratePhysicalDevices();

		if (debug) {
			std::cout << "There are " << availableDevices.size() << " physical devices available on this system\n";
		}
		/*
		* check if a suitable device can be found
		*/
		for (vk::PhysicalDevice device : availableDevices)
		{
			if (debug) {
				log_device_properties(device);
			}
			if (isSuitable(device, debug))
			{
				return device;
			}
		}
		return nullptr;
	}

	

	vk::Device create_logical_device(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, bool debug) {
		vkUtil::QueueFamilyIndices indices = vkUtil::findQueueFamilies(physicalDevice, surface, debug);
		float queuePriority = 1.0f;

		vk::DeviceQueueCreateInfo queueCreateInfo = vk::DeviceQueueCreateInfo(
			vk::DeviceQueueCreateFlags(), indices.graphicsFamily.value(), 1, &queuePriority
		);

		std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		vk::PhysicalDeviceFeatures deviceFeatures = vk::PhysicalDeviceFeatures();

		std::vector<const char*> enabledLayers;

		if (debug)
		{
			enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
		}
		vk::DeviceCreateInfo deviceInfo = vk::DeviceCreateInfo(
			vk::DeviceCreateFlags(), 1, &queueCreateInfo, enabledLayers.size(), enabledLayers.data(), deviceExtensions.size(), deviceExtensions.data(), &deviceFeatures);

		try {
			vk::Device device = physicalDevice.createDevice(deviceInfo);

			if (debug)
			{
				std::cout << "GPU has been successfully abstracted!" << std::endl;
			}

			return device;
		}
		catch (vk::SystemError err) {
			if (debug)
			{
				std::cout << "Device creation failed" << std::endl;
			}
			return nullptr;
		}
		return nullptr;
	}

	/**
		get the graphics queue

		/param physicalDevice the physical device
		/param device the logic device
		/param debug whether the system is running in debug mode
		/returns the physical device's graphics queue
	**/
	std::array<vk::Queue, 2> get_queue(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface, bool debug) {
		vkUtil::QueueFamilyIndices indices = vkUtil::findQueueFamilies(physicalDevice, surface, debug);

		return { {
				device.getQueue(indices.graphicsFamily.value(), 0),
				device.getQueue(indices.presentFamily.value(), 0),
			} };
	}

}