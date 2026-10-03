#include "EnginePCH.h"
#include "FogRenderer.h"
#include "RenderResourceManager.h"
#include "RenderCommand.h"

bool FFogRenderer::Init()
{
	// 셰이더 가져오기
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/HeightFogCommon.hlsl");
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

void FFogRenderer::OnRender(FRHITexture2D* SceneDepthTexture, FMatrix& ViewProj, FVector& CameraLocation) //Fog에 대한 변수 파라미터 추가필요(컴포넌트와 합의)
{
	if (!IsValid()) return;


	ID3D11DeviceContext* Context = FRenderCommand::GetContext();
	if (!Context)
	{
		return;
	}
	ID3D11RenderTargetView* CurrentRTV[1] = { nullptr };
	ID3D11DepthStencilView* CurrentDSV = nullptr;
	Context->OMGetRenderTargets(1, CurrentRTV, &CurrentDSV);
	if (CurrentDSV) //Depthstencil 해제
	{
		Context->OMSetRenderTargets(1, CurrentRTV, nullptr);
	}
	//SRV 바인딩
	FRenderCommand::BindShaderResource(0, SceneDepthTexture, EShaderBindFlagBits::Pixel);

	

	FFogConstants Constants;
	Constants.FogDensity; //초기화필요
	Constants.FogHeightFalloff; //초기화필요
	Constants.FogInscatteringColor; //초기화필요
	Constants.FogMaxOpacity; //초기화필요
	Constants.StartDistance;  //초기화필요
	Constants.FogCutoffDistance; //초기화필요
	Constants.InverseViewProjection = ViewProj.Inverse();
	Constants.CameraPosition = CameraLocation;
	//Constants.FogInscatteringColor[3] = 0.5f; 
	
	FRenderCommand::BindPipelineState(&PipelineState);
	FRenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants);
	FRenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	//FRenderCommand::BindSamplerState(0, ESamplerState::LinearWrap, EShaderBindFlagBits::Pixel);
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
