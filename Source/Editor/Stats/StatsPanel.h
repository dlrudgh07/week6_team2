#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Core/Stats.h"
#include <array>
#include <deque>
#include <vector>

class FStatsPanel : public IEditorPanel
{
  public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	void SetOpen(bool bOpen) override;

	const char* GetPanelName() const override
	{
		return "Stats";
	}

  private:
	struct FCostRow
	{
		FStatId Id;
		const char* Hint;
	};
	struct FRecentSample
	{
		double Time;
		double Total;
		uint64 Count;
	};
	struct FRecentCost
	{
		std::deque<FRecentSample> Samples;
		double LastTotal = 0.0;
		uint64 LastCount = 0;
		double Total = 0.0;
		uint64 Count = 0;
		double DisplayAverage = -1.0;
	};
	struct FCostGroup
	{
		std::vector<FRecentCost> Costs;
		std::vector<size_t> Order;
		double LastRefresh = -1.0;
		double LastObserved = -1.0;
	};
	std::array<FCostGroup, 3> CostGroups;
	void DrawCostBreakdown(size_t GroupIndex, const char* Title, const std::vector<FCostRow>& Rows);
};
