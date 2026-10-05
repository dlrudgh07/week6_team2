#pragma once

#include "RHI/RHIBuffer.h"
#include "Rendering/RenderCommand.h"
#include "RHI/PipelineState.h"
#include "Engine/Texture2D.h"

#include "Rendering/Vertex.h"
#include "Math/Color.h"

struct FFireballSceneData;

struct FFireballConstants
{
    FLinearColor Color;           // 16

    FVector FireballPosition;     // 12
    float Intensity;              // 4

    float Radius;                 // 4
    float RadiusFallOff;          // 4
    FVector2D Padding;            // 4
};

class FFireballRenderer
{
public:
    FFireballRenderer() = default;
    ~FFireballRenderer() = default;

    bool Init();
    bool IsValid() const { return Shader != nullptr; }
    void OnRender(FRHITexture2D* SceneDepthTexture, 
                  const FMatrix& ViewProj,
                  const FVector& CameraLocation,
                  const FFireballSceneData& FireBallData);

private:
    FShaderProgram* Shader = nullptr;
    TUniquePtr<FRHIUniformBuffer> ConstantBuffer;
    FPipelineState PipelineState;

    TUniquePtr<FRHIVertexBuffer> VertexBuffer;
	TUniquePtr<FRHIIndexBuffer> IndexBuffer;

    TArray<FVertex> Vertices;
	TArray<uint32> Indices;

    TUniquePtr<FRHIUniformBuffer> MVP;
	TUniquePtr<FRHIUniformBuffer> ScreenPx;

    uint32 MaxVertices = 0;
	uint32 MaxIndices = 0;

};