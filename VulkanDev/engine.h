#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include "frame.h"
#include "scene.h"
#include "vertex_menagerie.h"
#include "image.h"

class Engine {
public:
	Engine(int width, int height, GLFWwindow* window, bool debug);

	~Engine();

	void render(Scene* scene);
private:
	bool debugMode = true;

	//glfw window parameters
	int width{ 640 };
	int height{ 480 };

	GLFWwindow* window{nullptr};

	//vulkan instance
	vk::Instance instance{ nullptr };

	//debug callback
	
	vk::DebugUtilsMessengerEXT debugMessage{ nullptr };

	//dynamic instance dispatcher
	vk::DispatchLoaderDynamic dldi;

	vk::SurfaceKHR surface;

	//device-related variables 
	vk::PhysicalDevice physicalDevice{ nullptr };
	vk::Device device{ nullptr };
	vk::Queue graphicsQueue{ nullptr };
	vk::Queue presentQueue{ nullptr };
	vk::SwapchainKHR swapchain{ nullptr };
	std::vector<vkUtils::SwapChainFrame> swapchainFrames;
	vk::Format swapchainFormat;
	vk::Extent2D swapchainExtent;

	//pipeline-related variable
	vk::PipelineLayout pipelineLayout;
	vk::RenderPass renderpass;
	vk::Pipeline pipeline;

	//descriptor-related variables
	vk::DescriptorSetLayout frameSetLayout;
	vk::DescriptorPool frameDescriptorPool; //Descriptors bound on a "per frame" basis
	vk::DescriptorSetLayout meshSetLayout;
	vk::DescriptorPool meshDescriptorPool;  //Descriptors bound on a "pre mesh" basis

	//Command-related variables
	vk::CommandPool commandPool;
	vk::CommandBuffer mainCommandBuffer;

	//Synchronization objects
	int maxFrameInFlight, frameNumber;

	//asset pointers
	VertexMenagerie* meshes;
	std::unordered_map<meshTypes, vkImage::Texture*> materials;

	//instance setup
	void make_instance();

	//debug message
	void make_debug_message();

	//device setup
	void make_device();
	void make_swapchain();
	void recreate_swapchain();

	//pipeline setup
	void make_descriptor_set_layout();
	void make_pipeline();

	//final setup steps
	void finalize_setup();
	void make_framebuffers();
	void make_frame_resources();

	//asset creation
	void make_assets();

	void prepare_frame(uint32_t imageIndex, Scene* scene);
	void prepare_scene(vk::CommandBuffer commandBuffer);
	void record_draw_commands(vk::CommandBuffer commandBuffer, uint32_t imageIndex, Scene* scene);
	void render_objects(vk::CommandBuffer commandBuffer, meshTypes objectType, uint32_t& startInstance, uint32_t instanceCount);

	//Cleanup functions
	void cleanup_swapchain();
};