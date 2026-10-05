#include "EnginePCH.h"
#include "FireballRenderer.h"
#include "RenderResourceManager.h"

bool FFireballRenderer::Init()
{
    VertexBuffer = FRenderCommand::CreateDynamicVertexBuffer(sizeof(FTextVertex) * MaxVertices, sizeof(FTextVertex));
	IndexBuffer = FRenderCommand::CreateDynamicIndexBuffer(sizeof(uint32) * MaxIndices);

    // Firball Shader Load
    Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/FireballShader.hlsl");
    if (!Shader)
    {
        LOG(Error, "[Fog] Shader not found");
        return false;
    }

    // FFireballContants size 만큼 Cbuffer 생성
    ConstantBuffer = FRenderCommand::CreateConstantBuffer(sizeof(FFireballConstants));

    // Pipeline Set
    PipelineState.Shader = Shader;
    PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    PipelineState.RasterizerState = ERasterizerState::SolidNone;
    PipelineState.BlendState = EBlendState::AlphaBlend;
    PipelineState.DepthStencilState = EDepthStencilState::Disabled;

    return true;
}

void FFireballRenderer::OnRender(FRHITexture2D* SceneDepthTexture, 
                  const FMatrix& ViewProj,
                  const FVector& CameraLocation,
                  const FFireballSceneData& FireBallData)
{
    if (!IsValid()) return;

    ID3D11DeviceContext* Context = FRenderCommand::GetContext();
    if (!Context) return;

    // RTV
    ID3D11RenderTargetView* CurrentRTV[1] = { nullptr };
    
    // DSV
    ID3D11DepthStencilView* CurrentDSV = nullptr;

    // OM 에서 RSV, DSV 가져오기
    Context->OMGetRenderTargets(1, CurrentRTV, &CurrentDSV);

    // DSV 해제
    if (CurrentDSV) Context->OMSetRenderTargets(1, CurrentRTV, nullptr);

    // SRV 바인딩
    FRenderCommand::BindShaderResource(0, SceneDepthTexture, EShaderBindFlagBits::Pixel);

    FRenderCommand::UpdateBufferData(VertexBuffer.get(), Vertices.GetData(), sizeof(FTextVertex) * Vertices.Num());
	FRenderCommand::UpdateBufferData(IndexBuffer.get(), Indices.GetData(), sizeof(uint32) * Indices.Num());

    FFireballConstants Constants;
    Constants.Color = FireBallData.FogDensity;
    Constants.Intensity = FireBallData.Intensity;
    Constants.Radius = FireBallData.Radius;
    Constants.RadiusFallOff = FireBallData.RdiusFallOff;

    FRenderCommand::BindPipelineState(&PipelineState);
    FRenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants);
    FRenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
    FRenderCommand::BindSamplerState(0, ESamplerState::PointClamp, EShaderBindFlagBits::Pixel);
    FRenderCommand::BindVertexBuffer(VertexBuffer.get());
    FRenderCommand::BindIndexBuffer(IndexBuffer.get());
    FRenderCommand::DrawIndexed(Indices.Num());

    Vertices.Reset();
    Indices.Reset();
}

