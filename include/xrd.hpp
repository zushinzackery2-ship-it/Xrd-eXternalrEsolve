#pragma once
// ============================================================
// Xrd-eXternalrEsolve — Header-Only UE SDK 外部导出工具库 v2
// 单一头文件入口，include 即用
// ============================================================

// 核心层：类型、内存抽象、进程操作、PE 段缓存、全局上下文
#include "xrd/core/types.hpp"
#include "xrd/memory/memory.hpp"
#include "xrd/memory/memory_driver.hpp"
#include "xrd/memory/memory_shmem.hpp"
#include "xrd/core/process.hpp"
#include "xrd/core/process_sections.hpp"
#include "xrd/core/context.hpp"

// 偏移解析层
// globals: GObjects/GNames/GWorld/DebugCanvas
#include "xrd/resolve/globals/scan_gobjects.hpp"
#include "xrd/resolve/globals/scan_gnames.hpp"
#include "xrd/resolve/globals/scan_world.hpp"
#include "xrd/resolve/globals/scan_debug_canvas.hpp"
// uobject: UObject/UStruct/UFunction/UClass 偏移
#include "xrd/resolve/uobject/scan_offsets.hpp"
#include "xrd/resolve/uobject/scan_struct_offsets.hpp"
#include "xrd/resolve/uobject/scan_ufunction_offsets.hpp"
#include "xrd/resolve/uobject/scan_uclass_offsets.hpp"
// property: 属性偏移
#include "xrd/resolve/property/scan_property_offsets.hpp"
#include "xrd/resolve/property/scan_property_base_offsets.hpp"
#include "xrd/resolve/property/scan_property_base_offsets2.hpp"
#include "xrd/resolve/property/scan_property_offsets_extra.hpp"
// runtime: 运行时扫描
#include "xrd/resolve/runtime/scan_process_event.hpp"
#include "xrd/resolve/runtime/scan_append_string.hpp"
#include "xrd/resolve/runtime/scan_bones.hpp"

// 引擎封装层
#include "xrd/engine/names.hpp"
// objects: UObject/UStruct/UClass/FProperty
#include "xrd/engine/objects/objects.hpp"
#include "xrd/engine/objects/objects_search.hpp"
// world: UWorld/Actor/Pawn
#include "xrd/engine/world/world.hpp"
#include "xrd/engine/world/world_access.hpp"
#include "xrd/engine/world/world_levels.hpp"
#include "xrd/engine/world/world_actors.hpp"
// bones: 骨骼读取、批量操作、名称解析
#include "xrd/engine/bones/bones.hpp"
#include "xrd/engine/bones/bones_batch.hpp"
#include "xrd/engine/bones/bones_names.hpp"

// 碰撞通用类型（引擎无关）
#include "xrd/collision/collision_types.hpp"

// PhysX 碰撞模块：类型定义、PE 导出解析、内存读取器
#include "xrd/physx/physx_types.hpp"
#include "xrd/physx/physx_pe.hpp"
#include "xrd/physx/physx_reader.hpp"

// Chaos 碰撞模块：类型定义、内存读取器
#include "xrd/chaos/chaos_types.hpp"
#include "xrd/chaos/chaos_reader.hpp"

// Embree 建模模块：三角形化（header-only，无需 Embree SDK）
#include "xrd/embree/tessellation.hpp"
// 注意：xrd/embree/raycast_scene.hpp 需要链接 embree3，按需 include

// 便利函数层
#include "xrd/helpers/w2s.hpp"
#include "xrd/runtime/channel_pool.hpp"
#include "xrd/runtime/view_state.hpp"
#include "xrd/runtime/scene_watch.hpp"
#include "xrd/runtime/actor_tracker.hpp"
#include "xrd/runtime/bone_runtime.hpp"
// dump: SDK 导出、类型解析、枚举收集
#include "xrd/helpers/dump/dump_type_resolve.hpp"
#include "xrd/helpers/dump/dump_enum.hpp"
#include "xrd/helpers/dump/dump_collect.hpp"
#include "xrd/helpers/dump/dump_sdk.hpp"
#include "xrd/helpers/dump/dump_extra.hpp"

// AutoInit 放最后，因为它依赖上面所有模块
#include "xrd/init/auto_init.hpp"
