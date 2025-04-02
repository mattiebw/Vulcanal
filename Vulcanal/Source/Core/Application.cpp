#include "vulcpch.h"
#include "Core/Application.h"

Application::Application(ApplicationSpecification spec)
	: m_Specification(std::move(spec))
{
	VULC_ASSERT(!m_Specification.Name.empty() && "Application name cannot be empty");
	VULC_ASSERT(m_Specification.Version.Packed() > 0 && "Application version must be greater than 0");
}

bool Application::Initialise()
{
	VULC_INFO("Initialising application: {}", m_Specification.Name);

	return true;
}

void Application::Run()
{
	m_Running = true;

	while (m_Running)
	{
		
	}
}

void Application::Shutdown()
{
	VULC_INFO("Shutting down application: {}", m_Specification.Name);
}
