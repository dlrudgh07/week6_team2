#include "EnginePCH.h"
#include "Rendering/Mesh.h"
#include "ObjViewerApp.h"

#include "Asset/AssetManager.h"
#include "Input/InputSystem.h"
#include "Rendering/RenderCommand.h"
#include "Rendering/RenderResourceManager.h"

#include "Core/Application.h"

#include <commdlg.h>
#include <filesystem>

namespace
{
	// Window
	constexpr int32 WindowWidth = 1280;
	constexpr int32 WindowHeight = 720;
	constexpr const wchar_t* WindowTitle = L"Obj Viewer";
	constexpr const char* WindowTitleWithHint = "Obj Viewer - Ctrl+O: Open, F: Frame";

	// Asset
	constexpr const char* DefaultMeshPath = "Assets/Models/Rover/Rover.obj";

	// Camera Projection
	constexpr float CameraFovDegrees = 60.0f;
	constexpr float CameraNearZ = 0.1f;
	constexpr float CameraFarZ = 1000.0f;

	// Camera Control
	constexpr float OrbitSensitivity = 0.3f;        // 픽셀당 도
	constexpr float MaxPitch = 89.0f;               // 90°면 Forward와 WorldUp이 평행 → Cross가 0
	constexpr float ZoomStep = 0.9f;                // 휠 한 칸당 거리 배율
	constexpr float WheelDeltaPerNotch = 120.0f;    // Win32 WHEEL_DELTA
	constexpr float MinCameraDistance = 0.01f;
	constexpr float MaxCameraDistance = CameraFarZ * 0.5f;

	// CameraComponent와 같은 원근 행렬 (Forward = X, Right = Y, Up = Z)
	FMatrix MakePerspective(float FovDegrees, float Aspect, float NearZ, float FarZ)
	{
		const float YScale = 1.0f / tan(FMath::DegreesToRadians(FovDegrees) * 0.5f);
		const float XScale = YScale / Aspect;
		const float ZScale = FarZ / (FarZ - NearZ);
		const float ZOffset = -NearZ * FarZ / (FarZ - NearZ);

		return FMatrix(
			0.0f, 0.0f, ZScale, 1.0f,
			XScale, 0.0f, 0.0f, 0.0f,
			0.0f, YScale, 0.0f, 0.0f,
			0.0f, 0.0f, ZOffset, 0.0f);
	}

	// 역회전 행렬의 열이 Forward·Right·Up이 되도록 View 행렬을 만든다.
	FMatrix MakeLookAt(const FVector& Eye, const FVector& Target)
	{
		const FVector WorldUp(0.0f, 0.0f, 1.0f);
		const FVector F = (Target - Eye).Normalized();
		const FVector R = FVector::Cross(WorldUp, F).Normalized();
		const FVector U = FVector::Cross(F, R);

		const FMatrix Rotation(
			F.X, R.X, U.X, 0.0f,
			F.Y, R.Y, U.Y, 0.0f,
			F.Z, R.Z, U.Z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);

		return FMatrix::MakeTranslation(Eye * -1.0f) * Rotation;
	}

	// 선택한 .obj 경로를 반환한다. 취소하면 빈 문자열.
	FString OpenObjFileDialog(HWND Owner)
	{
		char FilePath[MAX_PATH] = {};

		OPENFILENAMEA Ofn = {};
		Ofn.lStructSize = sizeof(OPENFILENAMEA);
		Ofn.hwndOwner = Owner;
		Ofn.lpstrFile = FilePath;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrFilter = "OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
		Ofn.lpstrInitialDir = "Assets\\Models";
		// NOCHANGEDIR: 작업 디렉터리가 바뀌면 Assets·Resources 상대 경로를 찾지 못한다.
		Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

		return GetOpenFileNameA(&Ofn) ? FString(FilePath) : FString();
	}
}

bool FObjViewerApp::Init(HINSTANCE hInstance)
{
	RenderDevice = MakeUnique<FRenderDevice>();
	RenderCommand::Init(RenderDevice.get());

	Renderer = MakeUnique<FRenderer>();
	Renderer->Init();

	MainWindow = MakeUnique<FWindow>();
	if (!MainWindow->Create(hInstance, WindowWidth, WindowHeight, WindowTitle))
	{
		return false;
	}

	Swapchain = MakeUnique<FSwapchain>(RenderDevice.get(), MainWindow.get());
	CreateDepthBuffer(MainWindow->GetWidth(), MainWindow->GetHeight());

	FRenderResourceManager::Init();
	UAssetManager::Get().Init();
	if (!LoadMesh(DefaultMeshPath))
	{
		// 기본 메쉬가 없어도 단축키 안내는 보여준다.
		UpdateWindowTitle();
	}

	bIsRunning = true;
	return true;
}

void FObjViewerApp::Run()
{
	while (bIsRunning)
	{
		FInputSystem::UpdateInputStates();
		MainWindow->ProcessMessage(bIsRunning);
		if (!bIsRunning)
		{
			break;
		}

		HandleResize();

		// 최소화 중에는 그릴 대상이 없다.
		if (MainWindow->GetWidth() == 0 || MainWindow->GetHeight() == 0)
		{
			continue;
		}

		HandleShortcuts();
		UpdateCamera();
		RenderFrame();
	}
}

// Init의 역순으로 GPU 자원을 해제하고 Device를 마지막에 정리한다.
void FObjViewerApp::Shutdown()
{
	Mesh = nullptr;
	UAssetManager::Get().Shutdown();
	FRenderResourceManager::Shutdown();

	DepthBuffer.reset();
	Swapchain.reset();
	Renderer.reset();

	RenderDevice->Shutdown();
}

void FObjViewerApp::HandleResize()
{
	if (!MainWindow->CheckResized())
	{
		return;
	}

	const uint32 Width = MainWindow->GetWidth();
	const uint32 Height = MainWindow->GetHeight();
	if (Width == 0 || Height == 0)
	{
		return;
	}

	Swapchain->Resize(Width, Height);
	CreateDepthBuffer(Width, Height);
}

void FObjViewerApp::HandleShortcuts()
{
	if (FInputSystem::IsKeyDown(EKeyCode::Control) && FInputSystem::IsKeyPressed(EKeyCode::O))
	{
		// 모달 대화상자: 닫힐 때까지 루프가 멈춘다. 눌린 키 상태는 WM_KILLFOCUS에서 초기화된다.
		const FString Path = OpenObjFileDialog(MainWindow->GetHandle());
		if (!Path.empty() && !LoadMesh(Path))
		{
			MessageBoxA(MainWindow->GetHandle(), Path.c_str(), "Failed to load OBJ", MB_OK | MB_ICONERROR);
		}
	}

	if (FInputSystem::IsKeyPressed(EKeyCode::F))
	{
		FitCameraToMesh();
	}
}

void FObjViewerApp::UpdateCamera()
{
	if (FInputSystem::IsMouseDown(EMouseButton::Right))
	{
		CameraYaw += FInputSystem::GetMouseDeltaX() * OrbitSensitivity;
		CameraPitch += FInputSystem::GetMouseDeltaY() * OrbitSensitivity;
		CameraPitch = FMath::Clamp(CameraPitch, -MaxPitch, MaxPitch);
	}

	// GetWheelDelta는 읽으면 0으로 초기화되므로 프레임당 한 번만 호출한다.
	const int32 Wheel = FInputSystem::GetWheelDelta();
	if (Wheel != 0)
	{
		// 곱셈 줌: 가까울수록 천천히 다가간다.
		CameraDistance *= powf(ZoomStep, Wheel / WheelDeltaPerNotch);
		CameraDistance = FMath::Clamp(CameraDistance, MinCameraDistance, MaxCameraDistance);
	}
}

void FObjViewerApp::RenderFrame()
{
	const float Aspect = static_cast<float>(MainWindow->GetWidth()) / static_cast<float>(MainWindow->GetHeight());
	const FMatrix View = MakeLookAt(GetCameraEye(), CameraTarget);
	const FMatrix Projection = MakePerspective(CameraFovDegrees, Aspect, CameraNearZ, CameraFarZ);
	const FMatrix ViewProjection = View * Projection;

	BuildRenderPackets(RenderPackets, ViewProjection);

	FRenderingInfo Info = Swapchain->GetRenderingInfo();
	Info.DepthSteincil.Texture = DepthBuffer.get();

	RenderCommand::BeginRenderPass(Info);

	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer->RenderAll(RenderPackets, ViewProjection);

	RenderCommand::EndRenderPass(Info);

	Swapchain->SwapBuffers();
}

// 로드에 실패하면 기존 메쉬를 유지한다.
bool FObjViewerApp::LoadMesh(const FString& Path)
{
	UStaticMesh* NewMesh = UAssetManager::LoadObjStaticMesh(Path);
	if (!NewMesh)
	{
		return false;
	}

	Mesh = NewMesh;
	CurrentMeshPath = Path;

	CameraYaw = 0.0f;
	CameraPitch = 0.0f;
	FitCameraToMesh();

	UpdateWindowTitle();
	return true;
}

// 창 제목에 파일명·정점·삼각형 수와 단축키 안내를 표시한다.
void FObjViewerApp::UpdateWindowTitle()
{
	FString Title = WindowTitleWithHint;
	if (Mesh)
	{
		const FStaticMeshData& Data = Mesh->GetMeshData();
		Title = std::filesystem::path(CurrentMeshPath).filename().string()
			+ " (" + std::to_string(Data.Vertices.Num()) + " verts, "
			+ std::to_string(Data.Indices.Num() / 3) + " tris) - " + Title;
	}

	SetWindowTextA(MainWindow->GetHandle(), Title.c_str());
}

void FObjViewerApp::BuildRenderPackets(TArray<FRenderPacket>& OutPackets, const FMatrix& ViewProjection) const
{
	OutPackets.Reset();
	if (!Mesh)
	{
		return;
	}

	for (const FStaticMeshSection& Section : Mesh->GetMeshData().Sections)
	{
		FRenderPacket Packet;
		Packet.mesh = Mesh;
		Packet.model = FMatrix::Identity;
		Packet.MVP = Packet.model * ViewProjection;
		Packet.material = Mesh->GetMaterial(Section.MaterialSlotIndex);
		Packet.StartIndex = Section.StartIndex;
		Packet.IndexCount = Section.IndexCount;
		OutPackets.Add(Packet);
	}
}

void FObjViewerApp::CreateDepthBuffer(uint32 Width, uint32 Height)
{
	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;   // 깊이 24bit + 스텐실 8bit
	Desc.SampleDesc.Count = 1;                     // 백버퍼와 동일해야 함 (MSAA 없음)
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;     // FTexture2D가 DSV를 만들어 준다

	DepthBuffer = RenderCommand::CreateTexture2D(Desc);
}

// 메쉬의 경계 구가 시야에 모두 들어오도록 Target과 Distance를 정한다.
void FObjViewerApp::FitCameraToMesh()
{
	if (!Mesh)
	{
		return;
	}

	const FBox& AABB = Mesh->GetMeshData().AABB;
	CameraTarget = (AABB.Min + AABB.Max) * 0.5f;

	const float Radius = (AABB.Max - AABB.Min).Length() * 0.5f;
	const float HalfFov = FMath::DegreesToRadians(CameraFovDegrees) * 0.5f;
	CameraDistance = FMath::Clamp(Radius / sinf(HalfFov), MinCameraDistance, MaxCameraDistance);
}

// Yaw·Pitch로 바라보는 방향을 구하고 Target에서 Distance만큼 뒤로 물린다.
FVector FObjViewerApp::GetCameraEye() const
{
	const float YawRad = FMath::DegreesToRadians(CameraYaw);
	const float PitchRad = FMath::DegreesToRadians(CameraPitch);

	// Pitch > 0 이면 위에서 아래를 내려다본다.
	const FVector Forward(
		cosf(PitchRad) * cosf(YawRad),
		cosf(PitchRad) * sinf(YawRad),
		-sinf(PitchRad));

	return CameraTarget - Forward * CameraDistance;
}
