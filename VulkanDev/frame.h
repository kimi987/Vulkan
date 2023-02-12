#pragma once
#include "config.h"

namespace vkUtils {
	/**
		Holds the data structures associated with a "Frame"
	*/

	struct SwapChainFrame {
		vk::Image image;
		vk::ImageView imageView;
		vk::Framebuffer frameBuffer;
		vk::CommandBuffer commangBuffer;
		vk::Semaphore imageAvailable, renderFinished;
		vk::Fence inFlight;
	};
}