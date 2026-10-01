#pragma once

class UWorld;

class FJsonArchive
{
public:
	static bool SaveWorld(UWorld* World, const FString& Path);
	static bool LoadWorld(UWorld* World, const FString& Path);
};