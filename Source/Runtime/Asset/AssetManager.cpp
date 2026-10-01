#include "EnginePCH.h"
#include "AssetManager.h"
#include "Rendering/Buffer.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"

#include "Rendering/Vertex.h"
#include "Rendering/Texture2D.h"
#include "Rendering/RenderCommand.h"

#include "Rendering/RenderResourceManager.h"

#include "Asset/ObjImporter/ObjImporter.h"

#include "Rendering/GeometryGenerator.h"
#include "ObjectSystem/ObjectFactory.h"

#include "Rendering/ImageLoader.h"
#include "Collision/Ray.h"
#include <meshoptimizer.h>


namespace
{
	UStaticMesh* CreateStaticMesh(const FStaticMeshData* MeshData)
	{
		if (!MeshData || !MeshData->Vertices.Num() || !MeshData->Indices.Num())
			return nullptr;

		UStaticMesh* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();

		// Cooked Data 보관
		Mesh->MeshData = *MeshData;

		// Mesh에 MaterialSlot 없을 경우 Default Material 할당
		if (Mesh->MeshData.MaterialSlots.IsEmpty())
		{
			FStaticMaterialSlot DefaultSlot;
			DefaultSlot.Name = "Default";
			Mesh->MeshData.MaterialSlots.Add(DefaultSlot);
		}

		// Mesh에 Section이 없을 경우 전체 메쉬을 섹션 하나로 세팅
		if (Mesh->MeshData.Sections.IsEmpty())
		{
			FStaticMeshSection DefaultSection;
			DefaultSection.StartIndex = 0;
			DefaultSection.IndexCount = static_cast<uint32>(Mesh->MeshData.Indices.Num());
			DefaultSection.MaterialSlotIndex = 0;

			Mesh->MeshData.Sections.Add(DefaultSection);
		}

		const size_t VertexCount = Mesh->MeshData.Vertices.Num();
		const size_t IndexCount = Mesh->MeshData.Indices.Num();

		// 인덱스 버퍼 및 버텍스 캐시 최적화
		for (const FStaticMeshSection& Section : Mesh->MeshData.Sections)
		{
			if (Section.IndexCount == 0)
			{
				continue;
			}

			uint32* SectionIndices = Mesh->MeshData.Indices.GetData() + Section.StartIndex;

			meshopt_optimizeVertexCache(
				SectionIndices,
				SectionIndices,
				Section.IndexCount,
				VertexCount
			);

			meshopt_optimizeOverdraw(
				SectionIndices,
				SectionIndices,
				Section.IndexCount,
				reinterpret_cast<const float*>(&Mesh->MeshData.Vertices[0].Position),
				VertexCount,
				sizeof(FVertexPNCT),
				1.05f
			);
		}

		// 버텍스 버퍼 재정렬 및 인덱스 리매핑
		TArray<FVertexPNCT> OptimizedVertices;
		OptimizedVertices.SetNum(VertexCount);
		meshopt_optimizeVertexFetch(
			OptimizedVertices.GetData(),
			Mesh->MeshData.Indices.GetData(),
			IndexCount,
			Mesh->MeshData.Vertices.GetData(),
			VertexCount,
			sizeof(FVertexPNCT)
		);
		Mesh->MeshData.Vertices = std::move(OptimizedVertices);

		// 피킹 중 최초 구축 비용이 들어가지 않도록 로드 단계에서 Triangle BVH를 준비한다.
		PrepareMeshPickingBVH(Mesh->MeshData);

		// Vertex/Index GPU 업로드
		Mesh->VertexBuffer = RenderCommand::CreateStaticVertexBuffer(
			Mesh->MeshData.Vertices.GetData(),
			sizeof(FVertexPNCT) * static_cast<uint32>(Mesh->MeshData.Vertices.size()),
			sizeof(FVertexPNCT)
		);
		Mesh->IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
			Mesh->MeshData.Indices.GetData(),
			static_cast<uint32>(Mesh->MeshData.Indices.size())
		);

		if (!Mesh->VertexBuffer || !Mesh->IndexBuffer)
		{
			return nullptr;
		}

		Mesh->GenerateLODs();

		UMaterial* DefaultMaterial = UAssetManager::GetAssetByKey<UMaterial>("DefaultMaterial");
		if (!DefaultMaterial)
		{
			return nullptr;
		}

		// MaterialSlots를 실제 UMaterial로 변환
		for (const FStaticMaterialSlot& Slot : Mesh->MeshData.MaterialSlots)
		{
			UMaterial* Material = UMaterial::CreateInstance(DefaultMaterial);
			if (!Material)
			{
				return nullptr;
			}
			Material->BaseColor = Slot.BaseColor;
			if (Material->BaseColor.W < 1.0f)
			{
				Material->PSOType = EPSOType::StaticMesh_Translucent;
			}

			if (!Slot.DiffuseTexturePath.empty())
			{
				UTexture2D* Texture = UAssetManager::Get().LoadTexture(Slot.DiffuseTexturePath);

				if (Texture)
				{
					Material->Textures[0] = Texture;
				}
			}
			Mesh->Materials.Add(Material);
		}
		return Mesh;
	}

	FString MakeAssetKey(const FString& Path)
	{
		return fs::relative(Path, "Assets").generic_string();
	}
}

UAssetManager& UAssetManager::Get()
{
	static UAssetManager* Instance = FObjectFactory::ConstructObject<UAssetManager>();
	return *Instance;
}

void UAssetManager::ScanAssets(const fs::path& AssetRoot, std::function<void(float, const FString&)> OnProgress)
{
	if (!fs::exists(AssetRoot))
	{
		LOG(Error, "Asset root not found: {}", AssetRoot.generic_string());
		return;
	}

	// 애셋 목록 수집
	TArray<fs::path> AssetFiles;
	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(AssetRoot))
	{
		if (!Entry.is_regular_file()) continue;
		AssetFiles.Add(Entry.path());
	}

	const int32 TotalFiles = static_cast<int32>(AssetFiles.Num());
	int32 ProcessedCount = 0;

	// 애셋 순회 로드
	for (const fs::path& FilePath : AssetFiles)
	{
		FString Key = fs::relative(FilePath, AssetRoot).generic_string();
		FString Path = FilePath.generic_string();
		AssetPathMap.Add(Key, Path);
		LoadAsset(Key, Path);

		++ProcessedCount;
		// 진행도 콜백 호출
		if (OnProgress && TotalFiles > 0)
		{
			const float Ratio = static_cast<float>(ProcessedCount) / static_cast<float>(TotalFiles);
			OnProgress(Ratio, Key);
		}
	}
}

void UAssetManager::LoadAsset(const FString& Key, const FString& Path)
{
	FString Extension = fs::path(Path).extension().string();
	if (Extension == ".png")
	{
		if (UTexture2D* Texture = LoadTexture(Path))
		{
			AssetMap[Key] = Texture;
		}
	}
	if (Extension == ".json")
	{
		FString TexturePath = fs::path(Path).replace_extension(".png").string();
		if (UFont* Font = LoadFontAtlas(Path, TexturePath))
		{
			AssetMap[Key] = Font;
		}
	}
	if (Extension == ".obj")
	{
		LoadObjStaticMesh(Path);
	}
}

void UAssetManager::Init(std::function<void(float, const FString&)> OnProgress)
{
	FGeometryGenerator::CreateDefaultMeshDatas();
	// 머티리얼이 참조하므로 반드시 먼저 만든다
	CreateDefaultTextures();
	CreateDefaultMaterial();
	ScanAssets("Assets", OnProgress);
	CreateDefaultMeshes();
	CreateParticleMaterial();
}

void UAssetManager::CreateDefaultTextures()
{
	// 텍스처가 지정되지 않은 머티리얼이 검게 나오지 않도록 하는 1x1 흰색 텍스처.
	// 셰이더에서 곱해도 결과가 변하지 않으므로 "텍스처 없음"의 기본값으로 쓴다.
	const uint32 WhitePixel = 0xFFFFFFFF;

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = 1;
	Desc.Height = 1;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TUniquePtr<FTexture2D> Resource = RenderCommand::CreateTexture2D(Desc, &WhitePixel);
	if (!Resource)
	{
		return;
	}

	UTexture2D* WhiteTexture = FObjectFactory::ConstructObject<UTexture2D>();
	WhiteTexture->SetResource(std::move(Resource));
	WhiteTexture->SetPath("WhiteTexture");

	AssetMap["WhiteTexture"] = WhiteTexture;
}

void UAssetManager::CreateDefaultMeshes()
{
	// 메시 데이터 업로드
	FStaticMeshData* CubeData = FGeometryGenerator::GetMeshData("Cube");
	RegisterAsset("Cube", CreateStaticMesh(CubeData));

	FStaticMeshData* ConeData = FGeometryGenerator::GetMeshData("Cone");
	RegisterAsset("Cone", CreateStaticMesh(ConeData));

	FStaticMeshData* SphereData = FGeometryGenerator::GetMeshData("Sphere");
	RegisterAsset("Sphere", CreateStaticMesh(SphereData));

	FStaticMeshData* CylinderData = FGeometryGenerator::GetMeshData("Cylinder");
	RegisterAsset("Cylinder", CreateStaticMesh(CylinderData));

	FStaticMeshData* PlaneData = FGeometryGenerator::GetMeshData("Plane");
	RegisterAsset("Plane", CreateStaticMesh(PlaneData));

	FStaticMeshData* ArrowMeshData = FGeometryGenerator::GetMeshData("Arrow");
	RegisterAsset("Arrow", CreateStaticMesh(ArrowMeshData));

	FStaticMeshData* RingMeshData = FGeometryGenerator::GetMeshData("Ring");
	RegisterAsset("Ring", CreateStaticMesh(RingMeshData));

	FStaticMeshData* ScaleBarData = FGeometryGenerator::GetMeshData("ScaleBar");
	RegisterAsset("ScaleBar", CreateStaticMesh(ScaleBarData));

	FStaticMeshData* GizmoSphereData = FGeometryGenerator::GetMeshData("GizmoSphere");
	RegisterAsset("GizmoSphere", CreateStaticMesh(GizmoSphereData));

	const FParticleVertex Vertices[] =
	{
		{ FVector(0.0f, -0.5f,  0.5f), FVector2(0.0f, 0.0f) },
		{ FVector(0.0f, 0.5f,  0.5f), FVector2(1.0f, 0.0f) },
		{ FVector(0.0f, 0.5f, -0.5f), FVector2(1.0f, 1.0f) },
		{ FVector(0.0f, -0.5f, -0.5f), FVector2(0.0f, 1.0f) }
	};

	const uint32 Indices[] =
	{
		0, 1, 2,
		0, 2, 3
	};

	UStaticMesh* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();
	Mesh->VertexBuffer = RenderCommand::CreateStaticVertexBuffer(
		Vertices,
		sizeof(Vertices), sizeof(FParticleVertex));
	Mesh->IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
		Indices,
		ARRAYSIZE(Indices));

	FVertexPNCT Vertex{};
	Vertex.Position = FVector(0.0f, -0.5f, 0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, 0.5f, 0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, 0.5f, -0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, -0.5f, -0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);

	Mesh->MeshData.Indices = { 0, 1, 2, 0, 2, 3, 0,2,1, 0,3,2 };

	Mesh->MeshData.AABB.Min = FVector(-0.001f, -0.5, -0.5);
	Mesh->MeshData.AABB.Max = FVector(0.001f, 0.5, 0.5);

	RegisterAsset("ParticleQuad", Mesh);
}

void UAssetManager::CreateDefaultMaterial()
{
	UMaterial* DefaultMat = FObjectFactory::ConstructObject<UMaterial>();
	DefaultMat->PSOType = EPSOType::StaticMesh_Opaque;
	DefaultMat->Textures.Add(GetAssetByKey<UTexture2D>("WhiteTexture"));
	DefaultMat->ParamBuffer = RenderCommand::CreateConstantBuffer(sizeof(FStaticMeshMaterialParams));
	RegisterAsset("DefaultMaterial", DefaultMat);
}

void UAssetManager::CreateParticleMaterial()
{
	UMaterial* ParticleMat = FObjectFactory::ConstructObject<UMaterial>();
	ParticleMat->PSOType = EPSOType::Particle_AlphaBlend;
	ParticleMat->Textures.Add(GetAssetByKey<UTexture2D>("Assets/SubUV/StarParticle.png"));
	ParticleMat->ParamBuffer = RenderCommand::CreateConstantBuffer(256);
	RegisterAsset("SubUVMaterial", ParticleMat);
}

void UAssetManager::Shutdown()
{
	//AssetMap.Empty();
}


void UAssetManager::RegisterAsset(const FString& Key, URenderAsset* Asset)
{
	Asset->SetPath(Key); 
	AssetMap[Key] = Asset;
}

UTexture2D* UAssetManager::LoadTexture(const FString& InPath)
{
	if (URenderAsset** Found = AssetMap.FindOrNull(InPath))
	{
		return Cast<UTexture2D>(*Found);
	}

	FImageData Data = ImageLoader::LoadAuto(InPath);
	if (!Data.IsValid()) return nullptr;

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Data.Width;
	Desc.Height = Data.Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = Data.Format;              // 로더가 정한 포맷 (.hdr이면 float)
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TUniquePtr<FTexture2D> Resource = RenderCommand::CreateTexture2D(Desc, Data);

	if (!Resource) return nullptr;

	UTexture2D* Asset = FObjectFactory::ConstructObject<UTexture2D>();
	Asset->SetResource(std::move(Resource));
	RegisterAsset(InPath, Asset);

	return Asset;
}

UFont* UAssetManager::LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath)
{
	UFont* Font = FObjectFactory::ConstructObject<UFont>();

	if (!Font->LoadFontAtlasJson(JsonPath))
	{
		return nullptr;
	}

	Font->AtlasTexture = LoadTexture(AtlasTexturePath);
	if (!Font->AtlasTexture)
	{
		return nullptr;
	}

	RegisterAsset(JsonPath, Font);
	return Font;
}

UStaticMesh* UAssetManager::LoadObjStaticMesh(const FString& Path)
{
	const FString Key = MakeAssetKey(Path);
	if (UStaticMesh* Cached = GetAssetByKey<UStaticMesh>(Key))
	{
		return Cached;
	}

	EObjAxisPreset Preset = EObjAxisPreset::Default;
	TUniquePtr<FStaticMeshData> Data = FObjImporter::LoadStaticMeshData(Path, Preset);

	UStaticMesh* Mesh = CreateStaticMesh(Data.get());   // Data가 nullptr이면 nullptr 반환
	if (!Mesh)
	{
		return nullptr;
	}

	Mesh->ImportAxisPreset = Preset;
	Mesh->AppliedAxisPreset = Preset;

	Get().RegisterAsset(Key, Mesh);
	return Mesh;
}

bool UAssetManager::ReimportStaticMesh(UStaticMesh* Mesh)
{
	if (!Mesh)
		return false;

	const FString Key = Mesh->GetPath();

	FString* Path = Get().AssetPathMap.FindOrNull(Key);
	if (!Path)
		return false;

	std::filesystem::remove(*Path + ".bin");

	TUniquePtr<FStaticMeshData> Data = FObjImporter::LoadStaticMeshData(*Path, Mesh->ImportAxisPreset);

	if (!Data)
	{
		return false;
	}

	return Mesh->RebuildFromMeshData(std::move(*Data));
}