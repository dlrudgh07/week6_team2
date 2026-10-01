#pragma once

#include "Shader.h"
#include "RenderDevice.h"
#include "PipelineState.h"

enum class EPSOType : uint8
{
	StaticMesh_Opaque,       // 사과 5만 개 전용 (기본값)
	StaticMesh_Translucent,
	StaticMesh_Wireframe,
	Particle_AlphaBlend,
	Particle_Additive,
	Skybox,
	Grid,
	Outline_Mask,
	Outline_Draw,
	Count
};


class FRenderResourceManager
{
public:
	static FRenderResourceManager& Get()
	{
		static FRenderResourceManager* Instance = new FRenderResourceManager();
		return *Instance;
	}

	static void Init()
	{
		Get().ScanShaders("Resources/Shader");
		Get().InitPipelineStates();
	}

	void ScanShaders(const fs::path& ShaderRoot);

	static void Shutdown();

	static FShaderProgram* GetShaderProgram(const FString& InPath);

	static FPipelineState* GetPSO(const EPSOType& Intype);

private:
	void LoadOrCompileShader(const FString& Path);
	void InitPipelineStates();

	TMap<FString, TUniquePtr<FVertexShader>> VertexShaderMap;
	TMap<FString, TUniquePtr<FPixelShader>>  PixelShaderMap;
	TMap<FString, TUniquePtr<FShaderProgram>> ShaderProgramMap;


	TMap<EPSOType, TUniquePtr<FPipelineState>> PipelineStateMap;

}; 