#pragma once

#include <Windows.h>
#include <functional>


// UI 레이어가 창 메시지를 먼저 가로챌 수 있게 하는 훅.
// 런타임이 ImGui를 직접 알지 않도록 방향을 뒤집는다.
// true를 반환하면 그 메시지는 소비된 것으로 보고 더 처리하지 않는다.
using FWndProcHook = bool (*)(HWND, UINT, WPARAM, LPARAM);

class FWindow
{
public:
	// 훅은 등록한 쪽이 수명을 책임진다. 해제는 nullptr을 넘긴다.
	static void SetWndProcHook(FWndProcHook Hook);

	bool Create(HINSTANCE hInstance, int Width, int Height, const wchar_t* Title, bool bShowImmediately = true);
	void Show();
	void ProcessMessage(bool& bIsRunning);

	HWND GetHandle() const { return hWnd;  }

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }

	LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	bool CheckResized();

private:
	bool bIsResized = false;
	bool bIsInSizeMove = false;

	HWND hWnd = nullptr;

	uint32 Width = 1280;
	uint32 Height = 720;
 };

