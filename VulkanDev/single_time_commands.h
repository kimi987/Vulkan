#pragma once
#include "config.h"

namespace vkUtils {
	/*
		Begin recording a command buffer intended for a single submit.
	*/

	void startJob(vk::CommandBuffer commandBuffer);

	/*
		Finish recording a command buffer the submit it
	*/
	void endJob(vk::CommandBuffer, vk::Queue submissionQueue);
}