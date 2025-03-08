#include "structures.h"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type)
{
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};

	descriptorSetLayoutBinding.binding = binding;
	descriptorSetLayoutBinding.descriptorCount = 1;
	descriptorSetLayoutBinding.descriptorType = type;

	bindings.push_back(descriptorSetLayoutBinding);
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
	for (VkDescriptorSetLayoutBinding& binding : bindings)
	{
		binding.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo info = {};
	VkDescriptorSetLayout descriptorSetLayout;
	
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pNext = pNext;
	info.pBindings = bindings.data();
	info.bindingCount = (uint32_t)bindings.size();
	info.flags = flags;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &descriptorSetLayout));

	return descriptorSetLayout;
}

void DescriptorAllocator::initialize(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;

	for (PoolSizeRatio ratio : poolRatios)
	{
		poolSizes.push_back(VkDescriptorPoolSize{ .type = ratio.type, .descriptorCount = uint32_t(ratio.ratio * maxSets) });
	}

	VkDescriptorPoolCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.maxSets = maxSets;
	info.pPoolSizes = poolSizes.data();
	info.poolSizeCount = (uint32_t)poolSizes.size();
	info.flags = 0;
	
	vkCreateDescriptorPool(device, &info, nullptr, &pool);
}

void DescriptorAllocator::clearDescriptors(VkDevice device)
{
	vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::clear(VkDevice device)
{
	vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
	VkDescriptorSetAllocateInfo info = {};
	VkDescriptorSet descriptorSet;
	
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.pNext = nullptr;
	info.descriptorPool = pool;
	info.descriptorSetCount = 1;
	info.pSetLayouts = &layout;

	VK_CHECK(vkAllocateDescriptorSets(device, &info, &descriptorSet));

	return descriptorSet;
}
