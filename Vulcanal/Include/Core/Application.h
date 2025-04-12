#pragma once

#include "Render/Renderer.h"
#include "Render/Window.h"

struct ApplicationSpecification
{
	std::string Name    = "Application";
	std::string Author  = "Super Cool Game Corp";
	SemVer      Version = SemVer(1, 0, 0);
};

class Application
{
public:
	explicit Application(ApplicationSpecification spec);

	bool Initialise();
	void Run();
	void Shutdown();

	static void BeginImGUI();
	static bool OnSDLEvent(const SDL_Event& e);
	bool        OnWindowClosed();

	void Close();
	void ShowError(const std::string& message, const std::string& title = "Error") const;

	NODISCARD FORCEINLINE const ApplicationSpecification& GetSpecification() const { return m_Specification; }
	NODISCARD FORCEINLINE Window&                         GetWindow() { return m_Window; }
	NODISCARD FORCEINLINE const Window&                   GetWindow() const { return m_Window; }
	NODISCARD FORCEINLINE bool                            IsRunning() const { return m_Running; }

	NODISCARD FORCEINLINE static bool ShouldRestart() { return s_ShouldRestart; }
	NODISCARD FORCEINLINE static void RequestRestart(bool restart = true)
	{
		s_ShouldRestart = restart;
	}

	NODISCARD FORCEINLINE static Application* Get() { return s_Instance; }

	CascadingMulticastDelegate<false> OnApplicationCloseRequested;

protected:
	bool InitSDL() const;
	bool InitImGUI() const;

	ApplicationSpecification m_Specification;
	Window                   m_Window;
	Renderer                 m_Renderer;
	bool                     m_Running = false;

	// Test stuff.
	static s32 s_SelectedGPU;

	static Application* s_Instance;
	static bool         s_ShouldRestart;
};
