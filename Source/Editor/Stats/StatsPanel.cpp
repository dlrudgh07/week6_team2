#include "EnginePCH.h"
#include "Editor/Stats/StatsPanel.h"
#include <algorithm>
#include <vector>
#include <format>
#include <cmath>
#include "Core/StatDefinitions.h"
#include "Rendering/GPUProfiler.h"
#include "Tasks/TaskScheduler.h"

namespace
{
	void DrawNote(const char* Text)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.80f, 1.0f));
		ImGui::TextWrapped("%s", Text);
		ImGui::PopStyleColor();
	}

	bool IsSimilarCost(double A, double B)
	{
		return std::abs(A - B) <= (std::max)(0.05, 0.1 * (std::max)(A, B));
	}
}

void FStatsPanel::DrawCostBreakdown(size_t GroupIndex, const char* Title, const std::vector<FCostRow>& Rows)
{
	FCostGroup& Group = CostGroups[GroupIndex];
	const double Now = ImGui::GetTime();
	const bool bStartFresh = Group.Costs.size() != Rows.size()
		|| (Group.LastObserved >= 0.0 && Now - Group.LastObserved > 1.0);
	if (bStartFresh)
	{
		Group = {};
		Group.Costs.resize(Rows.size());
		for (size_t Index = 0; Index < Rows.size(); ++Index)
		{
			Group.Order.push_back(Index);
			const FStatRecord& Record = FStats::GetRecord(Rows[Index].Id);
			Group.Costs[Index].LastCount = Record.SampleCount;
			Group.Costs[Index].LastTotal = Record.TotalValue;
		}
	}
	Group.LastObserved = Now;
	for (size_t Index = 0; Index < Rows.size(); ++Index)
	{
		FRecentCost& Cost = Group.Costs[Index];
		const FStatRecord& Record = FStats::GetRecord(Rows[Index].Id);
		if (!FStats::IsEnabled(Rows[Index].Id) || Record.SampleCount < Cost.LastCount)
		{
			Cost = {};
			Cost.LastCount = Record.SampleCount;
			Cost.LastTotal = Record.TotalValue;
		}
		// Include all new calls, including multiple views and delayed GPU results.
		if (Record.SampleCount > Cost.LastCount)
		{
			const double Total = Record.TotalValue - Cost.LastTotal;
			const uint64 Count = Record.SampleCount - Cost.LastCount;
			Cost.Samples.push_back({Now, Total, Count});
			Cost.Total += Total;
			Cost.Count += Count;
			Cost.LastCount = Record.SampleCount;
			Cost.LastTotal = Record.TotalValue;
		}
		while (!Cost.Samples.empty() && Now - Cost.Samples.front().Time >= 1.0)
		{
			Cost.Total -= Cost.Samples.front().Total;
			Cost.Count -= Cost.Samples.front().Count;
			Cost.Samples.pop_front();
		}
	}
	if (Group.LastRefresh < 0.0 || Now - Group.LastRefresh >= 0.5)
	{
		Group.LastRefresh = Now;
		for (FRecentCost& Cost : Group.Costs)
			Cost.DisplayAverage = Cost.Count > 0 ? (std::max)(0.0, Cost.Total / Cost.Count) : -1.0;

		// Hysteresis is not a strict ordering. Move adjacent rows instead of using std::sort.
		for (size_t Index = 1; Index < Group.Order.size(); ++Index)
		{
			for (size_t Position = Index; Position > 0; --Position)
			{
				const double Left = Group.Costs[Group.Order[Position - 1]].DisplayAverage;
				const double Right = Group.Costs[Group.Order[Position]].DisplayAverage;
				if (Right < 0.0 || (Left >= 0.0 && (Right <= Left || IsSimilarCost(Left, Right))))
					break;
				std::swap(Group.Order[Position - 1], Group.Order[Position]);
			}
		}
	}

	double Largest = -1.0;
	int RecentCount = 0;
	for (const FRecentCost& Cost : Group.Costs)
	{
		Largest = (std::max)(Largest, Cost.DisplayAverage);
		RecentCount += Cost.DisplayAverage >= 0.0 ? 1 : 0;
	}
	ImGui::Dummy(ImVec2(0.0f, 10.0f));
	ImGui::SeparatorText(Title);
	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	if (Largest > 0.0)
	{
		ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "TOP CANDIDATES (1s avg)");
		for (const size_t Index : Group.Order)
		{
			const double Average = Group.Costs[Index].DisplayAverage;
			if (Average < 0.0 || !IsSimilarCost(Largest, Average))
				continue;
			ImGui::TextWrapped("%s  |  %.3f ms", FStats::GetRecord(Rows[Index].Id).Desc.Name.c_str(), Average);
			DrawNote(Rows[Index].Hint);
		}
	}
	else
		DrawNote(RecentCount > 0 ? "Measured cost: 0 ms" : "Waiting for recent samples");
	if (static_cast<size_t>(RecentCount) < Rows.size())
		ImGui::TextDisabled("Measured stages: %d / %d", RecentCount, static_cast<int>(Rows.size()));
	ImGui::Dummy(ImVec2(0.0f, 4.0f));
	ImGui::PushID(Title);
	if (ImGui::TreeNode("Stage details"))
	{
		ImGui::Dummy(ImVec2(0.0f, 8.0f));
		for (const size_t Index : Group.Order)
		{
			const FCostRow& Row = Rows[Index];
			const double Average = Group.Costs[Index].DisplayAverage;
			const FStatRecord& Record = FStats::GetRecord(Row.Id);
			ImGui::TextUnformatted(Record.Desc.Name.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", Row.Hint);
			if (Average < 0.0)
				ImGui::TextDisabled(Record.bEnabled ? "No recent sample" : "Off");
			else
			{
				const FString Label = std::format("{:.3f} ms (1s avg)", Average);
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetStyleColorVec4(
					IsSimilarCost(Largest, Average) ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
				ImGui::ProgressBar(Largest > 0.0 ? static_cast<float>(Average / Largest) : 0.0f,
					ImVec2(-1.0f, 0.0f), Label.c_str());
				ImGui::PopStyleColor();
			}
			ImGui::Dummy(ImVec2(0.0f, 7.0f));
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

bool FStatsPanel::Init()
{
	SetOpen(IsOpen());
	return true;
}

void FStatsPanel::SetOpen(bool bOpen)
{
	IEditorPanel::SetOpen(bOpen);
	FStats::SetDetailedCollectionEnabled(bOpen);
	if (!bOpen)
		CostGroups = {};
}

void FStatsPanel::Tick(float DeltaTime)
{
}

void FStatsPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(700, 650), ImGuiCond_FirstUseEver);
	bool bOpen = IsOpen();
	const bool bVisible = ImGui::Begin("Stats", &bOpen);
	if (!bOpen)
		SetOpen(false);
	if (!bVisible || !bOpen)
	{
		ImGui::End();
		return;
	}
	ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "PERFORMANCE OVERVIEW");
	ImGui::Dummy(ImVec2(0.0f, 8.0f));
	if (ImGui::Button("Enable GPU breakdown"))
	{
		for (const FStatId Id : {StatIds::GpuFrame(), StatIds::GpuOpaque(), StatIds::GpuHZB(),
			StatIds::GpuCull(), StatIds::GpuGrid(), StatIds::GpuEditor()})
			FStats::SetEnabled(Id, true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset samples"))
	{
		// UI runs after EndFrame: discard outstanding GPU samples from the old capture.
		FGPUProfiler::Get().Shutdown();
		FStats::ResetSamples();
		CostGroups = {};
	}
	DrawCostBreakdown(0, "CPU scene", {
		{StatIds::CaptureWorld(), "Next: object count / repeated scene collection"},
		{StatIds::OcclusionCullTime(), "Next: culling mode / Result Map Wait"},
		{StatIds::PacketBuild(), "Next: visible packets / matrix construction"},
		{StatIds::RenderSubmit(), "Next: Opaque CPU breakdown below"}
	});
	DrawCostBreakdown(1, "Opaque CPU", {
		{StatIds::RenderSort(), "Next: packet count / repeated sorting"},
		{StatIds::RenderMaterials(), "Next: material count / repeated updates"},
		{StatIds::RenderUpload(), "Next: CB written bytes / Map calls"},
		{StatIds::RenderDrawLoop(), "Next: draw calls / repeated binding"},
		{StatIds::RenderWorkers(), "Next: worker load balance / recording cost"},
		{StatIds::RenderExecute(), "Next: command list count / submission cost"}
	});
	DrawCostBreakdown(2, "GPU passes", {
		{StatIds::GpuOpaque(), "Next: resolution / LOD comparison"},
		{StatIds::GpuHZB(), "Next: GPU culling on/off comparison"},
		{StatIds::GpuCull(), "Next: tested objects / GPU culling on/off"},
		{StatIds::GpuGrid(), "Next: grid on/off comparison"},
		{StatIds::GpuEditor(), "Next: bounds / outline / gizmo on/off"}
	});

	// 스레드별 작업 통계 출력
	std::vector<Tasks::FThreadExecutionStats> ThreadStats;
	Tasks::FTaskScheduler::Get().GetThreadStats(ThreadStats);
	if (!ThreadStats.empty())
	{
		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		ImGui::SeparatorText("Worker Thread Activity");
		ImGui::Dummy(ImVec2(0.0f, 4.0f));
		if (ImGui::BeginTable("##ThreadActivity", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Thread", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn("Tasks Executed");
			ImGui::TableSetupColumn("Busy Time");
			ImGui::TableHeadersRow();

			for (const auto& Stat : ThreadStats)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (Stat.WorkerIndex < 0)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Main Thread");
				}
				else
				{
					ImGui::Text("Worker #%d", Stat.WorkerIndex);
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%u", Stat.TasksExecuted);

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.3f ms", Stat.BusyMs);
			}
			ImGui::EndTable();
		}
	}

	const TArray<FStatRecord>& Records = FStats::GetRecords();

	FString CurrentGroup;
	TArray<FString> DrawnGroups;
	ImGui::Dummy(ImVec2(0.0f, 12.0f));
	ImGui::SeparatorText("Detailed statistics");
	ImGui::Dummy(ImVec2(0.0f, 8.0f));

	for (const FStatRecord& Record : Records)
	{
		if (std::find(DrawnGroups.begin(), DrawnGroups.end(), Record.Desc.Group) != DrawnGroups.end())
			continue;

		CurrentGroup = Record.Desc.Group;
		DrawnGroups.Add(CurrentGroup);
		ImGui::Dummy(ImVec2(0.0f, 6.0f));

		if (!ImGui::CollapsingHeader(CurrentGroup.c_str()))
		{
			continue;
		}

		const FString TableId = "##" + CurrentGroup;

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 6.0f));
		if (ImGui::BeginTable(TableId.c_str(), 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.8f);
			ImGui::TableSetupColumn("Current / Last");
			ImGui::TableSetupColumn("Avg");
			ImGui::TableSetupColumn("Max");
			ImGui::TableSetupColumn("Count");
			ImGui::TableHeadersRow();

			for (int32 Index = 0; Index < Records.Num(); ++Index)
			{
				const FStatRecord& GroupRecord = Records[Index];
				if (GroupRecord.Desc.Group != CurrentGroup)
					continue;

				const double Average = GroupRecord.SampleCount > 0 ? GroupRecord.TotalValue / static_cast<double>(GroupRecord.SampleCount) : 0.0;

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::PushID(Index);
				bool bEnabled = GroupRecord.bEnabled;
				// Match the existing pink button theme without changing other panels.
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(
					bEnabled ? ImGuiCol_Button : ImGuiCol_FrameBg));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
				ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				if (ImGui::Checkbox("##enabled", &bEnabled))
					FStats::SetEnabled(static_cast<FStatId>(Index), bEnabled);
				ImGui::PopStyleColor(4);
				ImGui::SameLine();
				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextUnformatted(GroupRecord.Desc.Name.c_str());
				ImGui::PopTextWrapPos();
				ImGui::PopID();
				if (!bEnabled)
				{
					ImGui::TableSetColumnIndex(1);
					ImGui::TextDisabled("Off");
					continue;
				}

				ImGui::TableSetColumnIndex(1);

				switch (GroupRecord.Desc.Unit)
				{
				case EStatUnit::Milliseconds:
					ImGui::Text("%.3f ms", GroupRecord.CurrentValue);
					break;

				case EStatUnit::Count:
					ImGui::Text("%.0f", GroupRecord.CurrentValue);
					break;

				case EStatUnit::Bytes:
					ImGui::Text("%.0f B", GroupRecord.CurrentValue);
					break;
				}

				if (GroupRecord.Desc.Mode == EStatMode::Event)
				{
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%.3f", Average);

					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%.3f", GroupRecord.MaxValue);

					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%llu", static_cast<unsigned long long>(GroupRecord.SampleCount));
				}
				else
				{
					for (int Column = 2; Column <= 4; ++Column)
					{
						ImGui::TableSetColumnIndex(Column);
						ImGui::TextDisabled("-");
					}
				}
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();
	}
	ImGui::End();
}
