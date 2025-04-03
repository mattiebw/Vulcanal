#include "vulcpch.h"
#include "Render/Window.h"

#include <SDL3/SDL_events.h>

Window::Window(WindowSpecification spec)
	: m_Specification(std::move(spec))
{
}

bool Window::Create()
{
	VULC_ASSERT(!m_Specification.Title.empty() && "Window title cannot be empty");
	VULC_ASSERT(m_Specification.Size.x > 0 && m_Specification.Size.y > 0 && "Window size must be greater than 0");

	m_Window = SDL_CreateWindow(m_Specification.Title.data(), m_Specification.Size.x, m_Specification.Size.y,
	                            SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
	if (!m_Window)
	{
		VULC_ERROR("Failed to create window: {}", SDL_GetError());
		return false;
	}

	SDL_SetWindowResizable(m_Window, m_Specification.Resizable);
	if (m_Specification.Position == glm::ivec2(0, 0))
		SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	else
		SDL_SetWindowPosition(m_Window, m_Specification.Position.x, m_Specification.Position.y);
	SDL_SetWindowFullscreen(m_Window, m_Specification.Fullscreen);

	// MW @todo: vsync.

	SDL_ShowWindow(m_Window);

	return true;
}

void Window::PollEvents()
{
	SDL_Event windowEvent;

	while (SDL_PollEvent(&windowEvent))
	{
		switch (windowEvent.type)
		{
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			OnWindowClose.Execute();
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			glm::ivec2 newSize = {windowEvent.window.data1, windowEvent.window.data2};
			m_Specification.Size = newSize;
			OnWindowResize.Execute(newSize);
			break;
		case SDL_EVENT_WINDOW_MOVED:
			glm::ivec2 newPos = {windowEvent.window.data1, windowEvent.window.data2};
			m_Specification.Position = newPos;
			OnWindowMove.Execute(newPos);
			break;
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_KEY_DOWN:
			OnKeyboardEvent.Execute(windowEvent.key);
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			OnMouseButtonEvent.Execute(windowEvent.button);
			break;
		case SDL_EVENT_MOUSE_MOTION:
			OnMouseMotionEvent.Execute(windowEvent.motion);
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			OnMouseWheelEvent.Execute(windowEvent.wheel);
			break;
		default:
			break;
		}
	}
}

void Window::Destroy()
{
	// Unbind from all events
	OnWindowClose.UnbindAll();
	OnWindowResize.UnbindAll();
	OnWindowMove.UnbindAll();

	// Destroy the window, if it exists
	if (m_Window)
	{
		SDL_DestroyWindow(m_Window);
		m_Window = nullptr;
	}
}

void Window::SetTitle(const std::string& title)
{
	VULC_ASSERT(m_Window && "Window must be created before setting the title");
	SDL_SetWindowTitle(m_Window, title.data());
	m_Specification.Title = title;
}

void Window::SetSize(const glm::ivec2& size)
{
	VULC_ASSERT(m_Window && "Window must be created before setting the size");
	SDL_SetWindowSize(m_Window, size.x, size.y);
	m_Specification.Size = size;
}

void Window::SetPosition(const glm::ivec2& position)
{
	VULC_ASSERT(m_Window && "Window must be created before setting the position");
	SDL_SetWindowPosition(m_Window, position.x, position.y);
	m_Specification.Position = position;
}

void Window::SetFullscreen(bool fullscreen)
{
	VULC_ASSERT(m_Window && "Window must be created before setting fullscreen");
	if (fullscreen)
		SDL_SetWindowFullscreen(m_Window, true);
	else
		SDL_SetWindowFullscreen(m_Window, false);
	m_Specification.Fullscreen = fullscreen;
}

void Window::SetResizable(bool resizable)
{
	VULC_ASSERT(m_Window && "Window must be created before setting resizable");
	SDL_SetWindowResizable(m_Window, resizable);
	m_Specification.Resizable = resizable;
}

void Window::SetVSync(bool vsync)
{
	// MW @todo: Implement VSync
}
