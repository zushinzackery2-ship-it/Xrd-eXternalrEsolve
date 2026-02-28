#pragma once
// Xrd-eXternalrEsolve - Enum 收集与导出
// 遍历 GObjects 中的 UEnum 对象，读取枚举成员名和值

#include "../core/context.hpp"
#include "../engine/objects.hpp"
#include "../engine/names.hpp"
#include <string>
#include <vector>
#include <iostream>

namespace xrd
{
namespace detail
{

struct EnumMember
{
    std::string name;
    i64 value = 0;
};

struct EnumInfo
{
    uptr addr = 0;
    i32 pkgIndex = -1;  // 包对象的 GObjects 索引
    std::string name;
    std::string outerName;
    std::vector<EnumMember> members;
    u8 underlyingTypeSize = 1; // 枚举底层类型大小，由属性的 ElementSize 决定
};

// 从 UEnum 对象读取枚举成员
// UEnum::Names 是一个 TArray<TPair<FName, int64>>
inline std::vector<EnumMember> ReadEnumMembers(uptr enumObj)
{
    std::vector<EnumMember> result;
    if (!enumObj || Off().UEnum_Names == -1)
    {
        return result;
    }

    // TArray 布局: +0x00 Data*, +0x08 Count (i32), +0x0C Max (i32)
    uptr data = 0;
    i32 count = 0;
    GReadPtr(enumObj + Off().UEnum_Names, data);
    GReadI32(enumObj + Off().UEnum_Names + 8, count);

    if (!IsCanonicalUserPtr(data) || count <= 0 || count > 1024)
    {
        return result;
    }

    // 每个元素是 TPair<FName, int64> = 16 字节
    constexpr i32 kPairSize = 16;

    for (i32 i = 0; i < count; ++i)
    {
        uptr pairAddr = data + i * kPairSize;

        FName fname{};
        i64 value = 0;
        if (!GReadValue(pairAddr, fname))
        {
            continue;
        }
        GReadValue(pairAddr + 8, value);

        std::string name = GetNameFromFName(fname.ComparisonIndex, fname.Number);
        if (name.empty())
        {
            continue;
        }

        // 去掉 "EnumName::" 前缀（如果有的话）
        auto colonPos = name.rfind("::");
        if (colonPos != std::string::npos)
        {
            name = name.substr(colonPos + 2);
        }

        result.push_back({std::move(name), value});
    }

    return result;
}

// 收集所有 UEnum 对象
inline std::vector<EnumInfo> CollectAllEnums()
{
    std::vector<EnumInfo> enums;
    i32 total = GetTotalObjectCount();

    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = GetObjectByIndex(i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        std::string className = GetObjectClassName(obj);
        if (className != "Enum" && className != "UserDefinedEnum")
        {
            continue;
        }

        EnumInfo ei;
        ei.addr = obj;
        ei.name = GetObjectName(obj);
        if (ei.name.empty())
        {
            continue;
        }

        uptr pkg = GetOutermostOuter(obj);
        ei.outerName = pkg ? GetObjectName(pkg) : "Global";
        ei.pkgIndex = pkg ? GetObjectIndex(pkg) : -1;
        ei.members = ReadEnumMembers(obj);

        if (!ei.members.empty())
        {
            enums.push_back(std::move(ei));
        }
    }

    std::cerr << "[xrd] 收集到 " << enums.size() << " 个 Enum\n";
    return enums;
}

} // namespace detail
} // namespace xrd
