#pragma once

#include "PrimitiveComponent.h"
#include "Materials/Material.h"

// 렌더 중인 View 카메라를 향하는 Billboard 행렬 공급자.
// Runtime이 Editor의 View 구조를 모르도록 함수 포인터 + 컨텍스트로 받는다. (FTraceContext와 같은 방식)
struct FBillboardViewContext
{
	using FBuildMatrixFn = FMatrix (*)(const void* ViewContext, const FVector& WorldPosition, float Width, float Height);

	FBuildMatrixFn BuildMatrixFn = nullptr;
	const void* ViewContext = nullptr;
	// 반투명 거리 정렬에 쓰는 View 카메라 위치
	FVector CameraLocation;

	FMatrix BuildMatrix(const FVector& WorldPosition, float Width, float Height) const
	{
		return BuildMatrixFn(ViewContext, WorldPosition, Width, Height);
	}
};

class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
	PROPERTY(Material)
	PROPERTY(QuadMesh)
	REFLECT_END()

  public:
	UBillboardComponent();
	virtual ~UBillboardComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual bool LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit) override;
	// 클릭한 View의 실제 렌더 행렬로 Quad Mesh 교차를 판정한다.
	bool LineTraceComponentForView(const FRay& WorldRay, FHitResult& OutHit, const FMatrix& BillboardWorldMatrix);
	// 피킹 루프가 종류를 구분하지 않도록, View별 행렬 선택을 Billboard가 스스로 처리한다.
	virtual bool LineTraceWithContext(const FTraceContext& Context, FHitResult& OutHit) override;

	virtual int32 GetNumMaterials() const override
	{
		return 1;
	}
	virtual UMaterial* GetMaterial(int32 SlotIndex) const override
	{
		return SlotIndex == 0 ? Material : nullptr;
	}
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) override
	{
		if (SlotIndex == 0 && Material != InMaterial)
		{
			Material = InMaterial;
			MarkBoundsDirtyRecursive();
		}
	}

	virtual const FStaticMeshRenderData* GetMeshData() const override
	{
		return QuadMesh ? &QuadMesh->GetMeshData() : nullptr;
	}

	void GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const;
	// 기본 카메라 기준 월드 행렬로 Billboard 렌더 패킷을 제출한다.
	virtual void SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets) override;
	// 렌더 중인 View 카메라 기준 행렬로 렌더 패킷을 제출한다. (피킹의 ResolveBillboard와 같은 행렬)
	virtual void SubmitToRenderPacketsForView(TArray<FRenderPacket>& OutPackets, const FBillboardViewContext& View);

	virtual void Serialize(json& Handle, bool bIsLoading) override;

  protected:
	void AddRenderPacket(TArray<FRenderPacket>& OutPackets, const FMatrix& BillboardWorldMatrix, float CameraDistanceSquared) const;

	UMaterial* Material = nullptr;
	UStaticMesh* QuadMesh = nullptr;

  private:
};
