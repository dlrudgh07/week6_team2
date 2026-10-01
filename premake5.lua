workspace "Hitori"
	architecture "x64"
	startproject "HitoriEditor"

	configurations
	{
		"Debug",
		"Release",
		"ObjViewer",
	}

	multiprocessorcompile "On"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["ImGui"] = "Source/ThirdParty/ImGui"
IncludeDir["stb"]   = "Source/ThirdParty/stb"
IncludeDir["json"]  = "Source/ThirdParty/json"
IncludeDir["meshoptimizer"] = "Source/ThirdParty/meshoptimizer"

-- premake의 filter는 project()를 만나면 초기화된다.
-- 두 프로젝트가 같은 런타임(/MDd vs /MD)으로 컴파일되지 않으면 링크가 실패하므로
-- 공통 설정을 함수로 묶어 각 프로젝트에서 호출한다.
function CommonSettings()
	language   "C++"
	cppdialect "C++20"
	staticruntime "off"
	characterset  "Unicode"

	targetdir ("Build/Bin/" .. outputdir)
	objdir    ("Build/Intermediate/" .. outputdir)
	-- 에셋·쉐이더를 상대 경로로 읽으므로 작업 디렉터리는 저장소 루트다.
	debugdir  "%{wks.location}"

	defines
	{
		"WIN32_LEAN_AND_MEAN",
		"NOMINMAX",
		"UNICODE",
		"_UNICODE",
	}

	filter "files:**.hlsl"
		excludefrombuild "On"

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }
		defines { "ENGINE_PLATFORM_WINDOWS" }

	filter "configurations:Debug"
		defines { "ENGINE_DEBUG", "_DEBUG" }
		runtime  "Debug"
		symbols  "on"

	filter "configurations:Release"
		defines  { "ENGINE_RELEASE", "NDEBUG" }
		runtime  "Release"
		optimize "on"
		symbols  "on"
		-- LTCG: 컴파일 /GL + 링크 /LTCG. .cpp 파일 경계를 넘는 인라인을 허용한다.
		-- (premake 5.0.0-beta8 문법. 예전 flags { "LinkTimeOptimization" }는 이 버전에서 쓰지 않는다)
		linktimeoptimization "On"

	-- 에디터 없이 OBJ 파일만 열어보는 Viewer 빌드
	filter "configurations:ObjViewer"
		defines  { "ENGINE_RELEASE", "NDEBUG", "OBJ_VIEWER" }
		runtime  "Release"
		optimize "on"
		symbols  "on"

	filter {}
end


-- 외부 라이브러리. 별도 프로젝트로 두면 에디터의 소스 루트가 Source/Editor 하나로 좁혀져
-- Solution Explorer의 "모든 파일 표시"에서 폴더 구조가 그대로 보인다.
project "ImGui"
	location "Source/ThirdParty/ImGui"
	kind     "StaticLib"
	CommonSettings()

	files
	{
		"%{IncludeDir.ImGui}/*.h",
		"%{IncludeDir.ImGui}/*.cpp",
		"%{IncludeDir.ImGui}/backends/imgui_impl_win32.*",
		"%{IncludeDir.ImGui}/backends/imgui_impl_dx11.*",
	}

	includedirs
	{
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}


-- 런타임. 에디터를 모른다. include 경로에 Source/Editor가 없는 것이 그 방벽이다.
-- location이 소스 루트와 같아야 "모든 파일 표시"에서 폴더가 보인다.
project "HitoriEngine"
	location "Source/Runtime"
	kind     "StaticLib"
	CommonSettings()
	filter { "system:windows", "configurations:Release" }
    	buildoptions { "/GT" }
	filter {}
	pchheader "EnginePCH.h"
	pchsource "Source/Runtime/EnginePCH.cpp"

	-- Group larger subsystems while keeping class and file names aligned.
	vpaths
	{
		["ObjectSystem"] = { "Source/Runtime/ObjectSystem/**" },
		["World"] = { "Source/Runtime/World/**" },
		["Core/Names"] = { "Source/Runtime/Core/Name*" },
		["Core/Profiling"] = { "Source/Runtime/Core/StatOverlay.*", "Source/Runtime/Core/ScopeCycleCounter.*" },
		["Platform"] = { "Source/Runtime/Core/Window.*", "Source/Runtime/Core/EntryPoint.*" },
		["Rendering"] =
		{
			"Source/Runtime/Rendering/Renderer.*", "Source/Runtime/Rendering/RenderPacket.*",
			"Source/Runtime/Rendering/RenderCommand.*", "Source/Runtime/Rendering/RenderingInfo.*",
			"Source/Runtime/Rendering/RenderUtil.*", "Source/Runtime/Rendering/LineBatcher.*",
			"Source/Runtime/Rendering/SkyboxRenderer.*",
			"Source/Runtime/Rendering/GPUProfiler.*",
		},
		["Rendering/Device"] =
		{
			"Source/Runtime/Rendering/RenderDevice.*", "Source/Runtime/Rendering/Swapchain.*",
			"Source/Runtime/Rendering/PipelineState.*", "Source/Runtime/Rendering/RenderStates.*",
			"Source/Runtime/Rendering/RenderEnums.*",
		},
		["Rendering/Resources"] =
		{
			"Source/Runtime/Rendering/Buffer.*", "Source/Runtime/Rendering/Shader.*",
			"Source/Runtime/Rendering/Texture*", "Source/Runtime/Rendering/RenderResourceManager.*",
			"Source/Runtime/Rendering/ImageLoader.*",
		},
		["Rendering/Materials"] = { "Source/Runtime/Rendering/Material*" },
		["Rendering/Geometry"] =
		{
			"Source/Runtime/Rendering/Mesh.*", "Source/Runtime/Rendering/StaticMeshData.*",
			"Source/Runtime/Rendering/Vertex.*", "Source/Runtime/Rendering/GeometryGenerator.*",
		},
	}

	files
	{
		"Source/Runtime/**.h",
		"Source/Runtime/**.hpp",
		"Source/Runtime/**.cpp",
		"%{IncludeDir.meshoptimizer}/meshoptimizer.h",
		"%{IncludeDir.meshoptimizer}/allocator.cpp",
		"%{IncludeDir.meshoptimizer}/simplifier.cpp",
		"%{IncludeDir.meshoptimizer}/vcacheoptimizer.cpp",
		"%{IncludeDir.meshoptimizer}/vfetchoptimizer.cpp",
		"%{IncludeDir.meshoptimizer}/overdrawoptimizer.cpp",
	}

	filter "files:Source/ThirdParty/meshoptimizer/**.cpp"
		enablepch "Off"
	filter {}

	includedirs
	{
		"Source/Runtime",
		"%{IncludeDir.stb}",
		"%{IncludeDir.json}",
		"%{IncludeDir.meshoptimizer}",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}


-- 에디터 애플리케이션. ObjViewer 구성에서는 같은 exe가 뷰어로 빌드된다.
project "HitoriEditor"
	location "Source/Editor"
	kind     "WindowedApp"
	CommonSettings()

	pchheader "EnginePCH.h"
	pchsource "Source/Editor/EditorPCH.cpp"

	vpaths
	{
		["Application"] = { "Source/Editor/Application/**", "Source/Editor/EditorEntryPoint.cpp" },
		["UI"] = { "Source/Editor/EditorUI/**" },
		["Panels"] =
		{
			"Source/Editor/ContentDrawer/**", "Source/Editor/Details/**",
			"Source/Editor/EditorControls/**", "Source/Editor/Outliner/**",
			"Source/Editor/OutputLog/**", "Source/Editor/Settings/**",
		},
		["Viewports"] = { "Source/Editor/Viewports/**" },
		["Tools/ObjViewer"] = { "Source/Programs/ObjViewer/**" },
	}

	links { "HitoriEngine", "ImGui" }

	files
	{
		"Source/Editor/**.h",
		"Source/Editor/**.cpp",
		"Source/Programs/**.h",
		"Source/Programs/**.cpp",
		"Source/Editor/**.rc",   -- 창·exe 아이콘
	}

	includedirs
	{
		"Source",             -- "Editor/OutputLog/ConsolePanel.h"처럼 Source 기준 include를 쓴다
		"Source/Runtime",
		"Source/Programs",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
		"%{IncludeDir.stb}",
		"%{IncludeDir.json}",
		"%{IncludeDir.meshoptimizer}",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}

	filter "configurations:ObjViewer"
		targetname "ObjViewer"

	filter {}
