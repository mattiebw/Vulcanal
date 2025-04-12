workspace "Vulcanal"
	configurations { "Debug", "Release", "Dist" }
	platforms { "Win64", "Linux" }
	startproject "Vulcanal"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
vulkansdk = os.getenv("VULKAN_SDK")
print("Vulkan SDK is at " .. vulkansdk)

IncludeDir = {}
IncludeDir["spdlog"] = "Vulcanal/Vendor/spdlog/include"
IncludeDir["SDL"] = "Vulcanal/Vendor/SDL/Include"
IncludeDir["imgui"] = "Vulcanal/Vendor/imgui/"
IncludeDir["glm"] = "Vulcanal/Vendor/glm/Include"
IncludeDir["stb"] = "Vulcanal/Vendor/stb"
IncludeDir["entt"] = "Vulcanal/Vendor/entt"
IncludeDir["vulkan"] = vulkansdk .. "/include/"
IncludeDir["vkbootstrap"] = "Vulcanal/Vendor/vk-bootstrap/Include"
IncludeDir["VMA"] = "Vulcanal/Vendor/VMA/Include"

group "Vendor"
    include "Vulcanal/Vendor/imgui.lua"
group ""

project "Vulcanal"
	cppdialect "C++20"
	kind "ConsoleApp"
	staticruntime "On"
	language "C++"
	characterset "Unicode"
	location "Vulcanal"
	targetdir ("Build/%{prj.name}/" .. outputdir)
	objdir ("Build/%{prj.name}/Intermediates/" .. outputdir)
	debugdir ("Build/%{prj.name}/" .. outputdir)

	usestandardpreprocessor 'On'
	pchheader("vulcpch.h")
	pchsource "Vulcanal/Source/vulcpch.cpp"

	vpaths {
		["Include"] = {"Vulcanal/Include/**.h", "Vulcanal/Include/**.hpp"},
		["Source"] = {"Vulcanal/Source/**.cpp", "Vulcanal/Source/**.c"},
	}

	files { 
		"Vulcanal/Include/**.h", "Vulcanal/Include/**.hpp", 
		"Vulcanal/Source/**.cpp", "Vulcanal/Source/**.c",
		"Content/**",
		"TODO.md", "README.md",
	}

	includedirs 
	{ 
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.SDL}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb}",
		-- "%{IncludeDir.msdfgen}",
		-- "Vulcanal/Vendor/msdfgen-custom/",
		-- "%{IncludeDir.msdfatlasgen}",
		-- "%{IncludeDir.steamworks}",
		-- "%{IncludeDir.fmod}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.vulkan}",
		"%{IncludeDir.vkbootstrap}",
		"%{IncludeDir.VMA}",

		"Vulcanal/Include"
	}

	libdirs
	{
		vulkansdk .. "/lib/"	
	}

	filter "system:windows"
		libdirs 
		{
			"Vulcanal/Vendor/SDL/Lib/Win64"
			-- "Vulcanal/Vendor/Steamworks/Lib/Win64",
			-- "Vulcanal/Vendor/FMOD/Lib/Win64"
		}

		links
		{
			"vulkan-1"
		}

		-- links 
		-- {
		-- 	"steam_api64",

		-- 	-- FMOD
		-- 	"fmod_vc",
		-- 	"fmodstudio_vc",
		-- 	"fsbank_vc",
		-- }

	filter "system:linux"
		libdirs 
		{
			"Vulcanal/Vendor/SDL/Lib/Linux64",
			-- "Vulcanal/Vendor/Steamworks/Lib/Linux64",
			-- "Vulcanal/Vendor/FMOD/Lib/Linux64"
		}

		links
		{
			"vulkan"
		}
		
		-- links 
		-- {
		-- 	"steam_api",

		-- 	-- FMOD
		-- 	"fmod",
		-- 	"fmodstudio",
		-- 	"fsbank",
		-- }

	filter {}

	-- Linking order matters with gcc! freetype needs to be at the bottom.
	-- MW @todo: Understand linking order, consider using 'linkgroups "On"' to avoid linking order issues at the cost of link time.
	-- https://premake.github.io/docs/linkgroups/
	links
	{
		"imgui",
		"SDL3",

		-- MSDF
		-- "msdf-atlas-gen",
		-- "msdfgen",
		-- "freetype",
	}

	defines {
		"_CRT_SECURE_NO_WARNINGS"
	}

	filter "system:windows"
	 	prebuildcommands { "call \"../Scripts/RunPreprocessor.bat\" \"../../../../../Build/%{prj.name}/" .. outputdir .. "/Content/\" \"%{cfg.buildcfg}\"" }

	filter "system:linux"
		prebuildcommands { "../Scripts/RunPreprocessor.sh \"../../../../../Build/%{prj.name}/" .. outputdir .. "/Content/\" \"%{cfg.buildcfg}\"" }

	filter { "system:linux", "files:Vulcanal/Source/Vendor/stb.cpp" }
		optimize "On" -- MW @hack: stb doesn't compile properly with GCC without optimizations (@credit https://git.suyu.dev/suyu/suyu/pulls/63)

os.mkdir("Vulcanal/Source")
os.mkdir("Vulcanal/Include")

filter "configurations:Debug"
	defines { "VULC_DEBUG", "VULC_ENABLE_ASSERTS", "VULC_VK_DEBUG" }
	symbols "On"
	runtime "Debug"

filter "configurations:Release"
	defines { "VULC_RELEASE", "VULC_ENABLE_ASSERTS", "VULC_VK_DEBUG" }
	optimize "On"
	symbols "On"
	runtime "Release"

filter "configurations:Dist"
	defines { "VULC_DIST", "VULC_DISABLE_ASSERTS" }
	kind "WindowedApp"
	optimize "On"
	symbols "Off"
	runtime "Release"

filter "system:windows"
	systemversion "latest"
	defines { "VULC_PLATFORM_WINDOWS" }

	links
	{
		"version",
		"winmm",
		"Imm32",
		"Cfgmgr32",
		"Setupapi"
	}

filter "system:linux"
	defines { "VULC_PLATFORM_LINUX" }

filter "platforms:Win64"
	system "Windows"
	architecture "x64"

filter "platforms:Linux"
	buildoptions { "-static-libstdc++" }
	system "linux"
	architecture "x64"
