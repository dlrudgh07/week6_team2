#include "EnginePCH.h"
#include "Component/SpotLightComponent.h"

#include "Rendering/LineBatcher.h"

namespace
{
	// 밑면 원의 분할 수. 너무 높이면 라인 배처 정점을 빨리 소모한다.
	constexpr int32 CONE_SEGMENT_COUNT = 24;

	// 꼭짓점에서 밑면으로 뻗는 옆선 개수
	constexpr int32 CONE_SIDE_LINE_COUNT = 24;

	// Apex에서 Forward 방향으로 Range만큼 뻗은 원뿔을 와이어로 그린다.
	// Right/Up은 Forward에 직교하는 밑면 평면의 축이다.
	void AddWireCone(
		FLineBatcher* LineBatcher,
		const FVector& Apex,
		const FVector& Forward,
		const FVector& Right,
		const FVector& Up,
		float HalfAngleDegrees,
		float Range,
		const FVector4& Color)
	{
		const float HalfAngle = FMath::DegreesToRadians(HalfAngleDegrees);
		const float BaseRadius = std::tanf(HalfAngle) * Range;

		const FVector BaseCenter = Apex + Forward * Range;

		// 밑면 원
		FVector PreviousPoint;
		FVector FirstPoint;

		for (int32 i = 0; i < CONE_SEGMENT_COUNT; ++i)
		{
			const float Angle = (2.0f * PI * i) / CONE_SEGMENT_COUNT;

			const FVector Point =
				BaseCenter +
				Right * (std::cosf(Angle) * BaseRadius) +
				Up * (std::sinf(Angle) * BaseRadius);

			if (i == 0)
			{
				FirstPoint = Point;
			}
			else
			{
				LineBatcher->AddLine(PreviousPoint, Point, Color);
			}

			PreviousPoint = Point;
		}

		// 마지막 점과 첫 점을 이어 원을 닫는다
		LineBatcher->AddLine(PreviousPoint, FirstPoint, Color);

		// 옆선
		for (int32 i = 0; i < CONE_SIDE_LINE_COUNT; ++i)
		{
			const float Angle = (2.0f * PI * i) / CONE_SIDE_LINE_COUNT;

			const FVector Point =
				BaseCenter +
				Right * (std::cosf(Angle) * BaseRadius) +
				Up * (std::sinf(Angle) * BaseRadius);

			LineBatcher->AddLine(Apex, Point, Color);
		}
	}
}

void USpotLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (LineBatcher == nullptr)
	{
		return;
	}

	FTransform WorldTransform;
	WorldTransform.Location = GetWorldLocation();
	WorldTransform.Rotation = GetWorldRotation();

	FVector Apex = WorldTransform.Location;
	FVector Forward = WorldTransform.GetForward();
	FVector Right = WorldTransform.GetRight();
	FVector Up = WorldTransform.GetUp();

	// 에디터에서 값을 직접 드래그하므로 범위를 신뢰할 수 없다.
	// tan(90도)는 무한대라 원뿔 정점이 NaN이 되므로 여기서 막는다.
	const float SafeOuterAngle = FMath::Clamp(OuterConeAngle, 0.0f, 89.0f);
	const float SafeInnerAngle = FMath::Clamp(InnerConeAngle, 0.0f, SafeOuterAngle);
	const float SafeRadius = (AttenuationRadius > 0.0f) ? AttenuationRadius : 0.0f;

	// 바깥 원뿔은 라이트 색 그대로, 안쪽은 구분되도록 흐리게
	AddWireCone(LineBatcher, Apex, Forward, Right, Up, SafeOuterAngle, SafeRadius, LightColor);

	FVector4 InnerColor(LightColor.X * 0.5f, LightColor.Y * 0.5f, LightColor.Z * 0.5f, LightColor.W);

	AddWireCone(LineBatcher, Apex, Forward, Right, Up, SafeInnerAngle, SafeRadius, InnerColor);
}
