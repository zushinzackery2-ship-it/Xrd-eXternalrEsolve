#pragma once
// Xrd-eXternalrEsolve - Property 基础偏移发现（续）
// BoolProperty::Base 发现 + 汇总入口
// 从 scan_property_base_offsets.hpp 拆分，保持单文件 300 行以内

#include "scan_property_base_offsets.hpp"

namespace xrd
{
namespace resolve
{

// 发现 BoolProperty::Base 偏移
// 通过找一个 BoolProperty 并检查 FieldSize/ByteOffset/ByteMask/FieldMask 模式
inline bool DiscoverBoolPropertyBaseOffset(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    if (off.Property_Offset == -1)
    {
        return false;
    }

    // 基础 Property 大小 = Property_Offset + 对齐后 + 链表指针
    // 搜索范围需要覆盖 0x50~0x90（UE4/UE5 各版本）
    i32 searchStart = ((off.Property_Offset + 4) + 7) & ~7;
    i32 total = GetObjectCount(mem, off);

    for (i32 i = 0; i < std::min(total, 2000); ++i)
    {
        uptr obj = ReadObjectAt(mem, off, i);
        if (!IsCanonicalUserPtr(obj)) continue;

        uptr cls = 0;
        if (!ReadPtr(mem, obj + off.UObject_Class, cls)) continue;
        uptr metaCls = 0;
        if (!ReadPtr(mem, cls + off.UObject_Class, metaCls) ||
            metaCls != cls)
        {
            continue;
        }

        uptr prop = 0;
        if (off.UStruct_ChildProperties != -1)
        {
            ReadPtr(mem, obj + off.UStruct_ChildProperties, prop);
        }

        while (IsCanonicalUserPtr(prop))
        {
            uptr fieldCls = 0;
            ReadPtr(mem, prop + off.FField_Class, fieldCls);
            if (IsCanonicalUserPtr(fieldCls))
            {
                FName fn{};
                std::string fcName;
                if (ReadValue(mem,
                    fieldCls + off.FFieldClass_Name, fn))
                {
                    fcName = ResolveNameDirect(
                        mem, off,
                        fn.ComparisonIndex, fn.Number);
                }

                if (fcName == "BoolProperty")
                {
                    // 搜索 BoolProperty 特有的 4 字节结构
                    // 范围从 Offset_Internal 之后到 +0x90
                    for (i32 t = searchStart;
                         t <= searchStart + 0x40; t += 4)
                    {
                        u8 fs = 0, bo = 0, bm = 0, fm = 0;
                        mem.Read(prop + t + 0, &fs, 1);
                        mem.Read(prop + t + 1, &bo, 1);
                        mem.Read(prop + t + 2, &bm, 1);
                        mem.Read(prop + t + 3, &fm, 1);
                        // FieldSize=1, ByteMask/FieldMask 非零
                        if (fs == 1 && bm != 0 && fm != 0)
                        {
                            off.BoolProperty_Base = t;
                            std::cerr
                                << "[xrd] BoolProperty::Base +0x"
                                << std::hex << t
                                << std::dec << "\n";
                            return true;
                        }
                    }
                }
            }
            ReadPtr(mem, prop + off.FField_Next, prop);
        }

        if (off.BoolProperty_Base != -1) break;
    }
    return off.BoolProperty_Base != -1;
}

// 汇总入口：发现所有 Property 基础偏移
inline void DiscoverPropertyBaseOffsets(
    const IMemoryAccessor& mem,
    UEOffsets& off)
{
    std::cerr << "[xrd] 开始发现 Property 基础偏移...\n";
    DiscoverPropertyElementSizeOffset(mem, off);
    DiscoverPropertyArrayDimOffset(mem, off);
    DiscoverPropertyOffsetInternalOffset(mem, off);
    DiscoverPropertyFlagsOffset(mem, off);
    DiscoverBoolPropertyBaseOffset(mem, off);
    std::cerr << "[xrd] Property 基础偏移发现完成\n";
}

} // namespace resolve
} // namespace xrd
