#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

struct WindowSpecification
{
	std::string Title      = "Vulcanal";
	glm::ivec2  Size       = {1280, 720};
	glm::ivec2  Position   = {0, 0};
	bool        Fullscreen = false;
	bool        Resizable  = true;
	bool        VSync      = true;
};

class Window
{
public:
	explicit Window(WindowSpecification spec);
	~Window() { Destroy(); }

	Window(const Window& other)                = delete;
	Window(Window&& other) noexcept            = delete;
	Window& operator=(const Window& other)     = delete;
	Window& operator=(Window&& other) noexcept = delete;

	bool Create();
	void PollEvents();
	void Destroy();

	void SetTitle(const std::string& title);
	void SetSize(const glm::ivec2& size);
	void SetPosition(const glm::ivec2& position);
	void SetFullscreen(bool fullscreen);
	void SetResizable(bool resizable);
	void SetVSync(bool vsync);

	NODISCARD FORCEINLINE const std::string& GetTitle() const { return m_Specification.Title; }
	NODISCARD FORCEINLINE const glm::ivec2&  GetSize() const { return m_Specification.Size; }
	NODISCARD FORCEINLINE s32                GetWidth() const { return m_Specification.Size.x; }
	NODISCARD FORCEINLINE s32                GetHeight() const { return m_Specification.Size.y; }
	NODISCARD FORCEINLINE const glm::ivec2&  GetPosition() const { return m_Specification.Position; }
	NODISCARD FORCEINLINE bool               IsFullscreen() const { return m_Specification.Fullscreen; }
	NODISCARD FORCEINLINE bool               IsResizable() const { return m_Specification.Resizable; }
	NODISCARD FORCEINLINE bool               IsVSync() const { return m_Specification.VSync; }

	NODISCARD FORCEINLINE SDL_Window*                GetSDLWindow() const { return m_Window; }
	NODISCARD FORCEINLINE const WindowSpecification& GetSpecification() const { return m_Specification; }
	NODISCARD FORCEINLINE bool                       IsValid() const { return m_Window != nullptr; }

	CascadingMulticastDelegate<false>                              OnWindowClose;
	CascadingMulticastDelegate<false, const glm::ivec2&>           OnWindowResize;
	CascadingMulticastDelegate<false, const glm::ivec2&>           OnWindowMove;
	CascadingMulticastDelegate<false, const SDL_KeyboardEvent&>    OnKeyboardEvent;
	CascadingMulticastDelegate<false, const SDL_MouseButtonEvent&> OnMouseButtonEvent;
	CascadingMulticastDelegate<false, const SDL_MouseMotionEvent&> OnMouseMotionEvent;
	CascadingMulticastDelegate<false, const SDL_MouseWheelEvent&>  OnMouseWheelEvent;

protected:
	WindowSpecification m_Specification;

	SDL_Window* m_Window = nullptr;
};
