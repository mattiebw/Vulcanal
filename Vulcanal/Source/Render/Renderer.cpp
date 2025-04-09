#include "vulcpch.h"
#include "Render/Renderer.h"

#include "Core/Application.h"

#include <SDL3/SDL_vulkan.h>

#include "Render/Pipelines.h"

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
	VULC_ASSERT(m_Spec.App != nullptr, "Application must be set");
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

	if (!InitAllocator())
		return false;
	
	if (!InitSwapchain())
		return false;

	m_Spec.App->GetWindow().OnWindowResize.BindMethod(this, &Renderer::OnWindowResize);

	if (!InitCommands())
		return false;
	if (!InitSyncStructures())
		return false;
	if (!InitDescriptors())
		return false;
	if (!InitPipelines())
		return false;

	return true;
}

void Renderer::Render()
{
	FrameData& frame = GetCurrentFrame();

	// Let's wait for our render fence.
	VK_CHECK(vkWaitForFences(m_Device, 1, &frame.RenderFence, true, 1000000000));
	VK_CHECK(vkResetFences(m_Device, 1, &frame.RenderFence));

	// Perform any pending deletions from our frame.
	frame.FrameDeletionQueue.Flush();

	// Let's get our swapchain image.
	u32 swapchainImageIndex = 0;
	VK_CHECK(
		vkAcquireNextImageKHR(m_Device, m_Swapchain, 1000000000, frame.SwapchainSemaphore, nullptr, &swapchainImageIndex
		));

	// Reset our command buffer.
	VkCommandBuffer commandBuffer = frame.MainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

	// Begin our command buffer.
	VkCommandBufferBeginInfo beginInfo = CreateCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	// Update our draw extent.
	m_DrawExtent.width = m_DrawImage.Extent.width;
	m_DrawExtent.height = m_DrawImage.Extent.height;
	
	// Transition our draw image.
	TransitionImage(commandBuffer, m_DrawImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// This is where we're actually able to draw things!
	// Clear our screen.
	Clear(commandBuffer);

	// Okay, we're done drawing - lets prepare our draw and swapchain images for the blit.
	TransitionImage(commandBuffer, m_DrawImage.Image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	TransitionImage(commandBuffer, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
	                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Do the actual blit.
	BlitImageToImage(commandBuffer, m_DrawImage.Image, m_SwapchainImages[swapchainImageIndex], m_DrawExtent, m_SwapchainExtent);

	// Transition our swapchain image to the required format for presenting.
	TransitionImage(commandBuffer, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	
	// End our command buffer.
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// Submit our command buffer.
	VkCommandBufferSubmitInfo cmdInfo  = CreateCommandBufferSubmitInfo(commandBuffer);
	VkSemaphoreSubmitInfo     waitInfo = CreateSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
	                                                           frame.SwapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = CreateSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
	                                                             frame.RenderSemaphore);

	VkSubmitInfo2 submit = CreateSubmitInfo(&cmdInfo, &waitInfo, &signalInfo);

	// This is the big moment: submit our command buffer to the GPU.
	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, frame.RenderFence));

	// Present our swapchain.
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext            = nullptr;
	presentInfo.pSwapchains      = &m_Swapchain;
	presentInfo.swapchainCount   = 1;
	presentInfo.pWaitSemaphores    = &frame.RenderSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_GraphicsQueue, &presentInfo));

	// We're done with this frame. Let's iterate.
	m_FrameIndex++;
}

void Renderer::Shutdown()
{
	if (m_Device)
		vkDeviceWaitIdle(m_Device);

	for (s32 i = 0; i < FramesInFlight; i++)
		ShutdownFrameData(m_Frames[i]);

	m_DeletionQueue.Flush();

	DestroySwapchain();

	// We should be done destroying anything we allocated with VMA by this point.
	if (m_Allocator)
	{
		vmaDestroyAllocator(m_Allocator);
		m_Allocator = nullptr;
	}

	// Then destroy the surface...
	if (m_Surface)
	{
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		m_Surface = nullptr;
	}

	// And the logical device...
	if (m_Device)
	{
		vkDestroyDevice(m_Device, nullptr);
		m_Device = nullptr;
		m_GPU    = nullptr;
	}

	// The debug messenger...
	if (m_DebugMessenger)
	{
		vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);
		m_DebugMessenger = nullptr;
	}

	// And finally, the instance.
	if (m_Instance)
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
		m_Spec.App->ShowError(
			fmt::format("Failed to select Vulkan logical device: {}", logicalDeviceResult.error().message()),
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
	m_GraphicsQueue       = queueResult.value();
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
	VkFenceCreateInfo     fenceInfo     = CreateFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreInfo = CreateSemaphoreCreateInfo();

	for (s32 i = 0; i < FramesInFlight; i++)
	{
		VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &m_Frames[i].RenderFence));

		VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_Frames[i].RenderSemaphore));
		VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_Frames[i].SwapchainSemaphore));
	}

	return true;
}

bool Renderer::InitAllocator()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice         = m_GPU;
	allocatorInfo.device                 = m_Device;
	allocatorInfo.instance               = m_Instance;
	allocatorInfo.vulkanApiVersion       = VK_API_VERSION_1_3;
	allocatorInfo.flags                  = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	if (auto result = vmaCreateAllocator(&allocatorInfo, &m_Allocator); result != VK_SUCCESS)
	{
		m_Spec.App->ShowError(fmt::format("Failed to create Vulkan queue: {}", string_VkResult(result)),
		                      "Vulkan Error");
		return false;
	}
	
	return true;
}

bool Renderer::InitDescriptors()
{
	std::vector<PoolSizeRatio> sizes =
	{
		{ .Type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .Ratio = 1 }
	};

	m_DescriptorAllocator.InitPool(m_Device, 10, sizes);

	DescriptorLayoutBuilder builder;
	builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	m_DrawImageDescriptorLayout = builder.Build(m_Device, VK_SHADER_STAGE_COMPUTE_BIT);

	m_DescriptorSet = m_DescriptorAllocator.Allocate(m_Device, m_DrawImageDescriptorLayout);
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = m_DrawImage.ImageView;

	VkWriteDescriptorSet drawImageWrite = {};
	drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageWrite.pNext = nullptr;
	drawImageWrite.dstBinding = 0;
	drawImageWrite.dstSet = m_DescriptorSet;
	drawImageWrite.descriptorCount = 1;
	drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_Device, 1, &drawImageWrite, 0, nullptr);

	m_DeletionQueue.Defer([this]()
	{
		m_DescriptorAllocator.DestroyPool(m_Device);
		vkDestroyDescriptorSetLayout(m_Device, m_DrawImageDescriptorLayout, nullptr);
	});

	return true;
}

bool Renderer::InitPipelines()
{
	VkPipelineLayoutCreateInfo computeLayout = {};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_DrawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VK_CHECK(vkCreatePipelineLayout(m_Device, &computeLayout, nullptr, &m_GradientPipelineLayout));

	VkShaderModule shader;
	if (!LoadShaderModule("Content/Shaders/GradientTest.spv", m_Device, &shader))
	{
		VULC_ERROR("Failed to load Gradient Shader");
		return false;
	}

	VkPipelineShaderStageCreateInfo stageInfo = {};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.pNext = nullptr;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = shader;
	stageInfo.pName = "main"; // Entry point.

	VkComputePipelineCreateInfo computePipelineInfo = {};
	computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineInfo.pNext = nullptr;
	computePipelineInfo.layout = m_GradientPipelineLayout;
	computePipelineInfo.stage = stageInfo;

	VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_GradientPipeline));

	vkDestroyShaderModule(m_Device, shader, nullptr);

	m_DeletionQueue.Defer([this]()
	{
		vkDestroyPipelineLayout(m_Device, m_GradientPipelineLayout, nullptr);
		vkDestroyPipeline(m_Device, m_GradientPipeline, nullptr);
	});

	return true;
}

void Renderer::Clear(VkCommandBuffer cmd) const
{
	// // Let's get our clear colour.
	// VkClearColorValue clearValue;
	// glm::vec3         color = MathUtil::HSVToRGB(glm::vec3(static_cast<float>(m_FrameIndex % 360) / 360, 1.f, 1.f));
	// clearValue              = {{color.x, color.y, color.z, 1.0f}};
	//
	// VkImageSubresourceRange clearRange = ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
	//
	// vkCmdClearColorImage(cmd, m_DrawImage.Image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GradientPipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GradientPipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
	vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16), std::ceil(m_DrawExtent.height / 16), 1);
}

void Renderer::PrintDeviceInfo()
{
	VkPhysicalDeviceProperties2 properties = {};
	properties.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	vkGetPhysicalDeviceProperties2(m_GPU, &properties);

	VULC_INFO("Chosen GPU:\n\tName: {}\n\tDriver Version: {}.{}.{}\n\tAPI Version: {}.{}.{}",
	          properties.properties.deviceName,
	          VK_VERSION_MAJOR(properties.properties.driverVersion),
	          VK_VERSION_MINOR(properties.properties.driverVersion),
	          VK_VERSION_PATCH(properties.properties.driverVersion),
	          VK_VERSION_MAJOR(properties.properties.apiVersion),
	          VK_VERSION_MINOR(properties.properties.apiVersion),
	          VK_VERSION_PATCH(properties.properties.apiVersion));
}

bool Renderer::OnWindowResize(const glm::ivec2& newSize)
{
	VULC_ASSERT(m_Device);
	vkDeviceWaitIdle(m_Device);
	DestroySwapchain();
	CreateSwapchain(newSize.x, newSize.y);
	return false;
}

bool Renderer::CreateSwapchain(u32 width, u32 height)
{
	// We'll use vkb to create our swapchain.
	vkb::SwapchainBuilder swapchainBuilder(m_GPU, m_Device, m_Surface);
	m_SwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM; // Hardcoded format for now.

	// Let's build it!
	auto swapchainResult = swapchainBuilder
	                       .set_desired_format(VkSurfaceFormatKHR{
		                       .format = m_SwapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	                       })
	                       .set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR) // Relaxed VSync for now - make this customisable.
	                       .set_desired_extent(width, height)
	                       .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	                       .build();

	// Basic error checking.
	if (!swapchainResult.has_value())
	{
		m_Spec.App->ShowError(fmt::format("Failed to create Vulkan swapchain: {}", swapchainResult.error().message()),
		                      "Vulkan Error");
		return false;
	}

	// And more error checking!
	vkb::Swapchain& swapchain = swapchainResult.value();
	if (!swapchain.get_images().has_value())
	{
		m_Spec.App->ShowError(
			fmt::format("Failed to get swapchain images: {}", swapchain.get_images().error().message()),
			"Vulkan Error");
		return false;
	}

	// We've got our swapchain - let's update our state.
	m_Swapchain            = swapchain.swapchain;
	m_SwapchainImageFormat = swapchain.image_format;
	m_SwapchainExtent      = swapchain.extent;
	m_SwapchainImages      = swapchain.get_images().value();
	m_SwapchainImageViews  = swapchain.get_image_views().value();

	// Okay, now that the swapchain itself is created, let's create the separate image we'll actually draw to.
	VkExtent3D drawImageExtent = { width, height, 1 };
	m_DrawImage.Extent = drawImageExtent;

	// We'll hardcode the image format for now.
	m_DrawImage.Format = VK_FORMAT_R16G16B16A16_SFLOAT;

	// Our usage flags.
	VkImageUsageFlags uses = {};
	uses |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	uses |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	uses |= VK_IMAGE_USAGE_STORAGE_BIT;
	uses |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo imageCreateInfo = CreateImageCreateInfo(m_DrawImage.Format, uses, drawImageExtent);
	
	// We'll allocate it on local GPU memory.
	VmaAllocationCreateInfo imageAllocInfo = {};
	imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	imageAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// Actually allocate the memory, using VMA.
	vmaCreateImage(m_Allocator, &imageCreateInfo, &imageAllocInfo, &m_DrawImage.Image, &m_DrawImage.Allocation, nullptr);

	// Create the image view.
	VkImageViewCreateInfo imageViewInfo = CreateImageViewCreateInfo(m_DrawImage.Format, m_DrawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(vkCreateImageView(m_Device, &imageViewInfo, nullptr, &m_DrawImage.ImageView));
	
	return true;
}

bool Renderer::DestroySwapchain()
{
	if (m_DrawImage.Image)
	{
		vkDestroyImageView(m_Device, m_DrawImage.ImageView, nullptr);
		vmaDestroyImage(m_Allocator, m_DrawImage.Image, m_DrawImage.Allocation);

		m_DrawImage.Reset();
	}
	
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

	frameData.CommandPool        = nullptr;
	frameData.RenderFence        = nullptr;
	frameData.SwapchainSemaphore = nullptr;
	frameData.RenderSemaphore    = nullptr;
	frameData.MainCommandBuffer  = nullptr;

	frameData.FrameDeletionQueue.Flush();
}

VkBool32 Renderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                 VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                 void*                                       pUserData)
{
	std::string message = fmt::format("Vulkan Debug {} ({}): {}",
	                                  VulkanSeverityToString(messageSeverity),
	                                  VulkanMessageTypeToString(messageType),
	                                  pCallbackData->pMessage);

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		VULC_ERROR("{}", message);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		VULC_WARN("{}", message);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		VULC_INFO("{}", message);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		VULC_TRACE("{}", message);

	VULC_ASSERT(messageSeverity != VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "{}", message);

	return VK_FALSE;
}
