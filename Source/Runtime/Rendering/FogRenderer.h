#pragma once
#include "RHI/RHIBuffer.h"
#include "RHI/PipelineState.h"
#include "Engine/Texture2D.h"

#include "Rendering/RenderCommand.h"

struct FFogConstants
{
	FMatrix InverseViewProjection;
	FVector CameraPosition;
	float FogDensity; //안개의 전체적인 두께,밀도
	float FogHeightFalloff; // 높이에 따른 안개 감소율
	float FogHeight;
	float FogMaxOpacity; // 안개 최대 불투명도
	float StartDistance;
	float FogCutoffDistance;
	float FogInscatteringColor[3]; // 안개 색상
	// Size = 112 Byte
};

// Exponential Height Fog Render를 위한 class
class FFogRenderer
{
 public:
	FFogRenderer() = default;
	~FFogRenderer() = default;

	bool Init();

	bool IsValid() const{ return Shader != nullptr;}
	void OnRender(FRHITexture2D* SceneDepthTexture, const FMatrix& ViewProj, const FVector& CameraLocation);

private:
	FShaderProgram* Shader = nullptr;
	TUniquePtr<FRHIUniformBuffer> ConstantBuffer;
	//TUniquePtr<FRHITexture2D> FogTexture;
	FPipelineState PipelineState;
};
