#pragma once
#include "RHI/RHIBuffer.h"
#include "RHI/PipelineState.h"
#include "Engine/Texture2D.h"
#include "Rendering/RenderCommand.h"
#include "RenderingInfo.h"

struct FAntiAliasingConstants
{
	float Texel[2];
	float padding1;
	float padding2;
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
	void OnRender(const FRenderingInfo& ViewRenderingInfo);

  private:
	FShaderProgram* Shader = nullptr;
	TUniquePtr<FRHIUniformBuffer> ConstantBuffer;
	//TUniquePtr<FRHITexture2D> FogTexture;
	FPipelineState PipelineState;
};
