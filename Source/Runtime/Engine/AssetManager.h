// 에셋을 관리하는 클래스 추후 경로 기준을 게임 프로젝트 내 에셋 폴더 기준으로 변경해야 함
#pragma once

#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "Rendering/Renderer.h"
#include "RHI/RHIShader.h"

#include <filesystem>
#include <functional>

class UStaticMesh;
class FRHIShader;

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
	void CreateBillboardMaterial();
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

	TMap<FString, TUniquePtr<FRHIShader>> ShaderMap;
};
