#include "EnginePCH.h"
#include "RenderResourceManager.h"

#include "RenderCommand.h"

#include "RenderUtil.h"



struct FPipelineTableEntry
{
	EPSOType Type;
	const char* ShaderPath;
	D3D11_PRIMITIVE_TOPOLOGY Topology;
	ERasterizerState RasterizerState;
	EBlendState BlendState;
	EDepthStencilState DepthStencilState;
};


constexpr FPipelineTableEntry PipelineTable[] =
{
	// [StaticMesh_Opaque] 사과 5만 개 기본값
	{ EPSOType::StaticMesh_Opaque,		"Resources/Shader/StaticMeshShader.hlsl",    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidBack, EBlendState::Opaque,       EDepthStencilState::Default },
	// [StaticMesh_Translucent]
	{ EPSOType::StaticMesh_Translucent,	"Resources/Shader/StaticMeshShader.hlsl",    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidBack, EBlendState::AlphaBlend,   EDepthStencilState::ReadOnly },
	// [StaticMesh_Wireframe]
	{ EPSOType::StaticMesh_Wireframe,	"Resources/Shader/StaticMeshShader.hlsl",    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::Wireframe, EBlendState::Opaque,       EDepthStencilState::Default },
	// [Particle_AlphaBlend]
	{ EPSOType::Particle_AlphaBlend,	"Resources/Shader/ParticleSubUVShader.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::AlphaBlend,   EDepthStencilState::ReadOnly },
	// [Particle_Additive]
	{ EPSOType::Particle_Additive,		"Resources/Shader/ParticleSubUVShader.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Additive,     EDepthStencilState::ReadOnly },
	// [Skybox]
	{ EPSOType::StaticMesh_Opaque,		"Resources/Shader/SkyboxShader.hlsl",        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::ReadOnly },
	// [Grid]
	{ EPSOType::Grid,					"Resources/Shader/GridShader.hlsl",          D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidBack, EBlendState::AlphaBlend,   EDepthStencilState::Default },
	// [Outline_Mask]
	{ EPSOType::Outline_Mask,			"Resources/Shader/OutlineShader.hlsl",       D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::NoColorWrite, EDepthStencilState::StencilMask },
	// [Outline_Draw]
	{ EPSOType::Outline_Draw,			"Resources/Shader/OutlineShader.hlsl",       D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::StencilOutline }
};



void FRenderResourceManager::ScanShaders(const fs::path& ShaderRoot)
{
	std::error_code ErrorCode;

	if (!fs::exists(ShaderRoot, ErrorCode))
	{
		LOG(Error, "[Shader] Scan Root Not Found : {}", ShaderRoot.generic_string());
		return;
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(ShaderRoot))
	{
		if (!Entry.is_regular_file()) continue;
		if (Entry.path().extension() != ".hlsl") continue;
		if (Entry.path().filename().string().find("CS.hlsl") != std::string::npos) continue;

		FString Path = Entry.path().generic_string();
		LoadOrCompileShader(Path);
	}
}

void FRenderResourceManager::Shutdown()
{
	Get().VertexShaderMap.Empty();
	Get().PixelShaderMap.Empty();
	Get().ShaderProgramMap.Empty();
}

FShaderProgram* FRenderResourceManager::GetShaderProgram(const FString& InPath)
{
	if (TUniquePtr<FShaderProgram>* Found = Get().ShaderProgramMap.Find(InPath))
		return Found->get();

	LOG(Error, "[Shader] not found: {}", InPath);

	if (TUniquePtr<FShaderProgram>* Fallback =
		Get().ShaderProgramMap.Find("Resources/Shader/DefaultShader.hlsl"))
		return Fallback->get();

	return nullptr;
}

FPipelineState* FRenderResourceManager::GetPSO(const EPSOType& Intype)
{
	// Get().을 붙여서 싱글톤 인스턴스의 멤버에 접근
	if (auto* Found = Get().PipelineStateMap.Find(Intype))
	{
		return Found->get();
	}
	return nullptr;
}



void FRenderResourceManager::LoadOrCompileShader(const FString& Path)
{ 
	FString VSCSOPath;
	FString PSCSOPath;
	FShaderByteCode VSCode = RenderUtil::GetOrCompile(Path,"mainVS", EShaderType::Vertex, VSCSOPath);
	FShaderByteCode PSCode = RenderUtil::GetOrCompile(Path, "mainPS", EShaderType::Pixel, PSCSOPath);

	if (!VSCode.IsValid() || !PSCode.IsValid())
	{
		LOG(Error, "[Shader] compile failed: {}", Path);
		return;
	}

	TUniquePtr<FVertexShader> Vs = RenderCommand::CreateVertexShader(VSCode);
	TUniquePtr<FPixelShader>  Ps = RenderCommand::CreatePixelShader(PSCode);

	if (!Vs || !Vs->IsValid() || !Ps || !Ps->IsValid())
	{
		LOG(Error, "[Shader] device create failed: {}", Path);
		return;
	}

	FVertexShader* VsRaw = Vs.get();
	FPixelShader* PsRaw = Ps.get();

	VertexShaderMap[VSCSOPath] = std::move(Vs);
	PixelShaderMap[PSCSOPath] = std::move(Ps);
	ShaderProgramMap[Path] = MakeUnique<FShaderProgram>(VsRaw, PsRaw);

	LOG(Info, "[Shader] loaded: {}", Path);

}

void FRenderResourceManager::InitPipelineStates()
{
	static_assert(sizeof(PipelineTable) / sizeof(PipelineTable[0]) == static_cast<size_t>(EPSOType::Count), "PipelineTable size mismatch");
	for (size_t i = 0; i < static_cast<size_t>(EPSOType::Count); ++i)
	{
		const auto& Entry = PipelineTable[i];
		const EPSOType Type = static_cast<EPSOType>(i);
		PipelineStateMap[Type] = MakeUnique<FPipelineState>(
			GetShaderProgram(Entry.ShaderPath),
			Entry.Topology,
			Entry.RasterizerState,
			Entry.BlendState,
			Entry.DepthStencilState
		);
	}
	LOG(Info, "Pipeline States Initialized from Table.");
}
