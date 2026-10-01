#pragma once

#include "SceneComponent.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Mesh.h"
#include "Rendering/RenderPacket.h"
#include "Rendering/GeometryGenerator.h"
#include "Collision/HitResult.h"
#include "Collision/Ray.h"

enum class EPrimitiveType
{
	Sphere,
	Cube,
	Cone,
	Plane,

	Particle,
	ParticleQuad
};

class UPrimitiveComponent :public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	// Material은 여기서 등록하지 않는다. 컴포넌트마다 머티리얼을 다루는 방식이 달라서
	// (빌보드는 단일 머티리얼, 스태틱 메시는 슬롯별 덮어쓰기) 각자 등록한다.
	REFLECT_START(ClassName)
		PROPERTY(bVisible)
		REFLECT_END()
public:
	UPrimitiveComponent();
	virtual ~UPrimitiveComponent();

	virtual void BeginPlay() override;

	virtual void SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets);

	virtual const FStaticMeshData* GetMeshData() const { return nullptr; }

	// Outline처럼 컴포넌트를 통째로 한 번 더 그릴 때 쓰는 GPU 메시.
	// 메시가 없거나(텍스트) View에 따라 형상이 정해지는 컴포넌트(빌보드·파티클)는
	// 컴포넌트 월드 행렬만으로 같은 그림을 못 만들므로 nullptr을 돌려준다.
	virtual UStaticMesh* GetRenderMesh() const { return nullptr; }

	virtual int32 GetNumMaterials() const { return 0; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const { return nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) {}
	//
	// FShader* GetShader() const { return Shader.get(); };

	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible)
	{
		if (bVisible == bInVisible)
			return;
		bVisible = bInVisible;
		MarkBoundsDirtyRecursive();
	}

	virtual bool LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit);
	// 피킹 전용 판정: 클릭당 한 번 준비한 컨텍스트(레이 역수, 최근접 거리, Billboard 행렬 공급자)를 쓴다.
	// View에 따라 형상이 정해지는 컴포넌트(Billboard·Particle)는 이 함수를 override한다.
	virtual bool LineTraceWithContext(const FTraceContext& Context, FHitResult& OutHit);
	virtual FBox CalcLocalBounds() const override
	{
		const FStaticMeshData* Data = GetMeshData();
		return Data ? Data->AABB : Super::CalcLocalBounds();
	}

	

	

protected:
	virtual void OnBoundsMarkedDirty() override;
	bool TraceMesh(const FRay& WorldRay, const FStaticMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult, float MaxT = FLT_MAX);
	bool bVisible = true;

	/*TArray<UMaterial* MaterialOverride = nullptr;*/



};
