#pragma once

// 시연 PC에서 운영체제의 절전 정책이 성능을 깎지 않도록 실행 중에만 성능 모드를 켠다.
//
// 설정은 영향 범위에 따라 두 종류로 나뉜다.
//  - 시스템 전역 설정 (전원 계획, 전원 모드 오버레이)
//      프로세스가 끝나도 남는다. 그래서 생성자에서 원래 값을 저장하고 소멸자에서 되돌린다. (RAII)
//  - 프로세스 한정 설정 (EcoQoS 해제, 우선순위)
//      프로세스가 끝나면 운영체제가 알아서 버린다. 되돌릴 필요가 없다.
//
// 주의: 크래시나 디버거 강제 종료로 소멸자가 불리지 않으면 전원 계획이 복구되지 않는다.
//       그래서 EntryPoint에서는 Release 계열 빌드에서만 사용한다.

#include <Windows.h>
#include <powrprof.h>
#pragma comment(lib, "PowrProf.lib")

class FPerformanceModeGuard
{
public:
	FPerformanceModeGuard()
	{
		ApplyPowerPlan();
		ApplyProcessSettings();
	}

	~FPerformanceModeGuard()
	{
		RestorePowerPlan();
	}

	FPerformanceModeGuard(const FPerformanceModeGuard&) = delete;
	FPerformanceModeGuard& operator=(const FPerformanceModeGuard&) = delete;

private:
	// Windows 기본 "고성능" 전원 계획 (powercfg /list 에 나오는 GUID)
	static constexpr GUID HighPerformanceScheme =
		{ 0x8c5e7fda, 0xe8bf, 0x4a96, { 0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c } };

	// 설정 > 시스템 > 전원 > 전원 모드 "최고 성능" 오버레이.
	// 최신 노트북(Modern Standby)은 "고성능" 계획이 없고 "균형 조정" 계획 위에 이 오버레이를 얹는 방식이다.
	static constexpr GUID BestPerformanceOverlay =
		{ 0xded574b5, 0x45a0, 0x4f42, { 0x87, 0x37, 0x46, 0x34, 0x5c, 0x09, 0xc2, 0x38 } };

	// 오버레이 API는 powrprof.dll 이 export 하지만 헤더에 공개되지 않아 런타임에 주소를 찾는다.
	using FnGetOverlay = DWORD(WINAPI*)(GUID*);
	using FnSetOverlay = DWORD(WINAPI*)(GUID);

	GUID OriginalScheme{};
	GUID OriginalOverlay{};
	bool bSchemeChanged = false;
	bool bOverlayChanged = false;
	FnSetOverlay SetOverlay = nullptr;

	void ApplyPowerPlan()
	{
		// 1) 현재 전원 계획 저장
		GUID* Active = nullptr;
		if (PowerGetActiveScheme(nullptr, &Active) != ERROR_SUCCESS || Active == nullptr)
		{
			LOG(Warning, "PerformanceMode: 현재 전원 계획을 읽지 못해 전원 설정을 건너뜁니다.");
			return;
		}
		OriginalScheme = *Active;
		LocalFree(Active);

		if (IsEqualGUID(OriginalScheme, HighPerformanceScheme))
		{
			LOG(Info, "PerformanceMode: 이미 고성능 전원 계획입니다.");
			return;
		}

		// 2) 고성능 계획으로 전환 시도
		const DWORD SchemeResult = PowerSetActiveScheme(nullptr, &HighPerformanceScheme);
		if (SchemeResult == ERROR_SUCCESS)
		{
			bSchemeChanged = true;
			LOG(Info, "PerformanceMode: 전원 계획을 고성능으로 전환했습니다. (종료 시 복구)");
			return;
		}

		// 3) 고성능 계획이 없는 노트북이면 전원 모드 오버레이를 "최고 성능"으로 올린다.
		LOG(Info, "PerformanceMode: 고성능 계획 전환 실패(0x{:08X}), 전원 모드 오버레이로 대체합니다.", SchemeResult);
		ApplyOverlay();
	}

	void ApplyOverlay()
	{
		HMODULE PowrProf = GetModuleHandleW(L"powrprof.dll");
		if (PowrProf == nullptr)
		{
			PowrProf = LoadLibraryW(L"powrprof.dll");
		}
		if (PowrProf == nullptr)
		{
			return;
		}

		const auto GetOverlay = reinterpret_cast<FnGetOverlay>(GetProcAddress(PowrProf, "PowerGetEffectiveOverlayScheme"));
		SetOverlay = reinterpret_cast<FnSetOverlay>(GetProcAddress(PowrProf, "PowerSetActiveOverlayScheme"));
		if (GetOverlay == nullptr || SetOverlay == nullptr)
		{
			LOG(Warning, "PerformanceMode: 이 Windows 버전은 전원 모드 오버레이를 지원하지 않습니다.");
			return;
		}

		if (GetOverlay(&OriginalOverlay) != ERROR_SUCCESS)
		{
			return;
		}
		if (IsEqualGUID(OriginalOverlay, BestPerformanceOverlay))
		{
			LOG(Info, "PerformanceMode: 이미 전원 모드가 최고 성능입니다.");
			return;
		}

		if (SetOverlay(BestPerformanceOverlay) == ERROR_SUCCESS)
		{
			bOverlayChanged = true;
			LOG(Info, "PerformanceMode: 전원 모드를 최고 성능으로 전환했습니다. (종료 시 복구)");
		}
		else
		{
			LOG(Warning, "PerformanceMode: 전원 모드 전환에 실패했습니다.");
		}
	}

	void ApplyProcessSettings()
	{
		// EcoQoS 해제: Windows가 이 프로세스를 저클럭·저효율 스케줄링으로 보내지 못하게 한다.
		// ControlMask = "이 정책을 내가 직접 정하겠다", StateMask = 0 = "절전 끔"
		PROCESS_POWER_THROTTLING_STATE Throttling{};
		Throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
		Throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
		Throttling.StateMask = 0;
		if (!SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &Throttling, sizeof(Throttling)))
		{
			LOG(Warning, "PerformanceMode: EcoQoS 해제 실패 ({})", GetLastError());
		}

		// 다른 프로세스와의 CPU 경쟁에서 우선. REALTIME은 입력·드라이버 스레드까지 굶기므로 쓰지 않는다.
		if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
		{
			LOG(Warning, "PerformanceMode: 프로세스 우선순위 상향 실패 ({})", GetLastError());
		}
	}

	void RestorePowerPlan()
	{
		// 시스템 전역 설정만 되돌린다. 바꾼 적 없는 값은 건드리지 않는다.
		if (bSchemeChanged)
		{
			PowerSetActiveScheme(nullptr, &OriginalScheme);
		}
		if (bOverlayChanged && SetOverlay != nullptr)
		{
			SetOverlay(OriginalOverlay);
		}
	}
};
