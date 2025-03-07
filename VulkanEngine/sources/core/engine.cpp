#include "engine.h"

Engine* engineReference = nullptr;

Engine::Engine()
{
}

void Engine::initialize()
{
	assert(engineReference == nullptr);

	engineReference = this;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(windowExtent.width, windowExtent.height, "Vulkan Engine", nullptr, nullptr);

	glfwSetWindowPos(window, windowExtent.width / 10, windowExtent.height / 10);
	glfwSetWindowUserPointer(window, this);

	glfwSetKeyCallback(window, windowKeyCallback);
	glfwSetFramebufferSizeCallback(window, windowFramebufferSizeCallback);

	initializeVulkan();
	initializeSwapchain();
	initializeCommandStructures();
	initializeSyncStructures();

	isInitialized = true;
}

void Engine::run()
{
	while (!glfwWindowShouldClose(window))
	{
		float currTime = static_cast<float>(glfwGetTime());

		deltaTime = currTime - lastFrame;
		lastFrame = currTime;

		glfwPollEvents();

		if (stopRendering)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			continue;
		}

		render(deltaTime);
	}
}

void Engine::cleanUp()
{
	if (isInitialized)
	{
		vkDeviceWaitIdle(device);

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			vkDestroyCommandPool(device, frames[i].commandPool, nullptr);

			vkDestroyFence(device, frames[i].renderFence, nullptr);
			vkDestroySemaphore(device, frames[i].renderSemaphore, nullptr);
			vkDestroySemaphore(device, frames[i].swapchainSemaphore, nullptr);

			frames[i].deletionQueue.flush();
		}

		mainDeletionQueue.flush();

		cleanUpSwapchain();

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyDevice(device, nullptr);
		vkb::destroy_debug_utils_messenger(instance, debugMessenger);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);
		glfwTerminate();
	}

	engineReference = nullptr;
}

void Engine::render(float deltaTime)
{
	VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame().renderFence, true, UINT64_MAX));
	VK_CHECK(vkResetFences(device, 1, &getCurrentFrame().renderFence));

	// Request image from the swapchain.
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, getCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex));

	VkCommandBuffer cmd = getCurrentFrame().mainCommandBuffer;

	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	VkCommandBufferBeginInfo cmdBufferBeginInfo = vkeUtils::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBufferBeginInfo));

	// Make the swapchain image into writeable mode before rendering.
	vkeUtils::transitionImageLayout(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// Make a clear-color from frame number. This will flash with a 120 frame period.
	float flash = std::abs(std::sin(frameCount / 120.0f));
	VkClearColorValue clearColorValue{ { 0.0f, 0.0f, flash, 1.0f } };
	VkImageSubresourceRange clearRange = vkeUtils::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

	vkCmdClearColorImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearColorValue, 1, &clearRange);

	// Make the swapchain image into presentable mode.
	vkeUtils::transitionImageLayout(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdBufferSubmitInfo = vkeUtils::commandBufferSubmitInfo(cmd);
	VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo = vkeUtils::semaphoreSubmitInfo(getCurrentFrame().swapchainSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
	VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo = vkeUtils::semaphoreSubmitInfo(getCurrentFrame().renderSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
	
	// Submit command buffer to the queue and execute it.
	VkSubmitInfo2 submitInfo = vkeUtils::submitInfo(&cmdBufferSubmitInfo, &waitSemaphoreSubmitInfo, &signalSemaphoreSubmitInfo);

	VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, getCurrentFrame().renderFence));

	VkPresentInfoKHR presentInfo{};

	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &getCurrentFrame().renderSemaphore;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

	frameCount++;
}

void Engine::processInputs(float deltaTime)
{
}

void Engine::windowKeyCallback(GLFWwindow* window, int key, int scanCode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, true);
	}
}

void Engine::windowFramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	Engine* e = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));

	e->stopRendering = width == 0 || height == 0;
}

void Engine::initializeVulkan()
{
	vkb::InstanceBuilder vkbInstanceBuilder;
	vkb::Result<vkb::Instance> vkbResult = vkbInstanceBuilder
		.set_app_name("Vulkan Engine")
		.request_validation_layers(useValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();
	vkb::Instance vkbInstance = vkbResult.value();

	instance = vkbInstance.instance;
	debugMessenger = vkbInstance.debug_messenger;

	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface!");
	}

	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	VkPhysicalDeviceVulkan12Features moreFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };

	features.dynamicRendering = true;
	features.synchronization2 = true;
	moreFeatures.bufferDeviceAddress = true;
	moreFeatures.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector vkbGPUSelector{ vkbInstance };
	vkb::PhysicalDevice vkbGPU = vkbGPUSelector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(moreFeatures)
		.set_surface(surface)
		.select()
		.value();
	vkb::DeviceBuilder deviceBuilder{ vkbGPU };
	vkb::Device vkbDevice = deviceBuilder.build().value();

	device = vkbDevice.device;
	gpu = vkbGPU.physical_device;

	graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorCreateInfo{};

	allocatorCreateInfo.physicalDevice = gpu;
	allocatorCreateInfo.device = device;
	allocatorCreateInfo.instance = instance;
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	vmaCreateAllocator(&allocatorCreateInfo, &allocator);

	mainDeletionQueue.pushFunction([&]()
	{
		vmaDestroyAllocator(allocator);
	});
}

void Engine::initializeSwapchain()
{
	createSwapchain(windowExtent.width, windowExtent.height);
}

void Engine::initializeCommandStructures()
{
	VkCommandPoolCreateInfo cmdPoolCreateInfo = vkeUtils::commandPoolCreateInfo(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {

		VK_CHECK(vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &frames[i].commandPool));

		VkCommandBufferAllocateInfo cmdBufferAllocateInfo = vkeUtils::commandBufferAllocateInfo(frames[i].commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufferAllocateInfo, &frames[i].mainCommandBuffer));
	}
}

void Engine::initializeSyncStructures()
{
	VkFenceCreateInfo fenceCreateInfo = vkeUtils::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkeUtils::semaphoreCreateInfo();

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
	{
		VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));

		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore));
	}
}

void Engine::createSwapchain(uint32_t width, uint32_t height)
{
	swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::SwapchainBuilder vkbSwapchainBuilder{ gpu, device, surface };
	vkb::Swapchain vkbSwapchain = vkbSwapchainBuilder
		// .use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	swapchain = vkbSwapchain.swapchain;
	swapchainExtent = vkbSwapchain.extent;
	swapchainImages = vkbSwapchain.get_images().value();
	swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void Engine::cleanUpSwapchain()
{
	vkDestroySwapchainKHR(device, swapchain, nullptr);

	// Then, destroy swapchain resources.
	for (int i = 0; i < swapchainImageViews.size(); i++)
	{
		vkDestroyImageView(device, swapchainImageViews[i], nullptr);
	}
}
