#pragma once
#include "RHI/RHIBuffer.h"
#include "RHI/PipelineState.h"
#include "Engine/Texture2D.h"
#include "Rendering/RenderCommand.h"
#include "RenderingInfo.h"

struct FAntiAliasingConstants
{

};

// Exponential Height Fog Render를 위한 class
class FAntiAliasingRenderer
{
  public:
	FAntiAliasingRenderer() = default;
	~FAntiAliasingRenderer() = default;

	bool Init();

	bool IsValid() const
	{
		return Shader != nullptr;
	}
	void OnRender(FRHITexture2D* SceneDepthTexture, const FRenderingInfo& ViewRenderInfo);

  private:
	FShaderProgram* Shader = nullptr;
	TUniquePtr<FRHIUniformBuffer> ConstantBuffer;
	//TUniquePtr<FRHITexture2D> FogTexture;
	FPipelineState PipelineState;
};
