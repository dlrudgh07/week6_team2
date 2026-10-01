#pragma once

#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"

struct FVertex
{
	FVector Position = FVector();
	FVector2 UV = FVector2();
	FVector4 Color = FVector4();
};

struct FTextVertex
{
	FVector Position;
	FVector2 UV;
};

// Todo: subuv
struct FParticleVertex
{
	FVector Position;
	FVector2 UV;
};

// StaticMesh용 PNCT Vertex: Position / Normal / Color / Texcoord(UV)
struct FVertexPNCT
{
	FVector Position = FVector();
	FVector Normal = FVector();
	FVector4 Color = FVector4();
	FVector2 UV = FVector2();
};
