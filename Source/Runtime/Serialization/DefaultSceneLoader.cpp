#include "EnginePCH.h"
#include "DefaultSceneLoader.h"

#include <fstream>
#include <string>
#include <sstream>

#include "World/World.h"
#include "World/Level.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "Component/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Asset/AssetManager.h"
#include "Rendering/Mesh.h"
#include "Core/EngineLog.h"

bool FDefaultSceneLoader::LoadScene(UWorld* World, const FString& Path, std::function<void(float)> OnProgress)
{
	if (!World) return false;

	std::ifstream File(Path, std::ios::binary);
	if (!File.is_open())
	{
		LOG(Warning, "Failed to open scene file: {}", Path);
		return false;
	}

	File.seekg(0, std::ios::end);
	const size_t FileSize = static_cast<size_t>(File.tellg());
	File.seekg(0, std::ios::beg);

	LOG(Info, "Loading Default.scene (Single-Pass)...");

	FString Line;
	bool bInPrimitives = false;
	bool bInCamera = false;

	uint32 CurrentUUID = 0;

	FVector Loc(0.0f, 0.0f, 0.0f);
	FRotator Rot(0.0f, 0.0f, 0.0f);
	FVector Scale(1.0f, 1.0f, 1.0f);

	bool bHasObject = false;
	FString MeshAssetPath = "";

	TMap<FString, UStaticMesh*> MeshCache;

	int32 SpawnedCount = 0;

	// 매 줄 읽기
	while (std::getline(File, Line))
	{
		// PerspectiveCamera 섹션 시작 발견
		if (!bInPrimitives && Line.find("\"PerspectiveCamera\"") != FString::npos)
		{
			bInCamera = true;
			continue;
		}

		// Primitives 섹션 시작 발견
		if (Line.find("\"Primitives\"") != FString::npos)
		{
			bInCamera = false;
			bInPrimitives = true;
			continue;
		}

		// 카메라 데이터 파싱
		if (bInCamera && World->GetMainCamera())
		{
			if (Line.find("\"Location\"") != FString::npos)
			{
				float X = 0.0f, Y = 0.0f, Z = 0.0f;
				if (const char* Bracket = strchr(Line.c_str(), '['))
				{
					if (sscanf(Bracket + 1, "%f , %f , %f", &X, &Y, &Z) >= 3 ||
						sscanf(Bracket + 1, "%f, %f, %f", &X, &Y, &Z) >= 3)
					{
						FTransform CamTransform = World->GetMainCamera()->GetActorTransform();
						CamTransform.Location = FVector(X, Y, Z);
						if (World->GetMainCamera()->GetRootComponent())
							World->GetMainCamera()->GetRootComponent()->SetTransform(CamTransform);
					}
				}
			}
			else if (Line.find("\"Rotation\"") != FString::npos)
			{
				float R0 = 0.0f, R1 = 0.0f, R2 = 0.0f;
				if (const char* Bracket = strchr(Line.c_str(), '['))
				{
					if (sscanf(Bracket + 1, "%f , %f , %f", &R0, &R1, &R2) >= 3 ||
						sscanf(Bracket + 1, "%f, %f, %f", &R0, &R1, &R2) >= 3)
					{
						float Deg0 = R0 * 180.0f / 3.14159265f;
						float Deg1 = R1 * 180.0f / 3.14159265f;
						float Deg2 = R2 * 180.0f / 3.14159265f;
						FTransform CamTransform = World->GetMainCamera()->GetActorTransform();
						CamTransform.Rotation = FRotator(Deg1, Deg2, Deg0);
						if (World->GetMainCamera()->GetRootComponent())
							World->GetMainCamera()->GetRootComponent()->SetTransform(CamTransform);
					}
				}
			}
			else if (Line.find("\"FOV\"") != FString::npos)
			{
				float Fov = 60.0f;
				if (const char* Bracket = strchr(Line.c_str(), '['))
					if (sscanf(Bracket + 1, "%f", &Fov) >= 1 && World->GetMainCamera()->GetCameraComponent())
						World->GetMainCamera()->GetCameraComponent()->SetFieldOfView(Fov);
			}
			else if (Line.find("\"NearClip\"") != FString::npos)
			{
				float NearZ = 0.1f;
				if (const char* Bracket = strchr(Line.c_str(), '['))
					if (sscanf(Bracket + 1, "%f", &NearZ) >= 1 && World->GetMainCamera()->GetCameraComponent())
						World->GetMainCamera()->GetCameraComponent()->SetNearZ(NearZ);
			}
			else if (Line.find("\"FarClip\"") != FString::npos)
			{
				float FarZ = 100.0f;
				if (const char* Bracket = strchr(Line.c_str(), '['))
					if (sscanf(Bracket + 1, "%f", &FarZ) >= 1 && World->GetMainCamera()->GetCameraComponent())
						World->GetMainCamera()->GetCameraComponent()->SetFarZ(FarZ);
			}
			continue;
		}

		// Primitives 섹션 파싱
		if (bInPrimitives)
		{
			// 새로운 UUID 항목 시작 확인
			if (Line.find('{') != FString::npos)
			{
				// 숫자 UUID 추출
				size_t Q1 = Line.find('\"');
				if (Q1 != FString::npos)
				{
					size_t Q2 = Line.find('\"', Q1 + 1);
					if (Q2 != FString::npos)
					{
						FString Key = Line.substr(Q1 + 1, Q2 - Q1 - 1);
						char* EndPtr = nullptr;
						uint32 Uid = static_cast<uint32>(strtoul(Key.c_str(), &EndPtr, 10));
						if (Uid > 0 && EndPtr == Key.c_str() + Key.length())
						{
							CurrentUUID = Uid;
							Loc = FVector(0.0f, 0.0f, 0.0f);
							Rot = FRotator(0.0f, 0.0f, 0.0f);
							Scale = FVector(1.0f, 1.0f, 1.0f);

							bHasObject = true;
							MeshAssetPath = "";
						}
					}
				}
				continue;
			}

			if (bHasObject)
			{
				if (Line.find("\"Location\"") != FString::npos)
				{
					if (const char* Bracket = strchr(Line.c_str(), '['))
					{
						float X = 0.0f, Y = 0.0f, Z = 0.0f;
						if (sscanf(Bracket + 1, "%f , %f , %f", &X, &Y, &Z) >= 3 ||
							sscanf(Bracket + 1, "%f, %f, %f", &X, &Y, &Z) >= 3)
						{
							Loc = FVector(X, Y, Z);
						}
					}
				}
				else if (Line.find("\"Rotation\"") != FString::npos)
				{
					if (const char* Bracket = strchr(Line.c_str(), '['))
					{
						float R0 = 0.0f, R1 = 0.0f, R2 = 0.0f;
						if (sscanf(Bracket + 1, "%f , %f , %f", &R0, &R1, &R2) >= 3 ||
							sscanf(Bracket + 1, "%f, %f, %f", &R0, &R1, &R2) >= 3)
						{
							Rot = FRotator(R0, R1, R2);
						}
					}
				}
				else if (Line.find("\"Scale\"") != FString::npos)
				{
					if (const char* Bracket = strchr(Line.c_str(), '['))
					{
						float SX = 1.0f, SY = 1.0f, SZ = 1.0f;
						if (sscanf(Bracket + 1, "%f , %f , %f", &SX, &SY, &SZ) >= 3 ||
							sscanf(Bracket + 1, "%f, %f, %f", &SX, &SY, &SZ) >= 3)
						{
							Scale = FVector(SX, SY, SZ);
						}
					}
				}
				else if (Line.find("\"ObjStaticMeshAsset\"") != FString::npos)
				{
					size_t Colon = Line.find(':');
					size_t Q1 = Line.find('"', Colon);
					size_t Q2 = Line.find('"', Q1 + 1);

					MeshAssetPath = Line.substr(Q1 + 1, Q2 - Q1 - 1);
				}
				else if (Line.find('}') != FString::npos)
				{
					// 오브젝트 파싱 완료 -> 즉시 스폰
					FTransform SpawnTransform;
					SpawnTransform.Location = Loc;
					SpawnTransform.Rotation = Rot;
					SpawnTransform.Scale = Scale;

					AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(NAME_None, &SpawnTransform);
					if (Actor)
					{
						Actor->SetUUID(CurrentUUID);
						if (UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent())
						{
							Comp->SetUUID(CurrentUUID);

							FString FileName = fs::path(MeshAssetPath).filename().generic_string();
							UStaticMesh* Mesh = nullptr;
							if (UStaticMesh** CachedMesh = MeshCache.FindOrNull(FileName))
							{
								Mesh = *CachedMesh;
							}
							else
							{
								Mesh = UAssetManager::GetAssetByFileName<UStaticMesh>(FileName);

								MeshCache.Add(FileName, Mesh);
							}

							if (Mesh)
							{
								Comp->SetStaticMesh(Mesh);
							}
							else
							{
								LOG(Warning, "Mesh asset not found: {}", MeshAssetPath);
							}
							Comp->MarkBoundsDirty();
							FBox Box = Comp->CalcBounds();
						}
						++SpawnedCount;
						// 진행도 콜백 호출
						if (OnProgress && (SpawnedCount % 5 == 0))
						{
							const float StreamPos = static_cast<float>(File.tellg());
							const float Ratio = FileSize > 0 ? (StreamPos / static_cast<float>(FileSize)) : 0.0f;
							OnProgress(Ratio);
						}
					}

					bHasObject = false;
				}
			}
		}
	}

	LOG(Info, "Default.scene Single-Pass Loading Completed: {} actors spawned", SpawnedCount);
	return true;
}

