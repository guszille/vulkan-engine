#pragma once

#include <vulkan/vulkan.h>

#include <deque>
#include <functional>

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
	VkExtent3D imageExtent;
	VkFormat imageFormat;
	VmaAllocation allocation;
};
