#include "vulcpch.h"
#include "Core/Application.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_messagebox.h>

#ifndef VULC_NO_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#endif

#include "Core/Input/Input.h"

Application* Application::s_Instance = nullptr;

Application::Application(ApplicationSpecification spec)
	: m_Specification(std::move(spec)),
	  m_Window(Window({.Title = m_Specification.Name, .Size = {1280, 720}, .Fullscreen = false, .Resizable = true}))
{
	VULC_ASSERT(!m_Specification.Name.empty() && "Application name cannot be empty");
	VULC_ASSERT(m_Specification.Version.Packed() > 0 && "Application version must be greater than 0");
}

bool Application::Initialise()
{
	VULC_ASSERT(!s_Instance && "Application already initialised?");
	s_Instance = this;

	VULC_INFO("Initialising application: {}", m_Specification.Name);

	if (!InitSDL())
		return false;

	if (!m_Window.Create())
		return false;

	Input::Init();

	// Bindings to window events.
	m_Window.OnWindowClose.BindLambda([this]
	{
		Close();
		return false;
	});

	m_Window.OnKeyboardEvent.BindLambda([this](const SDL_KeyboardEvent& event)
	{
		Input::ProcessKeyboardInputEvent(event);
		return false;
	});

	m_Window.OnMouseButtonEvent.BindLambda([this](const SDL_MouseButtonEvent& event)
	{
		Input::ProcessMouseInputEvent(event);
		return false;
	});

	m_Window.OnMouseMotionEvent.BindLambda([this](const SDL_MouseMotionEvent& event)
	{
		Input::ProcessMouseMotionEvent(event);
		return false;
	});

	if (!m_Renderer.Init({.EnableValidationLayers = true, .App = this}))
	{
		// The renderer will do its own error logging.
		return false;
	}

	if (!InitImGUI())
	{
		VULC_ERROR("Failed to initialise ImGUI");
		return false;
	}

	return true;
}

void Application::Run()
{
	m_Running = true;

	while (m_Running)
	{
		Input::PreUpdate();
		m_Window.PollEvents();

		if (Input::IsKeyDownThisFrame(Scancode::Escape))
			Close();
	}
}

void Application::Shutdown()
{
	VULC_INFO("Shutting down application: {}", m_Specification.Name);

	Input::Shutdown();

	if (m_Window.IsValid())
		m_Window.Destroy();

	s_Instance = nullptr;
}

void Application::Close()
{
	if (!OnApplicationCloseRequested.Execute())
		return;

	VULC_INFO("Closing!", m_Specification.Name);
	m_Running = false;
}

void Application::ShowError(const std::string& message, const std::string& title) const
{
	VULC_ERROR("{}", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.data(), message.data(), m_Window.GetSDLWindow());
}

bool Application::InitSDL() const
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		ShowError(fmt::format("Failed to initialise SDL: {}", SDL_GetError()), "SDL Error");
		return false;
	}

	return true;
}

bool Application::InitImGUI()
{
	return true;
}
