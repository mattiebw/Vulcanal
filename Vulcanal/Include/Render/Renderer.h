﻿#pragma once

#include "Descriptors.h"
#include "Image.h"

class Application;
class Window;

struct RendererSpecification
{
	bool         EnableValidationLayers = false;
	Application* App                    = nullptr;
};

struct FrameData
{
	VkCommandPool   CommandPool       = nullptr;
	VkCommandBuffer MainCommandBuffer = nullptr;

	VkSemaphore SwapchainSemaphore = nullptr;
	VkSemaphore RenderSemaphore    = nullptr;
	VkFence     RenderFence        = nullptr;

	DeletionQueue FrameDeletionQueue;
};

constexpr u16 FramesInFlight = 2;

class Renderer
{
public:
	Renderer();
	~Renderer();

	Renderer(const Renderer& other)                = delete;
	Renderer(Renderer&& other) noexcept            = delete;
	Renderer& operator=(const Renderer& other)     = delete;
	Renderer& operator=(Renderer&& other) noexcept = delete;

	bool Init(RendererSpecification spec);
	void Render();
	void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;
	void Shutdown();

protected:
	bool InitInstance();
	bool InitDevice();
	bool InitSwapchain();
	bool InitCommands();
	bool InitSyncStructures();
	bool InitAllocator();
	bool InitDescriptors();
	bool InitPipelines();
	bool InitImGUI();

	void Clear(VkCommandBuffer cmd) const;
	void DrawImGUI(VkCommandBuffer cmd, VkImageView targetImage);

	void PrintDeviceInfo();

	bool OnWindowResize(const glm::ivec2& newSize);

	bool CreateSwapchain(u32 width, u32 height);
	bool DestroySwapchain();

	void ShutdownFrameData(FrameData& frameData) const;

	NODISCARD FORCEINLINE FrameData& GetCurrentFrame()
	{
		return m_Frames[m_FrameIndex % FramesInFlight];
	}

	static VkBool32 DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
	                              VkDebugUtilsMessageTypeFlagsEXT             messageType,
	                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	                              void*                                       pUserData);

	DeletionQueue m_DeletionQueue;

	// Core vulkan objects
	Window*                  m_Window;
	vkb::Instance            m_VKBInstance;
	VkInstance               m_Instance;
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	VkPhysicalDevice         m_GPU;
	VkDevice                 m_Device;
	VkSurfaceKHR             m_Surface;
	VmaAllocator             m_Allocator;

	// Swapchain objects
	VkSwapchainKHR           m_Swapchain;
	VkFormat                 m_SwapchainImageFormat;
	VkExtent2D               m_SwapchainExtent;
	std::vector<VkImage>     m_SwapchainImages;
	std::vector<VkImageView> m_SwapchainImageViews;

	// Descriptors and pipelines
	DescriptorAllocator   m_DescriptorAllocator;
	VkDescriptorSet       m_DescriptorSet;
	VkDescriptorSetLayout m_DrawImageDescriptorLayout;
	VkPipeline            m_GradientPipeline;
	VkPipelineLayout      m_GradientPipelineLayout;

	// ImGUI
	bool             m_ImGUIInitialised = false;
	VkDescriptorPool m_ImGUIDescriptorPool;

	// Immediate submission structures
	VkFence         m_ImmediateFence;
	VkCommandBuffer m_ImmediateCommandBuffer;
	VkCommandPool   m_ImmediateCommandPool;

	// Frame state data
	u64                                   m_FrameIndex = 0;
	std::array<FrameData, FramesInFlight> m_Frames;
	VkQueue                               m_GraphicsQueue;
	u32                                   m_GraphicsQueueFamily;
	AllocatedImage                        m_DrawImage;
	VkExtent2D                            m_DrawExtent;

	RendererSpecification m_Spec;
};
