#pragma once

#include "Core/Types.h"

enum class EObjectFlags : uint32
{
	RF_NoFlags = 0x00000000,	
	RF_Transient = 0x00000040,	
};

DEFINE_ENUM_OPERATORS(EObjectFlags)