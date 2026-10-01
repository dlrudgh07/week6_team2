#pragma once

#include "FiberTypes.h"
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <algorithm>

class FFiberJobManager
{
public:
	static FFiberJobManager& Get();

	void Initialize(uint32_t InNumWorkers = 0, uint32_t InNumFibers = 128, uint32_t InFiberStackSize = 64 * 1024);
	void Shutdown();

	uint32_t GetNumWorkers() const { return NumWorkers; }

	void RunJob(const FFiberJob& InJob);
	void RunJobs(const FFiberJob* InJobs, uint32_t InNumJobs, FFiberCounter* InCounter = nullptr);
	void WaitForCounter(FFiberCounter* InCounter, int32_t InTargetValue = 0);

	template<typename FuncType>
	void ParallelFor(int32_t TotalCount, int32_t ChunkSize, const FuncType& Function)
	{
		if (TotalCount <= 0) return;
		if (ChunkSize <= 0) ChunkSize = 1;

		int32_t NumJobs = (TotalCount + ChunkSize - 1) / ChunkSize;
		if (NumJobs <= 1)
		{
			Function(0, TotalCount);
			return;
		}

		struct FParallelTask
		{
			const FuncType* Func;
			int32_t Start;
			int32_t End;

			static void Execute(void* InData)
			{
				FParallelTask* Task = static_cast<FParallelTask*>(InData);
				const FuncType& Lambda = *Task->Func;
				Lambda(Task->Start, Task->End);
			}
		};

		std::vector<FParallelTask> Tasks(NumJobs);
		std::vector<FFiberJob> Jobs(NumJobs);
		FFiberCounter Counter;

		for (int32_t Index = 0; Index < NumJobs; ++Index)
		{
			int32_t Start = Index * ChunkSize;
			int32_t End = (std::min)(Start + ChunkSize, TotalCount);

			Tasks[Index] = { &Function, Start, End };
			Jobs[Index] = { &FParallelTask::Execute, &Tasks[Index], &Counter };
		}

		RunJobs(Jobs.data(), NumJobs, &Counter);
		WaitForCounter(&Counter, 0);
	}



	struct FFiberTaskContext
	{
		void* FiberHandle = nullptr;
		FFiberJob CurrentJob;
		FFiberJobManager* Manager = nullptr;
		std::atomic<bool> bIsSuspended{ true };

		FFiberCounter* WaitingOnCounter = nullptr;
		int32_t WaitingTargetValue = 0;
	};

	void FiberWorkerLoop(FFiberTaskContext* Context);

	void OnJobCompleted(FFiberCounter* Counter);
	void SuspendAndSwitch(FFiberTaskContext* WaitingFiber);
	void YieldCurrentFiber(FFiberTaskContext* Context);
	bool ExecuteOneJobOrReadyFiber();

private:
	FFiberJobManager() = default;
	~FFiberJobManager();

	FFiberTaskContext* CreateJobFiber();

private:
	std::atomic<bool> bIsRunning{ false };
	uint32_t NumWorkers = 0;
	uint32_t NumFibers = 0;
	uint32_t FiberStackSize = 0;

	std::vector<std::thread> Workers;
	std::vector<FFiberTaskContext*> AllocatedFibers;
	std::vector<FFiberTaskContext*> FreeFiberPool;
	std::deque<FFiberTaskContext*> SuspendFibers;

	std::queue<FFiberJob> JobQueue;
	std::mutex SchedulerMutex;
	std::condition_variable WakeCondition;
};