#pragma once

#include "Descriptors.h"
#include "Image.h"

class Application;
class Window;

struct RendererSpecification
{
	bool         EnableValidationLayers = false;
	Application* App                    = nullptr;
	int          GPUIndexOverride       = -1;
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

struct PushConstants
{
	glm::vec4 Colour1      = {}, Colour2 = {}, Colour3 = {};
	glm::vec3 ColourPoints = {};
};

constexpr u16 FramesInFlight = 2;

class Renderer
{
public:
	Renderer() = default;
	~Renderer();

	Renderer(const Renderer& other)                = delete;
	Renderer(Renderer&& other) noexcept            = delete;
	Renderer& operator=(const Renderer& other)     = delete;
	Renderer& operator=(Renderer&& other) noexcept = delete;

	bool Init(RendererSpecification spec);
	void Render();
	void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;
	void Shutdown();

	NODISCARD FORCEINLINE const std::vector<std::string>& GetGPUNames() const { return m_GPUNames; }
	NODISCARD FORCEINLINE s32                             GetSelectedGPUIndex() const { return m_GPUIndex; }

protected:
	// Initialisation functions
	bool InitInstance();
	bool InitDevice();
	bool InitSwapchain();
	bool InitCommands();
	bool InitSyncStructures();
	bool InitAllocator();
	bool InitDescriptors();
	bool InitPipelines();
	bool InitImGUI();

	// Structure creation and destruction functions
	bool CreateSwapchain(u32 width, u32 height);
	bool DestroySwapchain();
	void ShutdownFrameData(FrameData& frameData) const;

	// Utility functions
	void PrintDeviceInfo();

	// Drawing functions
	void Clear(VkCommandBuffer cmd) const;
	void DrawImGUI(VkCommandBuffer cmd, VkImageView targetImage);
	void OnDrawIMGui();

	// Event functions
	bool OnWindowResize(const glm::ivec2& newSize);

	// Getters
	NODISCARD FORCEINLINE FrameData& GetCurrentFrame() { return m_Frames[m_FrameIndex % FramesInFlight]; }

	static VkBool32 DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
	                              VkDebugUtilsMessageTypeFlagsEXT             messageType,
	                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	                              void*                                       pUserData);

	// Core vulkan objects
	Window*                  m_Window         = nullptr;
	vkb::Instance            m_VKBInstance    = {};
	VkInstance               m_Instance       = nullptr;
	VkDebugUtilsMessengerEXT m_DebugMessenger = nullptr;
	VkPhysicalDevice         m_GPU            = nullptr;
	VkDevice                 m_Device         = nullptr;
	VkSurfaceKHR             m_Surface        = nullptr;
	VmaAllocator             m_Allocator      = nullptr;
	std::vector<std::string> m_GPUNames       = {};
	s32                      m_GPUIndex       = -1;
	DeletionQueue            m_DeletionQueue  = {};

	// Swapchain objects
	VkSwapchainKHR           m_Swapchain            = nullptr;
	VkFormat                 m_SwapchainImageFormat = {};
	VkExtent2D               m_SwapchainExtent      = {};
	std::vector<VkImage>     m_SwapchainImages      = {};
	std::vector<VkImageView> m_SwapchainImageViews  = {};

	// Descriptors and pipelines
	DescriptorAllocator   m_DescriptorAllocator       = {};
	VkDescriptorSet       m_DescriptorSet             = nullptr;
	VkDescriptorSetLayout m_DrawImageDescriptorLayout = nullptr;
	VkPipeline            m_GradientPipeline          = nullptr;
	VkPipelineLayout      m_GradientPipelineLayout    = nullptr;

	// ImGUI
	bool             m_ImGUIInitialised    = false;
	VkDescriptorPool m_ImGUIDescriptorPool = nullptr;

	// Immediate submission structures
	VkFence         m_ImmediateFence         = nullptr;
	VkCommandBuffer m_ImmediateCommandBuffer = nullptr;
	VkCommandPool   m_ImmediateCommandPool   = nullptr;

	// Test stuff
	PushConstants m_PushConstants = {};

	// Frame state data
	u64                                   m_FrameIndex          = 0;
	std::array<FrameData, FramesInFlight> m_Frames              = {};
	VkQueue                               m_GraphicsQueue       = nullptr;
	u32                                   m_GraphicsQueueFamily = 0;
	AllocatedImage                        m_DrawImage           = {};
	VkExtent2D                            m_DrawExtent          = {};

	RendererSpecification m_Spec = {};
};
