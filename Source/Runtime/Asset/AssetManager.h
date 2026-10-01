#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Property.h"
#include "Rendering/Renderer.h"
#include "Rendering/Shader.h"

#include <filesystem>
#include <functional>

enum class EAssetType
{
};

class UStaticMesh;
class FShader;

namespace fs = std::filesystem;

class UAssetManager : public UObject
{
	DECLARE_CLASS(UAssetManager, UObject)

private:
	UAssetManager() = default;

	UAssetManager(const UAssetManager& src) =  delete;
	UAssetManager& operator= (const UAssetManager& src) = delete;

public:
	static UAssetManager& Get();

	void ScanAssets(const fs::path& AssetRoot, std::function<void(float, const FString&)> OnProgress = nullptr);
	void LoadAsset(const FString& Key, const FString& Path);

	void Init(std::function<void(float, const FString&)> OnProgress = nullptr);
	void CreateDefaultTextures();
	void CreateDefaultMeshes();
	void CreateDefaultMaterial();
	void CreateParticleMaterial();
	void Shutdown();

	template <typename T> static T* GetAssetByKey(const FString& Key)
	{
		URenderAsset** Found = Get().AssetMap.FindOrNull(Key);

		if (Found == nullptr || *Found == nullptr || !(*Found)->IsA<T>())
		{
			return nullptr;
		}
		return Cast<T>(*Found);
	}

	template <typename T> static T* GetAssetByFileName(const FString& FileName)
	{
		T* Result = nullptr;

		for (auto& [Key, Asset] : Get().AssetMap)
		{
			if (fs::path(Key).filename().generic_string() != FileName) continue;
			if (!Asset || !Asset->IsA<T>()) continue;

			// 파일명 중복시 nullptr
			if (Result) return nullptr;
			Result = Cast<T>(Asset);
		}
		return Result;
	}

	void RegisterAsset(const FString& Key, URenderAsset* Asset);

	UTexture2D* LoadTexture(const FString& InPath);
	UFont* LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath);
	static UStaticMesh* LoadObjStaticMesh(const FString& Path);
	static bool ReimportStaticMesh(UStaticMesh* Mesh);

private:
	// Assets 폴더 기준 상대 경로(Key) → 실제 파일 경로
	TMap<FString, FString> AssetPathMap;

	// Assets 폴더 기준 상대 경로(Key) → 로드된 에셋 객체
	TMap<FString, URenderAsset*> AssetMap;

	TMap<FString, TUniquePtr<FShader>> ShaderMap;
};