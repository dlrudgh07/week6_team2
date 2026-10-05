#pragma once

#include "RHI/RHIBuffer.h"
#include "Rendering/RenderCommand.h"
#include "RHI/PipelineState.h"
#include "Engine/Texture2D.h"

#include "Rendering/Vertex.h"
#include "Math/Color.h"

struct FFireballFrameConstants
{
    FMatrix ViewProj;           // 64 bytes
    FMatrix InvViewProj;        // 64 bytes
    FVector2D ScreenSize;       // 8  bytes
    FVector2D Padding;          // 8  bytes
};

struct FFireballConstants
{
    FVector FireballPosition;     // 12
    float Radius;                 // 4

    FLinearColor Color;           // 16
    
    float Intensity;              // 4
    float RadiusFallOff;          // 4
    FVector2D Padding;            // 8
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
                                 const TArray<FFireballSceneData>& Fireballs);

private:
    FShaderProgram* Shader = nullptr;
    
    TUniquePtr<FRHIUniformBuffer> PerFrameCB;
    TUniquePtr<FRHIUniformBuffer> PerObjCB;
    TUniquePtr<FRHIUniformBuffer> FireBallCB;

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

