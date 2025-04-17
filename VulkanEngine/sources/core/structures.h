#pragma once

#include <vulkan/vulkan.h>

#include <vkma/vk_mem_alloc.h>

#include <glm/glm.hpp>

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

struct AllocatedBuffer
{
	VkBuffer buffer;

	VmaAllocation allocation;
	VmaAllocationInfo allocationInfo;
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

class PipelineBuilder
{
public:
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	VkPipelineLayout pipelineLayout;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
	VkPipelineRenderingCreateInfo renderingCreateInfo;
	VkFormat colorAttachmentFormat;

	PipelineBuilder() { clear(); }

	void clear();
	void setShaders(VkShaderModule vertexShaderModule, VkShaderModule fragmentShaderModule);
	void setInputTopology(VkPrimitiveTopology primitiveTopology);
	void setPolygonMode(VkPolygonMode polygonMode);
	void setCullMode(VkCullModeFlags cullModeFlags, VkFrontFace frontFace);
	void disableMultisampling();
	void enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp);
	void disableDepthTest();
	void disableBlending();
	void setColorAttachmentFormat(VkFormat format);
	void setDepthFormat(VkFormat format);

	VkPipeline build(VkDevice device);
};

// The reason the uv parameters are interleaved is due to alignement limitations on GPUs.
// We want this structure to match the shader version so interleaving it like this improves it.
struct Vertex
{
	glm::vec3 position;
	float uvX;
	glm::vec3 normal;
	float uvY;
	glm::vec4 color;
};

struct GPUMeshBuffers
{
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer indexBuffer;

	VkDeviceAddress vertexBufferAddress;
};

struct GPUDrawPushConstants
{
	glm::mat4 worldMatrix;

	VkDeviceAddress vertexBufferAddress;
};
