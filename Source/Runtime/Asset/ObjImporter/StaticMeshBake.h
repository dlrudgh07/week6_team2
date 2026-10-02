// Cook이 끝난 FStaticMeshRenderData를 .bin으로 저장하고 읽는 클래스
#pragma once

#include "HAL/Platform.h"
#include "Containers/UnrealString.h"
#include "Asset/ObjImporter/ObjImportSettings.h"

struct FStaticMeshRenderData;

class FStaticMeshBake
{
public:
	static TUniquePtr<FStaticMeshRenderData> ReadBaked(const FString& BinPath, EObjAxisPreset& OutPreset);
	static void WriteBaked(const FString& BinPath, const FStaticMeshRenderData& Data, EObjAxisPreset Preset);
};
