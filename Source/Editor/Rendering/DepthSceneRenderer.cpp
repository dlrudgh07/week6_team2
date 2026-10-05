#include "EnginePCH.h"
#include "DepthSceneRenderer.h"
#include "Runtime/Rendering/RenderResourceManager.h"
#include "Rendering/RenderCommand.h"

bool FDepthSceneRenderer::Init()
{
	// 셰이더 가져오기
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/DepthSceneShader.hlsl");
	if (!Shader)
	{
		LOG(Error, "[DepthScene] shader not found");
		return false;
	}

	//Desc 필요없음

	//상수버퍼 만들기
	ConstantBuffer = FRenderCommand::CreateConstantBuffer(sizeof(FDepthSceneConstants));

	//Pipeline Set
	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;
	PipelineState.BlendState = EBlendState::Opaque;
	PipelineState.DepthStencilState = EDepthStencilState::ReadOnly;
	
	return true;
}

void FDepthSceneRenderer::OnRender(FRHITexture2D* SceneDepthTexture, const FMatrix& Projection)
{
	if (!IsValid())
		return;

	ID3D11DeviceContext* Context = FRenderCommand::GetContext();
	if (!Context)
	{
		return;
	}

	// 현재 RTV
	ID3D11RenderTargetView* CurrentRTV[1] = {nullptr};

	// 현재 DSV
	ID3D11DepthStencilView* CurrentDSV = nullptr;

	// OM에 연결되어 있는 RTV, DSV 가져오기
	Context->OMGetRenderTargets(1, CurrentRTV, &CurrentDSV);

	if (CurrentDSV) //Depthstencil 해제
	{
		Context->OMSetRenderTargets(1, CurrentRTV, nullptr);
	}

	//SRV 바인딩
	FRenderCommand::BindShaderResource(0, SceneDepthTexture, EShaderBindFlagBits::Pixel);

	FDepthSceneConstants Constants;
	Constants.InvProj = Projection.Inverse();
	

	FRenderCommand::BindPipelineState(&PipelineState);\
	FRenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants);
	FRenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	FRenderCommand::BindSamplerState(0, ESamplerState::PointClamp, EShaderBindFlagBits::Pixel);
	// 정점 버퍼 없이 3개. VS가 SV_VertexID로 삼각형을 만든다.
	FRenderCommand::Draw(3);

	//원상복구단계

	//SRV 바인드 해제
	FRenderCommand::PSSetShaderResource(0, nullptr);

	if (CurrentDSV)
	{
		Context->OMSetRenderTargets(1, CurrentRTV, CurrentDSV);
		CurrentDSV->Release();
	}
	
	if (CurrentRTV[0])
	{
		CurrentRTV[0]->Release();
	}
	return;
}
