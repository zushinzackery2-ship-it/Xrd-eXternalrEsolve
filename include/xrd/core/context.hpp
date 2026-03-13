#pragma once
// Xrd-eXternalrEsolve - 全局上下文
// 保存运行时状态：进程句柄、偏移表、内存访问器

#include "types.hpp"
#include "../memory/memory.hpp"
#include "process.hpp"
#include "process_sections.hpp"
#include "../chaos/chaos_types.hpp"
#include <memory>
#include <iostream>
#include <mutex>

namespace xrd
{

// ─── UE 偏移表：所有需要运行时发现的偏移都在这里 ───
struct UEOffsets
{
    // UObject 布局
    i32 UObject_Vft    = 0x00;
    i32 UObject_Flags  = -1;
    i32 UObject_Index  = -1;
    i32 UObject_Class  = -1;
    i32 UObject_Name   = -1;
    i32 UObject_Outer  = -1;

    // UField
    i32 UField_Next = -1;

    // UStruct
    i32 UStruct_SuperStruct     = -1;
    i32 UStruct_Children        = -1;
    i32 UStruct_ChildProperties = -1;
    i32 UStruct_Size            = -1;

    // UFunction
    i32 UFunction_FunctionFlags = -1;
    i32 UFunction_ExecFunction  = -1;

    // UClass
    i32 UClass_CastFlags              = -1;
    i32 UClass_ClassDefaultObject     = -1;
    i32 UClass_ImplementedInterfaces  = -1;

    // UEnum
    i32 UEnum_Names = -1;

    // FProperty (UE4.25+) / UProperty
    i32 Property_ArrayDim      = -1;
    i32 Property_ElementSize   = -1;
    i32 Property_PropertyFlags = -1;
    i32 Property_Offset        = -1;

    // 类型化属性偏移
    i32 ByteProperty_Enum       = -1;
    i32 BoolProperty_Base       = -1;
    i32 ObjectProperty_Class    = -1;
    i32 ClassProperty_MetaClass = -1;
    i32 StructProperty_Struct   = -1;
    i32 ArrayProperty_Inner     = -1;
    i32 MapProperty_Base        = -1;
    i32 SetProperty_ElementProp = -1;
    i32 EnumProperty_Base       = -1;
    i32 DelegateProperty_Sig    = -1;

    // FField (UE4.25+)
    i32 FField_Class = 0x08;
    i32 FField_Owner = 0x10;
    i32 FField_Next  = 0x20;
    i32 FField_Name  = 0x28;

    // FFieldClass
    i32 FFieldClass_Name      = 0x00;
    i32 FFieldClass_CastFlags = 0x10;

    // FName
    i32 FName_CompIdx = 0x00;
    i32 FName_Number  = 0x04;
    i32 FName_Size    = 0x08;

    // GObjects / GNames / GWorld 的虚拟地址
    uptr GObjects = 0;
    uptr GNames   = 0;
    uptr GWorld   = 0;

    // GObjects 布局
    bool bIsChunkedObjArray = true;
    i32  ChunkSize          = 64 * 1024;
    i32  FUObjectItemSize   = 0x18;
    i32  FUObjectItemInitialOffset = 0x00;

    // GNames 布局
    bool bUseNamePool       = true;
    i32  FNamePoolBlockBits = 16;
    i32  FNameEntryStride   = 2;

    // FProperty vs UProperty
    bool bUseFProperty = true;

    // UE5 双精度
    bool bUseDoublePrecision = false;

    // DebugCanvasObject TMap Data 指针的 .data 段地址
    // 链路: [此地址] -> +0x20 -> +0x280 -> ViewProj 4x4 矩阵
    uptr DebugCanvasObjCacheAddr = 0;

    // 骨骼相关
    bool kAutoScanUSceneComponentComponentToWorld = true;
    i32  ComponentToWorld_Offset          = -1;
    i32  ComponentSpaceTransforms_Offset  = -1;
    i32  RefSkeletonBoneInfo_Offset       = -1;

    // Actor/Pawn 链
    // UWorld -> OwningGameInstance -> LocalPlayers[0] -> PlayerController -> Pawn
    i32 UWorld_PersistentLevel                  = -1;
    i32 UWorld_Levels                           = -1;
    i32 UWorld_OwningGameInstance               = -1;
    i32 UGameInstance_LocalPlayers              = -1;
    i32 ULocalPlayer_PlayerController           = -1;
    i32 ULevel_Actors                           = -1;
    i32 APlayerController_Pawn                  = -1;
    i32 APlayerController_PlayerCameraManager   = -1;

    // ProcessEvent
    i32  ProcessEvent_VTableIndex = -1;
    uptr ProcessEvent_Addr       = 0;

    // AppendString
    uptr AppendNameToString = 0;

    // 物理后端检测结果
    enum PhysicsBackend : u8
    {
        ePhysicsUnknown = 0,
        ePhysX          = 1,
        eChaos          = 2
    };
    PhysicsBackend physicsBackend = ePhysicsUnknown;

    // PhysX
    uptr PhysXDllBase    = 0;   // PhysX3_x64.dll 基址
    uptr PhysXGlobalPtr  = 0;   // NpPhysics* 全局实例指针值

    // Chaos（偏移表独立存放在 ChaosOffsets 中）
    uptr ChaosPhysScene  = 0;   // FPhysScene_Chaos* 地址
};

// ─── 全局上下文 ───
struct Context
{
    u32    pid     = 0;
    HANDLE process = nullptr;

    ModuleInfo mainModule;
    std::vector<SectionCache> sections;

    UEOffsets off;
    ChaosOffsets chaosOff;

    std::unique_ptr<IMemoryAccessor> mem;

    bool inited = false;

    std::mutex mtx;
};

// 全局单例
inline Context& Ctx()
{
    static Context ctx;
    return ctx;
}

inline void ResetContext()
{
    auto& ctx = Ctx();
    if (ctx.process)
    {
        CloseHandle(ctx.process);
        ctx.process = nullptr;
    }
    ctx.pid = 0;
    ctx.mainModule = ModuleInfo{};
    ctx.sections.clear();
    ctx.off = UEOffsets{};
    ctx.chaosOff = ChaosOffsets{};
    ctx.mem.reset();
    ctx.inited = false;
}

inline bool IsInited()
{
    return Ctx().inited;
}

// 线程局部内存访问器覆盖：设置后该线程的 Mem() 返回此指针而非全局通道
// 用于多通道共享内存场景，每个工作线程绑定独立 slot 消除 mutex 争抢
// 注意：不使用 thread_local 关键字，因为手动映射注入时 TLS 目录未被 loader 处理，
// 访问 thread_local 会导致 ACCESS_VIOLATION。改用 TlsAlloc API。
inline DWORD g_memOverrideTlsIndex = TLS_OUT_OF_INDEXES;
inline LONG  g_memOverrideTlsInitState = 0;

inline DWORD GetMemOverrideTlsIndex()
{
    DWORD idx = g_memOverrideTlsIndex;
    if (idx != TLS_OUT_OF_INDEXES)
    {
        return idx;
    }

    if (InterlockedCompareExchange(&g_memOverrideTlsInitState, 1, 0) == 0)
    {
        idx = TlsAlloc();
        g_memOverrideTlsIndex = idx;
    }
    else
    {
        while ((idx = g_memOverrideTlsIndex) == TLS_OUT_OF_INDEXES)
        {
            SwitchToThread();
        }
    }

    return idx;
}

inline void SetThreadMemAccessor(IMemoryAccessor* accessor)
{
    DWORD idx = GetMemOverrideTlsIndex();
    if (idx != TLS_OUT_OF_INDEXES)
    {
        TlsSetValue(idx, accessor);
    }
}

inline void ClearThreadMemAccessor()
{
    DWORD idx = g_memOverrideTlsIndex;
    if (idx != TLS_OUT_OF_INDEXES)
    {
        TlsSetValue(idx, nullptr);
    }
}

inline const IMemoryAccessor& Mem()
{
    DWORD idx = g_memOverrideTlsIndex;
    if (idx != TLS_OUT_OF_INDEXES)
    {
        auto* override = (IMemoryAccessor*)TlsGetValue(idx);
        if (override)
        {
            return *override;
        }
    }
    return *Ctx().mem;
}

inline const UEOffsets& Off()
{
    return Ctx().off;
}

// ─── 便利读取封装（使用全局上下文） ───

inline bool GReadPtr(uptr address, uptr& out)
{
    if (!IsInited())
    {
        out = 0;
        return false;
    }
    return ReadPtr(Mem(), address, out);
}

inline bool GReadI32(uptr address, i32& out)
{
    if (!IsInited())
    {
        out = 0;
        return false;
    }
    return ReadI32(Mem(), address, out);
}

inline bool GReadU32(uptr address, u32& out)
{
    if (!IsInited())
    {
        out = 0;
        return false;
    }
    return ReadU32(Mem(), address, out);
}

template<typename T>
inline bool GReadValue(uptr address, T& out)
{
    if (!IsInited())
    {
        out = T{};
        return false;
    }
    return ReadValue(Mem(), address, out);
}

inline bool GReadCString(uptr address, std::string& out, std::size_t maxLen = 256)
{
    if (!IsInited())
    {
        out.clear();
        return false;
    }
    return ReadCString(Mem(), address, out, maxLen);
}

} // namespace xrd
