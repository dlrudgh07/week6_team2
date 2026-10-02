#pragma once

#include "Rendering/Vertex.h"
#include "Rendering/StaticMeshResources.h"

class FGeometryGenerator
{
public:
	static TUniquePtr<FStaticMeshRenderData> CreateTextQuad(const FVector& Start, const FVector& End, const FVector4& Color);
	static TUniquePtr<FStaticMeshRenderData> CreateLine(const FVector& Start, const FVector& End, const FVector4& Color);
	static TUniquePtr<FStaticMeshRenderData> CreatePlane(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	static TUniquePtr<FStaticMeshRenderData> CreateAxis();

	static TUniquePtr<FStaticMeshRenderData> CreateCone(float Radius, float Height, int Segments, const FVector4& Color);
	static TUniquePtr<FStaticMeshRenderData> CreateCylinder(float Radius, float Height, int Segments, const FVector4& Color);
	static TUniquePtr<FStaticMeshRenderData> CreateArrow(float BodyRadius, float BodyHeight, float HeadRadius, float HeadHeight, int Segments, const FVector4& Color); // Cylinder + Cone 합성
	static TUniquePtr<FStaticMeshRenderData> CreateScaleBar(float BodyRadius, float BodyLength, float HeadSize, float Segments, const FVector4& Color); // Cylinder + Cone 합성
	static TUniquePtr<FStaticMeshRenderData> CreateRing(float Radius, float TubeRadius, int Segments, int TubeSegments, const FVector4& Color);
	static TUniquePtr<FStaticMeshRenderData> CreateCube(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));

	static TUniquePtr<FStaticMeshRenderData> CreateSphere(float _radius, uint32 _numSlices, uint32 _numStacks, const FVector4& Color);

	static FStaticMeshRenderData* GetMeshData(const FString& InName);

	static void CreateDefaultMeshDatas();

private:
	inline static TMap<FString, TUniquePtr<FStaticMeshRenderData>> MeshDataMap;
};