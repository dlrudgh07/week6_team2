#pragma once

// Move Update 처리하는 방법
enum class ETeleportType : uint8
{
	None,            // 일반 이동
	TeleportPhysics, // 순간 이동 (속도 유지)
	ResetPhysics,    // 순간 이동 (물리 상태 초기화)
};
