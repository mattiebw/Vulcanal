#pragma once

class Application;

class Window;

struct RendererSpecification
{
	bool         EnableValidationLayers = false;
	Application* App                    = nullptr;
};

struct FrameData
{
	VkCommandPool CommandPool = nullptr;
	VkCommandBuffer MainCommandBuffer = nullptr;

	VkSemaphore SwapchainSemaphore = nullptr;
	VkSemaphore RenderSemaphore = nullptr;
	VkFence RenderFence = nullptr;
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
	void Shutdown();

protected:
	bool InitInstance();
	bool InitDevice();
	bool InitSwapchain();
	bool InitCommands();
	bool InitSyncStructures();

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

	// Core vulkan objects
	Window*                  m_Window;
	vkb::Instance            m_VKBInstance;
	VkInstance               m_Instance;
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	VkPhysicalDevice         m_GPU;
	VkDevice                 m_Device;
	VkSurfaceKHR             m_Surface;

	// Swapchain objects
	VkSwapchainKHR           m_Swapchain;
	VkFormat                 m_SwapchainImageFormat;
	VkExtent2D               m_SwapchainExtent;
	std::vector<VkImage>     m_SwapchainImages;
	std::vector<VkImageView> m_SwapchainImageViews;

	// Frame state data
	u64 m_FrameIndex = 0;
	std::array<FrameData, FramesInFlight> m_Frames;
	VkQueue m_GraphicsQueue;
	u32 m_GraphicsQueueFamily;

	RendererSpecification m_Spec;
};
