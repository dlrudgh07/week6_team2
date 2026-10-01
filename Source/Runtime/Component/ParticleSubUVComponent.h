#pragma once

#include "BillboardComponent.h"

struct FParticle
{
	// 파티클 한 개의 물리·수명·SubUV 프레임 상태를 담는다.
	FVector Location;
	FVector Velocity;
	float LifeTime;

	float Age;
	float Scale;
	uint32 SubUVFrame;
	float FrameTimer;

	float Alpha;

	bool bAlive;
};

struct FSubUVConstants
{
	// 한 파티클을 그릴 때 Shader에 전달할 Atlas 프레임과 투명도 값을 담는다.
	float CurrentFrame;
	float AtlasRowSize;
	float AtlasColSize;
	//float Padding = 0.0f;
	float Alpha;
};

// 가장 단순한 SubUV 파티클입니다.
// Atlas 전체를 Col x Row로 나누고 CurrentFrame 하나만 변경합니다.
class UParticleSubUVComponent : public UBillboardComponent
{
	DECLARE_CLASS(UParticleSubUVComponent, UBillboardComponent)

	REFLECT_START(ClassName)
		PROPERTY(ColSize)
		PROPERTY(RowSize)
		PROPERTY(FrameRate)
	REFLECT_END()

public:
	UParticleSubUVComponent();
	virtual ~UParticleSubUVComponent() = default;
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual void SubmitToRenderPackets(TArray<FRenderPacket>& OutPackets) override;
	// Adapter가 View별 거리 정렬 입력을 만들 수 있도록 현재 파티클 배열을 읽기 전용으로 제공한다.
	const TArray<FParticle>& GetParticlesForView() const { return Particles; }
	// 프레임의 첫 View 전에 파티클 인덱스별 상수를 준비하고 네 View에서 재사용한다.
	void BeginViewSubmission();
	// 불투명 파티클의 기존 정렬 경로를 선택한다.
	bool UsesOpaqueMaterial() const;
	void SetSubUVSize(uint32 Cols, uint32 Rows);
	void SetFrameRate(float InFrameRate);

private:
	void RespawnParticle(FParticle& Particle);

	// Todo: Move to util class
	float GetRandomNumberBetween(float start, float end) const;
	float Lerp(float first, float second, float alpha) const;
	float GedSmoothStepedRatio(float start, float end, float value) const;

private:
	static const float MAX_NORMALIZED_VALUE;

	// ---- 튜닝 값 ----
	// 방출은 수명이 끝난 파티클이 무작위 시점에 리스폰되는 방식이다.
	// 초당 평균 방출량 ≈ ParticleCount / 평균 수명. 수명 범위가 넓을수록 방출 간격이 더 불규칙해진다.
	int32 ParticleCount = 10;           // 동시에 존재하는 파티클 수 (생성자에서만 반영)

	float MinLifeTime = 6.0f;           // 수명 최소값(초)
	float MaxLifeTime = 8.0f;          // 수명 최대값(초)

	float MinRiseSpeed = 0.7f;          // 상승 속도 최소값
	float MaxRiseSpeed = 0.9f;          // 상승 속도 최대값

	float StartScale = 1.2f;            // 생성 시 크기
	float EndScale = 1.5f;              // 수명이 끝날 때 크기

	float FadeStart = 0.8f;             // 수명 비율이 이 값을 넘으면 페이드아웃 시작 (0~1)

	TArray<FSubUVConstants> Constants;

	uint32 ColSize;
	uint32 RowSize;
	float FrameRate;

	// Todo: Reserve particles
	TArray<FParticle> Particles;
};
