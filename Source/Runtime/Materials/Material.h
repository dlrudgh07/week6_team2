#pragma once

#include "MaterialInterface.h"
#include "Rendering/RenderStates.h"
#include "Rendering/Texture2D.h"
#include "Rendering/RenderResourceManager.h"


struct FStaticMeshMaterialParams
{
	FVector4 BaseColor;
	FVector2 UVOffset;
	float bOpaque; // 불투명 여부 플래그
	float Padding;
};

class UMaterial : public UMaterialInterface
{
	DECLARE_CLASS(UMaterial, UMaterialInterface)

public:
	UMaterial() = default;
	virtual ~UMaterial() override = default;

	EPSOType PSOType = EPSOType::StaticMesh_Opaque;
	TArray<UTexture2D*> Textures;
	TUniquePtr<FConstantBuffer> ParamBuffer;
	ESamplerState SamplerState = ESamplerState::LinearClamp;

	FVector4 BaseColor = FVector4(1, 1, 1, 1);
	FVector2 UVScrollSpeed = FVector2(0.0f, 0.0f);

	bool bIsInstance = false;

	// 인스턴스가 복제된 원본 머티리얼
	const UMaterial* Parent = nullptr;

	// 원본 머티리얼 복제본 생성
	static UMaterial* CreateInstance(const UMaterial* Source);

	// 최상위 에셋 머티리얼 반환
	const UMaterial* GetBaseAsset() const;

	static json SaveMaterial(const UMaterial* Material);
	static UMaterial* LoadMaterial(const json& In);
};