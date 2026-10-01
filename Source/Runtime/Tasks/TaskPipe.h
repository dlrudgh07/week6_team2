#pragma once

#include "Task.h"
#include <mutex>
#include <string>

namespace Tasks
{
    // 순차 실행을 보장하는 태스크 파이프
    class FPipe
    {
    public:
        FPipe(std::string InDebugName = "Pipe");
        ~FPipe() = default;

        // 파이프에 순차 실행할 작업 등록
        template<typename FunctorType>
        FTask Launch(FunctorType&& Functor, ETaskPriority Priority = ETaskPriority::Normal)
        {
            std::lock_guard<std::mutex> Lock(PipeMutex);

            FTaskBase* NewTaskBase = new FTaskBase(std::forward<FunctorType>(Functor), Priority);

            // 이전 작업이 남아있으면 선행 조건으로 연결
            if (LastTask.IsValid())
            {
                NewTaskBase->AddPrerequisite(LastTask.GetImpl());
            }

            FTask NewTask(NewTaskBase);
            LastTask = NewTask;

            // 선행 조건이 이미 충족되었으면 즉시 시작
            NewTaskBase->OnPrerequisiteCompleted();

            // 내부 생성 참조 해제
            NewTaskBase->Release();

            return NewTask;
        }

        // 파이프의 모든 작업 완료 대기
        void WaitUntilEmpty();

    private:
        std::string DebugName;
        std::mutex PipeMutex;
        FTask LastTask;
    };
}
