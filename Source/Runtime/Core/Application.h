#pragma once
#include <Windows.h>

class FApplication
{
public:
    virtual ~FApplication() = default;

    virtual bool Init(HINSTANCE hInstance) = 0;
    virtual void Run() = 0;
    virtual void Shutdown() = 0;
};