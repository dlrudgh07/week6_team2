#include "EnginePCH.h"
#include "FogRenderer.h"
#include "RenderResourceManager.h"
#include "Rendering/RenderCommand.h"
#include "FogSceneData.h"
bool FFogRenderer::Init()
{
	// 셰이더 가져오기
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/HeightFogShader.hlsl");
	if (!Shader)
	{
		LOG(Error, "[Fog] shader not found");
		return false;
	}
	
	//Desc 필요없음
	
	//상수버퍼 만들기
	ConstantBuffer = FRenderCommand::CreateConstantBuffer(sizeof(FFogConstants));

	//Pipeline Set
	PipelineState.Shader = Shader; 
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;
	PipelineState.BlendState = EBlendState::AlphaBlend;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;

	return true;
}

void FFogRenderer::OnRender(FRHITexture2D* SceneDepthTexture, const FMatrix& ViewProj, const FVector& CameraLocation, const FFogSceneData& FogData)
{
	if (!IsValid()) return;


	ID3D11DeviceContext* Context = FRenderCommand::GetContext();
	if (!Context)
	{
		return;
	}

	// 현재 RTV
	ID3D11RenderTargetView* CurrentRTV[1] = { nullptr };
	
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

	FFogConstants Constants;
	Constants.FogDensity = FogData.FogDensity;
	Constants.FogHeightFalloff = FogData.FogHeightFalloff;
	Constants.FogInscatteringColor = FLinearColor(FogData.FogInscatteringColor[0], FogData.FogInscatteringColor[1], FogData.FogInscatteringColor[2], 1.0f);
	Constants.FogMaxOpacity = FogData.FogMaxOpacity;
	Constants.StartDistance = FogData.StartDistance;
	Constants.FogCutoffDistance = FogData.FogCutoffDistance;
	Constants.FogHeight = FogData.FogHeight;
	Constants.InverseViewProjection = ViewProj.Inverse();
	Constants.CameraPosition = CameraLocation;

	FRenderCommand::BindPipelineState(&PipelineState);
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
	// 
	if (CurrentRTV[0])
	{
		CurrentRTV[0]->Release();
	}
	return;
}
