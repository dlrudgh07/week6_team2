#pragma once

class UPrimitiveComponent;

struct FHitResult
{
	UPrimitiveComponent* HitComponent = nullptr;
	float Distance = FLT_MAX;
	FVector ImpactPoint = FVector();
};