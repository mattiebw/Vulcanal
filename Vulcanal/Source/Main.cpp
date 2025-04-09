#include "vulcpch.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_main.h>

#include "Core/Application.h"

namespace
{
	bool g_ShouldRestart = true;
}

int main(int argc, char *argv[])
{
	InitLog(SDL_GetPrefPath("Mattie", "Vulcanal"));

	do
	{
		Application application({.Name = "Vulcanal", .Version = SemVer(1, 0, 0)});
		if (application.Initialise())
		{
			application.Run();
			application.Shutdown();
		}
		else
		{
			g_ShouldRestart = false;
			return -1;
		}
	}
	while (g_ShouldRestart);

	return 0;
}
