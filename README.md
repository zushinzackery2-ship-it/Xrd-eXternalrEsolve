<div align="center">

# Xrd-eXternalrEsolve

**Unreal Engine 外部进程 SDK 导出与运行时访问库（header-only）**

*跨进程读取 | Header-Only | 全自动偏移发现 | SDK 导出*

![C++](https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Header Only](https://img.shields.io/badge/Header--Only-Yes-green?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

</div>

---

> [!CAUTION]
> **免责声明**  
> 本项目仅用于学习研究 Unreal Engine 内部结构与算法还原，以及在合法授权前提下的游戏 Modding/插件开发学习与验证，不得用于任何违反游戏服务条款或法律法规的行为。  
> 使用本项目产生的一切后果由使用者自行承担，作者不承担任何责任。  
> 请在合法合规的前提下使用本项目。

> [!NOTE]
> **版本兼容性说明**  
> 本项目面向 Unreal Engine 4/5（Windows x64）。不同 UE 版本在部分结构偏移与布局上存在差异，AutoInit 会自动适配。  
> SDK 导出格式对齐 [Dumper-7](https://github.com/Encryqed/Dumper-7)，包含正确的 `#pragma pack` / `alignas` / trailing padding 处理。

> [!IMPORTANT]
> **代码重构说明**  
> 当前项目包含相当大量的 AI 重构代码，可能存在维护性问题。  
> 功能实现集中在 `include/xrd`，入口侧尽量保持薄封装。

---

## 特性

| 功能 | 说明 |
|:-----|:-----|
| **Header-Only** | 单一入口 `#include <xrd.hpp>`，无需编译库文件 |
| **全自动偏移发现** | `AutoInit()` 六阶段扫描 GObjects / GNames / GWorld / ProcessEvent / AppendString / GCanvas / PlayerController / Pawn / CameraManager 等全部关键偏移 |
| **访问器抽象** | `IMemoryAccessor` 接口解耦算法与内存后端，内置 WinAPI / SharedMem 两套实现，并支持自定义扩展 |
| **线程安全** | 名称缓存/属性偏移缓存均使用 `shared_mutex`，支持多线程并发读取 |
| **SDK 导出** | 生成与 Dumper-7 格式对齐的 CppSDK，含 `#pragma pack` / `alignas` / trailing padding |
| **World 链式访问** | `UWorld → GameInstance → LocalPlayers[0] → PlayerController → Pawn` 全链路偏移一次性缓存 |
| **多 Level Actor 聚合** | `GetAllActors()` 汇总 `PersistentLevel + Levels` 中所有已加载关卡的 Actor，并做去重缓存 |
| **FVector 精度自动检测** | 运行时区分 UE4 (float) / UE5 (double) |
| **骨骼系统** | ComponentToWorld 自动扫描、双缓冲 ComponentSpaceTransforms、四元数合理性验证、按名称过滤式读取，并把骨骼数组扫描限制在 `USkinnedMeshComponent` 自身成员范围内 |
| **W2S** | 内置 WorldToScreen 投影 |
| **反射式字段访问** | `ReadActorFieldPtr/Int32/Float` 通过属性名自动查找偏移（带缓存），无需硬编码 |
| **线程局部访问器** | `SetThreadMemAccessor()` 通过 TLS 槽将当前线程绑定到独立通道，`Mem()` 自动返回线程局部覆盖，多线程零 mutex 争抢 |
| **可取消初始化** | `SetAutoInitCancelCallback()` 支持在重试等待和扫描阶段中断 `AutoInit()`，取消时自动 `ResetContext()` |
| **PhysX 碰撞读取** | 远程读取 PhysX 3.4 场景数据（Actor/Shape/Geometry），支持 Box/Sphere/Capsule/ConvexMesh 碰撞体 |
| **Chaos 碰撞读取** | 远程读取 UE5 Chaos 物理场景（FPhysScene_Chaos），通过反射自动发现 BodyInstance/PhysicsProxy/AggGeom 偏移 |
| **Embree 遮挡检测** | 基于 Embree 的 raycast 遮挡查询，支持碰撞体曲面细分与 BVH 加速 |
| **碰撞线框渲染** | Box/Sphere/Capsule/ConvexMesh 线框数据生成，支持世界坐标变换与屏幕投影 |

---

## 核心 API 列表

| 分类 | API | 说明 |
|:-----|:----|:-----|
| **上下文** | `AutoInit()` / `AutoInit(processName)` | WinAPI 模式自动发现 UE 进程、填充全局上下文 |
|  | `AutoInitSharedMem()` / `AutoInitDriver()` | 通过共享内存通道初始化（`AutoInitDriver` 为兼容别名） |
|  | `IsInited()` | 是否已初始化 |
|  | `Mem()` | 返回 `IMemoryAccessor` 引用 |
|  | `Off()` | 返回偏移结构 `UEOffsets` |
|  | `SetGObjects(rva)` / `SetGNames(rva)` / `SetGWorld(rva)` | 手动设置 RVA（AutoInit 前调用） |
|  | `SetAutoInitCancelCallback(callback)` | 设置取消回调，在 AutoInit 重试与扫描流程中提前终止 |
| **线程绑定** | `SetThreadMemAccessor(accessor)` | 将当前线程的 `Mem()` 绑定到指定访问器（多通道隔离） |
|  | `ClearThreadMemAccessor()` | 清除当前线程绑定，恢复使用全局通道 |
| **World** | `GetUWorld()` | 获取 UWorld 指针 |
|  | `GetPlayerController()` | 链式获取本地 PlayerController |
|  | `GetAPawn()` | 链式获取本地 Pawn |
| **Actor** | `GetAllActors()` | 汇总所有已加载 Level 的 Actor 指针并去重 |
|  | `GetActorsOfClass(className)` | 按类名精确筛选 Actor |
|  | `GetActorsOfClassContains(keyword)` | 按类名模糊筛选 Actor |
|  | `IsActorOfClass(actor, className)` | 检查 Actor 是否属于指定类（含继承链） |
| **对象/名称** | `GetObjectName(obj)` | 获取 UObject 名称（线程安全缓存） |
|  | `GetObjectClassName(obj)` | 获取 UObject 类名（线程安全缓存） |
|  | `GetObjectFullName(obj)` | 获取完整路径名 |
| **字段反射** | `ReadActorFieldPtr(actor, propName)` | 通过属性名读取指针字段（偏移自动缓存） |
|  | `ReadActorFieldInt32(actor, propName)` | 通过属性名读取 int32 字段 |
|  | `ReadActorFieldFloat(actor, propName)` | 通过属性名读取 float 字段 |
| **坐标 / W2S** | `GetActorWorldPos(actor, outPos)` | 获取 Actor 世界坐标 |
|  | `WorldToScreen(pos, w, h, outScreen)` | 世界坐标转屏幕坐标 |
|  | `GetVPMatrix(outMatrix)` | 通过 GCanvas 链路读取 ViewProjection 矩阵 |
| **骨骼** | `GetBoneWorldLocation(mesh, index, outPos)` | 获取单根骨骼世界坐标 |
|  | `GetAllBoneWorldLocations(mesh, vp, w, h, ...)` | 批量获取所有骨骼坐标 + 屏幕投影 |
|  | `GetFilteredBoneWorldLocations(mesh, indices, ...)` | 按索引列表过滤式读取骨骼 |
|  | `GetCachedBoneNames(mesh)` | 获取骨骼名称列表（带缓存） |
| **PhysX** | `PhysXReader(mem, globalPtr)` | 构造 PhysX 读取器 |
|  | `ReadStaticCollision(outData)` | 读取所有 static actor 碰撞数据（仅 eSIMULATION_SHAPE） |
|  | `ReadActorData(addr, outData)` | 读取单个 Actor 的 pose + shapes |
|  | `ReadConvexMeshData(addr, outData)` | 读取 ConvexMesh 顶点与边数据 |
| **Chaos** | `InitChaosOffsets(ctx)` | 反射发现 Chaos 碰撞偏移 |
|  | `ChaosReader::ReadAllActors()` | 读取所有 Chaos 碰撞 Actor |
| **Embree** | `RaycastScene::Build(shapes)` | 从碰撞体构建 BVH |
|  | `RaycastScene::IsOccluded(o, t)` | 射线遮挡查询 |
| **SDK 导出** | `DumpSdk(path)` | 完整导出 (CppSDK + Dump + Mapping) |
|  | `DumpCppSdk(path)` | 仅 C++ SDK |
|  | `DumpSpaceSdk(path)` | Dump 格式 |
|  | `DumpMapping(path)` | Mapping 格式 |

---

## 快速开始

### 编译环境

- **C++ 标准**：C++20
- **编译器**：MSVC（Visual Studio 2022）
- **平台**：Windows x64
- **链接方式**：静态运行时（/MT）

```bat
@echo off
call "D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
cl /std:c++20 /EHsc /O2 /MT /I"include" /Fe:main.exe main.cpp /link /MACHINE:X64
```

### 最小示例

```cpp
#include <xrd.hpp>
#include <iostream>

int main()
{
    if (!xrd::AutoInit())
    {
        std::cerr << "AutoInit failed\n";
        return 1;
    }

    xrd::DumpSdk(L"Output");
    return 0;
}
```

### 共享内存模式

```cpp
#include <xrd.hpp>

int main()
{
    if (!xrd::AutoInitSharedMem(L"MyGame-Win64-Shipping.exe"))
    {
        return 1;
    }

    return 0;
}
```

### 指定进程 / 手动偏移

```cpp
// 指定进程名
xrd::AutoInit(L"MyGame-Win64-Shipping.exe");

// 手动设置 RVA（AutoInit 前调用）
xrd::SetGObjects(0x04952C50);
xrd::SetGNames(0x04916900);
```

### 取消 AutoInit

```cpp
static std::atomic<bool> g_stopRequested = false;

xrd::SetAutoInitCancelCallback([]() -> bool
{
    return g_stopRequested.load();
});

if (!xrd::AutoInit(L"MyGame-Win64-Shipping.exe"))
{
    // 如果回调返回 true，AutoInit 会立刻终止并自动 ResetContext()
}
```

### 运行时数据访问

```cpp
#include <xrd.hpp>

int main()
{
    xrd::AutoInit(L"MyGame-Win64-Shipping.exe");

    // World 链式访问
    uptr pawn = xrd::GetAPawn();

    // 按类名筛选 Actor
    auto zombies = xrd::GetActorsOfClassContains("Zombie");
    for (uptr actor : zombies)
    {
        // 反射式字段读取（偏移自动缓存）
        uptr mesh = xrd::ReadActorFieldPtr(actor, "Mesh");
        float hp  = xrd::ReadActorFieldFloat(actor, "Health");

        // 骨骼坐标
        xrd::FVector headPos;
        xrd::GetBoneWorldLocation(mesh, 0, headPos);

        // W2S
        xrd::FVector2D screen;
        xrd::WorldToScreen(headPos, 1920, 1080, screen);
    }

    return 0;
}
```

---

## 访问器架构

```
应用代码
    │  调用 xrd::Mem().Read(...)
    ▼
IMemoryAccessor（纯虚接口）
    │  Read / Write / ReadBatch
    ▼
┌─────────────────────┐  ┌──────────────────────┐  ┌──────────────────────┐
│ WinApiMemoryAccessor │  │ SharedMemoryAccessor │  │ CustomMemoryAccessor │
│ (ReadProcessMemory)  │  │ (DAT + SharedMem)    │  │ (用户自定义实现)      │
└─────────────────────┘  └──────────────────────┘  └──────────────────────┘
```

- **最小内存访问抽象**：所有算法只依赖 `IMemoryAccessor` 接口
- **WinApiMemoryAccessor**：基于 `ReadProcessMemory`，开箱即用
- **SharedMemoryAccessor**：通过 DAT 握手 + 共享内存事件通道访问驱动后端，适合高频批量读取
- **CustomMemoryAccessor**：可以按项目需求扩展自己的读取后端
- **线程局部覆盖**：`SetThreadMemAccessor()` 基于 Win32 TLS API (`TlsAlloc` / `TlsSetValue`) 绑定当前线程访问器，`Mem()` 优先返回线程局部覆盖；多个工作线程可以各自绑定独立访问器，减少争抢

---

## AutoInit 六阶段流程

```
AutoInit()
│
├─ Phase 1: 进程附加
│   ├─ 枚举进程、打开句柄
│   ├─ 获取主模块基址
│   └─ 缓存 .text / .data / .rdata PE 段
│
├─ Phase 2: GObjects / GNames 定位
│   ├─ 扫描 .data 段候选地址
│   ├─ 验证 FUObjectArray 布局 (对象数/最大数/分块)
│   └─ 验证 NamePool 含 "None" 条目
│
├─ Phase 3: 结构偏移发现
│   ├─ UObject (Flags/Index/Class/Name/Outer)
│   ├─ UStruct (SuperStruct/Children/ChildProperties/Size)
│   ├─ FField (Name/Class/Next/CastFlags)
│   ├─ Property (Offset/Size/ArrayDim/ElementSize)
│   ├─ 类型化属性 (StructProperty/ObjectProperty/EnumProperty/...)
│   ├─ UFunction (FunctionFlags/Func)
│   ├─ UClass (ClassFlags/DefaultObject/Interfaces)
│   └─ UEnum::Names
│
├─ Phase 4: UField::Next 推导
│
├─ Phase 5: 运行时扫描
│   ├─ GWorld 定位 (UWorld + 已加载 Levels 的 Actor 链聚合验证)
│   ├─ ProcessEvent VTable 索引扫描
│   ├─ AppendString 扫描
│   ├─ FVector 精度检测 (float / double)
│   └─ GCanvas 定位 (ViewProjection 矩阵链路)
│
└─ Phase 6: World 链偏移反射发现
    ├─ UWorld::OwningGameInstance
    ├─ UWorld::PersistentLevel
    ├─ UWorld::Levels
    ├─ UGameInstance::LocalPlayers
    ├─ ULocalPlayer::PlayerController
    ├─ APlayerController::Pawn
    └─ APlayerController::PlayerCameraManager
```

所有发现的偏移缓存在 `xrd::Ctx().off` (`UEOffsets` 结构体) 中，后续访问无需重复扫描。

如果安装了 `SetAutoInitCancelCallback()`，那么重试等待和关键扫描阶段都会轮询该回调；一旦返回 `true`，当前初始化会立即终止并执行 `ResetContext()`。

当某个初始化阶段耗时明显偏长时，日志会输出 `[xrd][Perf] 阶段名 耗时 N ms`，便于快速定位卡在函数/类偏移发现、World 链、FVector 精度检测或 Chaos 初始化等热点。

---

## 线程安全

| 组件 | 机制 | 说明 |
|:-----|:-----|:-----|
| **FName 缓存** | `std::shared_mutex` | `GetNameFromFName` 并发读不阻塞 |
| **UObject 名称缓存** | `std::shared_mutex` | `GetObjectName` / `GetFFieldName` 并发安全 |
| **类名缓存** | `std::shared_mutex` | `GetObjectClassName` / `GetFFieldClassName` 并发安全 |
| **属性偏移缓存** | `std::shared_mutex` | `GetPropertyOffsetByName` 同一 class+属性只遍历一次，后续并发读 |
| **线程局部访问器** | Win32 TLS API | `SetThreadMemAccessor` / `ClearThreadMemAccessor` 通过 `TlsAlloc` / `TlsSetValue` 绑定，各线程独立通道 |
| **骨骼名缓存** | `std::mutex` | `GetCachedBoneNames` / `PrecacheBoneNames` 互斥保护 |

多线程场景下可安全地从不同线程并发调用上述 API。

---

<details>
<summary><strong>目录结构</strong></summary>

```
Xrd-eXternalrEsolve/
├── include/
│   ├── xrd.hpp                                  # 主入口（单一 include）
│   └── xrd/
│       ├── core/                                # 基础设施
│       │   ├── types.hpp                        #   基本类型 (uptr/i32/u32/FName...)
│       │   ├── context.hpp                      #   全局上下文 & UEOffsets
│       │   ├── process.hpp                      #   进程附加
│       │   └── process_sections.hpp             #   PE 段缓存
│       ├── memory/                              # 内存访问器
│       │   ├── memory.hpp                       #   IMemoryAccessor 抽象 + WinAPI 实现
│       │   └── ...                              #   其他可选访问器实现
│       ├── init/                                # 初始化流程
│       │   ├── auto_init.hpp                    #   六阶段自动初始化入口
│       │   ├── init_cancel.hpp                  #   AutoInit 取消回调
│       │   ├── init_common.hpp                  #   公共扫描逻辑 + FVector 精度检测
│       │   ├── init_chaos_scan.hpp              #   Chaos 小偏移自动发现
│       │   ├── init_chaos.hpp                   #   Chaos 偏移反射发现
│       │   ├── init_world_chain.hpp             #   World 链偏移反射发现
│       │   └── init_helpers.hpp                 #   初始化辅助工具
│       ├── engine/                              # UE 对象封装
│       │   ├── names.hpp                        #   FName 解析 (NamePool / ChunkedArray)
│       │   ├── objects/                         #   UObject 系统
│       │   │   ├── objects.hpp                  #     UObject / UStruct / FProperty 读取
│       │   │   └── objects_search.hpp           #     对象搜索 & 属性偏移缓存
│       │   ├── world/                           #   游戏世界
│       │   │   ├── world.hpp                    #     UWorld / ULevel / Actor 数组
│       │   │   ├── world_access.hpp             #     World / Level 基础访问
│       │   │   ├── world_levels.hpp             #     已加载 Level 聚合与 GetAllActors
│       │   │   └── world_actors.hpp             #     PlayerController / Pawn / Actor 过滤
│       │   └── bones/                           #   骨骼系统
│       │       ├── bones.hpp                    #     骨骼世界坐标 (双缓冲)
│       │       ├── bones_batch.hpp              #     骨骼批量读取 & 过滤
│       │       ├── bones_offsets.hpp            #     SkinnedMeshComponent 范围限定扫描
│       │       └── bones_names.hpp              #     骨骼名称读取与缓存
│       ├── physx/                               # PhysX 碰撞读取
│       │   ├── physx_types.hpp                  #   PhysX 结构定义 & 偏移
│       │   ├── physx_reader.hpp                 #   PhysXReader 远程读取器
│       │   └── physx_pe.hpp                     #   PhysX DLL PE 解析 (RVA 定位)
│       ├── chaos/                               # Chaos 碰撞读取
│       │   ├── chaos_types.hpp                  #   Chaos 结构定义
│       │   ├── chaos_reflection.hpp             #   Chaos 反射辅助
│       │   └── chaos_reader.hpp                 #   ChaosReader 远程读取器
│       ├── collision/                           # 碰撞线框
│       │   └── collision_types.hpp              #   碰撞体类型与线框数据结构
│       ├── embree/                              # Embree 遮挡检测
│       │   ├── raycast_scene.hpp                #   BVH 构建 & 射线查询
│       │   └── tessellation.hpp                 #   碰撞体曲面细分
│       ├── resolve/                             # 偏移扫描器
│       │   ├── globals/                         #   全局指针扫描
│       │   │   ├── scan_gobjects.hpp            #     GObjects 定位
│       │   │   ├── scan_gnames.hpp              #     GNames 定位
│       │   │   ├── scan_world.hpp               #     GWorld 定位
│       │   │   └── scan_debug_canvas.hpp        #     GCanvas 扫描
│       │   ├── uobject/                         #   UObject 偏移扫描
│       │   │   ├── scan_offsets.hpp             #     UObject 基础偏移
│       │   │   ├── scan_struct_offsets.hpp       #     UStruct 偏移
│       │   │   ├── scan_ufunction_offsets.hpp   #     UFunction 偏移
│       │   │   └── scan_uclass_offsets.hpp      #     UClass 偏移
│       │   ├── property/                        #   Property 偏移扫描 (6 个文件)
│       │   │   ├── scan_property_offsets.hpp
│       │   │   ├── scan_property_base_offsets.hpp
│       │   │   ├── scan_property_base_offsets2.hpp
│       │   │   ├── scan_property_offsets_extra.hpp
│       │   │   ├── scan_property_offsets_struct.hpp
│       │   │   └── scan_property_offsets_typed.hpp
│       │   └── runtime/                         #   运行时扫描
│       │       ├── scan_process_event.hpp       #     ProcessEvent VTable 扫描
│       │       ├── scan_append_string.hpp       #     AppendString 扫描
│       │       ├── scan_exec_helpers.hpp        #     Exec/AppendString 扫描辅助
│       │       ├── scan_runtime_common.hpp      #     运行时扫描公共入口
│       │       ├── scan_signature_helpers.hpp   #     签名/字符串扫描辅助
│       │       └── scan_bones.hpp               #     骨骼偏移扫描
│       ├── helpers/                             # SDK 导出 & 工具
│       │   ├── w2s.hpp                          #   WorldToScreen / GetVPMatrix
│       │   └── dump/                            #   SDK 导出
│       │       ├── dump_sdk.hpp                 #     SDK 导出主逻辑
│       │       ├── dump_sdk_struct.hpp          #     Class/Struct 代码生成
│       │       ├── dump_sdk_writer.hpp          #     文件写出
│       │       ├── dump_sdk_func_gen.hpp        #     函数签名生成
│       │       ├── dump_sdk_format.hpp          #     属性名格式化
│       │       ├── dump_sdk_infra.hpp           #     导出基础设施
│       │       ├── dump_type_resolve.hpp        #     类型名解析
│       │       ├── dump_collect.hpp             #     属性收集
│       │       ├── dump_deps.hpp                #     依赖分析
│       │       ├── dump_dep_sort.hpp            #     拓扑排序
│       │       ├── dump_prefix.hpp              #     StructEntry 定义
│       │       ├── dump_predefined.hpp          #     预定义类型
│       │       ├── dump_enum.hpp                #     枚举导出
│       │       ├── dump_extra.hpp               #     Dump/Mapping 格式
│       │       ├── dump_function_flags.hpp      #     函数标志位
│       │       ├── dump_property_flags.hpp      #     属性标志位
│       │       └── gen/                         #     预生成基础类型头文件 (11 个)
│       └── runtime/                             # 运行时缓存
│           └── actor_enumeration_cache.hpp      #   Actor 全量枚举缓存
├── LICENSE                                      # MIT
└── README.md
```

</details>

---

## 修结构

- **结构定义**：所有偏移字段集中在 `core/context.hpp` 的 `UEOffsets` 结构体
- **偏移扫描**：`resolve/` 下按职责分组（globals / uobject / property / runtime），`init/auto_init.hpp` 按阶段串联调用
- **FVector 精度检测**：通过反射读取 `RelativeLocation.ElementSize`（24=double, 12=float），逻辑在 `init/init_common.hpp`
- **物理后端**：运行时自动检测 PhysX / Chaos，分别由 `physx/` 和 `chaos/` 模块处理

## 常见问题

| 问题 | 原因 | 解决 |
|:-----|:-----|:-----|
| GObjects 未找到 | 游戏数据段加密或非标准布局 | 手动 `SetGObjects(rva)` |
| GNames 未找到 | NamePool 分块大小不同 | 检查 ChunkSize (0x2000 / 0x4000) |
| GetAPawn 返回 0 | GWorld 未找到或玩家未 Spawn | 确认游戏已进入关卡 |
| 骨骼位置异常 | Double/Float 检测不准 | 检查 RelativeLocation.ElementSize 反射结果 |
| SDK padding 不对齐 | MinAlignment 读取偏差 | 对比 Dumper-7 输出排查 |
| 多线程崩溃 | 旧版本缓存无锁 | 更新到最新版（已加 shared_mutex） |

---

<div align="center">

**Platform:** Windows x64 | **License:** MIT

</div>
