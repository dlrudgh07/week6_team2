#pragma once
#include "RHI/RHIBuffer.h"
#include "RHI/PipelineState.h"
#include "Engine/Texture2D.h"

#include "Rendering/RenderCommand.h"
#include "Math/Color.h"

struct FFogConstants
{
	// 64-bytes
	FMatrix InverseViewProjection;		

	// 16-bytes
	FVector CameraPosition;				// 12-bytes
	float Padding0;						// 4-bytes (Padding)

	// 16-bytes 
	FLinearColor FogInscatteringColor; 	// 안개 색상
	
	// 16-bytes
	float FogDensity; 					// 안개의 전체적인 두께,밀도
	float FogHeightFalloff; 			// 높이에 따른 안개 감소율
	float FogHeight;
	float FogMaxOpacity; 				// 안개 최대 불투명도

	// 16-bytes
	float StartDistance;				
	float FogCutoffDistance;
	float Padding1;
	float Padding2;

	// Total Size = 128 Byte
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
