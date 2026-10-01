#include "EnginePCH.h"
#include "Rendering/Material.h"
#include "Rendering/RenderResourceManager.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"
#include "Rendering/RenderCommand.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "BillboardComponent.h"

#include "World/World.h"

const float UParticleSubUVComponent::MAX_NORMALIZED_VALUE = 1.0f;

UParticleSubUVComponent::UParticleSubUVComponent()
{
	ColSize = 8;
	RowSize = 8;
	FrameRate = 12.0f;
}

// 파티클 에셋과 배열을 준비하고 초기 상태를 채운다.
void UParticleSubUVComponent::BeginPlay()
{
	Super::BeginPlay();

	Particles.Reserve(ParticleCount);
	for (int32 i = 0; i < ParticleCount; ++i)
	{
		FParticle Particle;
		RespawnParticle(Particle);

		Particle.Age = GetRandomNumberBetween(0.0f, Particle.LifeTime);
		Particle.Location.Z += Particle.Velocity.Z * Particle.Age;

		const float LifeRatio = Particle.Age / Particle.LifeTime;
		Particle.Scale = Lerp(StartScale, EndScale, LifeRatio);

		const float ElapsedFrames = Particle.Age * FrameRate;
		Particle.SubUVFrame = static_cast<uint32>(ElapsedFrames) % (ColSize * RowSize);

		Particles.Add(Particle);
	}
}

// 열·행 개수가 양수인지 검사해 SubUV 분할 수를 설정한다.
void UParticleSubUVComponent::SetSubUVSize(uint32 NewColSize, uint32 NewRowSize)
{
	assert(NewColSize > 0 && NewRowSize > 0);

	ColSize = NewColSize;
	RowSize = NewRowSize;
}

// 유효한 프레임 속도를 저장하고 음수·0이면 기본값을 쓴다.
void UParticleSubUVComponent::SetFrameRate(float InFrameRate)
{
	FrameRate = (InFrameRate > 0.0f) ? InFrameRate : 1.0f;
}

// DeltaTime으로 이동·수명·Atlas 프레임을 갱신한다.
void UParticleSubUVComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	const uint32 TotalFrames = ColSize * RowSize;

	for (FParticle& Particle : Particles)
	{
		Particle.Location += Particle.Velocity * DeltaTime;
		Particle.Age += DeltaTime;

		if (Particle.Age >= Particle.LifeTime)
		{
			RespawnParticle(Particle);

			continue;
		}

		const float LifeRatio = Particle.Age / Particle.LifeTime;
		
		Particle.Scale = Lerp(StartScale, EndScale, LifeRatio);

		if (LifeRatio > FadeStart)
		{
			const float FadeRatio = GedSmoothStepedRatio(FadeStart, MAX_NORMALIZED_VALUE, LifeRatio);
			Particle.Alpha = MAX_NORMALIZED_VALUE - FadeRatio;
		}
		else
		{
			Particle.Alpha = MAX_NORMALIZED_VALUE;
		}

		Particle.FrameTimer += DeltaTime;
		const float FrameDuration = MAX_NORMALIZED_VALUE / FrameRate;

		if (Particle.FrameTimer >= FrameDuration)
		{
			Particle.FrameTimer = 0.0f;

			++Particle.SubUVFrame;
			Particle.SubUVFrame %= TotalFrames;
		}
	}
}

// 기본 카메라 기준으로 파티클 렌더 패킷을 배열에 직접 수집
void UParticleSubUVComponent::SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets)
{
	assert(QuadMesh != nullptr);
	assert(Material != nullptr);
	
	Constants.Reset();
	Constants.Reserve(Particles.Num());

	const FVector CameraPos = GetOwner()->GetWorld()->GetMainCamera()->GetCameraComponent()->GetWorldLocation();
	for (FParticle& Particle : Particles)
	{
		if (Particle.bAlive == false)
		{
			continue;
		}

		FMatrix WorldMatrix = FMatrix::Identity; 
		Super::GetWorldTransformedMatrix(&WorldMatrix);

		WorldMatrix.M[0][0] *= Particle.Scale;
		WorldMatrix.M[0][1] *= Particle.Scale;
		WorldMatrix.M[0][2] *= Particle.Scale;
		WorldMatrix.M[0][3] *= Particle.Scale;

		WorldMatrix.M[1][0] *= Particle.Scale;
		WorldMatrix.M[1][1] *= Particle.Scale;
		WorldMatrix.M[1][2] *= Particle.Scale;
		WorldMatrix.M[1][3] *= Particle.Scale;

		WorldMatrix.M[2][0] *= Particle.Scale;
		WorldMatrix.M[2][1] *= Particle.Scale;
		WorldMatrix.M[2][2] *= Particle.Scale;
		WorldMatrix.M[2][3] *= Particle.Scale;

		WorldMatrix.M[3][0] = Particle.Location.X;
		WorldMatrix.M[3][1] = Particle.Location.Y;
		WorldMatrix.M[3][2] = Particle.Location.Z;
		WorldMatrix.M[3][3] = 1.0f;

		const FVector ParticlePos = Particle.Location;
		const FVector CameraToParticleVec = ParticlePos - CameraPos;

		const float CameraToParticleDistance = 
			CameraToParticleVec.X * CameraToParticleVec.X +
			CameraToParticleVec.Y * CameraToParticleVec.Y +
			CameraToParticleVec.Z * CameraToParticleVec.Z;

		FRenderPacket Packet;
		Packet.model = WorldMatrix;
		Packet.mesh = QuadMesh;
		Packet.material = Material;

		FSubUVConstants C;
		C.CurrentFrame =  Particle.SubUVFrame;
		C.AtlasColSize = ColSize;
		C.AtlasRowSize = RowSize;
		C.Alpha = Particle.Alpha;

		Constants.Add(C);

		Packet.MaterialParamData = &Constants.Last();
		Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
		Packet.CameraDistanceSquared = CameraToParticleDistance;

		OutPackets.Add(Packet);
	}
}

// 파티클 위치·속도·수명 등 재생성 상태를 초기화한다.
void UParticleSubUVComponent::RespawnParticle(FParticle& Particle)
{
	Particle.Location = GetWorldLocation();

	Particle.Velocity.X = 0.0f;
	Particle.Velocity.Y = 0.0f;
	Particle.Velocity.Z = GetRandomNumberBetween(MinRiseSpeed, MaxRiseSpeed);

	Particle.LifeTime = GetRandomNumberBetween(MinLifeTime, MaxLifeTime);
	Particle.Age = 0.0f;
	Particle.SubUVFrame = static_cast<uint32>(GetRandomNumberBetween(0.0f, static_cast<float>((ColSize * RowSize) - 1)));
	Particle.Scale = StartScale;
	Particle.FrameTimer = 0.0f;
	Particle.bAlive = true;

	Particle.Alpha = MAX_NORMALIZED_VALUE;
}

// Todo: Move to util class
// 난수를 지정 실수 구간으로 변환한다.
float UParticleSubUVComponent::GetRandomNumberBetween(float start, float end) const
{
	float randomNumber = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

	return start + randomNumber * (end - start);
}

// 두 값을 alpha 비율로 선형 보간한다.
float UParticleSubUVComponent::Lerp(float left, float right, float alpha) const
{
	//assert(alpha >= 0.0f && alpha <= 1.0f);
	if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}
	
	if (alpha > 1.0f)
	{
		alpha = 1.0f;
	}

	return left + (right - left) * alpha;
}

// 정규화한 구간 비율에 삼차 smoothstep을 적용한다.
float UParticleSubUVComponent::GedSmoothStepedRatio(float start, float end, float value) const
{
	float normedValue = (value - start) / (end - start);
	//assert(normedValue >= 0.0f && normedValue <= 1.0f);

	if (normedValue < 0.0f)
	{
		normedValue = 0.0f;
	}

	if (normedValue > 1.0f)
	{
		normedValue = 1.0f;
	}

	return normedValue * normedValue * (3.0f - 2.0f * normedValue);
}

// 프레임당 한 번 전체 인덱스 순서로 상수를 준비하며 View 제출 중 재할당하지 않는다.
void UParticleSubUVComponent::BeginViewSubmission()
{
	Constants.Reset();
	Constants.Reserve(Particles.Num());
    for (const FParticle& Particle : Particles)
    {
        FSubUVConstants Value{};
        Value.CurrentFrame = static_cast<float>(Particle.SubUVFrame);
        Value.AtlasColSize = static_cast<float>(ColSize);
        Value.AtlasRowSize = static_cast<float>(RowSize);
        Value.Alpha = Particle.Alpha;
        Constants.Add(Value);
    }
}

// 실제 재질의 블렌드 상태로 프레임 공통 정렬과 View별 정렬을 구분한다.
bool UParticleSubUVComponent::UsesOpaqueMaterial() const
{
    return Material && Material->PSOType == EPSOType::StaticMesh_Opaque;
}
