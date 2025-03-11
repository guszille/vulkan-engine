#pragma once

#include <vulkan/vulkan.h>

#include <vkma/vk_mem_alloc.h>

#include <span>
#include <deque>
#include <functional>

#include "utils.h"

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void pushFunction(std::function<void()>&& function)
	{
		deletors.push_back(function);
	}

	void flush()
	{
		// Reverse iterate the deletion queue to execute all the functions.
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
		{
			(*it)();
		}

		deletors.clear();
	}
};

struct AllocatedImage
{
	VkImage image;
	VkImageView imageView;
	VkFormat imageFormat;
	VkExtent2D imageExtent2D;
	VkExtent3D imageExtent3D;

	VmaAllocation allocation;
};

struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void addBinding(uint32_t binding, VkDescriptorType type);
	void clear();
	VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator
{
	struct PoolSizeRatio
	{
		VkDescriptorType type;
		float ratio;
	};

	VkDescriptorPool pool;

	void initialize(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
	void clearDescriptors(VkDevice device);
	void clear(VkDevice device);
	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};
