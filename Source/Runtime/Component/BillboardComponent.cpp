#include "EnginePCH.h"
#include "Rendering/Material.h"
#include "BillboardComponent.h"

#include "Asset/AssetManager.h"
#include "Serialization/TypeSerializer.h"

#include "GameFramework/Actor.h"
#include "World/World.h"

// Billboard 컴포넌트의 초기 상태를 구성한다.
UBillboardComponent::UBillboardComponent()
{
	QuadMesh = UAssetManager::GetAssetByKey<UStaticMesh>("ParticleQuad");
	Material = UAssetManager::GetAssetByKey<UMaterial>("SubUVMaterial");
}

// Billboard 컴포넌트의 소멸을 처리한다.
UBillboardComponent::~UBillboardComponent()
{
}

// 기본 Mesh와 Material을 에셋 관리자에서 가져온다.
void UBillboardComponent::BeginPlay()
{
	Super::BeginPlay();

}

// 부모 컴포넌트의 프레임 갱신을 호출한다.
void UBillboardComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

bool UBillboardComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
{
	if (!QuadMesh) return false;

	FMatrix BillboardMatrix;
	GetWorldTransformedMatrix(&BillboardMatrix);   // 카메라를 향하는, 실제로 그려지는 행 렬
	return TraceMesh(WorldRay, QuadMesh->GetMeshData(), BillboardMatrix, OutHit);
}

// View별 렌더 행렬을 그대로 사용해 메인 카메라와 다른 방향에서도 같은 면을 선택한다.
bool UBillboardComponent::LineTraceComponentForView(
	const FRay& WorldRay, FHitResult& OutHit, const FMatrix& BillboardWorldMatrix)
{
	return QuadMesh && TraceMesh(WorldRay, QuadMesh->GetMeshData(), BillboardWorldMatrix, OutHit);
}

// View별 행렬 공급자가 있으면 클릭한 View의 실제 렌더 행렬로, 없으면 메인 카메라 기준으로 판정한다.
// (기존 World 루프의 Billboard 분기와 같은 동작. ParticleSubUV도 이 함수를 상속받는다)
bool UBillboardComponent::LineTraceWithContext(const FTraceContext& Context, FHitResult& OutHit)
{
	if (Context.ResolveBillboard)
	{
		return LineTraceComponentForView(Context.Ray, OutHit, Context.ResolveBillboard(*this, Context.ViewContext));
	}
	return LineTraceComponent(Context.Ray, OutHit);
}

// 기본 카메라 기준으로 패킷 배열에 추가
void UBillboardComponent::SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets)
{
	if (QuadMesh == nullptr || Material == nullptr)
	{
		return;
	}

	FMatrix BillboardWorldMatrix;
	GetWorldTransformedMatrix(&BillboardWorldMatrix);
	SubmitToRenderPackets(OutPackets, BillboardWorldMatrix);
}

// 전달받은 행렬 기준으로 패킷 배열에 직접 추가
void UBillboardComponent::SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets, const FMatrix& BillboardWorldMatrix)
{
	if (QuadMesh == nullptr || Material == nullptr)
		return;

	FRenderPacket Packet;
	Packet.mesh = QuadMesh;
	Packet.material = Material;
	Packet.model = BillboardWorldMatrix;
	OutPackets.Add(Packet);
}

void UBillboardComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);

	if (bIsLoading)
	{
		// 예전 파일은 "Material"이 문자열(경로)이라 형식을 확인하고 읽는다
		if (Handle.contains("Material") && Handle["Material"].is_object())
		{
			if (UMaterial* Loaded = UMaterial::LoadMaterial(Handle["Material"]))
			{
				Material = Loaded;   // 못 만들었으면 생성자 기본값 유지
			}
		}
	}
	else
	{
		Handle["Material"] = Material ? UMaterial::SaveMaterial(Material) : json(nullptr);
	}
}

// 카메라를 향하는 기저와 위치·크기로 Billboard 행렬을 구성한다.
void UBillboardComponent::GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const
{
	OutWorldMatrix->SetIdentity();

	const FTransform& Transform = GetOwner()->GetWorld()->GetMainCamera()->GetCameraComponent()->GetTransform();

	FVector Right = Transform.GetRight().Normalized();
	FVector Up = Transform.GetUp().Normalized();
	FVector Forward = Transform.GetForward().Normalized();

	FVector BbUp = Transform.GetUp().Normalized();
	FVector BbRight = FVector::Cross(BbUp, Forward).Normalized();
	FVector BbFwd = FVector::Cross(BbUp, BbRight);

	if (BbRight.Length() <= 1e-6f)
	{
		BbRight = Right;
		BbFwd = FVector::Cross(BbUp, BbRight);
	}

	const FVector WorldPos = GetWorldLocation();
	const FVector WorldScale = GetWorldScale3D();

	// Y -> Billboard Right
	OutWorldMatrix->M[0][0] = BbFwd.X;
	OutWorldMatrix->M[0][1] = BbFwd.Y;
	OutWorldMatrix->M[0][2] = BbFwd.Z;
	OutWorldMatrix->M[0][3] = 0.0f;

	// Z -> Billboard Up
	OutWorldMatrix->M[1][0] = BbRight.X;
	OutWorldMatrix->M[1][1] = BbRight.Y;
	OutWorldMatrix->M[1][2] = BbRight.Z;
	OutWorldMatrix->M[1][3] = 0.0f;

	// X -> Billboard Forward
	OutWorldMatrix->M[2][0] = BbUp.X;
	OutWorldMatrix->M[2][1] = BbUp.Y;
	OutWorldMatrix->M[2][2] = BbUp.Z;
	OutWorldMatrix->M[2][3] = 0.0f;

	// Position
	OutWorldMatrix->M[3][0] = WorldPos.X;
	OutWorldMatrix->M[3][1] = WorldPos.Y;
	OutWorldMatrix->M[3][2] = WorldPos.Z;
	OutWorldMatrix->M[3][3] = 1.0f;
}
