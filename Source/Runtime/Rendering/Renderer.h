#pragma once

#include "RenderPacket.h"
#include "Buffer.h"
#include "Texture2D.h"
#include "Text/Font.h"


#include "RenderingInfo.h"

struct FPerObjectConstants
{
	FMatrix World;
};

static_assert(sizeof(FPerObjectConstants) == sizeof(FMatrix));

class UCameraComponent;

class FRenderer
{
	friend class UTexture2D;
public:

	bool Init();

	// 기존 단일 카메라의 ViewProjection으로 렌더 패킷 배열 전체를 그린다.
	void RenderAll(TArray<FRenderPacket>& InPackets, UCameraComponent* CameraComponent);

	// Adapter가 계산한 ViewProjection을 직접 받아 View별 렌더 패킷 배열을 그린다.
	void RenderAll(TArray<FRenderPacket>& InPackets, const FMatrix& ViewProjection);

	// 배열을 정렬해 불투명 패킷만 그린다. 반투명은 RenderTranslucent 호출 전까지 보관한다.
	// 연속 배열 기반 불투명 패킷 고속 렌더링
	void RenderOpaque(TArray<FRenderPacket>& InPackets, const FMatrix& ViewProjection, bool bWireframe = false);

	// RenderOpaque가 보관한 반투명 패킷을 먼 것부터 그린다.
	void RenderTranslucent(const FMatrix& ViewProjection);

private:
	// 정렬된 RenderPackets에서 반투명 패킷이 시작되는 위치
	uint32 FirstTranslucentIndex = 0;
	TUniquePtr<FConstantBuffer> CB;
	TUniquePtr<FConstantBuffer> Temp;
	TUniquePtr<FConstantBuffer> ViewCB;

	D3D11_VIEWPORT ViewportInfo;

	uint32 Width;
	uint32 Height;

	FLOAT ClearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };

	static constexpr uint32 PerObjectSlotSize = 256;
	static constexpr uint32 InitialPacketCapacity = 50000;

	// 워커 전용 지연 컨텍스트와 상수 버퍼
	struct FDeferredWorker
	{
		ComPtr<ID3D11DeviceContext> Context;
		ComPtr<ID3D11DeviceContext1> Context1;
		TUniquePtr<FConstantBuffer> PerObjectCB;
	};
	TArray<FDeferredWorker> DeferredWorkers;
	bool bDeferredWorkersInitialized = false;

	void EnsureDeferredWorkers();
	bool EnsureConstantBufferCapacity(TUniquePtr<FConstantBuffer>& Buffer, uint32 PacketCount);
	void DrawPackets(uint32 Begin, uint32 End, const FMatrix& ViewProjection);
	void BindMaterial(UMaterial* material, ID3D11DeviceContext* Context = nullptr, bool bBindPipelineState = true, bool bWireframe = false);
	void UpdateMaterialParams(UMaterial* material);
	void UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrixRegister& ViewProjection);
};
