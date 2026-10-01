#pragma once

#include "Buffer.h"
#include "PipelineState.h"
#include "Texture2D.h"

// HLSL의 cbuffer SkyboxConstants와 레이아웃이 일치해야 한다.
// float3 뒤의 Padding은 16바이트 정렬 때문에 필요하다.
struct FSkyboxConstants
{
	FMatrix InverseViewProjection;
	FVector CameraPosition;
	float   Padding = 0.0f;
};

// 등장방형 파노라마 한 장을 배경으로 그린다.
// 정점 버퍼 없이 전체 화면 삼각형 하나만 그리므로 메시가 필요 없다.
class FSkyboxRenderer
{
public:
	FSkyboxRenderer() = default;
	~FSkyboxRenderer() = default;

	bool Init(const FString& PanoramaPath);

	// 불투명 물체보다 먼저 호출한다. 깊이를 쓰지 않으므로 이후 물체가 위에 그려진다.
	void OnRender(const FMatrix& ViewProjection, const FVector& CameraPosition);

	bool IsValid() const { return Shader != nullptr && PanoramaTexture != nullptr; }

private:
	FShaderProgram* Shader = nullptr;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
	TUniquePtr<FTexture2D> PanoramaTexture;
	FPipelineState PipelineState;
};
