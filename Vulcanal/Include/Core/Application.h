#pragma once

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

protected:
	ApplicationSpecification m_Specification;
	
	bool m_Running = false;
};
