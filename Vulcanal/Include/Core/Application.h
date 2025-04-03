#pragma once

#include "Render/Window.h"

struct ApplicationSpecification
{
	std::string Name = "Application";
	SemVer Version = SemVer(1, 0, 0);
};

class Application
{
public:
	explicit Application(ApplicationSpecification spec);

	bool Initialise();
	void Run();
	void Shutdown();

	void Close();
	void ShowError(const std::string& message, const std::string& title = "Error") const;

	NODISCARD FORCEINLINE const ApplicationSpecification& GetSpecification() const { return m_Specification; }
	NODISCARD FORCEINLINE const Window& GetWindow() const { return m_Window; }
	NODISCARD FORCEINLINE bool IsRunning() const { return m_Running; }
	
	NODISCARD FORCEINLINE static Application* Get() { return s_Instance; }
	
	CascadingMulticastDelegate<false> OnApplicationCloseRequested;
	
protected:
	bool InitSDL() const;
	bool InitImGUI();

	ApplicationSpecification m_Specification;
	Window m_Window;
	bool m_Running = false;

	static Application* s_Instance;
};
