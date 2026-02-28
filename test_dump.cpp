// 实际运行测试：AutoInit + DumpSdk
// 带 60 秒超时保护，防止依赖收集卡死
#include <xrd.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <Windows.h>

std::atomic<bool> g_finished{false};

void DumpThread()
{
    std::cerr << "=== Xrd-eXternalrEsolve Dump Test ===\n\n";

    if (!xrd::AutoInit())
    {
        std::cerr << "[FAIL] AutoInit 失败\n";
        g_finished = true;
        return;
    }

    std::cerr << "\n[OK] AutoInit 成功\n\n";

    if (!xrd::DumpSdk(L"Xrd-eXternalrEsolve/XrdOutput"))
    {
        std::cerr << "[FAIL] DumpSdk 失败\n";
        g_finished = true;
        return;
    }

    std::cerr << "\n[OK] SDK 导出完成，输出目录: XrdOutput/\n";

    if (!xrd::DumpSpaceSdk(L"Xrd-eXternalrEsolve/XrdOutput"))
    {
        std::cerr << "[WARN] DumpSpaceSdk 失败\n";
    }
    else
    {
        std::cerr << "[OK] GObjects-Dump 导出完成\n";
    }

    if (!xrd::DumpMapping(L"Xrd-eXternalrEsolve/XrdOutput"))
    {
        std::cerr << "[WARN] DumpMapping 失败\n";
    }
    else
    {
        std::cerr << "[OK] Mapping 导出完成\n";
    }

    std::cerr << "\n=== 全部完成 ===\n";
    g_finished = true;
}

int main()
{
    constexpr int TIMEOUT_SEC = 90;

    std::thread worker(DumpThread);

    auto start = std::chrono::steady_clock::now();
    while (!g_finished)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(500));
        auto elapsed = std::chrono::duration_cast<
            std::chrono::seconds>(
                std::chrono::steady_clock::now() - start)
            .count();
        if (elapsed >= TIMEOUT_SEC)
        {
            std::cerr << "\n[TIMEOUT] " << TIMEOUT_SEC
                << " 秒超时，强制退出\n";
            // 强制终止进程
            TerminateProcess(GetCurrentProcess(), 2);
        }
    }

    worker.join();
    return 0;
}
