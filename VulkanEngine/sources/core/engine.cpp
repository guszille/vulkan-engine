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
	initializeDescriptors();
	initializePipelines();
	initializeImgui();

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

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();

		ImGui::NewFrame();

		if (ImGui::Begin("Background"))
		{
			ComputeEffect& selectedComputeEffect = backgroundEffects[currentBackgroundEffect];

			ImGui::Text("Selected Effect: ", selectedComputeEffect.name);
			
			ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

			ImGui::InputFloat4("data1", (float*)&selectedComputeEffect.pushConstants.data1);
			ImGui::InputFloat4("data2", (float*)&selectedComputeEffect.pushConstants.data2);
			ImGui::InputFloat4("data3", (float*)&selectedComputeEffect.pushConstants.data3);
			ImGui::InputFloat4("data4", (float*)&selectedComputeEffect.pushConstants.data4);
		}

		ImGui::End();

		ImGui::Render();

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

void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VK_CHECK(vkResetFences(device, 1, &immFence));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

	VkCommandBuffer cmd = immCommandBuffer;

	VkCommandBufferBeginInfo cmdBufferBeginInfo = vkeUtils::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBufferBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdBufferSubmitInfo = vkeUtils::commandBufferSubmitInfo(cmd);
	VkSubmitInfo2 submitInfo = vkeUtils::submitInfo(&cmdBufferSubmitInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, immFence));

	VK_CHECK(vkWaitForFences(device, 1, &immFence, true, UINT64_MAX));
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

	renderInBackground(deltaTime, cmd);

	// Transition the draw image and the swapchain image into their correct transfer layouts.
	vkeUtils::transitionImageLayout(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkeUtils::transitionImageLayout(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Execute a copy from the draw image into the swapchain image.
	vkeUtils::copyImageToImage(cmd, drawImage.image, swapchainImages[swapchainImageIndex], drawImage.imageExtent2D, swapchainExtent);

	// Make the swapchain image into attachment optimal, so we can draw on it.
	vkeUtils::transitionImageLayout(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	renderImgui(deltaTime, cmd, swapchainImageViews[swapchainImageIndex]);

	// Make the swapchain image into presentable mode.
	vkeUtils::transitionImageLayout(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

void Engine::renderInBackground(float deltaTime, VkCommandBuffer cmd)
{
	// Make a clear-color from frame number. This will flash with a 120 frame period.
	float flash = std::abs(std::sin(frameCount / 120.0f));
	VkClearColorValue clearColorValue{ { 0.0f, 0.0f, flash, 1.0f } };
	VkImageSubresourceRange clearRange = vkeUtils::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

	// vkCmdClearColorImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearColorValue, 1, &clearRange);

	ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

	// Bind the background compute pipeline.
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	// Bind the descriptor set containing the draw image for the compute pipeline.
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, defaultPipelineLayout, 0, 1, &drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(cmd, defaultPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.pushConstants);

	// Execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it.
	vkCmdDispatch(cmd, (uint32_t)std::ceil(drawImage.imageExtent2D.width / 16), (uint32_t)std::ceil(drawImage.imageExtent2D.height / 16), 1);
}

void Engine::renderImgui(float deltaTime, VkCommandBuffer cmd, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo colorAttachment = vkeUtils::renderingAttachmentInfo(targetImageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, nullptr);
	VkRenderingInfo renderingInfo = vkeUtils::renderingInfo(swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderingInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
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

	drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	drawImage.imageExtent2D = { windowExtent.width, windowExtent.height };
	drawImage.imageExtent3D = { windowExtent.width, windowExtent.height, 1 };

	VkImageUsageFlags drawImageUsages{};

	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo imageCretaeInfo = vkeUtils::imageCreateInfo(drawImage.imageFormat, drawImage.imageExtent3D, drawImageUsages);
	VmaAllocationCreateInfo imageAllocationCreateinfo{};

	imageAllocationCreateinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	imageAllocationCreateinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(allocator, &imageCretaeInfo, &imageAllocationCreateinfo, &drawImage.image, &drawImage.allocation, nullptr);

	VkImageViewCreateInfo imageViewCreateinfo = vkeUtils::imageViewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(device, &imageViewCreateinfo, nullptr, &drawImage.imageView));

	mainDeletionQueue.pushFunction([=]()
		{
			vkDestroyImageView(device, drawImage.imageView, nullptr);

			vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
		});
}

void Engine::initializeCommandStructures()
{
	VkCommandPoolCreateInfo cmdPoolCreateInfo = vkeUtils::commandPoolCreateInfo(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {

		VK_CHECK(vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &frames[i].commandPool));

		VkCommandBufferAllocateInfo cmdBufferAllocateInfo = vkeUtils::commandBufferAllocateInfo(frames[i].commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufferAllocateInfo, &frames[i].mainCommandBuffer));
	}

	VK_CHECK(vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &immCommandPool));

	VkCommandBufferAllocateInfo cmdBufferAllocateInfo = vkeUtils::commandBufferAllocateInfo(immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufferAllocateInfo, &immCommandBuffer));

	mainDeletionQueue.pushFunction([=]()
		{
			vkDestroyCommandPool(device, immCommandPool, nullptr);
		});
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

	VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immFence));

	mainDeletionQueue.pushFunction([=]()
		{
			vkDestroyFence(device, immFence, nullptr);
		});
}

void Engine::initializeDescriptors()
{
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};

	// Create a descriptor pool that will hold 10 sets with 1 image each.
	globalDescriptorAllocator.initialize(device, 10, sizes);

	// Make the descriptor set layout for our compute draw.
	{
		DescriptorLayoutBuilder builder;

		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

		drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	// Allocate a descriptor set for our draw image.
	drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);

	VkDescriptorImageInfo descriptorImageInfo{};

	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	descriptorImageInfo.imageView = drawImage.imageView;

	VkWriteDescriptorSet writeDescriptorSet{};

	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.pNext = nullptr;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.dstSet = drawImageDescriptors;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeDescriptorSet.pImageInfo = &descriptorImageInfo;

	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

	mainDeletionQueue.pushFunction([&]()
		{
			globalDescriptorAllocator.clear(device);

			vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, nullptr);
		});
}

void Engine::initializePipelines()
{
	initializeBackgroundPipelines();
}

void Engine::initializeBackgroundPipelines()
{
	VkPushConstantRange pushConstantRange{};
	VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo{};

	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(ComputePushConstants);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computePipelineLayoutCreateInfo.pNext = nullptr;
	computePipelineLayoutCreateInfo.pSetLayouts = &drawImageDescriptorLayout;
	computePipelineLayoutCreateInfo.setLayoutCount = 1;
	computePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	computePipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(device, &computePipelineLayoutCreateInfo, nullptr, &defaultPipelineLayout));

	VkShaderModule gradientShaderModule;
	VkShaderModule skyShaderModule;

	if (!vkeUtils::loadShaderModule("sources/shaders/gradient.comp.spv", device, &gradientShaderModule))
	{
		fmt::print("Error when building the compute shader. \n");
	}

	if (!vkeUtils::loadShaderModule("sources/shaders/sky.comp.spv", device, &skyShaderModule))
	{
		fmt::print("Error when building the compute shader. \n");
	}

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo[2]{ {}, {} };

	pipelineShaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[0].pNext = nullptr;
	pipelineShaderStageCreateInfo[0].stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineShaderStageCreateInfo[0].module = gradientShaderModule;
	pipelineShaderStageCreateInfo[0].pName = "main";

	pipelineShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[1].pNext = nullptr;
	pipelineShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineShaderStageCreateInfo[1].module = skyShaderModule;
	pipelineShaderStageCreateInfo[1].pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo[2]{ {}, {} };

	computePipelineCreateInfo[0].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo[0].pNext = nullptr;
	computePipelineCreateInfo[0].layout = defaultPipelineLayout;
	computePipelineCreateInfo[0].stage = pipelineShaderStageCreateInfo[0];

	computePipelineCreateInfo[1].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo[1].pNext = nullptr;
	computePipelineCreateInfo[1].layout = defaultPipelineLayout;
	computePipelineCreateInfo[1].stage = pipelineShaderStageCreateInfo[1];

	ComputeEffect gradientComputeEffect;
	ComputeEffect skyComputeEffect;

	gradientComputeEffect.name = "Gradient";
	gradientComputeEffect.pipelineLayout = defaultPipelineLayout;
	gradientComputeEffect.pushConstants = {};
	gradientComputeEffect.pushConstants.data1 = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
	gradientComputeEffect.pushConstants.data2 = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

	skyComputeEffect.name = "Sky";
	skyComputeEffect.pipelineLayout = defaultPipelineLayout;
	skyComputeEffect.pushConstants = {};
	skyComputeEffect.pushConstants.data1 = glm::vec4(0.1f, 0.2f, 0.4f, 0.97f);

	VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo[0], nullptr, &gradientComputeEffect.pipeline));
	VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo[1], nullptr, &skyComputeEffect.pipeline));

	backgroundEffects.push_back(gradientComputeEffect);
	backgroundEffects.push_back(skyComputeEffect);

	vkDestroyShaderModule(device, gradientShaderModule, nullptr);
	vkDestroyShaderModule(device, skyShaderModule, nullptr);

	mainDeletionQueue.pushFunction([=]()
	{
		vkDestroyPipelineLayout(device, defaultPipelineLayout, nullptr);
		vkDestroyPipeline(device, gradientComputeEffect.pipeline, nullptr);
		vkDestroyPipeline(device, skyComputeEffect.pipeline, nullptr);
	});
}

void Engine::initializeImgui()
{
	// Create a descriptor pool for ImGUI.
	VkDescriptorPoolSize poolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateinfo{};
	VkDescriptorPool imguiDescriptorPool;

	descriptorPoolCreateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateinfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCreateinfo.maxSets = 1000;
	descriptorPoolCreateinfo.poolSizeCount = (uint32_t)std::size(poolSizes);
	descriptorPoolCreateinfo.pPoolSizes = poolSizes;

	VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolCreateinfo, nullptr, &imguiDescriptorPool));

	// Initialize imgui library.
	ImGui::CreateContext();

	ImGui_ImplGlfw_InitForVulkan(window, true);

	ImGui_ImplVulkan_InitInfo imguiVulkanInitInfo{};

	imguiVulkanInitInfo.Instance = instance;
	imguiVulkanInitInfo.PhysicalDevice = gpu;
	imguiVulkanInitInfo.Device = device;
	imguiVulkanInitInfo.Queue = graphicsQueue;
	imguiVulkanInitInfo.DescriptorPool = imguiDescriptorPool;
	imguiVulkanInitInfo.MinImageCount = 3;
	imguiVulkanInitInfo.ImageCount = 3;
	imguiVulkanInitInfo.UseDynamicRendering = true;
	imguiVulkanInitInfo.PipelineRenderingCreateInfo = {};
	imguiVulkanInitInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	imguiVulkanInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	imguiVulkanInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;
	imguiVulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&imguiVulkanInitInfo);
	ImGui_ImplVulkan_CreateFontsTexture();

	mainDeletionQueue.pushFunction([=]()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();

		ImGui::DestroyContext();

		vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
	});
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
