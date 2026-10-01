#pragma once

#include "Stats.h"

namespace StatIds
{
inline FStatId PickingTotal()
{
	static const FStatId Id = FStats::Register({"Picking", "Total", EStatUnit::Milliseconds, EStatMode::Event, true, true});
	return Id;
}

inline FStatId PickingBroad()
{
	static const FStatId Id = FStats::Register({"Picking", "Broad", EStatUnit::Milliseconds, EStatMode::Event, true, true});
	return Id;
}

inline FStatId PickingNarrow()
{
	static const FStatId Id = FStats::Register({"Picking", "Narrow", EStatUnit::Milliseconds, EStatMode::Event, true, true});
	return Id;
}

inline FStatId OcclusionCaptured()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Captured", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionStatic()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Static", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionDynamic()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Dynamic", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionFrustumRejected()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Frustum Rejected", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionRejected()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Occlusion Rejected", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionVisible()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Visible", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId RenderPackets()
{
	static const FStatId Id = FStats::Register({"Render", "Packets (Active View)", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionOccluders()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Occluders", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionSourceTriangles()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Source Triangles", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}


inline FStatId OcclusionBVHTested()
{
	static const FStatId Id = FStats::Register({"Occlusion", "BVH Tested", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionBVHPruned()
{
	static const FStatId Id = FStats::Register({"Occlusion", "BVH Pruned", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionCullTime()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Cull", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId OcclusionBVHBuildTime()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Last BVH Build", EStatUnit::Milliseconds, EStatMode::Gauge, true});
	return Id;
}
inline FStatId FrameFPS()
{
	static const FStatId Id = FStats::Register({"Frame", "FPS", EStatUnit::Count, EStatMode::Gauge, true, true});
	return Id;
}

inline FStatId FrameTime()
{
	static const FStatId Id = FStats::Register({"Frame", "Frame Time", EStatUnit::Milliseconds, EStatMode::Gauge, true, true});
	return Id;
}

inline FStatId OcclusionTested()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Occlusion Tested", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionTriangleBudget()
{
	static const FStatId Id = FStats::Register({"Occlusion", "Triangle Budget Exceeded (0/1)", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId OcclusionCpuBudget()
{
	static const FStatId Id = FStats::Register({"Occlusion", "CPU Budget Exceeded (0/1)", EStatUnit::Count, EStatMode::Gauge, true});
	return Id;
}

inline FStatId MemoryObjects()
{
	static const FStatId Id = FStats::Register({"Memory", "Object Bytes", EStatUnit::Bytes, EStatMode::Gauge, false});
	return Id;
}

inline FStatId MemoryAllocations()
{
	static const FStatId Id = FStats::Register({"Memory", "Object Allocations", EStatUnit::Count, EStatMode::Gauge, false});
	return Id;
}

inline FStatId MemoryProcess()
{
	static const FStatId Id = FStats::Register({"Memory", "Process Working Set", EStatUnit::Bytes, EStatMode::Gauge, false});
	return Id;
}

inline FStatId CaptureWorld()
{
	static const FStatId Id = FStats::Register({"Scene", "Capture World CPU", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId PacketBuild()
{
	static const FStatId Id = FStats::Register({"Render", "Packet Build CPU (excl. cull)", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId RenderSort()
{
	static const FStatId Id = FStats::Register({"Render", "Sort CPU", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId RenderSubmit()
{
	static const FStatId Id = FStats::Register({"Render", "Opaque Submit CPU (incl. workers)", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId DrawCalls()
{
	static const FStatId Id = FStats::Register({"Render", "Opaque Draw Calls / Frame", EStatUnit::Count, EStatMode::FrameSum, true});
	return Id;
}

inline FStatId Triangles()
{
	static const FStatId Id = FStats::Register({"Render", "Opaque Triangles / Frame", EStatUnit::Count, EStatMode::FrameSum, true});
	return Id;
}

inline FStatId CBUpload()
{
	static const FStatId Id = FStats::Register({"Render", "Object CB Written Bytes / Frame", EStatUnit::Bytes, EStatMode::FrameSum, true});
	return Id;
}

inline FStatId PickCandidates()
{
	static const FStatId Id = FStats::Register({"Picking", "Candidates / Pick", EStatUnit::Count, EStatMode::Event, true, true});
	return Id;
}

inline FStatId GpuReadbackCPU()
{
	static const FStatId Id = FStats::Register({"GPU Culling CPU", "Result Map Wait", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId GpuReadbackFailures()
{
	static const FStatId Id = FStats::Register({"GPU Culling CPU", "Result Map Failures / Frame", EStatUnit::Count, EStatMode::FrameSum, true});
	return Id;
}

inline FStatId GpuFrame()
{
	static const FStatId Id = FStats::Register({"GPU", "Viewport Render (excl. UI/Present)", EStatUnit::Milliseconds, EStatMode::Event, false});
	return Id;
}

inline FStatId GpuOpaque()
{
	static const FStatId Id = FStats::Register({"GPU", "Opaque / Pass", EStatUnit::Milliseconds, EStatMode::Event, false});
	return Id;
}

inline FStatId GpuHZB()
{
	static const FStatId Id = FStats::Register({"GPU", "HZB Build / Pass", EStatUnit::Milliseconds, EStatMode::Event, false});
	return Id;
}

inline FStatId GpuCull()
{
	static const FStatId Id = FStats::Register({"GPU", "Occlusion Dispatch / Pass", EStatUnit::Milliseconds, EStatMode::Event, false});
	return Id;
}

inline FStatId GpuSkipped()
{
	static const FStatId Id = FStats::Register({"GPU", "Profiler Frames Skipped / Frame", EStatUnit::Count, EStatMode::FrameSum, true});
	return Id;
}

inline FStatId RenderMaterials()
{
	static const FStatId Id = FStats::Register({"Render Detail CPU", "Material collect + update", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId RenderUpload()
{
	static const FStatId Id = FStats::Register({"Render Detail CPU", "Immediate bulk CB upload", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId RenderDrawLoop()
{
	static const FStatId Id = FStats::Register({"Render Detail CPU", "Immediate bind + draw (fallback CB included)", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId RenderWorkers()
{
	static const FStatId Id = FStats::Register({"Render Detail CPU", "Worker record + completion wait", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId RenderExecute()
{
	static const FStatId Id = FStats::Register({"Render Detail CPU", "Execute command lists", EStatUnit::Milliseconds, EStatMode::Event, true});
	return Id;
}

inline FStatId GpuGrid()
{
	static const FStatId Id = FStats::Register({"GPU", "Grid / Pass", EStatUnit::Milliseconds, EStatMode::Event, false});
	return Id;
}

inline FStatId GpuEditor()
{
	static const FStatId Id = FStats::Register({"GPU", "Editor overlays / Pass", EStatUnit::Milliseconds, EStatMode::Event, false});
	return Id;
}

// Register before UI iteration; defaults belong to each definition.
inline void RegisterAll()
{
	RenderMaterials();
	RenderUpload();
	RenderDrawLoop();
	RenderWorkers();
	RenderExecute();
	GpuGrid();
	GpuEditor();

	CaptureWorld();
	PacketBuild();
	RenderSort();
	RenderSubmit();
	DrawCalls();
	Triangles();
	CBUpload();
	PickCandidates();
	GpuReadbackCPU();
	GpuReadbackFailures();
	GpuFrame();
	GpuOpaque();
	GpuHZB();
	GpuCull();
	GpuSkipped();

	PickingTotal();
	PickingBroad();
	PickingNarrow();
	OcclusionCaptured();
	OcclusionStatic();
	OcclusionDynamic();
	OcclusionFrustumRejected();
	OcclusionRejected();
	OcclusionVisible();
	RenderPackets();
	OcclusionOccluders();
	OcclusionSourceTriangles();
	OcclusionBVHTested();
	OcclusionBVHPruned();
	OcclusionCullTime();
	OcclusionBVHBuildTime();
	FrameFPS();
	FrameTime();
	OcclusionTested();
	OcclusionTriangleBudget();
	OcclusionCpuBudget();
	MemoryObjects();
	MemoryAllocations();
	MemoryProcess();
}
} // namespace StatIds
