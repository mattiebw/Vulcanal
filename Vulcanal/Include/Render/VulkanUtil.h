#pragma once

#ifdef VULC_VK_DEBUG
#define VK_CHECK(x) \
	do { \
		VkResult err = x; \
		if (err != VK_SUCCESS) { \
			/* VULC_ERROR("Vulkan error detected: {}", string_VkResult(err)); */ \
			auto errString = string_VkResult(err); \
			VULC_ASSERT(err == VK_SUCCESS, "Vulkan error: {}", errString); \
		} \
	} while (0)
#else
#define VK_CHECK(x) x
#endif

inline VkCommandPoolCreateInfo CreateCommandPoolCreateInfo(u32 queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
{
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.queueFamilyIndex = queueFamilyIndex;
	info.flags = flags;
	return info;
}

inline VkCommandBufferAllocateInfo CreateCommandBufferAllocateInfo(VkCommandPool commandPool, bool primary = true)
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = 1;
	commandBufferAllocateInfo.level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	return commandBufferAllocateInfo;
}
