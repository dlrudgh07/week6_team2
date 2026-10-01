#include "EnginePCH.h"
#include "World.h"
#include "Level.h"
#include <limits>
#include "ObjectSystem/ObjectFactory.h"
#include "Core/EngineStatics.h"
#include "GameFramework/Actor/StaticMeshActor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Input/InputSystem.h"

#include "ObjectSystem/UObjectIterator.h"

#include "Collision/Ray.h"
#include "Component/BillboardComponent.h"

UWorld::~UWorld()
{
}

bool UWorld::Init()
{
	// Spawn Actor로 카메라 생성하고 세팅하기
	PersistentLevel = FObjectFactory::ConstructObject<ULevel>();

	if (!PersistentLevel)
	{
		LOG(Error, "Failed to create PersistentLevel");
		return false;
	}

	//레벨 연결
	PersistentLevel->SetWorld(this);
	Levels.Add(PersistentLevel);
	CurrentLevel = PersistentLevel;

	//카메라 생성
	CreateMainCamera();

	return true;
}

AActor* UWorld::SpawnActor(UClass* Class, FName InName, const FTransform* Transform)
{
	if (!Class)
		return nullptr;
	if (!Class->IsChildOf(AActor::StaticClass()))
		return nullptr;

	// 1. ObjectFactory로 Actor 생성
	AActor* NewActor = Cast<AActor>(FObjectFactory::ConstructObject(Class, PersistentLevel, InName));

	if (!NewActor)
	{
		LOG(Error, "SpawnActor : Failed to create Actor");
		return nullptr;
	}

	// 2. Actor에 World/Level 연결
	NewActor->World = this;
	NewActor->Level = PersistentLevel;

	// 3. Transform 적용
	const FTransform SpawnTransform = Transform ? *Transform : FTransform::Identity;

	if (NewActor->GetRootComponent())
	{
		NewActor->GetRootComponent()->SetTransform(SpawnTransform);

		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(NewActor->GetRootComponent()))
		{
			WorldPrimitiveComponents.Add(PrimComp);
			++PrimitiveTopologyRevision;
		}
	}

	// 4. Level->Actors에 등록
	PersistentLevel->AddActor(NewActor);
	RefreshActorTickRegistration(NewActor);

	// 5. PlayList에 추가
	BeginPlayList.Enqueue(NewActor);

	return NewActor;
}

void UWorld::Tick(float DeltaTime)
{
	while (!BeginPlayList.IsEmpty())
	{
		BeginPlayList.Peek()->BeginPlay();
		BeginPlayList.Dequeue();
	}

	for (AActor* Actor : TickActors)
	{
		if (Actor)
			Actor->Tick(DeltaTime);
	}

	if (MainCamera)
	{
		MainCamera->Tick(DeltaTime);
	}

	// 모든 Actor와 Component의 Transform 갱신이 끝난 뒤 피킹 공간 인덱스를 갱신한다.
	PrimitiveBVH.Update(WorldPrimitiveComponents, PrimitiveTopologyRevision, DirtyPrimitiveComponents);
	DirtyPrimitiveComponents.Reset();

}

void UWorld::ClearWorld()
{
	// BeginPlay 대기 중인 Actor 제거
	while (!BeginPlayList.IsEmpty())
	{
		BeginPlayList.Dequeue();
	}

	PathTracker.SetPlaybackEnabled(false);
	PathTracker.SetPathRenderingEnabled(false);
	PathTracker.ClearPath();

	for (ULevel* Level : Levels)
	{
		Level->ClearActors();
	}
	WorldPrimitiveComponents.Reset();
	DirtyPrimitiveComponents.Reset();
	DirtyRenderPrimitiveComponents.Reset();
	TickActors.Reset();
	++PrimitiveTopologyRevision;
	PrimitiveBVH.Reset();
	LOG(Info, "{} : ", PersistentLevel->GetActorNum());
}

void UWorld::GatherRenderPackets(TArray<FRenderPacket>& OutPackets)
{
	OutPackets.Reset();
	//for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	for (TObjectIterator<UPrimitiveComponent> Itr; Itr; ++Itr)
	{
		if (*Itr && Itr->IsVisible())
			Itr->SubmitToRenderPackets(OutPackets);
	}
}

// 메인 카메라 생성
void UWorld::CreateMainCamera()
{
	if (MainCamera)
		return;

	MainCamera = FObjectFactory::ConstructObject<ACameraActor>();

	if (!MainCamera)
	{
		LOG(Error, "Failed to create MainCamera");
		return;
	}

	MainCamera->World = this;
	MainCamera->Level = nullptr;
	MainCamera->GetCameraComponent()->SetRelativeLocation(FVector(-5.0f, -5.0f, 5.0f));
}

int32 UWorld::GetActorNum()
{
	return PersistentLevel->GetActorNum();
}

bool UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor)
		return false;

	ULevel* Level = Actor->GetLevel();

	if (!Level)
		return false;

	// 1. BeginPlay 대기열에서 제거
	TQueue<AActor*> NewBeginPlayList;

	while (!BeginPlayList.IsEmpty())
	{
		AActor* PendingActor = BeginPlayList.Peek();
		BeginPlayList.Dequeue();

		if (PendingActor != Actor)
		{
			NewBeginPlayList.Enqueue(PendingActor);
		}
	}

	BeginPlayList = std::move(NewBeginPlayList);

	// 4. Level의 Actors에서 제거
	for (int32 i = Level->Actors.Num() - 1; i >= 0; --i)
	{
		if (Level->Actors[i] == Actor)
		{
			Level->Actors.RemoveAt(i, 1);
			break;
		}
	}

	FString ActorName = Actor->GetName();
	uint32 ActorUUID = Actor->GetUUID();
	for (int32 Index = TickActors.Num() - 1; Index >= 0; --Index)
	{
		if (TickActors[Index] == Actor)
		{
			TickActors.RemoveAt(Index, 1);
			break;
		}
	}
	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		for (int32 Index = WorldPrimitiveComponents.Num() - 1; Index >= 0; --Index)
		{
			if (WorldPrimitiveComponents[Index].Get() == Primitive)
			{
				WorldPrimitiveComponents.RemoveAt(Index, 1);
				++PrimitiveTopologyRevision;
				break;
			}
		}
	}

	// 6. Actor 삭제
	delete Actor;

	LOG(Info, "Destroy Actor : {} UUID {}", ActorName, ActorUUID);

	return true;
}

// 다른 World의 객체를 제외하고 Component 교차 중 최근접 결과를 선택한다.
bool UWorld::LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit, const TArray<FLineTraceCandidate>& Candidates, FBillboardTraceTransform ResolveBillboard, const void* ViewContext)
{
	OutHit = FHitResult();
	// 클릭당 한 번: 레이 역수, 최근접 거리, Billboard 행렬 공급자를 한 곳에 모은다
	FTraceContext Context = MakeTraceContext(WorldRay, ResolveBillboard, ViewContext);
	//for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	for (const FLineTraceCandidate& Candidate : Candidates)
	{
		UPrimitiveComponent* It = Candidate.Primitive;
		if (!It)
			continue;
		// Bounds 후보는 가까운 순서다. 현재 실제 Hit보다 뒤에서 시작하면 정밀 검사를 생략한다.
		// Bounds를 신뢰할 수 없는 후보는 기존처럼 항상 검사한다.
		if (Candidate.bHasBoundsDistance && Candidate.BoundsDistance >= Context.BestDistance)
			continue;
		LineTraceCandidate(Context, It, OutHit);
	}
	return OutHit.HitComponent != nullptr;
}

bool UWorld::LineTraceCandidate(FTraceContext& Context, UPrimitiveComponent* Primitive, FHitResult& InOutHit)
{
	//해당월드에 있음
	if (!Primitive || !Primitive->IsVisible())
		return false;
	FHitResult Hit;
	// Billboard·Particle은 LineTraceWithContext override에서 View 행렬로 판정하므로
	// 여기서는 컴포넌트 종류를 구분하지 않는다 (Cast 제거)
	if (Primitive->LineTraceWithContext(Context, Hit) && Hit.HitComponent && Hit.Distance >= 0.0f && Hit.Distance < InOutHit.Distance)
	{
		InOutHit = Hit;
		Context.BestDistance = Hit.Distance; // 이후 후보는 이보다 먼 박스의 정밀 판정을 건너뛴다
		return true;
	}
	return false;
}

void UWorld::GatherLineTraceCandidates(
	const FRay& WorldRay, TArray<FLineTraceCandidate>& OutCandidates) const
{
	PrimitiveBVH.GatherRayCandidates(WorldRay, OutCandidates);
}

void UWorld::TraceLineClosest(FTraceContext& Context, const FPrimitiveBVH::FRayNarrowTestFn NarrowTest, void* UserContext) const
{
	PrimitiveBVH.TraceRayClosest(Context, NarrowTest, UserContext);
}

void UWorld::MarkPrimitiveBoundsDirty(UPrimitiveComponent* Primitive)
{
	if (Primitive)
	{
		DirtyPrimitiveComponents.Add(Primitive);
		DirtyRenderPrimitiveComponents.Add(Primitive);
	}
}

void UWorld::RefreshActorTickRegistration(AActor* Actor)
{
	for (int32 Index = TickActors.Num() - 1; Index >= 0; --Index)
	{
		if (TickActors[Index] == Actor)
			TickActors.RemoveAt(Index, 1);
	}
	if (Actor && Actor->IsActorTickEnabled())
		TickActors.Add(Actor);
}

void UWorld::BeginPlay()
{
}

void UWorld::EndPlay()
{
}
