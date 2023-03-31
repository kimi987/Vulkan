#include "engine.h"
#include "instance.h"
#include "logging.h"
#include "device.h"
#include "swapchain.h"
#include "pipeline.h"
#include "framebuffer.h"
#include "commands.h"
#include "sync.h"
#include "descriptors.h"

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

	make_descriptor_set_layout();
	make_pipeline();
	std::cout << "Making a graphics pipeline done\n";
	finalize_setup();
	std::cout << "finalize_setup done\n";
	make_assets();
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

	make_swapchain();
	frameNumber = 0;
}

/*
	Make a swapchain
*/
void Engine::make_swapchain() {
	vkInit::SwapChainBundle bundle = vkInit::create_swapchain(device, physicalDevice, surface, width, height, debugMode);
	swapchain = bundle.swapChain;
	swapchainFrames = bundle.frames;
	swapchainFormat = bundle.format;
	swapchainExtent = bundle.extent;
	maxFrameInFlight = static_cast<int>(swapchainFrames.size());

	for (vkUtils::SwapChainFrame& frame : swapchainFrames)
	{
		frame.logicalDevice = device;
		frame.physicalDevice = physicalDevice;
		frame.width = swapchainExtent.width;
		frame.height = swapchainExtent.height;

		frame.make_depth_resources();
	}
}

/*
	The swapchain must be recreated upon resize or minimization, among other cases
*/
void Engine::recreate_swapchain() {
	width = 0;
	height = 0;
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	device.waitIdle();

	cleanup_swapchain();
	make_swapchain();
	make_framebuffers();
	make_frame_resources();
	vkInit::commandBufferInputChunk commandBufferInput = { device, commandPool, swapchainFrames };
	vkInit::make_frame_command_buffers(commandBufferInput, debugMode);
}

void Engine::make_descriptor_set_layout() {
	vkInit::descriptorSetLayoutData bindings;
	bindings.count = 2;
	bindings.indices.push_back(0);
	bindings.types.push_back(vk::DescriptorType::eUniformBuffer);
	bindings.counts.push_back(1);
	bindings.stages.push_back(vk::ShaderStageFlagBits::eVertex);
	
	bindings.indices.push_back(1);
	bindings.types.push_back(vk::DescriptorType::eStorageBuffer);
	bindings.counts.push_back(1);
	bindings.stages.push_back(vk::ShaderStageFlagBits::eVertex);

	frameSetLayout = vkInit::make_descriptor_set_layout(device, bindings);

	//Binding for individual draw calls
	bindings.count = 1;

	bindings.indices[0] = 0;
	bindings.types[0] = vk::DescriptorType::eCombinedImageSampler;
	bindings.counts[0] = 1;
	bindings.stages[0] = vk::ShaderStageFlagBits::eFragment;

	meshSetLayout = vkInit::make_descriptor_set_layout(device, bindings);
}

void Engine::make_pipeline() {
	vkInit::GraphicsPipelineInBundle specification = {};
	specification.device = device;
	specification.vertexFilePath = "shaders/vertex.spv";
	specification.fragmentFilePath = "shaders/fragment.spv";
	specification.swapchainExtent = swapchainExtent;
	specification.swapchainImageFormat = swapchainFormat;
	specification.descriptorSetLayouts = {frameSetLayout, meshSetLayout};
	specification.depthFormat = swapchainFrames[0].depthFormat;

	vkInit::GraphicsPipelineOutBundle output = vkInit::create_graphics_pipeline(specification);

	pipelineLayout = output.layout;
	renderpass = output.renderPass;
	pipeline = output.pipeline;
}

/*
	Make a framebuffer for each frame
*/
void Engine::make_framebuffers() {
	vkInit::framebufferInput frameBufferInput;
	frameBufferInput.device = device;
	frameBufferInput.renderpass = renderpass;
	frameBufferInput.swapchainExtent = swapchainExtent;
	vkInit::make_framebuffers(frameBufferInput, swapchainFrames, debugMode);
}

void Engine::finalize_setup() {
	
	make_framebuffers();

	commandPool = vkInit::make_command_pool(device, physicalDevice, surface, debugMode);

	vkInit::commandBufferInputChunk commandBufferInput = { device, commandPool, swapchainFrames };
	mainCommandBuffer = vkInit::make_command_buffer(commandBufferInput, debugMode);
	
	vkInit::make_frame_command_buffers(commandBufferInput, debugMode);

	make_frame_resources();

}

void Engine::make_frame_resources() {
	vkInit::descriptorSetLayoutData bindings;
	bindings.count = 2;
	bindings.types.push_back(vk::DescriptorType::eUniformBuffer);
	bindings.types.push_back(vk::DescriptorType::eStorageBuffer);

	frameDescriptorPool = vkInit::make_descriptor_pool(device, static_cast<uint32_t>(swapchainFrames.size()), bindings);

	for (vkUtils::SwapChainFrame& frame : swapchainFrames)
	{
		frame.imageAvailable = vkInit::make_semaphore(device);
		frame.renderFinished = vkInit::make_semaphore(device);

		frame.inFlight = vkInit::make_fence(device);

		frame.make_descriptor_resources();

		frame.descriptorSet = vkInit::allocate_descriptor_set(device, frameDescriptorPool, frameSetLayout);
	}
}

void Engine::make_assets() {
	//Meshes
	meshes = new VertexMenagerie();

	std::vector<float> vertices = { {
			 0.0f, -0.1f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, //0
			 0.1f,  0.1f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, //1
			-0.1f,  0.1f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f  //2
		} };
	std::vector<uint32_t> indices = { {
			0,1,2
	} };

	meshTypes type = meshTypes::TRIANGLE;
	meshes->consume(type, vertices, indices);

	vertices = { {
		-0.1f,  0.1f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, //0
		-0.1f, -0.1f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, //1
		 0.1f, -0.1f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, //2
		 0.1f,  0.1f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, //3
	} };
	indices = { {
			0,1,2,
			2,3,0
	} };
	type = meshTypes::SQUARE;
	meshes->consume(type, vertices, indices);

	vertices = { {
		 -0.1f, -0.05f, 1.0f, 1.0f, 1.0f, 0.0f, 0.25f, //0
		-0.04f, -0.05f, 1.0f, 1.0f, 1.0f, 0.3f, 0.25f, //1
		-0.06f,   0.0f, 1.0f, 1.0f, 1.0f, 0.2f,  0.5f, //2
		  0.0f,  -0.1f, 1.0f, 1.0f, 1.0f, 0.5f,  0.0f, //3
		 0.04f, -0.05f, 1.0f, 1.0f, 1.0f, 0.7f, 0.25f, //4
		  0.1f, -0.05f, 1.0f, 1.0f, 1.0f, 1.0f, 0.25f, //5
		 0.06f,   0.0f, 1.0f, 1.0f, 1.0f, 0.8f,  0.5f, //6
		 0.08f,   0.1f, 1.0f, 1.0f, 1.0f, 0.9f,  1.0f, //7
		  0.0f,  0.02f, 1.0f, 1.0f, 1.0f, 0.5f,  0.6f, //8
		-0.08f,   0.1f, 1.0f, 1.0f, 1.0f, 0.1f,  1.0f  //9
	} };
	indices = { {
		0,1,2,
		1,3,4,
		2,1,4,
		4,5,6,
		2,4,6,
		6,7,8,
		2,6,8,
		2,8,9,
	} };
	type = meshTypes::STAR;
	meshes->consume(type, vertices, indices);

	vertexBufferFinalizationChunk finalizationInfo;
	finalizationInfo.logicalDevice = device;
	finalizationInfo.physicalDevice = physicalDevice;
	finalizationInfo.commandBuffer = mainCommandBuffer;
	finalizationInfo.queue = graphicsQueue;
	meshes->finalize(finalizationInfo);

	//Materials

	std::unordered_map<meshTypes, const char*> filenames = {
		{meshTypes::TRIANGLE, "tex/face.jpg"},
		{meshTypes::SQUARE, "tex/haus.jpg"},
		{meshTypes::STAR, "tex/noroi.png"}
	};
		
	//Make a descriptor pool to allocate sets.
	vkInit::descriptorSetLayoutData bindings;
	bindings.count = 1;
	bindings.types.push_back(vk::DescriptorType::eCombinedImageSampler);
	meshDescriptorPool = vkInit::make_descriptor_pool(device, static_cast<uint32_t>(filenames.size()), bindings);

	vkImage::TextureInputChunk textureInfo;
	textureInfo.commandBuffer = mainCommandBuffer;
	textureInfo.queue = graphicsQueue;
	textureInfo.logicalDevice = device;
	textureInfo.physicalDevice = physicalDevice;
	textureInfo.layout = meshSetLayout;
	textureInfo.descriptorPool = meshDescriptorPool;

	for (const auto & [object, filename] : filenames)
	{
		textureInfo.filename = filename;
		materials[object] = new vkImage::Texture(textureInfo);
	}
}


void Engine::prepare_frame(uint32_t imageIndex, Scene* scene) {
	vkUtils::SwapChainFrame& _frame = swapchainFrames[imageIndex];

	glm::vec3 eye = { 1.0f, 0.0f, -1.0f };
	glm::vec3 center = { 0.0f, 0.0f, 0.0f };
	glm::vec3 up = { 0.0f, 0.0f, -1.0f };
	glm::mat4 view = glm::lookAt(eye, center, up);

	glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height), 0.1f, 10.0f);

	projection[1][1] *= -1;

	_frame.cameraData.view = view;
	_frame.cameraData.projection = projection;
	_frame.cameraData.viewProjection = projection * view;
	
	memcpy(_frame.cameraDataWriteLocation, &(_frame.cameraData), sizeof(vkUtils::UBO));

	size_t i = 0;
	for(glm::vec3& position : scene->trianglePositions)
	{
		_frame.modelTransforms[i++] = glm::translate(glm::mat4(1.0f), position);
	}
	for (glm::vec3& position : scene->squarePositions)
	{
		_frame.modelTransforms[i++] = glm::translate(glm::mat4(1.0f), position);
	}
	for (glm::vec3& position : scene->starPositions)
	{
		_frame.modelTransforms[i++] = glm::translate(glm::mat4(1.0f), position);
	}
	memcpy(_frame.modelBufferWriteLocation, _frame.modelTransforms.data(), i * sizeof(glm::mat4));

	_frame.write_descriptor_set();
}



void Engine::prepare_scene(vk::CommandBuffer commandBuffer) {
	vk::Buffer vertexBuffers[] = { meshes->vertexBuffer.buffer };
	vk::DeviceSize offsets[] = { 0 };
	commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
	commandBuffer.bindIndexBuffer(meshes->indexBuffer.buffer, 0, vk::IndexType::eUint32);
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

	vk::ClearValue colorClear;
	std::array<float, 4> colors = { 1.0f, 0.5f, 0.25f, 1.0f };
	colorClear.color = vk::ClearColorValue(colors);
	vk::ClearValue depthClear;

	depthClear.depthStencil = vk::ClearDepthStencilValue({ 1.0f, 0});
	std::vector<vk::ClearValue> clearValues = { {colorClear, depthClear} };

	renderpassInfo.clearValueCount = clearValues.size();
	renderpassInfo.pClearValues = clearValues.data();

	commandBuffer.beginRenderPass(&renderpassInfo, vk::SubpassContents::eInline);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, swapchainFrames[imageIndex].descriptorSet, nullptr);
	prepare_scene(commandBuffer);

	uint32_t startInstance = 0;

	//Triangles
	render_objects(commandBuffer, meshTypes::TRIANGLE, startInstance, static_cast<uint32_t>(scene->trianglePositions.size()));

	
	//Squares
	render_objects(commandBuffer, meshTypes::SQUARE, startInstance, static_cast<uint32_t>(scene->squarePositions.size()));

	//Stars
	render_objects(commandBuffer, meshTypes::STAR, startInstance, static_cast<uint32_t>(scene->starPositions.size()));

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

void Engine::render_objects(vk::CommandBuffer commandBuffer, meshTypes objectType, uint32_t& startInstance, uint32_t instanceCount) {
	int vertexCount = meshes->indexCounts.find(objectType)->second;
	int firstVertex = meshes->firstIndices.find(objectType)->second;
	materials[objectType]->use(commandBuffer, pipelineLayout);
	commandBuffer.drawIndexed(vertexCount, instanceCount, firstVertex, 0, startInstance);
	startInstance += instanceCount;
}

void Engine::render(Scene* scene) {
	device.waitForFences(1, &(swapchainFrames[frameNumber].inFlight), VK_TRUE, UINT64_MAX);
	device.resetFences(1, &(swapchainFrames[frameNumber].inFlight));


	uint32_t imageIndex;

	try
	{
		vk::ResultValue acquire = device.acquireNextImageKHR(swapchain, UINT64_MAX, swapchainFrames[frameNumber].imageAvailable, nullptr);
		imageIndex = acquire.value;
	}
	catch (vk::OutOfDateKHRError error)
	{
		std::cout << "Recreate" << std::endl;
		recreate_swapchain();
		return;
	}
	catch (vk::IncompatibleDisplayKHRError error) {
		std::cout << "Recreate" << std::endl;
		recreate_swapchain();
		return;
	}
	catch (vk::SystemError error) {
		std::cout << "Failed to acquire swapchain image!" << std::endl;
	}

	

	vk::CommandBuffer commandbuffer = swapchainFrames[frameNumber].commangBuffer;

	commandbuffer.reset();

	prepare_frame(imageIndex, scene);

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

	vk::Result present;

	try {
		present = presentQueue.presentKHR(presentInfo);
	}
	catch (vk::OutOfDateKHRError error) {
		present = vk::Result::eErrorOutOfDateKHR;
	}

	if (present == vk::Result::eErrorOutOfDateKHR || present == vk::Result::eSuboptimalKHR)
	{
		std::cout << "Recreate" << std::endl;
		recreate_swapchain();
		return;
	}

	frameNumber = (frameNumber + 1) % maxFrameInFlight;
}

/*
	Free the memory associated with the swapchain objects
*/
void Engine::cleanup_swapchain() {
	for (vkUtils::SwapChainFrame frame : swapchainFrames) {
		frame.destroy();
	}
	device.destroySwapchainKHR(swapchain);

	device.destroyDescriptorPool(frameDescriptorPool);
}

Engine::~Engine() {

	device.waitIdle();

	if (debugMode)
	{
		std::cout << "Goodbye see you! " << std::endl;
	}

	device.destroyCommandPool(commandPool);

	device.destroyPipeline(pipeline);
	device.destroyPipelineLayout(pipelineLayout);
	device.destroyRenderPass(renderpass);

	cleanup_swapchain();

	device.destroyDescriptorSetLayout(frameSetLayout);
	device.destroyDescriptorSetLayout(meshSetLayout);
	device.destroyDescriptorPool(meshDescriptorPool);

	delete meshes;

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

