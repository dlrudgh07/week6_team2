// 레벨, 물리, 틱 등을 관리하는 클래스
#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "Math/Transform.h"
#include "Rendering/Renderer.h"
#include "Engine/PrimitiveBVH.h"
#include "Camera/CameraActor.h"

class AActor;
class ULevel;
class UBillboardComponent;

enum class EWorldType
{
	Editor,
	EditorPreview,
	PIE,
	Game,
};

class UWorld : public UObject
{
	DECLARE_CLASS(UWorld, UObject)

  public:
	UWorld() = default;
	virtual ~UWorld();

	bool Init();

	AActor* SpawnActor(UClass* Class, FName InName = NAME_None, const FTransform* Transform = nullptr);

	template <class T> T* SpawnActor(FName InName = NAME_None, const FTransform* Transform = nullptr)
	{
		return CastChecked<T>(SpawnActor(T::StaticClass(), InName, Transform));
	}

	void Tick(float DeltaTime);

	void ClearWorld();

	void GatherRenderPackets(TArray<FRenderPacket>& OutPackets);

	void CreateMainCamera();

	// 카메라 Get/Set
	ACameraActor* GetMainCamera() const;
	void SetMainCamera(ACameraActor* Camera);

	ULevel* GetCurrentLevel() const;

	int32 GetActorCount();

	bool DestroyActor(AActor* Actor);

	// View별 Billboard 행렬 공급자는 이 동기 호출 동안만 사용하며 저장하지 않는다.
	using FBillboardTraceTransform = FMatrix (*)(const UBillboardComponent&, const void*);

	// 현재 World의 Component에 Ray를 전달하고 가장 가까운 유효 교차를 반환한다.
	bool LineTraceSingle(const FRay& WorldRay,
		FHitResult& OutHit,
		const TArray<FLineTraceCandidate>& Candidates,
		FBillboardTraceTransform ResolveBillboard = nullptr,
		const void* ViewContext = nullptr);

	// Tick에서 갱신한 World BVH로 Ray가 통과하는 Primitive 후보를 수집한다.
	void GatherLineTraceCandidates(const FRay& WorldRay, TArray<FLineTraceCandidate>& OutCandidates) const;

	// Tick에서 갱신한 World BVH를 가까운 노드부터 순회하며 후보를 바로 NarrowTest로 정밀 검사한다.
	void TraceLineClosest(FTraceContext& Context, FPrimitiveBVH::FRayNarrowTestFn NarrowTest, void* UserContext) const;

	// 후보 하나를 정밀 검사한다. 지금까지보다 가까이 실제로 맞았을 때만 InOutHit과 Context.BestDistance를 갱신한다.
	bool LineTraceCandidate(FTraceContext& Context, UPrimitiveComponent* Primitive, FHitResult& InOutHit);

	void MarkPrimitiveBoundsDirty(UPrimitiveComponent* Primitive);

	void BeginPlay();
	void EndPlay();

	const TArray<TWeakObjectPtr<UPrimitiveComponent>>& GetWorldPrimitiveComponents() const;

	uint64 GetPrimitiveTopologyRevision() const;

	const TArray<TWeakObjectPtr<UPrimitiveComponent>>& GetDirtyRenderPrimitiveComponents() const;

	void ClearDirtyRenderPrimitiveComponents();

	void AddComponent(UPrimitiveComponent* PrimComp);

  private:
	friend class AActor;
	void RefreshActorTickRegistration(AActor* Actor);

	TQueue<AActor*> BeginPlayList;
	ACameraActor* MainCamera = nullptr;
	ULevel* Level = nullptr;
	EWorldType WorldType = EWorldType::Editor;
	TArray<AActor*> TickActors;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> WorldPrimitiveComponents;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> DirtyPrimitiveComponents;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> DirtyRenderPrimitiveComponents;
	uint64 PrimitiveTopologyRevision = 1;
	FPrimitiveBVH PrimitiveBVH;
};
