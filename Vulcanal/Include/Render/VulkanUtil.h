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

VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags flags);
void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

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

inline VkFenceCreateInfo CreateFenceCreateInfo(VkFenceCreateFlags flags = 0)
{
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}

inline VkSemaphoreCreateInfo CreateSemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0)
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}

inline VkCommandBufferBeginInfo CreateCommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0)
{
	VkCommandBufferBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;
	info.pInheritanceInfo = nullptr;
	info.flags = flags;
	return info;
}

inline VkSemaphoreSubmitInfo CreateSemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.semaphore = semaphore;
	submitInfo.stageMask = stageMask;
	submitInfo.deviceIndex = 0;
	submitInfo.value = 1;

	return submitInfo;
}

inline VkCommandBufferSubmitInfo CreateCommandBufferSubmitInfo(VkCommandBuffer commandBuffer)
{
	VkCommandBufferSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = commandBuffer;
	info.deviceMask = 0;

	return info;
}

inline VkSubmitInfo2 CreateSubmitInfo(const VkCommandBufferSubmitInfo* commandBuffer,
	const VkSemaphoreSubmitInfo* signalSemaphore, const VkSemaphoreSubmitInfo* waitSemaphore)
{
	VkSubmitInfo2 info = {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	info.pNext = nullptr;

	info.waitSemaphoreInfoCount = waitSemaphore == nullptr ? 0 : 1;
	info.pWaitSemaphoreInfos = waitSemaphore;

	info.signalSemaphoreInfoCount = signalSemaphore == nullptr ? 0 : 1;
	info.pSignalSemaphoreInfos = signalSemaphore;

	info.commandBufferInfoCount = 1;
	info.pCommandBufferInfos = commandBuffer;

	return info;
}
