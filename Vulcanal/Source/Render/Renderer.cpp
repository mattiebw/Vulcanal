﻿#include "vulcpch.h"
#include "Render/Renderer.h"

#include <SDL3/SDL_vulkan.h>

#ifndef VULC_NO_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#endif

#include "Core/Application.h"
#include "Render/Pipelines.h"

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

	if (!InitImGUI())
		return false;
	
	m_PushConstants.Colour1 = glm::vec4(1, 0, 0, 1);
	m_PushConstants.Colour2 = glm::vec4(0, 1, 0, 1);
	m_PushConstants.Colour3 = glm::vec4(0, 0, 1, 1);
	m_PushConstants.ColourPoints = glm::vec3(0.1f, 0.5f, 0.8f);

	m_Spec.App->OnDrawIMGui.BindMethod(this, &Renderer::OnDrawIMGui);

	return true;
}

void Renderer::Render()
{
	if (m_SwapchainDirty)
		RecreateSwapchain();
	
	FrameData& frame = GetCurrentFrame();

	// Let's wait for our render fence.
	VK_CHECK(vkWaitForFences(m_Device, 1, &frame.RenderFence, true, 1000000000));
	VK_CHECK(vkResetFences(m_Device, 1, &frame.RenderFence));

	// Perform any pending deletions from our frame.
	frame.FrameDeletionQueue.Flush();

	// Time to get the swapchain image that we'll blit to when we present.
	// Let's quickly talk about semaphores. The swapchain semaphore we pass in here is used to signal that the swapchain
	// image is available. So, you'll see later when we submit our command buffer that we wait on this semaphore before
	// we actually start executing the commands. When we submit, we also provide a  semaphore (frame.RenderSemaphore),
	// to be signalled when the command buffer is done executing. We use this to know when we can present the swapchain
	// image to the screen - the call to vkQueuePresent takes the render semaphore as a wait semaphore parameter.
	VK_CHECK(
		vkAcquireNextImageKHR(m_Device, m_Swapchain, 1000000000, frame.SwapchainSemaphore, nullptr, &m_SwapchainImageIndex
		));

	// Reset our command buffer.
	VkCommandBuffer commandBuffer = frame.MainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

	// Begin our command buffer.
	VkCommandBufferBeginInfo beginInfo = CreateCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	// Update our draw extent.
	m_DrawExtent.width  = m_DrawImage.Extent.width;
	m_DrawExtent.height = m_DrawImage.Extent.height;

	// Transition our draw image.
	TransitionImage(commandBuffer, m_DrawImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// This is where we're actually able to draw things!
	// Clear our screen.
	Clear(commandBuffer);

	// Okay, we're done drawing - lets prepare our draw and swapchain images for the blit.
	TransitionImage(commandBuffer, m_DrawImage.Image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	TransitionImage(commandBuffer, m_SwapchainImages[m_SwapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
	                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Do the actual blit.
	BlitImageToImage(commandBuffer, m_DrawImage.Image, m_SwapchainImages[m_SwapchainImageIndex], m_DrawExtent,
	                 m_SwapchainExtent);

#ifndef VULC_NO_IMGUI
	TransitionImage(commandBuffer, m_SwapchainImages[m_SwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	DrawImGUI(commandBuffer, m_SwapchainImageViews[m_SwapchainImageIndex]);
	TransitionImage(commandBuffer, m_SwapchainImages[m_SwapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
#else
	// Transition our swapchain image to the required format for presenting.
	TransitionImage(commandBuffer, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
#endif

	// End our command buffer.
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// Submit our command buffer.
	VkCommandBufferSubmitInfo cmdInfo  = CreateCommandBufferSubmitInfo(commandBuffer);
	VkSemaphoreSubmitInfo     waitInfo = CreateSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
	                                                           frame.SwapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = CreateSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
	                                                             frame.RenderSemaphore);

	VkSubmitInfo2 submit = CreateSubmitInfo(&cmdInfo, &signalInfo, &waitInfo);

	// This is the big moment: submit our command buffer to the GPU.
	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, frame.RenderFence));
}

void Renderer::Present()
{
	// Present our swapchain.
	VkPresentInfoKHR presentInfo   = {};
	presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext              = nullptr;
	presentInfo.pSwapchains        = &m_Swapchain;
	presentInfo.swapchainCount     = 1;
	presentInfo.pWaitSemaphores    = &GetCurrentFrame().RenderSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices      = &m_SwapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_GraphicsQueue, &presentInfo));

	// We're done with this frame. Let's iterate.
	m_FrameIndex++;
}

void Renderer::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const
{
	VK_CHECK(vkResetFences(m_Device, 1, &m_ImmediateFence));
	VK_CHECK(vkResetCommandBuffer(m_ImmediateCommandBuffer, 0));

	VkCommandBufferBeginInfo beginInfo = CreateCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(m_ImmediateCommandBuffer, &beginInfo));

	function(m_ImmediateCommandBuffer);

	VK_CHECK(vkEndCommandBuffer(m_ImmediateCommandBuffer));

	VkCommandBufferSubmitInfo submitInfo = CreateCommandBufferSubmitInfo(m_ImmediateCommandBuffer);
	VkSubmitInfo2             submit     = CreateSubmitInfo(&submitInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, m_ImmediateFence));
	VK_CHECK(vkWaitForFences(m_Device, 1, &m_ImmediateFence, true, 9999999999));
}

void Renderer::Shutdown()
{
	if (m_Device)
		vkDeviceWaitIdle(m_Device);
	else
		return; // If we have no device, we really shouldn't be here!

	// Shutdown ImGUI
	if (m_ImGUIInitialised)
	{
		ImGui_ImplVulkan_Shutdown();
		m_ImGUIInitialised = false;
	}

	if (m_ImGUIDescriptorPool)
	{
		vkDestroyDescriptorPool(m_Device, m_ImGUIDescriptorPool, nullptr);
		m_ImGUIDescriptorPool = nullptr;
	}

	// Destroy our immediate command pool and fence.
	if (m_ImmediateCommandPool)
	{
		vkDestroyCommandPool(m_Device, m_ImmediateCommandPool, nullptr);
		m_ImmediateCommandPool   = nullptr;
		m_ImmediateCommandBuffer = nullptr;
	}

	if (m_ImmediateFence)
	{
		vkDestroyFence(m_Device, m_ImmediateFence, nullptr);
		m_ImmediateFence = nullptr;
	}

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

	deviceSelector
		.set_minimum_version(1, 3)
		.set_required_features_12(deviceFeatures12)
		.set_required_features_13(deviceFeatures13)
		// Still selecting an integrated Radeon over a 3070 on my laptop, so we'll do some manual selection.
		.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);

	auto deviceResult = deviceSelector.select_devices();
	if (!deviceResult.has_value())
	{
		m_Spec.App->ShowError(fmt::format("Failed to select Vulkan device: {}", deviceResult.error().message()),
		                      "Vulkan Error");
		return false;
	}

	// We're going to assume this won't fail if select_devices() didn't fail.
	m_GPUNames = deviceSelector.select_device_names().value();

	auto& devices = deviceResult.value();
	VULC_ASSERT(!devices.empty(), "No devices found!"); // Don't think this can happen.
	u32 gpuIndex = 0;
	if (m_Spec.GPUIndexOverride >= 0 && static_cast<size_t>(m_Spec.GPUIndexOverride) < devices.size())
		gpuIndex = m_Spec.GPUIndexOverride;
	else if (m_Spec.GPUIndexOverride != 0)
	{
		// No override - let's select the GPU with the highest VRAM.
		u64 maxMemory = 0;

		// For each device...
		for (u32 deviceIt = 0; deviceIt < devices.size(); deviceIt++)
		{
			// Add up the size of all the heaps that are device local - this will exclude system RAM on integrated GPUs.
			const auto& memoryProperties = devices[deviceIt].memory_properties;
			u64         memSum           = 0;
			for (u32 heapIt = 0; heapIt < memoryProperties.memoryHeapCount; heapIt++)
			{
				if (memoryProperties.memoryHeaps[heapIt].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
					memSum += memoryProperties.memoryHeaps[heapIt].size;
			}

			// And see if it's the largest we've seen so far.
			if (memSum > maxMemory)
			{
				maxMemory = memSum;
				gpuIndex  = deviceIt;
			}
		}
	}
	m_GPUIndex = static_cast<s32>(gpuIndex);

	vkb::DeviceBuilder deviceBuilder(devices[gpuIndex]);
	auto               logicalDeviceResult = deviceBuilder.build();
	if (!logicalDeviceResult.has_value())
	{
		m_Spec.App->ShowError(
			fmt::format("Failed to select Vulkan logical device: {}", logicalDeviceResult.error().message()),
			"Vulkan Error");
		return false;
	}

	const auto& logicalDevice = logicalDeviceResult.value();
	m_GPU                     = devices[gpuIndex].physical_device;
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
		VkCommandBufferAllocateInfo bufferInfo = CreateCommandBufferAllocateInfo(m_Frames[i].CommandPool, 1, true);
		VK_CHECK(vkAllocateCommandBuffers(m_Device, &bufferInfo, &m_Frames[i].MainCommandBuffer));
	}

	// And our immediate command buffer.
	VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_ImmediateCommandPool));
	VkCommandBufferAllocateInfo immediateCommandBufferInfo = CreateCommandBufferAllocateInfo(
		m_ImmediateCommandPool, 1, true);
	VK_CHECK(vkAllocateCommandBuffers(m_Device, &immediateCommandBufferInfo, &m_ImmediateCommandBuffer));

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

	// And init our immediate fence.
	VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &m_ImmediateFence));

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
		{.Type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .Ratio = 1}
	};

	m_DescriptorAllocator.InitPool(m_Device, 10, sizes);

	DescriptorLayoutBuilder builder;
	builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	m_DrawImageDescriptorLayout = builder.Build(m_Device, VK_SHADER_STAGE_COMPUTE_BIT);

	m_DescriptorSet                 = m_DescriptorAllocator.Allocate(m_Device, m_DrawImageDescriptorLayout);
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout           = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView             = m_DrawImage.ImageView;

	VkWriteDescriptorSet drawImageWrite = {};
	drawImageWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageWrite.pNext                = nullptr;
	drawImageWrite.dstBinding           = 0;
	drawImageWrite.dstSet               = m_DescriptorSet;
	drawImageWrite.descriptorCount      = 1;
	drawImageWrite.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo           = &imageInfo;

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
	computeLayout.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext                      = nullptr;
	computeLayout.pSetLayouts                = &m_DrawImageDescriptorLayout;
	computeLayout.setLayoutCount             = 1;

	VkPushConstantRange pushConstant = {};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(PushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(m_Device, &computeLayout, nullptr, &m_GradientPipelineLayout));

	VkShaderModule shader;
	if (!LoadShaderModule("Content/Shaders/GradientTest.spv", m_Device, &shader))
	{
		VULC_ERROR("Failed to load Gradient Shader");
		return false;
	}

	VkPipelineShaderStageCreateInfo stageInfo = {};
	stageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.pNext                           = nullptr;
	stageInfo.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module                          = shader;
	stageInfo.pName                           = "main"; // Entry point.

	VkComputePipelineCreateInfo computePipelineInfo = {};
	computePipelineInfo.sType                       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineInfo.pNext                       = nullptr;
	computePipelineInfo.layout                      = m_GradientPipelineLayout;
	computePipelineInfo.stage                       = stageInfo;

	VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_GradientPipeline));

	vkDestroyShaderModule(m_Device, shader, nullptr);

	m_DeletionQueue.Defer([this]()
	{
		vkDestroyPipelineLayout(m_Device, m_GradientPipelineLayout, nullptr);
		vkDestroyPipeline(m_Device, m_GradientPipeline, nullptr);
	});

	return true;
}

bool Renderer::InitImGUI()
{
	VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets                    = 1000;
	pool_info.poolSizeCount              = static_cast<u32>(std::size(pool_sizes));
	pool_info.pPoolSizes                 = pool_sizes;

	VK_CHECK(vkCreateDescriptorPool(m_Device, &pool_info, nullptr, &m_ImGUIDescriptorPool));

	ImGui_ImplVulkan_InitInfo vulkanInitInfo   = {};
	vulkanInitInfo.Instance                    = m_Instance;
	vulkanInitInfo.PhysicalDevice              = m_GPU;
	vulkanInitInfo.Device                      = m_Device;
	vulkanInitInfo.Queue                       = m_GraphicsQueue;
	vulkanInitInfo.DescriptorPool              = m_ImGUIDescriptorPool;
	vulkanInitInfo.MinImageCount               = 3;
	vulkanInitInfo.ImageCount                  = 3;
	vulkanInitInfo.UseDynamicRendering         = true;
	vulkanInitInfo.PipelineRenderingCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .pNext = nullptr
	};
	vulkanInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
	vulkanInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_SwapchainImageFormat;
	vulkanInitInfo.MSAASamples                                         = VK_SAMPLE_COUNT_1_BIT;
	vulkanInitInfo.CheckVkResultFn = &CheckImGUIVkResult; 

	VULC_CHECK(ImGui_ImplVulkan_Init(&vulkanInitInfo), "Failed to init imgui vulkan backend");
	VULC_CHECK(ImGui_ImplVulkan_CreateFontsTexture(), "Failed to init imgui fonts texture");

	m_ImGUIInitialised = true;

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
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GradientPipelineLayout, 0, 1, &m_DescriptorSet, 0,
	                        nullptr);

	vkCmdPushConstants(cmd, m_GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &m_PushConstants);
	
	vkCmdDispatch(cmd, static_cast<u32>(std::ceil(m_DrawExtent.width / 16)),
	              static_cast<u32>(std::ceil(m_DrawExtent.height / 16)), 1);
}

void Renderer::DrawImGUI(VkCommandBuffer cmd, VkImageView targetImage)
{
	VkRenderingAttachmentInfo colorAttachment = CreateRenderingColorAttachmentInfo(targetImage, nullptr,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = CreateRenderingInfo(m_SwapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	
	vkCmdEndRendering(cmd);
}

void Renderer::OnDrawIMGui()
{
	ImGui::Begin("Gradient");
	ImGui::DragFloat4("Colour 1", &m_PushConstants.Colour1.r, 0.01f, 0, 1);
	ImGui::DragFloat4("Colour 2", &m_PushConstants.Colour2.r, 0.01f, 0, 1);
	ImGui::DragFloat4("Colour 3", &m_PushConstants.Colour3.r, 0.01f, 0, 1);
	ImGui::DragFloat3("Colour Points", &m_PushConstants.ColourPoints.r, 0.01f, 0, 1);
	ImGui::End();
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
	m_SwapchainDirty = true;
	return false;
}

void Renderer::SetVSync(bool vsync)
{
	m_Spec.VSync = vsync;
	m_SwapchainDirty = true;
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
	                       .set_desired_present_mode(
	                       		m_Spec.VSync
	                       		? VK_PRESENT_MODE_FIFO_RELAXED_KHR
	                       		: VK_PRESENT_MODE_IMMEDIATE_KHR)
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
	VkExtent3D drawImageExtent = {width, height, 1};
	m_DrawImage.Extent         = drawImageExtent;

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
	imageAllocInfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
	imageAllocInfo.requiredFlags           = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// Actually allocate the memory, using VMA.
	vmaCreateImage(m_Allocator, &imageCreateInfo, &imageAllocInfo, &m_DrawImage.Image, &m_DrawImage.Allocation,
	               nullptr);

	// Create the image view.
	VkImageViewCreateInfo imageViewInfo = CreateImageViewCreateInfo(m_DrawImage.Format, m_DrawImage.Image,
	                                                                VK_IMAGE_ASPECT_COLOR_BIT);
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

void Renderer::RecreateSwapchain()
{
	VULC_ASSERT(m_Device);
	vkDeviceWaitIdle(m_Device);
	DestroySwapchain();
	CreateSwapchain(m_Spec.App->GetWindow().GetWidth(), m_Spec.App->GetWindow().GetHeight());

	// BODGE FIX FOR RESIZING
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout           = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView             = m_DrawImage.ImageView;

	VkWriteDescriptorSet drawImageWrite = {};
	drawImageWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageWrite.pNext                = nullptr;
	drawImageWrite.dstBinding           = 0;
	drawImageWrite.dstSet               = m_DescriptorSet;
	drawImageWrite.descriptorCount      = 1;
	drawImageWrite.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo           = &imageInfo;

	vkUpdateDescriptorSets(m_Device, 1, &drawImageWrite, 0, nullptr);

	m_SwapchainDirty = false;
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

void Renderer::CheckImGUIVkResult(VkResult result)
{
	VK_CHECK(result);
}
