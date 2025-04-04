#include "vulcpch.h"

#include "Core/Application.h"

SDL_Window* GetAppWindow()
{
	Application* app = Application::Get();
	if (!app)
		return nullptr;

	return app->GetWindow().GetSDLWindow();
}
