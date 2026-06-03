#include "Interop.h"

#include <vector>

namespace Interop
{
    namespace
    {
        enum class Event : uint8_t
        {
            FRAME_START = 0,
            RENDER = 1,
        };

        struct
        {
            std::vector<FnVoid> OnFrameStartCallbacks;
            std::vector<FnVoid> OnBeforePresentCallbacks;
        } _callbacks;
    }

    typedef void (*UnityEventCallback)(int, void*);
    DLL_EXPORT UnityEventCallback GetUnityEventFunc();
}

void Interop::On_Frame(FnVoid&& fn)
{
    _callbacks.OnFrameStartCallbacks.push_back(fn);
}

void Interop::On_Before_Present(FnVoid&& fn)
{
    _callbacks.OnBeforePresentCallbacks.push_back(fn);
}

DLL_EXPORT void OnRenderEvent(int eventId, void* data) {
    using namespace Interop;
    if (static_cast<Event>(eventId) == Event::FRAME_START)
    {
        for (const FnVoid& cb : _callbacks.OnFrameStartCallbacks)
        {
            cb();
        }
    }
    else if (static_cast<Event>(eventId) == Event::RENDER) 
    {
        for (const FnVoid& cb : _callbacks.OnBeforePresentCallbacks)
        {
            cb();
        }
    }
}

DLL_EXPORT Interop::UnityEventCallback Interop::GetUnityEventFunc()
{
    return OnRenderEvent;
}