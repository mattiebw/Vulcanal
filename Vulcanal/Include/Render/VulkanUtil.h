#pragma once

#ifdef VULC_VK_DEBUG
#define VK_CHECK(x) \
	do { \
		VkResult err = x; \
		if (err != VK_SUCCESS) { \
			VULC_ERROR("Vulkan error detected: {}", string_VkResult(err)); \
			VULC_ASSERT(err == VK_SUCCESS); \
		} \
	} while (0)
#else
#define VK_CHECK(x) x
#endif
