#include "vulcpch.h"
#include "Core/Application.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_filesystem.h>

#ifndef VULC_NO_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#endif

#include "Core/Input/Input.h"

Application* Application::s_Instance      = nullptr;
bool         Application::s_ShouldRestart = false;
s32          Application::s_SelectedGPU   = -1;

Application::Application(ApplicationSpecification spec)
	: m_Specification(std::move(spec)),
	  m_Window(Window({.Title = m_Specification.Name, .Size = {1280, 720}, .Fullscreen = false, .Resizable = true}))
{
	VULC_ASSERT(!m_Specification.Name.empty(), "Application name cannot be empty");
	VULC_ASSERT(!m_Specification.Author.empty(), "Application author cannot be empty");
	VULC_ASSERT(m_Specification.Version.Packed() > 0, "Application version must be greater than 0");
}

bool Application::Initialise()
{
	VULC_ASSERT(!s_Instance, "Application already initialised?");
	s_Instance = this;

	InitLog(SDL_GetPrefPath(m_Specification.Author.c_str(), m_Specification.Name.c_str()));

	VULC_INFO("Initialising application: {} by {}", m_Specification.Name, m_Specification.Author);
	auto workingDir = std::filesystem::current_path().string();
	VULC_INFO("Working directory: {}", workingDir);

	if (!InitSDL())
		return false;

	if (!m_Window.Create())
		return false;

	Input::Init();

	// Bindings to window events.
	m_Window.OnWindowClose.BindMethod(this, &Application::OnWindowClosed);

	m_Window.OnSDLEvent.BindStatic(&Application::OnSDLEvent);

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

	if (!InitImGUI())
	{
		VULC_ERROR("Failed to initialise ImGUI");
		return false;
	}

	if (!m_Renderer.Init({.EnableValidationLayers = true, .App = this, .GPUIndexOverride = s_SelectedGPU}))
	{
		// The renderer will do its own error logging.
		return false;
	}
	s_SelectedGPU = m_Renderer.GetSelectedGPUIndex();

	return true;
}

void Application::Run()
{
	m_Running = true;

	while (m_Running)
	{
		Input::PreUpdate();
		m_Window.PollEvents();

		static s32 theInteger = 5;
		VULC_CHECK(!Input::IsKeyDownThisFrame(Scancode::R),
		           "DON'T PRESS R!! Static number: {}, window size: {}, window title: {}", theInteger,
		           m_Window.GetSize(), m_Window.GetTitle());
		VULC_CHECK(!Input::IsKeyDownThisFrame(Scancode::B));

		if (Input::IsKeyDownThisFrame(Scancode::A))
			PrintAssertionReport();

		if (Input::IsKeyDownThisFrame(Scancode::Escape))
			Close();

		BeginImGUI();

		OnDrawIMGui.Execute();
		
		// ImGUI commands goes here.
		ImGui::Begin("App Settings");
		ImGui::Checkbox("Should Restart", &s_ShouldRestart);
		const auto& gpuNames = m_Renderer.GetGPUNames();
		if (ImGui::BeginCombo("GPU", gpuNames[s_SelectedGPU].c_str()))
		{
			for (s32 i = 0; i < static_cast<s32>(gpuNames.size()); i++)
			{
				const bool isSelected = (i == s_SelectedGPU);
				if (ImGui::Selectable(gpuNames[i].c_str(), isSelected))
					s_SelectedGPU = i;
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		bool vsync = m_Renderer.GetSpecification().VSync;
		if (ImGui::Checkbox("VSync", &vsync))
			m_Renderer.SetVSync(vsync);
		ImGui::End();

		ImGui::Render();

		m_Renderer.Render();
	}
}

void Application::Shutdown()
{
	VULC_INFO("Shutting down application: {}", m_Specification.Name);

	m_Renderer.Shutdown();

	if (ImGui::GetCurrentContext())
	{
		// The renderer shuts down the Vulkan backend.
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}

	Input::Shutdown();

	if (m_Window.IsValid())
		m_Window.Destroy();

	// We'll only shutdown the log fully if we're not restarting.
	// If we are restarting, we don't want to re-initialise the log, as that'll create a new log file.
	if (ShouldRestart())
		ShutdownLog();

	s_Instance = nullptr;
}

void Application::BeginImGUI()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

bool Application::OnSDLEvent(const SDL_Event& e)
{
#ifndef VULC_NO_IMGUI
	ImGui_ImplSDL3_ProcessEvent(&e);
#endif

	return false;
}

bool Application::OnWindowClosed()
{
	Close();
	return false;
}

void Application::Close()
{
	if (!m_Running)
		return;

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

bool Application::InitImGUI() const
{
	ImGui::CreateContext();
	VULC_CHECK(ImGui_ImplSDL3_InitForVulkan(m_Window.GetSDLWindow()), "Failed to init imgui SDL3 backend for Vulkan");
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_DockingEnable;

	return true;
}
