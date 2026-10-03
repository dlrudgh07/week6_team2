#pragma once

#include "Engine/World.h"

struct FWorldContext
{
	UWorld* World = nullptr;
	EWorldType WorldType = EWorldType::Editor;
};
