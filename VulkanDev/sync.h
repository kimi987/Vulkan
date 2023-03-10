#pragma once
#include "config.h"

namespace vkInit {
	/**
		Make a semaphore.
		\param device the logical device
		\param debug whether the system is running in debug mode
		\returns the created semaphore
	*/
	vk::Semaphore make_semaphore(vk::Device device) {
		vk::SemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.flags = vk::SemaphoreCreateFlags();
		try {
			return device.createSemaphore(semaphoreInfo);
		}
		catch (vk::SystemError err) {
	
			std::cout << "Failed to create semaphore " << std::endl;
			return nullptr;
		}
	}

	/**
		Make a fence.
		\param device the logical device
		\param debug whether the system is running in debug mode
		\returns the created fence
	*/
	vk::Fence make_fence(vk::Device device) {
		vk::FenceCreateInfo fenceInfo = {};
		fenceInfo.flags = vk::FenceCreateFlags() | vk::FenceCreateFlagBits::eSignaled;

		try {
			return device.createFence(fenceInfo);
		}
		catch (vk::SystemError err) {
			std::cout << "Failed to create fence " << std::endl;
			return nullptr;
		}
	}
}