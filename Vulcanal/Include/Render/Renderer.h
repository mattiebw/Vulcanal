#pragma once

class Application;

class Window;

struct RendererSpecification
{
	bool         EnableValidationLayers = false;
	Application* App                    = nullptr;
};

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
	void Shutdown();

protected:
	bool InitInstance();
	bool InitDevice();
	bool InitSwapchain();
	bool InitCommands();
	bool InitSyncStructures();

	bool CreateSwapchain(u32 width, u32 height);
	bool DestroySwapchain();

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

	RendererSpecification m_Spec;
};
