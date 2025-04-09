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
void BlitImageToImage(VkCommandBuffer commandBuffer, VkImage source, VkImage destination, VkExtent2D sourceExt,
	VkExtent2D destinationExt, VkFilter filter = VK_FILTER_LINEAR);

inline const char* VulkanSeverityToString(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
	switch (severity)  // NOLINT(clang-diagnostic-switch-enum)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		return "Verbose";
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		return "Info";
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		return "Warning";
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		return "Error";
	default:
		return "Unknown";
	}
}

inline const char* VulkanMessageTypeToString(VkDebugUtilsMessageTypeFlagsEXT type)
{
	switch (type)
	{
	case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
		return "General";
	case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
		return "Validation";
	case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
		return "Performance";
	default:
		return "Unknown";
	}
}

inline VkCommandPoolCreateInfo CreateCommandPoolCreateInfo(u32 queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
{
	VkCommandPoolCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.queueFamilyIndex = queueFamilyIndex;
	info.flags = flags;
	return info;
}

inline VkCommandBufferAllocateInfo CreateCommandBufferAllocateInfo(VkCommandPool commandPool, u32 count = 1, bool primary = true)
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = count;
	commandBufferAllocateInfo.level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	return commandBufferAllocateInfo;
}

inline VkFenceCreateInfo CreateFenceCreateInfo(VkFenceCreateFlags flags = 0)
{
	VkFenceCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}

inline VkSemaphoreCreateInfo CreateSemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0)
{
	VkSemaphoreCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}

inline VkCommandBufferBeginInfo CreateCommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0)
{
	VkCommandBufferBeginInfo info;
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;
	info.pInheritanceInfo = nullptr;
	info.flags = flags;
	return info;
}

inline VkSemaphoreSubmitInfo CreateSemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo submitInfo;
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
	VkCommandBufferSubmitInfo info;
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

inline VkImageCreateInfo CreateImageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags,
	VkExtent3D extent, u32 mipLevels = 1, u32 arrayLayers = 1, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
	VkImageType type = VK_IMAGE_TYPE_2D, VkImageCreateFlags flags = 0)
{
	VkImageCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.imageType = type;
	info.format = format;
	info.extent = extent;
	info.mipLevels = mipLevels;
	info.arrayLayers = arrayLayers;
	info.samples = samples;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usageFlags;
	info.flags = flags;
	
	return info;
}

inline VkImageViewCreateInfo CreateImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags,
	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
	u32 baseMip = 0, u32 levelCount = 1, u32 baseArray = 0, u32 layerCount = 1)
{
	VkImageViewCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;
	info.viewType = viewType;
	info.image = image;
	info.format = format;
	info.subresourceRange.baseMipLevel = baseMip;
	info.subresourceRange.levelCount = levelCount;
	info.subresourceRange.baseArrayLayer = baseArray;
	info.subresourceRange.layerCount = layerCount;
	info.subresourceRange.aspectMask = aspectFlags;
	
	return info;
}
