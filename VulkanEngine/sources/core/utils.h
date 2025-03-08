#pragma once

#define FMT_HEADER_ONLY

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <fmt/core.h>

#include <vector>
#include <fstream>

#define VK_CHECK(x)                                                          \
	do {                                                                     \
		VkResult err = x;                                                    \
		if (err) {                                                           \
			fmt::println("Detected VULKAN error: {}", string_VkResult(err)); \
			abort();                                                         \
		}                                                                    \
	} while (0)

namespace vkeUtils
{
	VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool commandPool, uint32_t commandBufferCount = 1);
	VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
	VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);

	VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);
	VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);
	VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2 stageMask);
	
	VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* commandBufferInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo, VkSemaphoreSubmitInfo* signalSemaphoreInfo);

	VkImageCreateInfo imageCreateInfo(VkFormat format, VkExtent3D extent, VkImageUsageFlags usageFlags);
	VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
	VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);
	void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyImageToImage(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, VkExtent2D srcSize, VkExtent2D dstSize);

	bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}
