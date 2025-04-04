#include "vulcpch.h"
#include "Render/Renderer.h"

#include "Core/Application.h"

#include <SDL3/SDL_vulkan.h>

Renderer::Renderer()
	: m_Window(nullptr), m_Instance(nullptr), m_DebugMessenger(nullptr), m_GPU(nullptr), m_Device(nullptr),
	  m_Surface(nullptr)
{
}

Renderer::~Renderer()
{
	Shutdown();
}

bool Renderer::Init(RendererSpecification spec)
{
	m_Spec = std::move(spec);
	VULC_ASSERT(m_Spec.App != nullptr,  "Application must be set");
	m_Window = &m_Spec.App->GetWindow();
	VULC_ASSERT(m_Window, "Window must be created before initializing the renderer");

	if (!InitInstance())
		return false;

	// Then, let's create our surface.
	if (!SDL_Vulkan_CreateSurface(m_Window->GetSDLWindow(), m_Instance, nullptr, &m_Surface))
	{
		m_Spec.App->ShowError(fmt::format("Failed to create Vulkan surface: {}", SDL_GetError()), "Vulkan Error");
		return false;
	}

	if (!InitDevice())
		return false;
	PrintDeviceInfo();
	if (!InitSwapchain())
		return false;
	if (!InitCommands())
		return false;
	if (!InitSyncStructures())
		return false;

	return true;
}

void Renderer::Render()
{
	FrameData& frame = GetCurrentFrame();

	// Let's wait for our render fence.
	VK_CHECK(vkWaitForFences(m_Device, 1, &frame.RenderFence, true, 1000000000));
	VK_CHECK(vkResetFences(m_Device, 1, &frame.RenderFence));

	// Let's get our swapchain image.
	u32 swapchainImageIndex = 0;
	VK_CHECK(vkAcquireNextImageKHR(m_Device, m_Swapchain, 1000000000, frame.SwapchainSemaphore, nullptr, &swapchainImageIndex));

	// Reset our command buffer.
	VkCommandBuffer commandBuffer = frame.MainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

	// Begin our command buffer.
	VkCommandBufferBeginInfo beginInfo = CreateCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	// Transition our swapchain image.
	TransitionImage(commandBuffer, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// Let's get our clear colour.
	VkClearColorValue clearValue;
	glm::vec3 color = MathUtil::HSVToRGB(glm::vec3(static_cast<float>(m_FrameIndex % 360) / 360, 1.f, 1.f));
	clearValue = { { color.x, color.y, color.z, 1.0f } };

	VkImageSubresourceRange clearRange = ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

	vkCmdClearColorImage(commandBuffer, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	TransitionImage(commandBuffer, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkCommandBufferSubmitInfo cmdInfo = CreateCommandBufferSubmitInfo(commandBuffer);
	VkSemaphoreSubmitInfo waitInfo = CreateSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame.SwapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = CreateSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.RenderSemaphore);

	VkSubmitInfo2 submit = CreateSubmitInfo(&cmdInfo, &waitInfo, &signalInfo);

	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, frame.RenderFence));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &frame.RenderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_GraphicsQueue, &presentInfo));

	m_FrameIndex++;
}

void Renderer::Shutdown()
{
	if (m_Device)
		vkDeviceWaitIdle(m_Device);

	for (s32 i = 0; i < FramesInFlight; i++)
		ShutdownFrameData(m_Frames[i]);
	
	DestroySwapchain();
	
	if (m_Surface)
	{
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		m_Surface = nullptr;
	}

	if (m_Device)
	{
		vkDestroyDevice(m_Device, nullptr);
		m_Device = nullptr;
		m_GPU = nullptr;
	}

	if (m_DebugMessenger)
	{
		vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);
		m_DebugMessenger = nullptr;
	}

	if (m_Instance != nullptr)
	{
		vkDestroyInstance(m_Instance, nullptr);
		m_Instance = nullptr;
	}
}

bool Renderer::InitInstance()
{
	vkb::InstanceBuilder            instanceBuilder;
	const ApplicationSpecification& appSpec = m_Spec.App->GetSpecification();

	auto instance = instanceBuilder
	                .set_app_name(appSpec.Name.data())
	                .set_app_version(appSpec.Version.Major, appSpec.Version.Minor, appSpec.Version.Patch)
	                .request_validation_layers(m_Spec.EnableValidationLayers)
	                .set_debug_callback(DebugCallback)
	                .require_api_version(1, 3, 0)
	                .build();

	if (!instance.has_value())
	{
		m_Spec.App->ShowError(fmt::format("Failed to create Vulkan instance: {}", instance.error().message()),
		                      "Vulkan Error");
		return false;
	}

	m_VKBInstance    = instance.value();
	m_Instance       = m_VKBInstance.instance;
	m_DebugMessenger = m_VKBInstance.debug_messenger;

	return true;
}

bool Renderer::InitDevice()
{
	VkPhysicalDeviceVulkan12Features deviceFeatures12 = {};
	deviceFeatures12.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	deviceFeatures12.bufferDeviceAddress              = true;
	deviceFeatures12.descriptorIndexing               = true;

	VkPhysicalDeviceVulkan13Features deviceFeatures13 = {};
	deviceFeatures13.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	deviceFeatures13.dynamicRendering                 = true;
	deviceFeatures13.synchronization2                 = true;

	vkb::PhysicalDeviceSelector deviceSelector(m_VKBInstance, m_Surface);

	auto device = deviceSelector
	              .set_minimum_version(1, 3)
	              .set_required_features_12(deviceFeatures12)
	              .set_required_features_13(deviceFeatures13)
	              .select();

	if (!device.has_value())
	{
		m_Spec.App->ShowError(fmt::format("Failed to select Vulkan device: {}", device.error().message()),
		                      "Vulkan Error");
		return false;
	}

	vkb::DeviceBuilder deviceBuilder(device.value());
	auto               logicalDeviceResult = deviceBuilder.build();
	if (!logicalDeviceResult.has_value())
	{
		m_Spec.App->ShowError(fmt::format("Failed to select Vulkan logical device: {}", logicalDeviceResult.error().message()),
							  "Vulkan Error");
		return false;
	}

	const auto& logicalDevice = logicalDeviceResult.value();
	m_GPU                     = device.value().physical_device;
	m_Device                  = logicalDevice.device;

	// Now, initialise the queue and queue family.
	auto queueResult = logicalDevice.get_queue(vkb::QueueType::graphics);
	if (!queueResult.has_value())
	{
		m_Spec.App->ShowError(fmt::format("Failed to create Vulkan queue: {}", queueResult.error().message()),
							  "Vulkan Error");
		return false;
	}
	m_GraphicsQueue = queueResult.value();
	m_GraphicsQueueFamily = logicalDevice.get_queue_index(vkb::QueueType::graphics).value();

	return true;
}

bool Renderer::InitSwapchain()
{
	if (!CreateSwapchain(m_Window->GetWidth(), m_Window->GetHeight()))
		return false;
	
	return true;
}

bool Renderer::InitCommands()
{
	// Basic creation info for our command pools.
	// We want to be able to reset individual command buffers, not just the whole pool.
	VkCommandPoolCreateInfo commandPoolInfo = CreateCommandPoolCreateInfo(m_GraphicsQueueFamily,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (s32 i = 0; i < FramesInFlight; i++)
	{
		// Create our command pool...
		VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_Frames[i].CommandPool));

		// Finally, create our command buffers.
		VkCommandBufferAllocateInfo bufferInfo = CreateCommandBufferAllocateInfo(m_Frames[i].CommandPool, true);
		VK_CHECK(vkAllocateCommandBuffers(m_Device, &bufferInfo, &m_Frames[i].MainCommandBuffer));
	}
	
	return true;
}

bool Renderer::InitSyncStructures()
{
	VkFenceCreateInfo fenceInfo = CreateFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreInfo = CreateSemaphoreCreateInfo();

	for (s32 i = 0; i < FramesInFlight; i++)
	{
		VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &m_Frames[i].RenderFence));

		VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_Frames[i].RenderSemaphore));
		VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_Frames[i].SwapchainSemaphore));
	}
	
	return true;
}

void Renderer::PrintDeviceInfo()
{
	VkPhysicalDeviceProperties2 properties = {};
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	vkGetPhysicalDeviceProperties2(m_GPU, &properties);

	VULC_INFO("Chosen GPU:\n\tName: {}\n\tDriver Version: {}.{}.{}\n\tAPI Version: {}.{}.{}", properties.properties.deviceName,
		VK_VERSION_MAJOR(properties.properties.driverVersion), VK_VERSION_MINOR(properties.properties.driverVersion), VK_VERSION_PATCH(properties.properties.driverVersion),
		VK_VERSION_MAJOR(properties.properties.apiVersion), VK_VERSION_MINOR(properties.properties.apiVersion), VK_VERSION_PATCH(properties.properties.apiVersion));
}

bool Renderer::CreateSwapchain(u32 width, u32 height)
{
	vkb::SwapchainBuilder swapchainBuilder(m_GPU, m_Device, m_Surface);
	m_SwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	auto swapchainResult = swapchainBuilder
	                       .set_desired_format(VkSurfaceFormatKHR{
		                       .format = m_SwapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	                       })
	                       .set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
	                       .set_desired_extent(width, height)
	                       .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	                       .build();

	if (!swapchainResult.has_value())
	{
		m_Spec.App->ShowError(fmt::format("Failed to create Vulkan swapchain: {}", swapchainResult.error().message()),
		                      "Vulkan Error");
		return false;
	}

	vkb::Swapchain& swapchain = swapchainResult.value();
	if (!swapchain.get_images().has_value())
	{
		m_Spec.App->ShowError(
			fmt::format("Failed to get swapchain images: {}", swapchain.get_images().error().message()),
			"Vulkan Error");
		return false;
	}

	m_Swapchain            = swapchain.swapchain;
	m_SwapchainImageFormat = swapchain.image_format;
	m_SwapchainExtent      = swapchain.extent;
	m_SwapchainImages      = swapchain.get_images().value();
	m_SwapchainImageViews  = swapchain.get_image_views().value();

	return true;
}

bool Renderer::DestroySwapchain()
{
	if (!m_Swapchain)
		return true;
	
	vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

	for (auto imageView : m_SwapchainImageViews)
		vkDestroyImageView(m_Device, imageView, nullptr);
	m_SwapchainImageViews.clear();
	m_SwapchainImages.clear();

	return true;
}

void Renderer::ShutdownFrameData(FrameData& frameData) const
{
	if (frameData.CommandPool)
		vkDestroyCommandPool(m_Device, frameData.CommandPool, nullptr);

	if (frameData.RenderFence)
		vkDestroyFence(m_Device, frameData.RenderFence, nullptr);
	if (frameData.SwapchainSemaphore)
		vkDestroySemaphore(m_Device, frameData.SwapchainSemaphore, nullptr);
	if (frameData.RenderSemaphore)
		vkDestroySemaphore(m_Device, frameData.RenderSemaphore, nullptr);

	frameData.CommandPool = nullptr;
	frameData.MainCommandBuffer = nullptr;
}

VkBool32 Renderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                 VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                 void*                                       pUserData)
{
	std::string message = fmt::format("Vulkan Debug {} ({}): {}",
	                                  string_VkDebugUtilsMessageSeverityFlagBitsEXT(messageSeverity),
	                                  string_VkDebugUtilsMessageTypeFlagsEXT(messageType),
	                                  pCallbackData->pMessage);

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		VULC_ERROR("{}", message);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		VULC_WARN("{}", message);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		VULC_INFO("{}", message);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		VULC_TRACE("{}", message);

	VULC_ASSERT(messageSeverity != VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Vulkan error occurred: {}", message);

	return VK_FALSE;
}
