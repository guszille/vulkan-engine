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
