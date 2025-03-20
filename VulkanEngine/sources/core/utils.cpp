#include "utils.h"

VkCommandPoolCreateInfo vkeUtils::commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.queueFamilyIndex = queueFamilyIndex;
	info.flags = flags;

	return info;
}

VkCommandBufferAllocateInfo vkeUtils::commandBufferAllocateInfo(VkCommandPool commandPool, uint32_t commandBufferCount)
{
	VkCommandBufferAllocateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext = nullptr;
	info.commandPool = commandPool;
	info.commandBufferCount = commandBufferCount;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	return info;
}

VkCommandBufferBeginInfo vkeUtils::commandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
	VkCommandBufferBeginInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;
	info.pInheritanceInfo = nullptr;
	info.flags = flags;

	return info;
}

VkCommandBufferSubmitInfo vkeUtils::commandBufferSubmitInfo(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;

	return info;
}

VkFenceCreateInfo vkeUtils::fenceCreateInfo(VkFenceCreateFlags flags)
{
	VkFenceCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;

	return info;
}

VkSemaphoreCreateInfo vkeUtils::semaphoreCreateInfo(VkSemaphoreCreateFlags flags)
{
	VkSemaphoreCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;

	return info;
}

VkSemaphoreSubmitInfo vkeUtils::semaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2 stageMask)
{
	VkSemaphoreSubmitInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	info.pNext = nullptr;
	info.semaphore = semaphore;
	info.stageMask = stageMask;
	info.deviceIndex = 0;
	info.value = 1;

	return info;
}

VkSubmitInfo2 vkeUtils::submitInfo(VkCommandBufferSubmitInfo* commandBufferInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo, VkSemaphoreSubmitInfo* signalSemaphoreInfo)
{
	VkSubmitInfo2 info = {};

	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	info.pNext = nullptr;
	info.commandBufferInfoCount = 1;
	info.pCommandBufferInfos = commandBufferInfo;
	info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
	info.pWaitSemaphoreInfos = waitSemaphoreInfo;
	info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
	info.pSignalSemaphoreInfos = signalSemaphoreInfo;

	return info;
}

VkImageCreateInfo vkeUtils::imageCreateInfo(VkFormat format, VkExtent3D extent, VkImageUsageFlags usageFlags)
{
	VkImageCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = extent;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usageFlags;

	return info;
}

VkImageViewCreateInfo vkeUtils::imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = format;
	info.image = image;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspectFlags;

	return info;
}

VkImageSubresourceRange vkeUtils::imageSubresourceRange(VkImageAspectFlags aspectMask)
{
	VkImageSubresourceRange imageSubresourceRange = {};

	imageSubresourceRange.aspectMask = aspectMask;
	imageSubresourceRange.baseMipLevel = 0;
	imageSubresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	imageSubresourceRange.baseArrayLayer = 0;
	imageSubresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return imageSubresourceRange;
}

void vkeUtils::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier2 imageMemoryBarrier = {};
	VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	VkDependencyInfo info = {};

	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	imageMemoryBarrier.oldLayout = oldLayout;
	imageMemoryBarrier.newLayout = newLayout;
	imageMemoryBarrier.subresourceRange = vkeUtils::imageSubresourceRange(aspectMask);
	imageMemoryBarrier.image = image;

	info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	info.pNext = nullptr;
	info.imageMemoryBarrierCount = 1;
	info.pImageMemoryBarriers = &imageMemoryBarrier;

	vkCmdPipelineBarrier2(cmd, &info);
}

void vkeUtils::copyImageToImage(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, VkExtent2D srcSize, VkExtent2D dstSize)
{
	VkImageBlit2 blitRegion = {};
	VkBlitImageInfo2 blitInfo = {};

	blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blitRegion.pNext = nullptr;
	blitRegion.srcOffsets[1].x = srcSize.width;
	blitRegion.srcOffsets[1].y = srcSize.height;
	blitRegion.srcOffsets[1].z = 1;
	blitRegion.dstOffsets[1].x = dstSize.width;
	blitRegion.dstOffsets[1].y = dstSize.height;
	blitRegion.dstOffsets[1].z = 1;
	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;
	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blitInfo.pNext = nullptr;
	blitInfo.srcImage = srcImage;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.dstImage = dstImage;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	vkCmdBlitImage2(cmd, &blitInfo);
}

bool vkeUtils::loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		fmt::println("Can't open file at {}.", filePath);

		return false;
	}

	// Find what the size of the file is by looking up the location of the cursor.
	size_t fileSize = (size_t)file.tellg();

	// SpirV expects the buffer to be on uint32.
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);
	file.close();

	VkShaderModuleCreateInfo shaderModuleCreateinfo = {};
	VkShaderModule shaderModule;

	shaderModuleCreateinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateinfo.pNext = nullptr;
	shaderModuleCreateinfo.codeSize = buffer.size() * sizeof(uint32_t); // It has to be in bytes.
	shaderModuleCreateinfo.pCode = buffer.data();

	if (vkCreateShaderModule(device, &shaderModuleCreateinfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		return false;
	}

	*outShaderModule = shaderModule;

	return true;
}

VkPipelineShaderStageCreateInfo vkeUtils::pipelineShaderStageCreateInfo(VkShaderStageFlagBits stageFlagBits, VkShaderModule shaderModule, const char* entry)
{
	VkPipelineShaderStageCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.stage = stageFlagBits;
	info.module = shaderModule;
	info.pName = entry;

	return info;
}

VkPipelineLayoutCreateInfo vkeUtils::pipelineLayoutCreateInfo()
{
	VkPipelineLayoutCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.setLayoutCount = 0;
	info.pSetLayouts = nullptr;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;

	return info;
}

VkRenderingAttachmentInfo vkeUtils::renderingAttachmentInfo(VkImageView imageView, VkImageLayout imageLayout, VkClearValue* clearValue)
{
	VkRenderingAttachmentInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	info.pNext = nullptr;
	info.imageView = imageView;
	info.imageLayout = imageLayout;
	info.loadOp = clearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	if (clearValue)
	{
		info.clearValue = *clearValue;
	}

	return info;
}

VkRenderingInfo vkeUtils::renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment)
{
	VkRenderingInfo info{};

	info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	info.pNext = nullptr;
	info.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, renderExtent };
	info.layerCount = 1;
	info.colorAttachmentCount = 1;
	info.pColorAttachments = colorAttachment;
	info.pDepthAttachment = depthAttachment;
	info.pStencilAttachment = nullptr;

	return info;
}
