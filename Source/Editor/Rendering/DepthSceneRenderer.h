#pragma once
#include "RHI/RHIBuffer.h"
#include "RHI/PipelineState.h"
#include "Engine/Texture2D.h"
#include "Runtime/Rendering/RenderingInfo.h"
#include "Rendering/RenderCommand.h"
/// Depth Scene Renderer를 위한 class
struct FDepthSceneConstants
{
	FMatrix InvProj;
};

class FDepthSceneRenderer
{
  public:
	FDepthSceneRenderer() = default;
	~FDepthSceneRenderer() = default;

	bool Init();

	bool IsValid() const
	{
		return Shader != nullptr;
	}
	void OnRender(FRHITexture2D* SceneDepthTexture, const FMatrix& ViewProj);
  private:
	FShaderProgram* Shader = nullptr;
	TUniquePtr<FRHIUniformBuffer> ConstantBuffer;
	//TUniquePtr<FRHITexture2D> FogTexture;
	FPipelineState PipelineState;
};
