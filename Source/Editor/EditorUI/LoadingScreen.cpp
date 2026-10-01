#include "EnginePCH.h"
#include "LoadingScreen.h"
#include "Asset/AssetManager.h"
#include "Rendering/Texture2D.h"
#include "Input/InputSystem.h"
#include "ThirdParty/ImGui/imgui.h"
#include <chrono>

FLoadingScreen::FLoadingScreen() = default;
FLoadingScreen::~FLoadingScreen() = default;

bool FLoadingScreen::Init()
{
	SpriteTexture = UAssetManager::Get().LoadTexture("Assets/LoadingSprite.png");
	CurrentProgress = 0.0f;
	TargetProgress = 0.0f;
	ElapsedTime = 0.0f;
	CompletionHoldTimer = 0.0f;
	CurrentFrameIndex = 0;
	bIsFinished = false;
	bSceneLoaded = false;
	CurrentStatusText = "Initializing engine subsystems...";
	return SpriteTexture != nullptr;
}

void FLoadingScreen::SetProgress(float InProgress)
{
	TargetProgress = (std::min)(1.0f, (std::max)(TargetProgress, InProgress));
	CurrentProgress = TargetProgress;
	if (InProgress >= 1.0f)
	{
		bSceneLoaded = true;
	}
}

void FLoadingScreen::SetSceneLoaded(bool bLoaded)
{
	bSceneLoaded = bLoaded;
	if (bLoaded)
	{
		TargetProgress = 1.0f;
	}
}

void FLoadingScreen::SetStatusText(const FString& InText)
{
	CurrentStatusText = InText;
}

void FLoadingScreen::Tick(float DeltaTime)
{
	if (bIsFinished)
		return;

	if (DeltaTime <= 0.0f)
	{
		static auto PrevTime = std::chrono::steady_clock::now();
		auto CurrTime = std::chrono::steady_clock::now();
		DeltaTime = std::chrono::duration<float>(CurrTime - PrevTime).count();
		PrevTime = CurrTime;
		if (DeltaTime > 0.1f)
		{
			DeltaTime = 0.033f;
		}
	}

	ElapsedTime += DeltaTime;

	// 진행도 갱신
	const float ApproachRate = (bSceneLoaded ? 0.9f : 0.5f);
	CurrentProgress = (std::min)(1.0f, (std::max)(CurrentProgress, (std::min)(TargetProgress, CurrentProgress + DeltaTime * ApproachRate)));

	// 진행도 연동 애니메이션 갱신
	const float AnimationCycles = 0.7f;
	CurrentFrameIndex = (static_cast<int32>(CurrentProgress * static_cast<float>(TotalFrames) * AnimationCycles) % TotalFrames + TotalFrames) % TotalFrames;

	// 상태 텍스트 갱신
	if (CurrentProgress >= 1.0f)
	{
		CurrentStatusText = "Ready";
	}
	else if (CurrentStatusText.empty())
	{
		CurrentStatusText = "Loading assets...";
	}

	// 건너뛰기 입력 확인
	if (FInputSystem::IsKeyPressed(EKeyCode::Space) || ImGui::IsMouseClicked(0))
	{
		if (bSceneLoaded)
		{
			CurrentProgress = 1.0f;
			bIsFinished = true;
			return;
		}
	}

	// 로딩 완료 판정
	if (bSceneLoaded && CurrentProgress >= 1.0f)
	{
		// 완료 화면 잠시 유지
		CompletionHoldTimer += DeltaTime;
		if (CompletionHoldTimer >= 0.35f)
		{
			bIsFinished = true;
		}
	}
}

void FLoadingScreen::Draw()
{
	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(Viewport->WorkPos);
	ImGui::SetNextWindowSize(Viewport->WorkSize);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("##LoadingScreen", nullptr, WindowFlags))
	{
		ImGui::PushFont(nullptr, 20.0f);

		const float ViewportWidth = Viewport->WorkSize.x;
		const float ViewportHeight = Viewport->WorkSize.y;

		const float SpriteWidth = 256.0f;
		const float SpriteHeight = 256.0f;
		const float BarWidth = 480.0f;
		const float BarHeight = 16.0f;

		// 중앙 배치를 위한 세로 위치 계산
		const float TextHeight = ImGui::GetTextLineHeight();
		const float TotalContentHeight = SpriteHeight + 20.0f + TextHeight + 16.0f + BarHeight + 12.0f + TextHeight;
		float CursorY = (ViewportHeight - TotalContentHeight) * 0.5f;
		if (CursorY < 20.0f) CursorY = 20.0f;

		// 스프라이트 렌더링
		if (SpriteTexture && SpriteTexture->GetResource())
		{
			ID3D11ShaderResourceView* SRV = SpriteTexture->GetResource()->GetSRV();
			if (SRV)
			{
				// 경계 분할점 정의
				static constexpr float ColCuts[] = { 0.0f, 251.0f, 501.0f, 759.0f, 1003.0f, 1267.0f, 1536.0f };
				static constexpr float RowCuts[] = { 0.0f, 274.0f, 523.0f, 774.0f, 1024.0f };

				// 행과 열 기반 좌표 계산
				const int32 Col = CurrentFrameIndex % FrameColumns;
				const int32 Row = CurrentFrameIndex / FrameColumns;
				const float U0 = ColCuts[Col] / 1536.0f;
				const float U1 = ColCuts[Col + 1] / 1536.0f;
				const float V0 = RowCuts[Row] / 1024.0f;
				const float V1 = RowCuts[Row + 1] / 1024.0f;

				const float SpriteX = (ViewportWidth - SpriteWidth) * 0.5f;
				ImGui::SetCursorPos(ImVec2(SpriteX, CursorY));
				ImGui::Image(reinterpret_cast<ImTextureID>(SRV), ImVec2(SpriteWidth, SpriteHeight), ImVec2(U0, V0), ImVec2(U1, V1));
			}
		}

		CursorY += SpriteHeight + 20.0f;

		// 타이틀 텍스트
		const char* TitleText = "H I T O R I   E N G I N E";
		const ImVec2 TitleSize = ImGui::CalcTextSize(TitleText);
		ImGui::SetCursorPos(ImVec2((ViewportWidth - TitleSize.x) * 0.5f, CursorY));
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.65f, 1.0f), "%s", TitleText);

		CursorY += TitleSize.y + 16.0f;

		// 프로그레스 바
		const float BarX = (ViewportWidth - BarWidth) * 0.5f;
		ImGui::SetCursorPos(ImVec2(BarX, CursorY));
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.45f, 0.65f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
		ImGui::ProgressBar(CurrentProgress, ImVec2(BarWidth, BarHeight), "");
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);

		CursorY += BarHeight + 12.0f;

		// 상태 텍스트 및 퍼센트
		char PercentBuffer[32];
		snprintf(PercentBuffer, sizeof(PercentBuffer), "%d%%", static_cast<int>(CurrentProgress * 100.0f));
		const ImVec2 StatusSize = ImGui::CalcTextSize(CurrentStatusText.c_str());
		const ImVec2 PercentSize = ImGui::CalcTextSize(PercentBuffer);

		ImGui::SetCursorPos(ImVec2(BarX, CursorY));
		ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.78f, 1.0f), "%s", CurrentStatusText.c_str());

		ImGui::SetCursorPos(ImVec2(BarX + BarWidth - PercentSize.x, CursorY));
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", PercentBuffer);

		ImGui::PopFont();
	}
	ImGui::End();

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(1);
}
