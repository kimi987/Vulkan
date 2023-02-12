#include "engine.h"
#include "instance.h"
#include "logging.h"
#include "device.h"
#include "swapchain.h"
#include "pipeline.h"
#include "framebuffer.h"
#include "commands.h"
#include "sync.h"

Engine::Engine(int width, int height, GLFWwindow* window, bool debug) {

	this->width = width;
	this->height = height;
	this->window = window;
	this->debugMode = debug;
	if (debugMode) {
		std::cout << "Making a graphics engine\n";
	}
	make_instance();

	make_debug_message();

	make_device();

	make_pipeline();

	finalize_setup();
}

void Engine::make_instance() {
	instance = vkInit::make_instance(debugMode, "ID Tech 12");
	dldi = vk::DispatchLoaderDynamic(instance, vkGetInstanceProcAddr);

	VkSurfaceKHR c_style_surface;
	if (glfwCreateWindowSurface(instance, window, nullptr, &c_style_surface) != VK_SUCCESS)
	{
		if (debugMode) {
			std::cout << "Failed to abstract glfw surface for Vulkan\n";
		}
	}
	else if (debugMode) {
		std::cout << "Successfully abstracted glfw surface for Vulkan\n";
	}

	surface = c_style_surface;
}

void Engine::make_debug_message() {
	if (!debugMode) {
		return;
	}

	debugMessage = vkInit::make_debug_messenger(instance, dldi);
}

void Engine::make_device() {
	physicalDevice = vkInit::choose_physical_device(instance, debugMode);
	vkUtil::findQueueFamilies(physicalDevice, surface, debugMode);
	device = vkInit::create_logical_device(physicalDevice, surface, debugMode);
	std::array<vk::Queue, 2> queues = vkInit::get_queue(physicalDevice, device, surface, debugMode);
	graphicsQueue = queues[0];
	presentQueue = queues[1];

	vkInit::SwapChainBundle bundle = vkInit::create_swapchain(device, physicalDevice, surface, width, height, debugMode);
	swapchain = bundle.swapChain;
	swapchainFrames = bundle.frames;
	swapchainFormat = bundle.format;
	swapchainExtent = bundle.extent;

	maxFrameInFlight = static_cast<int>(swapchainFrames.size());
	frameNumber = 0;
}

void Engine::make_pipeline() {
	vkInit::GraphicsPipelineInBundle specification = {};
	specification.device = device;
	specification.vertexFilePath = "shaders/vertex.spv";
	specification.fragmentFilePath = "shaders/fragment.spv";
	specification.swapchainExtent = swapchainExtent;
	specification.swapchainImageFormat = swapchainFormat;

	vkInit::GraphicsPipelineOutBundle output = vkInit::create_graphics_pipeline(specification, debugMode);

	pipelineLayout = output.layout;
	renderpass = output.renderPass;
	pipeline = output.pipeline;
}

void Engine::finalize_setup() {
	vkInit::framebufferInput framebufferInput;
	framebufferInput.device = device;
	framebufferInput.renderpass = renderpass;
	framebufferInput.swapchainExtent = swapchainExtent;
	vkInit::make_framebuffers(framebufferInput, swapchainFrames, debugMode);

	commandPool = vkInit::make_command_pool(device, physicalDevice, surface, debugMode);

	vkInit::commandBufferInputChunk commandBufferInput = { device, commandPool, swapchainFrames };
	mainCommandBuffer = vkInit::make_command_buffers(commandBufferInput, debugMode);
	
	for (vkUtils::SwapChainFrame& frame : swapchainFrames)
	{
		frame.imageAvailable = vkInit::make_semaphore(device, debugMode);
		frame.renderFinished = vkInit::make_semaphore(device, debugMode);
		frame.inFlight = vkInit::make_fence(device, debugMode);
	}

}

void Engine::record_draw_commands(vk::CommandBuffer commandBuffer, uint32_t imageIndex, Scene* scene) {
	vk::CommandBufferBeginInfo beginInfo = {};

	try {
		commandBuffer.begin(beginInfo);
	}
	catch (vk::SystemError err) {
		if (debugMode) {
			std::cout << "Failed to begin recording command buffer!" << std::endl;
		}
	}

	vk::RenderPassBeginInfo renderpassInfo = {};
	renderpassInfo.renderPass = renderpass;
	renderpassInfo.framebuffer = swapchainFrames[imageIndex].frameBuffer;
	renderpassInfo.renderArea.offset.x = 0;
	renderpassInfo.renderArea.offset.y = 0;
	renderpassInfo.renderArea.extent = swapchainExtent;
	vk::ClearValue clearColor = { std::array<float, 4>{1.0f, 0.5f, 0.25f, 1.0f} };
	renderpassInfo.clearValueCount = 1;
	renderpassInfo.pClearValues = &clearColor;

	commandBuffer.beginRenderPass(&renderpassInfo, vk::SubpassContents::eInline);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

	for (glm::vec3 position : scene->trianglePositions)
	{
		glm::mat4 model = glm::translate(glm::mat4(1.0), position);
		vkUtil::ObjectData objectData;
		objectData.model = model;
		commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(objectData), &objectData);
		commandBuffer.draw(3, 1, 0, 0);
	}

	commandBuffer.endRenderPass();

	try {
		commandBuffer.end();
	}
	catch (vk::SystemError err) {
		if (debugMode) {
			std::cout << "failed to record command buffer!" << std::endl;
		}
	}
}

void Engine::render(Scene* scene) {
	device.waitForFences(1, &(swapchainFrames[frameNumber].inFlight), VK_TRUE, UINT64_MAX);
	device.resetFences(1, &(swapchainFrames[frameNumber].inFlight));

	//acquireNextImageKHR(vk::SwapChainKHR, timeout, semaphore_to_signal, fence)
	uint32_t imageIndex{ device.acquireNextImageKHR(swapchain, UINT64_MAX, swapchainFrames[frameNumber].imageAvailable, nullptr).value };

	vk::CommandBuffer commandbuffer = swapchainFrames[frameNumber].commangBuffer;

	commandbuffer.reset();

	record_draw_commands(commandbuffer, imageIndex, scene);

	vk::SubmitInfo submitInfo = {};

	vk::Semaphore waitSemaphores[] = { swapchainFrames[frameNumber].imageAvailable };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandbuffer;

	vk::Semaphore signalSemaphores[] = { swapchainFrames[frameNumber].renderFinished };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	try {
		graphicsQueue.submit(submitInfo, swapchainFrames[frameNumber].inFlight);
	}
	catch (vk::SystemError err) {
		if (debugMode) {
			std::cout << "failed to submit draw command buffer!" << std::endl;
		}
	}

	vk::PresentInfoKHR presentInfo = {};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	vk::SwapchainKHR swapChains[] = { swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	presentQueue.presentKHR(presentInfo);

	frameNumber = (frameNumber + 1) % maxFrameInFlight;
}

Engine::~Engine() {
	if (debugMode)
	{
		std::cout << "Goodbye see you! " << std::endl;
	}

	device.destroyCommandPool(commandPool);

	device.destroyPipeline(pipeline);
	device.destroyPipelineLayout(pipelineLayout);
	device.destroyRenderPass(renderpass);

	for (vkUtils::SwapChainFrame frame : swapchainFrames)
	{
		device.destroyImageView(frame.imageView);
		device.destroyFramebuffer(frame.frameBuffer);
		device.destroyFence(frame.inFlight);
		device.destroySemaphore(frame.imageAvailable);
		device.destroySemaphore(frame.renderFinished);
	}
	
	device.destroySwapchainKHR(swapchain);
	device.destroy();

	instance.destroySurfaceKHR(surface);
	if (debugMode) {
		instance.destroyDebugUtilsMessengerEXT(debugMessage, nullptr, dldi);
	}
	//vulkan destroy 
	instance.destroy();
	//terminate glfw
	glfwTerminate();
}

