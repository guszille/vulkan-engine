#pragma once

#define GLFW_INCLUDE_VULKAN
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <GLFW/glfw3.h>

#include <vkb/VkBootstrap.h>

#include <vkma/vk_mem_alloc.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <array>
#include <chrono>
#include <thread>
#include <cassert>

#include "utils.h"
#include "structures.h"
#include "loader.h"

constexpr uint32_t FRAMES_IN_FLIGHT = 2;

struct Frame
{
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

	VkFence renderFence;
	VkSemaphore renderSemaphore, swapchainSemaphore;

	DeletionQueue deletionQueue;
};

struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect
{
	const char* name;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	ComputePushConstants pushConstants;
};

class Engine
{
public:
	Engine();

	bool isInitialized = false;
	bool stopRendering = false;
	bool resizeRequested = false;
	bool useValidationLayers = true;

	float deltaTime = 0.0f;
	float lastFrame = 0.0f;

	uint32_t frameCount = 0;

	GLFWwindow* window = nullptr;
	VkExtent2D windowExtent{ 1600, 900 };

	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkPhysicalDevice gpu = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily;

	VmaAllocator allocator;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainImageFormat;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkExtent2D swapchainExtent;

	VkExtent2D drawExtent;
	float renderScale = 1.0f;

	AllocatedImage drawImage;
	AllocatedImage depthImage;

	Frame frames[FRAMES_IN_FLIGHT];

	DescriptorAllocator globalDescriptorAllocator;

	VkDescriptorSet drawImageDescriptors = VK_NULL_HANDLE;
	VkDescriptorSetLayout drawImageDescriptorLayout = VK_NULL_HANDLE;

	VkPipelineLayout defaultPipelineLayout = VK_NULL_HANDLE;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect = 0;

	VkPipelineLayout meshPipelineLayout;
	VkPipeline meshPipeline;

	// Immediate submit structures.
	VkFence immFence;
	VkCommandBuffer immCommandBuffer;
	VkCommandPool immCommandPool;

	DeletionQueue mainDeletionQueue;

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	void initialize();
	void run();
	void cleanUp();

	Frame& getCurrentFrame() { return frames[frameCount % FRAMES_IN_FLIGHT]; };
	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	GPUMeshBuffers uploadMesh(std::span<Vertex> vertices, std::span<uint32_t> indices);

private:
	void render(float deltaTime);
	void renderInBackground(float deltaTime, VkCommandBuffer cmd);
	void renderGeometry(float deltaTime, VkCommandBuffer cmd);
	void renderImgui(float deltaTime, VkCommandBuffer cmd, VkImageView targetImageView);
	void processInputs(float deltaTime);

	static void windowKeyCallback(GLFWwindow* window, int key, int scanCode, int action, int mods);
	static void windowFramebufferSizeCallback(GLFWwindow* window, int width, int height);

	void initializeVulkan();
	void initializeSwapchain();
	void initializeCommandStructures();
	void initializeSyncStructures();
	void initializeDescriptors();
	void initializePipelines();
	void initializeBackgroundPipelines();
	void initializeMeshPipeline();
	void initializeImgui();
	void initalizeDefaultData();

	void createSwapchain(uint32_t width, uint32_t height);
	void resizeSwapchain();
	void cleanUpSwapchain();

	AllocatedBuffer createBuffer(size_t allocationSize, VkBufferUsageFlags bufferUsageFlags, VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);
};
