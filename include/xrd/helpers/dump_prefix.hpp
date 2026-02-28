#pragma once
// Xrd-eXternalrEsolve - 前缀工具和全局查找表
// 提供 AddStructPrefix / GetEntryLookup / LookupPrefixedName
// 被 dump_type_resolve.hpp 和 dump_sdk_struct.hpp 共同使用

#include "../core/types.hpp"
#include <string>
#include <unordered_map>

namespace xrd
{
namespace detail
{

// 收集结构体条目信息（前向声明，供查找表使用）
struct StructEntry
{
    uptr addr = 0;
    i32 objIndex = -1;          // GObjects 索引，用于排序
    i32 pkgIndex = -1;          // 包对象的 GObjects 索引（对标 Rei-Dumper GetPackageIndex）
    std::string name;
    std::string fullName;
    std::string outerName;  // 包名
    std::string superName;
    i32 superSize = 0;
    i32 size = 0;
    bool isClass = false;
    bool isActorChild = false;     // 继承自 Actor（含 Actor 自身）
    bool isInterfaceChild = false; // 继承自 Interface（含 Interface 自身）
    bool isFinal = true;           // 对标 Rei-Dumper：默认 final，被继承则非 final
    i32 alignment = 1;             // 对标 Rei-Dumper：struct/class 的对齐值
    i32 highestMemberAlign = 1;    // 自身属性的最高成员对齐（不含继承链）
    bool bUseExplicitAlignment = false; // MinAlignment > HighestMemberAlignment 时为 true
    bool bHasReusedTrailingPadding = false; // 对标 Rei-Dumper：子类成员侵入父类尾部 padding
    bool bCanSkipTrailingPad = false; // packed 类且 Align(lastMemberEnd, alignment)==size，子类可从 lastMemberEnd 开始
    i32 lastMemberEnd = 0;  // 对标 Rei-Dumper：最后一个成员的结束偏移
    i32 unalignedSize = 0;  // 对标 Rei-Dumper：未对齐的原始大小（可能被子类缩小）
    // 包名冲突时的命名空间前缀（如 "CommonFunction_0"）
    // 空字符串表示无冲突
    std::string collisionNs;
    // 对标 Rei-Dumper：UE 完整类型名（用于 BP_STATIC_CLASS_IMPL_FULLNAME）
    std::string objClassName;
};

// 参考 Rei-Dumper GetCppName: 总是加前缀，通过继承链判断 A/I/U/F
inline std::string AddStructPrefix(
    const std::string& name,
    bool isClass,
    bool isActorChild = false,
    bool isInterfaceChild = false)
{
    if (name.empty())
    {
        return name;
    }
    if (isClass)
    {
        if (isActorChild)
        {
            return "A" + name;
        }
        if (isInterfaceChild)
        {
            return "I" + name;
        }
        return "U" + name;
    }
    return "F" + name;
}

// 全局条目查找表（在导出前由 DumpCppSdk 填充）
// 用于在生成代码时查找 super 的 actor/interface 标记
inline std::unordered_map<std::string, const StructEntry*>& GetEntryLookup()
{
    static std::unordered_map<std::string, const StructEntry*> lookup;
    return lookup;
}

// 根据名字查找 super 的前缀
inline std::string LookupPrefixedName(
    const std::string& name, bool isClass)
{
    auto& lookup = GetEntryLookup();
    auto it = lookup.find(name);
    if (it != lookup.end())
    {
        return AddStructPrefix(
            name, it->second->isClass,
            it->second->isActorChild,
            it->second->isInterfaceChild);
    }
    // 回退：类默认 U 前缀，结构体默认 F 前缀
    return AddStructPrefix(name, isClass);
}

// 清理包名前缀（去除 /Script/ 等，取最后一个路径组件）
inline std::string StripPackagePrefix(const std::string& raw)
{
    std::string name = raw;
    for (auto& prefix : {"/Script/", "/Game/", "/Engine/"})
    {
        if (name.find(prefix) == 0)
        {
            name = name.substr(std::string(prefix).size());
            break;
        }
    }
    while (!name.empty() && name[0] == '/')
    {
        name = name.substr(1);
    }
    // 取最后一个路径组件
    auto lastSlash = name.rfind('/');
    if (lastSlash != std::string::npos)
    {
        name = name.substr(lastSlash + 1);
    }
    return name.empty() ? raw : name;
}

} // namespace detail
} // namespace xrd
