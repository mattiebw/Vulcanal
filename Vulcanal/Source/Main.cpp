#include "vulcpch.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_main.h>

#include "Core/Application.h"

int main(int argc, char* argv[])
{
	do
	{
		Application application({.Name = "Vulcanal", .Author = "Mattie", .Version = SemVer(1, 0, 0)});
		if (application.Initialise())
		{
			application.Run();
			application.Shutdown();
		}
		else
		{
			Application::RequestRestart(false);
			return -1;
		}
	}
	while (Application::ShouldRestart());

	return 0;
}
