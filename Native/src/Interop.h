#pragma once

#define DLL_EXPORT extern "C" __declspec(dllexport)

namespace Interop
{
    using FnVoid = void (*)();

    void On_Frame(FnVoid&& fn); // Main thread
    void On_Before_Present(FnVoid&& fn); // Render thread
}