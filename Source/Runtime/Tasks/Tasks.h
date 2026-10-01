#pragma once

#include "TaskTypes.h"
#include "TaskScheduler.h"
#include "TaskBase.h"
#include "Task.h"
#include "TaskPipe.h"
#include <type_traits>
#include <vector>
#include <algorithm>

namespace Tasks
{
    // 보이드 반환 람다 디스패치
    template<typename FunctorType>
    requires std::is_invocable_r_v<void, FunctorType>
    FTask Launch(const char* DebugName, FunctorType&& Functor, ETaskPriority Priority = ETaskPriority::Normal)
    {
        (void)DebugName;

        FTaskBase* TaskImpl = new FTaskBase(std::forward<FunctorType>(Functor), Priority);
        FTask TaskHandle(TaskImpl);

        // 선행 조건이 없으므로 바로 등록
        TaskImpl->OnPrerequisiteCompleted();

        return TaskHandle;
    }

    // 선행 작업이 있는 보이드 반환 람다 디스패치
    template<typename FunctorType>
    requires std::is_invocable_r_v<void, FunctorType>
    FTask Launch(const char* DebugName, FunctorType&& Functor, const FTask& Prerequisite, ETaskPriority Priority = ETaskPriority::Normal)
    {
        (void)DebugName;

        FTaskBase* TaskImpl = new FTaskBase(std::forward<FunctorType>(Functor), Priority);
        FTask TaskHandle(TaskImpl);

        if (Prerequisite.IsValid())
        {
            TaskImpl->AddPrerequisite(Prerequisite.GetImpl());
        }

        TaskImpl->OnPrerequisiteCompleted();

        return TaskHandle;
    }

    // 결과값을 반환하는 람다 디스패치
    template<typename FunctorType>
    requires (!std::is_void_v<std::invoke_result_t<FunctorType>>)
    auto Launch(const char* DebugName, FunctorType&& Functor, ETaskPriority Priority = ETaskPriority::Normal)
    {
        (void)DebugName;
        using ReturnType = std::invoke_result_t<FunctorType>;

        auto ResultStorage = std::make_shared<ReturnType>();

        FTaskBase* TaskImpl = new FTaskBase([Func = std::forward<FunctorType>(Functor), Storage = ResultStorage]() mutable
        {
            *Storage = Func();
        }, Priority);

        TTask<ReturnType> TaskHandle(TaskImpl, ResultStorage);

        TaskImpl->OnPrerequisiteCompleted();

        return TaskHandle;
    }

    // 병렬 반복문 분할 실행
    template<typename FunctorType>
    void ParallelFor(int32_t TotalCount, int32_t ChunkSize, const FunctorType& Functor, ETaskPriority Priority = ETaskPriority::Normal)
    {
        if (TotalCount <= 0)
        {
            return;
        }

        if (ChunkSize <= 0)
        {
            ChunkSize = 1;
        }

        const int32_t NumJobs = (TotalCount + ChunkSize - 1) / ChunkSize;
        if (NumJobs <= 1)
        {
            Functor(0, TotalCount);
            return;
        }

        std::vector<FTask> TaskHandles;
        TaskHandles.reserve(NumJobs - 1);

        for (int32_t Index = 0; Index < NumJobs - 1; ++Index)
        {
            const int32_t Start = Index * ChunkSize;
            const int32_t End = Start + ChunkSize;

            TaskHandles.push_back(Launch("ParallelForTask", [Start, End, &Functor]()
            {
                Functor(Start, End);
            }, Priority));
        }

        // 마지막 구간은 현재 스레드에서 직접 처리
        const int32_t LastStart = (NumJobs - 1) * ChunkSize;
        Functor(LastStart, TotalCount);

        // 모든 분할 태스크 완료 대기
        for (const FTask& Handle : TaskHandles)
        {
            Handle.Wait();
        }
    }
}
