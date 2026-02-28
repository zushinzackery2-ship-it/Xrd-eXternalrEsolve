#pragma once
// ============================================================
// Xrd-eXternalrEsolve — Header-Only UE SDK 外部导出工具库 v2
// 单一头文件入口，include 即用
// ============================================================

// 核心层：类型、内存抽象、进程操作、PE 段缓存、全局上下文
#include "xrd/core/types.hpp"
#include "xrd/core/memory.hpp"
#include "xrd/core/memory_driver.hpp"
#include "xrd/core/process.hpp"
#include "xrd/core/process_sections.hpp"
#include "xrd/core/context.hpp"

// 偏移解析层：自动扫描 GObjects/GNames/UObject偏移/UStruct偏移/GWorld/骨骼/FVector精度/类型化属性偏移
#include "xrd/resolve/scan_gobjects.hpp"
#include "xrd/resolve/scan_gnames.hpp"
#include "xrd/resolve/scan_offsets.hpp"
#include "xrd/resolve/scan_struct_offsets.hpp"
#include "xrd/resolve/scan_property_offsets.hpp"
#include "xrd/resolve/scan_property_base_offsets.hpp"
#include "xrd/resolve/scan_property_base_offsets2.hpp"
#include "xrd/resolve/scan_property_offsets_extra.hpp"
#include "xrd/resolve/scan_ufunction_offsets.hpp"
#include "xrd/resolve/scan_uclass_offsets.hpp"
#include "xrd/resolve/scan_process_event.hpp"
#include "xrd/resolve/scan_append_string.hpp"
#include "xrd/resolve/scan_world.hpp"
#include "xrd/resolve/scan_bones.hpp"
#include "xrd/resolve/fvector_detect.hpp"

// 引擎封装层：FName 解析、UObject 遍历/搜索、UWorld/Actor/Pawn
#include "xrd/engine/names.hpp"
#include "xrd/engine/objects.hpp"
#include "xrd/engine/objects_search.hpp"
#include "xrd/engine/world.hpp"
#include "xrd/engine/world_actors.hpp"
#include "xrd/engine/bones.hpp"
#include "xrd/engine/bones_batch.hpp"
#include "xrd/engine/bones_names.hpp"

// 便利函数层：WorldToScreen、类型解析、枚举收集、属性收集、SDK 导出
#include "xrd/helpers/w2s.hpp"
#include "xrd/helpers/dump_type_resolve.hpp"
#include "xrd/helpers/dump_enum.hpp"
#include "xrd/helpers/dump_collect.hpp"
#include "xrd/helpers/dump_sdk.hpp"
#include "xrd/helpers/dump_extra.hpp"

// AutoInit 放最后，因为它依赖上面所有模块
#include "xrd/core/auto_init.hpp"
