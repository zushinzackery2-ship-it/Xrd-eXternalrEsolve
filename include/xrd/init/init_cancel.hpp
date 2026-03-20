#pragma once

#include "../core/context.hpp"
#include <iostream>

namespace xrd
{
using AutoInitCancelCallback = bool (*)();

inline AutoInitCancelCallback& AutoInitCancelCallbackStorage()
{
    static AutoInitCancelCallback callback = nullptr;
    return callback;
}

inline void SetAutoInitCancelCallback(AutoInitCancelCallback callback)
{
    AutoInitCancelCallbackStorage() = callback;
}

namespace detail
{
    inline bool IsAutoInitCancellationRequested()
    {
        AutoInitCancelCallback callback = AutoInitCancelCallbackStorage();
        return callback != nullptr && callback();
    }

    inline bool AbortAutoInitIfRequested(const char* modeTag)
    {
        if (!IsAutoInitCancellationRequested())
        {
            return false;
        }

        std::cerr << "[xrd] " << modeTag << " 收到取消请求，终止当前初始化\n";
        ResetContext();
        return true;
    }

    inline bool SleepForAutoInitRetry(DWORD delayMs, const char* modeTag)
    {
        constexpr DWORD kSleepSliceMs = 25;
        DWORD elapsedMs = 0;
        while (elapsedMs < delayMs)
        {
            if (AbortAutoInitIfRequested(modeTag))
            {
                return false;
            }

            DWORD remainingMs = delayMs - elapsedMs;
            DWORD sleepMs = (remainingMs < kSleepSliceMs) ? remainingMs : kSleepSliceMs;
            Sleep(sleepMs);
            elapsedMs += sleepMs;
        }

        return !AbortAutoInitIfRequested(modeTag);
    }
}
}
