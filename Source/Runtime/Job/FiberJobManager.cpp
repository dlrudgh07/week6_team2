#include "EnginePCH.h"
#include "FiberJobManager.h"
#include <immintrin.h>
#include <windows.h>

static thread_local FFiberJobManager::FFiberTaskContext
    *TLS_CurrentFiberContext = nullptr;
static thread_local void *TLS_ThreadWorkerFiber = nullptr;

static VOID CALLBACK FiberEntryPoint(PVOID lpParameter) {
  FFiberJobManager::FFiberTaskContext *Context =
      static_cast<FFiberJobManager::FFiberTaskContext *>(lpParameter);
  if (Context && Context->Manager) {
    Context->Manager->FiberWorkerLoop(Context);
  }
}

FFiberJobManager &FFiberJobManager::Get() {
  static FFiberJobManager Instance;
  return Instance;
}

FFiberJobManager::~FFiberJobManager() { Shutdown(); }

void FFiberJobManager::Initialize(uint32_t InNumWorkers, uint32_t InNumFibers,
                                  uint32_t InFiberStackSize) {
  if (bIsRunning.load()) {
    return;
  }

  bIsRunning.store(true);
  FiberStackSize = InFiberStackSize;

  if (InNumWorkers == 0) {
    uint32_t HardwareThreads = std::thread::hardware_concurrency();
    if (HardwareThreads == 0) {
      HardwareThreads = 4;
    }
    NumWorkers = (HardwareThreads > 1) ? (HardwareThreads - 1) : 1;
  } else {
    NumWorkers = InNumWorkers;
  }

  NumFibers = (std::max)(InNumFibers, NumWorkers * 8);

  if (!IsThreadAFiber()) {
    TLS_ThreadWorkerFiber = ConvertThreadToFiber(nullptr);
  } else {
    TLS_ThreadWorkerFiber = GetCurrentFiber();
  }

  {
    std::lock_guard<std::mutex> Lock(SchedulerMutex);
    FreeFiberPool.reserve(NumFibers);
    AllocatedFibers.reserve(NumFibers);
    for (uint32_t Index = 0; Index < NumFibers; ++Index) {
      FFiberTaskContext *Context = CreateJobFiber();
      if (Context) {
        FreeFiberPool.push_back(Context);
        AllocatedFibers.push_back(Context);
      }
    }
  }

  Workers.reserve(NumWorkers);
  for (uint32_t Index = 0; Index < NumWorkers; ++Index) {

    Workers.emplace_back([this]() {
      if (!IsThreadAFiber()) {
        TLS_ThreadWorkerFiber = ConvertThreadToFiber(nullptr);
      } else {
        TLS_ThreadWorkerFiber = GetCurrentFiber();
      }

      while (bIsRunning.load(std::memory_order_relaxed)) {
        FFiberTaskContext *FiberToRun = nullptr;
        {
          std::unique_lock<std::mutex> Lock(SchedulerMutex);
          WakeCondition.wait(Lock, [this]() {
            return !SuspendFibers.empty() || !JobQueue.empty() ||
                   !bIsRunning.load(std::memory_order_relaxed);
          });

          if (!bIsRunning.load(std::memory_order_relaxed) &&
              SuspendFibers.empty() && JobQueue.empty()) {
            break;
          }

          if (!SuspendFibers.empty()) {
            FiberToRun = SuspendFibers.front();
            SuspendFibers.pop_front();
          } else if (!JobQueue.empty()) {
            if (!FreeFiberPool.empty()) {
              FFiberJob Job = JobQueue.front();
              JobQueue.pop();

              FiberToRun = FreeFiberPool.back();
              FreeFiberPool.pop_back();
              FiberToRun->CurrentJob = Job;
            }
          }
        }

        if (FiberToRun) {
          while (!FiberToRun->bIsSuspended.load(std::memory_order_acquire)) {
            _mm_pause();
          }
          FiberToRun->bIsSuspended.store(false, std::memory_order_relaxed);

          TLS_CurrentFiberContext = FiberToRun;
          SwitchToFiber(FiberToRun->FiberHandle);
          FiberToRun->bIsSuspended.store(true, std::memory_order_release);
          TLS_CurrentFiberContext = nullptr;

          if (FiberToRun->CurrentJob.Function == nullptr &&
              FiberToRun->WaitingOnCounter == nullptr) {
            std::lock_guard<std::mutex> Lock(SchedulerMutex);
            FreeFiberPool.push_back(FiberToRun);
          } else if (FiberToRun->WaitingOnCounter != nullptr) {
            FFiberCounter *Counter = FiberToRun->WaitingOnCounter;
            const int32_t Target = FiberToRun->WaitingTargetValue;
            FiberToRun->WaitingOnCounter = nullptr;

            bool bAlreadyDone = false;
            while (Counter->Lock.test_and_set(std::memory_order_acquire)) {
            }
            if (Counter->Value.load(std::memory_order_relaxed) <= Target) {
              bAlreadyDone = true;
            } else {
              Counter->WaitingFibers.push_back(FiberToRun);
            }
            Counter->Lock.clear(std::memory_order_release);

            if (bAlreadyDone) {
              std::lock_guard<std::mutex> Lock(SchedulerMutex);
              SuspendFibers.push_back(FiberToRun);
              WakeCondition.notify_all();
            }
          }
        }
      }
    });
  }
}

void FFiberJobManager::Shutdown() {
  if (!bIsRunning.exchange(false)) {
    return;
  }

  WakeCondition.notify_all();

  for (std::thread &Worker : Workers) {
    if (Worker.joinable()) {
      Worker.join();
    }
  }
  Workers.clear();

  {
    std::lock_guard<std::mutex> Lock(SchedulerMutex);
    for (FFiberTaskContext *Context : AllocatedFibers) {
      if (Context) {
        if (Context->FiberHandle) {
          DeleteFiber(Context->FiberHandle);
        }
        delete Context;
      }
    }
    FreeFiberPool.clear();
    AllocatedFibers.clear();
    SuspendFibers.clear();
  }
}

void FFiberJobManager::RunJob(const FFiberJob &InJob) {
  RunJobs(&InJob, 1, InJob.Counter);
}

void FFiberJobManager::RunJobs(const FFiberJob *InJobs, uint32_t InNumJobs,
                               FFiberCounter *InCounter) {
  if (!InJobs || InNumJobs == 0) {
    return;
  }

  if (InCounter) {
    InCounter->bDone.store(false, std::memory_order_relaxed);
    InCounter->Value.fetch_add(InNumJobs, std::memory_order_seq_cst);
  }

  {
    std::lock_guard<std::mutex> Lock(SchedulerMutex);
    for (uint32_t Index = 0; Index < InNumJobs; ++Index) {
      FFiberJob Job = InJobs[Index];
      if (InCounter) {
        Job.Counter = InCounter;
      }
      JobQueue.push(Job);
    }
  }

  WakeCondition.notify_all();
}

void FFiberJobManager::OnJobCompleted(FFiberCounter *Counter) {
  if (!Counter) {
    return;
  }

  int32_t Remaining =
      Counter->Value.fetch_sub(1, std::memory_order_acq_rel) - 1;

  // 카운터 완료 시 대기 파이버 재개
  if (Remaining <= 0) {
    while (Counter->Lock.test_and_set(std::memory_order_acquire)) {
    }
    std::vector<void *> ResumedList = std::move(Counter->WaitingFibers);
    Counter->WaitingFibers.clear();
    // 완료 표식
    Counter->bDone.store(true, std::memory_order_release);
    Counter->Lock.clear(std::memory_order_release);

    if (!ResumedList.empty()) {
      std::lock_guard<std::mutex> Lock(SchedulerMutex);
      for (void *Item : ResumedList) {
        FFiberTaskContext *ResumedFiber =
            static_cast<FFiberTaskContext *>(Item);
        if (ResumedFiber) {
          SuspendFibers.push_back(ResumedFiber);
        }
      }
      WakeCondition.notify_all();
    }
  }
}

void FFiberJobManager::SuspendAndSwitch(FFiberTaskContext *WaitingFiber) {
  void *Target = TLS_ThreadWorkerFiber;
  SwitchToFiber(Target);
}

void FFiberJobManager::YieldCurrentFiber(FFiberTaskContext *Context) {
  void *Target = TLS_ThreadWorkerFiber;
  SwitchToFiber(Target);
}

bool FFiberJobManager::ExecuteOneJobOrReadyFiber() {
  FFiberTaskContext *FiberToRun = nullptr;
  {
    std::unique_lock<std::mutex> Lock(SchedulerMutex);
    if (!SuspendFibers.empty()) {
      FiberToRun = SuspendFibers.front();
      SuspendFibers.pop_front();
    } else if (!JobQueue.empty()) {
      if (!FreeFiberPool.empty()) {
        FFiberJob Job = JobQueue.front();
        JobQueue.pop();

        FiberToRun = FreeFiberPool.back();
        FreeFiberPool.pop_back();
        FiberToRun->CurrentJob = Job;
      } else {
        return false;
      }
    }
  }

  if (FiberToRun) {
    while (!FiberToRun->bIsSuspended.load(std::memory_order_acquire)) {
      _mm_pause();
    }
    FiberToRun->bIsSuspended.store(false, std::memory_order_relaxed);

    TLS_CurrentFiberContext = FiberToRun;
    SwitchToFiber(FiberToRun->FiberHandle);
    FiberToRun->bIsSuspended.store(true, std::memory_order_release);
    TLS_CurrentFiberContext = nullptr;

    if (FiberToRun->CurrentJob.Function == nullptr &&
        FiberToRun->WaitingOnCounter == nullptr) {
      std::lock_guard<std::mutex> Lock(SchedulerMutex);
      FreeFiberPool.push_back(FiberToRun);
    } else if (FiberToRun->WaitingOnCounter != nullptr) {
      FFiberCounter *Counter = FiberToRun->WaitingOnCounter;
      const int32_t Target = FiberToRun->WaitingTargetValue;
      FiberToRun->WaitingOnCounter = nullptr;

      bool bAlreadyDone = false;
      while (Counter->Lock.test_and_set(std::memory_order_acquire)) {
      }
      if (Counter->Value.load(std::memory_order_relaxed) <= Target) {
        bAlreadyDone = true;
      } else {
        Counter->WaitingFibers.push_back(FiberToRun);
      }
      Counter->Lock.clear(std::memory_order_release);

      if (bAlreadyDone) {
        std::lock_guard<std::mutex> Lock(SchedulerMutex);
        SuspendFibers.push_back(FiberToRun);
        WakeCondition.notify_all();
      }
    }
    return true;
  }
  return false;
}

void FFiberJobManager::WaitForCounter(FFiberCounter *InCounter,
                                      int32_t InTargetValue) {
  if (!InCounter) {
    return;
  }

  if (InCounter->Value.load(std::memory_order_acquire) <= InTargetValue &&
      (InTargetValue != 0 || InCounter->bDone.load(std::memory_order_acquire))) {
    return;
  }

  if (TLS_CurrentFiberContext) {
    FFiberTaskContext *WaitingFiber = TLS_CurrentFiberContext;
    WaitingFiber->WaitingOnCounter = InCounter;
    WaitingFiber->WaitingTargetValue = InTargetValue;
    SuspendAndSwitch(WaitingFiber);
    return;
  }

  while (InCounter->Value.load(std::memory_order_acquire) > InTargetValue ||
         (InTargetValue == 0 && !InCounter->bDone.load(std::memory_order_acquire))) {
    if (!ExecuteOneJobOrReadyFiber()) {
      std::this_thread::yield();
    }
  }
}

void FFiberJobManager::FiberWorkerLoop(FFiberTaskContext *Context) {
  while (bIsRunning.load(std::memory_order_relaxed)) {
    if (Context->CurrentJob.Function) {
      try {
        Context->CurrentJob.Function(Context->CurrentJob.Data);
      } catch (...) {
      }
    }

    if (Context->CurrentJob.Counter) {
      OnJobCompleted(Context->CurrentJob.Counter);
      Context->CurrentJob.Counter = nullptr;
    }

    Context->CurrentJob.Function = nullptr;
    Context->CurrentJob.Data = nullptr;

    YieldCurrentFiber(Context);
  }
}

FFiberJobManager::FFiberTaskContext *FFiberJobManager::CreateJobFiber() {
  FFiberTaskContext *Context = new FFiberTaskContext();
  Context->Manager = this;
  Context->FiberHandle = CreateFiberEx(FiberStackSize, FiberStackSize, 0,
                                       FiberEntryPoint, Context);
  if (!Context->FiberHandle) {
    delete Context;
    return nullptr;
  }
  return Context;
}