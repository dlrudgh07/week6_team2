#pragma once

#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
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

	virtual int32 GetNumMaterials() const override { return 1; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const override { return SlotIndex == 0 ? Material : nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) override
	{
		if (SlotIndex == 0 && Material != InMaterial)
		{
			Material = InMaterial;
			MarkBoundsDirtyRecursive();
		}
	}

	virtual const FStaticMeshData* GetMeshData() const override { return QuadMesh ? &QuadMesh->GetMeshData() : nullptr; }

	void GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const;
	// 기본 카메라 기준 월드 행렬로 Billboard 렌더 패킷을 제출한다.
	virtual void SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets) override;
	// View별 Adapter가 계산한 Billboard 행렬을 사용해 같은 렌더 패킷 형식으로 제출한다.
	void SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets, const FMatrix& BillboardWorldMatrix);

	virtual void Serialize(json& Handle, bool bIsLoading) override;

protected:
	UMaterial* Material = nullptr;
	UStaticMesh* QuadMesh = nullptr;
private:

};
