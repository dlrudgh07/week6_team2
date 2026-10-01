#include "EnginePCH.h"

#include "Window.h"
#include "../resource.h"
#include "Input/InputSystem.h"

namespace
{
	// 등록 전에는 nullptr이며, 그 동안의 메시지는 창이 직접 처리한다.
	FWndProcHook GWndProcHook = nullptr;
}

void FWindow::SetWndProcHook(FWndProcHook Hook)
{
	GWndProcHook = Hook;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (GWndProcHook && GWndProcHook(hWnd, msg, wParam, lParam))
		return true;

	FWindow* window = nullptr;

	if (msg == WM_NCCREATE)
	{
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		window = reinterpret_cast<FWindow*>(cs->lpCreateParams);

		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	}
	else
	{
		window = reinterpret_cast<FWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}

	if (window)
		return window->HandleMessage(hWnd, msg, wParam, lParam);

	return DefWindowProc(hWnd, msg, wParam, lParam);   // return 0 대신
}

bool FWindow::Create(HINSTANCE hInstance, int InWidth, int InHeight, const wchar_t* Title, bool bShowImmediately)
{
	Width = InWidth;
	Height = InHeight;
	const wchar_t CLASS_NAME[] = L"EngineWindowClass";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2));
	// 검은색 배경 브러시 지정
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	RegisterClassW(&wc);

	DWORD style = WS_OVERLAPPEDWINDOW;

	// 원하는 클라이언트 크기 반영
	RECT rc = { 0, 0, Width, Height };
	AdjustWindowRect(&rc, style, FALSE);
	int WindowWidth = rc.right - rc.left;
	int WindowHeight = rc.bottom - rc.top;

	hWnd = CreateWindowEx(
		0, CLASS_NAME, Title,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight,
		nullptr, nullptr, hInstance, this);

	if (hWnd == nullptr)
		return false;

	// 즉시 표시 설정 확인
	if (bShowImmediately)
	{
		ShowWindow(hWnd, SW_SHOW);
	}

	return true;
}

void FWindow::Show()
{
	// 창 출력 및 화면 갱신
	if (hWnd)
	{
		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);
	}
}

void FWindow::ProcessMessage(bool& bIsRunning)
{
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) { bIsRunning = false; }
	}
}

bool FWindow::CheckResized()
{
	// WM_SIZE/WM_EXITSIZEMOVE 전달 여부에만 의존하지 않고 실제 Client 크기를
	// 비교한다. ImGui platform window 처리나 대화형 크기 조절 중 메시지가
	// 합쳐져도 다음 프레임에 반드시 Swapchain 크기를 갱신한다.
	if (hWnd)
	{
		RECT ClientRect{};
		if (GetClientRect(hWnd, &ClientRect))
		{
			const uint32 ClientWidth = static_cast<uint32>(
				(std::max)(0L, ClientRect.right - ClientRect.left));
			const uint32 ClientHeight = static_cast<uint32>(
				(std::max)(0L, ClientRect.bottom - ClientRect.top));
			if (Width != ClientWidth || Height != ClientHeight)
			{
				Width = ClientWidth;
				Height = ClientHeight;
				bIsResized = true;
			}
		}
	}

	const bool bResult = bIsResized;
	bIsResized = false;
	return bResult;
}

LRESULT FWindow::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		FInputSystem::OnMouseDown(EMouseButton::Left);
		break;
	case WM_LBUTTONUP:
		FInputSystem::OnMouseUp(EMouseButton::Left);
		break;
	case WM_RBUTTONDOWN:
		FInputSystem::OnMouseDown(EMouseButton::Right);
		break;
	case WM_RBUTTONUP:
		FInputSystem::OnMouseUp(EMouseButton::Right);
		break;
	case WM_MBUTTONDOWN:
		FInputSystem::OnMouseDown(EMouseButton::Middle);
		break;
	case WM_MBUTTONUP:
		FInputSystem::OnMouseUp(EMouseButton::Middle);
		break;
	case WM_XBUTTONDOWN:
	{
		int Button = GET_XBUTTON_WPARAM(wParam); // XBUTTON1 또는 XBUTTON2 매크로
		if (Button == XBUTTON1)
			FInputSystem::OnMouseDown(EMouseButton::Side1);
		else if (Button == XBUTTON2)
			FInputSystem::OnMouseDown(EMouseButton::Side2);
		break;
	}

	case WM_XBUTTONUP:
	{
		int Button = GET_XBUTTON_WPARAM(wParam); // XBUTTON1 또는 XBUTTON2 매크로
		if (Button == XBUTTON1)
			FInputSystem::OnMouseUp(EMouseButton::Side1);
		else if (Button == XBUTTON2)
			FInputSystem::OnMouseUp(EMouseButton::Side2);
		break;
	}

	case WM_MOUSEMOVE:
	{
		int MouseX = (int)(short)LOWORD(lParam);
		int MouseY = (int)(short)HIWORD(lParam);
		FInputSystem::OnMouseMove(MouseX, MouseY);
	}
	break;

	case WM_MOUSEWHEEL:
		FInputSystem::OnMouseWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam));
		break;

	case WM_KEYDOWN:
		FInputSystem::OnKeyDown(wParam);
		break;

	case WM_KEYUP:
		FInputSystem::OnKeyUp(wParam);
		break;

	// 포커스를 잃으면 이후의 KeyUp/ButtonUp 메시지가 이 창으로 오지 않으므로
	// 눌린 상태가 그대로 남는다. 여기서 전부 비워준다.
	case WM_KILLFOCUS:
		FInputSystem::ClearAllStates();
		break;

	case WM_ACTIVATEAPP:
		if (wParam == FALSE)
			FInputSystem::ClearAllStates();
		break;

	// 포커스가 없는 동안 커서가 이동했어도 델타가 튀지 않도록 현재 위치로 맞춰준다.
	case WM_SETFOCUS:
	{
		POINT Cursor;
		if (GetCursorPos(&Cursor) && ScreenToClient(hWnd, &Cursor))
			FInputSystem::SyncMousePosition(Cursor.x, Cursor.y);
		break;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_SIZE:
		Width = LOWORD(lParam);
		Height = HIWORD(lParam);
		if (wParam != SIZE_MINIMIZED)
			bIsResized = true;
		break;

	case WM_ENTERSIZEMOVE:
		bIsInSizeMove = true;
		break;
	case WM_EXITSIZEMOVE:
	{
		RECT rc;
		GetClientRect(hWnd, &rc);
		Width = rc.right - rc.left;
		Height = rc.bottom - rc.top;

		bIsResized = true;
		bIsInSizeMove = false;
		break;
	}
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}
