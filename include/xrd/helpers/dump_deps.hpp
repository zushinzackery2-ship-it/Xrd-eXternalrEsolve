#pragma once
// Xrd-eXternalrEsolve - 跨包依赖收集
// 对标 Rei-Dumper PackageManagerUtils::GetPropertyDependency
// 使用 FFieldClass::CastFlags 位域判断属性类型（避免字符串比较的远程读取开销）

#include "../core/context.hpp"
#include "../engine/objects.hpp"
#include "dump_prefix.hpp"
#include "dump_collect.hpp"
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>

namespace xrd
{
namespace detail
{

// EClassCastFlags 常量（对标 UE 引擎定义）
namespace ECF
{
    constexpr u64 ByteProperty                    = 0x0000000000000040ULL;
    constexpr u64 StructProperty                  = 0x0000000000100000ULL;
    constexpr u64 ArrayProperty                   = 0x0000000000200000ULL;
    constexpr u64 DelegateProperty                = 0x0000000000800000ULL;
    constexpr u64 MulticastDelegateProperty       = 0x0000000002000000ULL;
    constexpr u64 ObjectPropertyBase              = 0x0000000004000000ULL;
    constexpr u64 MapProperty                     = 0x0000400000000000ULL;
    constexpr u64 SetProperty                     = 0x0000800000000000ULL;
    constexpr u64 EnumProperty                    = 0x0001000000000000ULL;
    constexpr u64 MulticastInlineDelegateProperty = 0x0004000000000000ULL;
    constexpr u64 MulticastSparseDelegateProperty = 0x0008000000000000ULL;
    constexpr u64 OptionalProperty                = 0x0100000000000000ULL;
}

// UClass CastFlags 缓存（UClass 对象数量有限，缓存后避免重复远程读取）
inline std::unordered_map<uptr, u64>& GetUClassCastFlagsCache()
{
    static std::unordered_map<uptr, u64> cache;
    return cache;
}

// 读取 UObject 的 UClass::CastFlags（带缓存）
inline u64 ReadUClassCastFlags(uptr obj)
{
    if (!obj)
    {
        return 0;
    }
    uptr cls = GetObjectClass(obj);
    if (!cls)
    {
        return 0;
    }
    auto& cache = GetUClassCastFlagsCache();
    auto it = cache.find(cls);
    if (it != cache.end())
    {
        return it->second;
    }
    u64 flags = 0;
    auto& off = Off();
    if (off.UClass_CastFlags != -1)
    {
        GReadValue(cls + off.UClass_CastFlags, flags);
    }
    cache[cls] = flags;
    return flags;
}

// 依赖信息：某个包需要 include 哪些其他包的文件
struct PackageDeps
{
    struct DepInfo
    {
        bool needStructs = false;
        bool needClasses = false;
    };
    std::unordered_map<i32, DepInfo> deps;
};

// FFieldClass CastFlags 缓存（FFieldClass 全局只有几十个，缓存后避免重复远程读取）
inline std::unordered_map<uptr, u64>& GetFieldClassCache()
{
    static std::unordered_map<uptr, u64> cache;
    return cache;
}

// 读取 FField 的 CastFlags（带缓存优化）
// FField→Class 指针需要每次读取，但 Class→CastFlags 可以缓存
inline u64 ReadFieldCastFlags(uptr ffield)
{
    if (!ffield)
    {
        return 0;
    }
    uptr cls = 0;
    GReadPtr(ffield + Off().FField_Class, cls);
    if (!cls)
    {
        return 0;
    }
    auto& cache = GetFieldClassCache();
    auto it = cache.find(cls);
    if (it != cache.end())
    {
        return it->second;
    }
    u64 flags = 0;
    GReadValue(cls + Off().FFieldClass_CastFlags, flags);
    cache[cls] = flags;
    return flags;
}

// 从单个 FField 属性收集依赖的 struct/enum 的 GObjects 索引
// 对标 Rei-Dumper PackageManagerUtils::GetPropertyDependency
// 使用 CastFlags 位域代替字符串比较，大幅减少远程读取次数
inline void CollectPropertyDep(
    uptr prop,
    u64 castFlags,
    std::unordered_set<i32>& store)
{
    auto& off = Off();

    // 用栈模拟递归，避免深层嵌套导致栈溢出
    struct WorkItem
    {
        uptr prop;
        u64 flags;
    };
    std::vector<WorkItem> stack;
    stack.push_back({prop, castFlags});

    while (!stack.empty())
    {
        auto item = stack.back();
        stack.pop_back();

        if (!item.prop || !IsCanonicalUserPtr(item.prop))
        {
            continue;
        }

        // StructProperty → 底层 struct 索引
        if (item.flags & ECF::StructProperty)
        {
            if (off.StructProperty_Struct != -1)
            {
                uptr structPtr = 0;
                GReadPtr(
                    item.prop + off.StructProperty_Struct,
                    structPtr);
                if (IsCanonicalUserPtr(structPtr))
                {
                    i32 idx = GetObjectIndex(structPtr);
                    if (idx > 0) store.insert(idx);
                }
            }
            continue;
        }

        // EnumProperty → 关联 enum 索引
        if (item.flags & ECF::EnumProperty)
        {
            if (off.EnumProperty_Base != -1)
            {
                uptr enumPtr = 0;
                GReadPtr(
                    item.prop + off.EnumProperty_Base + 8,
                    enumPtr);
                if (IsCanonicalUserPtr(enumPtr))
                {
                    i32 idx = GetObjectIndex(enumPtr);
                    if (idx > 0) store.insert(idx);
                }
            }
            continue;
        }

        // ByteProperty → 可能关联 enum
        if (item.flags & ECF::ByteProperty)
        {
            if (off.ByteProperty_Enum != -1)
            {
                uptr enumPtr = 0;
                GReadPtr(
                    item.prop + off.ByteProperty_Enum,
                    enumPtr);
                if (IsCanonicalUserPtr(enumPtr))
                {
                    i32 idx = GetObjectIndex(enumPtr);
                    if (idx > 0) store.insert(idx);
                }
            }
            continue;
        }

        // ArrayProperty → 压入内部属性
        if (item.flags & ECF::ArrayProperty)
        {
            if (off.ArrayProperty_Inner != -1)
            {
                uptr innerProp = 0;
                GReadPtr(
                    item.prop + off.ArrayProperty_Inner,
                    innerProp);
                if (IsCanonicalUserPtr(innerProp))
                {
                    u64 innerFlags =
                        ReadFieldCastFlags(innerProp);
                    stack.push_back({innerProp, innerFlags});
                }
            }
            continue;
        }

        // SetProperty → 压入元素属性
        if (item.flags & ECF::SetProperty)
        {
            if (off.SetProperty_ElementProp != -1)
            {
                uptr elemProp = 0;
                GReadPtr(
                    item.prop + off.SetProperty_ElementProp,
                    elemProp);
                if (IsCanonicalUserPtr(elemProp))
                {
                    u64 elemFlags =
                        ReadFieldCastFlags(elemProp);
                    stack.push_back({elemProp, elemFlags});
                }
            }
            continue;
        }

        // MapProperty → 压入 key + value 属性
        if (item.flags & ECF::MapProperty)
        {
            if (off.MapProperty_Base != -1)
            {
                uptr keyProp = 0, valProp = 0;
                GReadPtr(
                    item.prop + off.MapProperty_Base,
                    keyProp);
                GReadPtr(
                    item.prop + off.MapProperty_Base + 8,
                    valProp);
                if (IsCanonicalUserPtr(keyProp))
                {
                    u64 kf = ReadFieldCastFlags(keyProp);
                    stack.push_back({keyProp, kf});
                }
                if (IsCanonicalUserPtr(valProp))
                {
                    u64 vf = ReadFieldCastFlags(valProp);
                    stack.push_back({valProp, vf});
                }
            }
            continue;
        }

        // OptionalProperty → 压入 value 属性
        // 排除 ObjectPropertyBase（对标 Rei-Dumper）
        if ((item.flags & ECF::OptionalProperty)
            && !(item.flags & ECF::ObjectPropertyBase))
        {
            if (off.StructProperty_Struct != -1)
            {
                uptr valProp = 0;
                GReadPtr(
                    item.prop + off.StructProperty_Struct,
                    valProp);
                if (IsCanonicalUserPtr(valProp))
                {
                    u64 vf = ReadFieldCastFlags(valProp);
                    stack.push_back({valProp, vf});
                }
            }
            continue;
        }

        // DelegateProperty / MulticastInlineDelegateProperty
        // / MulticastSparseDelegateProperty
        // → 压入签名函数的参数属性
        if ((item.flags & ECF::DelegateProperty)
            || (item.flags & ECF::MulticastInlineDelegateProperty)
            || (item.flags & ECF::MulticastSparseDelegateProperty))
        {
            if (off.DelegateProperty_Sig != -1)
            {
                uptr sigFunc = 0;
                GReadPtr(
                    item.prop + off.DelegateProperty_Sig,
                    sigFunc);
                // 验证指针确实指向 UFunction，防止垃圾指针导致无限遍历
                if (IsCanonicalUserPtr(sigFunc))
                {
                    // 检查 UObject::Class 名是否为 "Function"
                    uptr sigCls = 0;
                    GReadPtr(sigFunc + off.UObject_Class, sigCls);
                    bool isFuncObj = false;
                    if (IsCanonicalUserPtr(sigCls))
                    {
                        std::string cn = GetObjectName(sigCls);
                        isFuncObj = (cn == "Function"
                            || cn == "DelegateFunction"
                            || cn == "SparseDelegateFunction");
                    }
                    if (isFuncObj)
                    {
                        uptr sigProp =
                            GetChildProperties(sigFunc);
                        int sigDepth = 0;
                        while (sigProp && sigDepth < 128)
                        {
                            if (!IsCanonicalUserPtr(sigProp))
                            {
                                break;
                            }
                            u64 sf =
                                ReadFieldCastFlags(sigProp);
                            if (sf)
                            {
                                stack.push_back(
                                    {sigProp, sf});
                            }
                            sigProp = GetFFieldNext(sigProp);
                            sigDepth++;
                        }
                    }
                }
            }
            continue;
        }

        // 其他属性类型不产生依赖
    }
}

// 全局枚举名→GObjects索引查找表（枚举名不含 E 前缀）
// 用于缓存路径中查找枚举依赖
inline std::unordered_map<std::string, i32>&
GetEnumIndexLookup()
{
    static std::unordered_map<std::string, i32> lookup;
    return lookup;
}

// 收集一个 UStruct 的所有属性依赖
// 对标 Rei-Dumper PackageManagerUtils::GetDependencies
// 复用 CollectProperties 缓存，避免重复遍历 FField 链
inline std::unordered_set<i32> CollectStructDeps(uptr structObj)
{
    std::unordered_set<i32> deps;

    // 直接从已缓存的 PropertyInfo 中提取依赖的 struct/enum 名
    // 再通过全局查找表转换为 GObjects 索引
    // 这样完全避免了重复的 FField 链遍历（ReadProcessMemory）
    auto& props = detail::GetPropertiesCache();
    auto pit = props.find(structObj);
    if (pit == props.end())
    {
        // 缓存未命中时才走原始路径（理论上不应发生，因为对齐计算阶段已填充缓存）
        if (Off().bUseFProperty)
        {
            uptr prop = GetChildProperties(structObj);
            int depth = 0;
            while (prop && depth < 512)
            {
                if (!IsCanonicalUserPtr(prop))
                {
                    break;
                }
                u64 flags = ReadFieldCastFlags(prop);
                if (flags)
                {
                    CollectPropertyDep(prop, flags, deps);
                }
                prop = GetFFieldNext(prop);
                depth++;
            }
        }
    }
    else
    {
        // 缓存命中：从 typeName 中提取依赖名，再查 GObjects 索引
        auto& lookup = GetEntryLookup();
        for (auto& pi : pit->second)
        {
            // 从类型字符串中提取裸名（去掉 class/struct/前缀/指针）
            std::string t = pi.typeName;
            // 去掉 const
            if (t.find("const ") == 0) t = t.substr(6);
            // 去掉 class/struct 关键字
            if (t.find("class ") == 0) t = t.substr(6);
            if (t.find("struct ") == 0) t = t.substr(7);
            // 去掉尾部 */& 和空格
            while (!t.empty()
                && (t.back() == '*' || t.back() == '&'
                    || t.back() == ' '))
            {
                t.pop_back();
            }
            // 处理模板（TArray<X>、TMap<K,V> 等）
            // 递归提取所有尖括号内的类型名
            std::vector<std::string> toCheck;
            toCheck.push_back(t);
            while (!toCheck.empty())
            {
                std::string cur = toCheck.back();
                toCheck.pop_back();
                auto lt = cur.find('<');
                auto gt = cur.rfind('>');
                if (lt != std::string::npos
                    && gt != std::string::npos)
                {
                    std::string inner =
                        cur.substr(lt + 1, gt - lt - 1);
                    // 按逗号分割（忽略嵌套尖括号）
                    int depth2 = 0;
                    size_t start = 0;
                    for (size_t i = 0; i < inner.size(); ++i)
                    {
                        if (inner[i] == '<') depth2++;
                        else if (inner[i] == '>') depth2--;
                        else if (inner[i] == ','
                            && depth2 == 0)
                        {
                            std::string part =
                                inner.substr(start, i - start);
                            // 修剪前导空格
                            while (!part.empty() && part[0] == ' ')
                            {
                                part = part.substr(1);
                            }
                            toCheck.push_back(part);
                            start = i + 1;
                        }
                    }
                    {
                        std::string part = inner.substr(start);
                        // 修剪前导空格
                        while (!part.empty() && part[0] == ' ')
                        {
                            part = part.substr(1);
                        }
                        toCheck.push_back(part);
                    }
                    continue;
                }
                // 去掉前缀后查表
                if (cur.length() > 1)
                {
                    char prefix = cur[0];
                    if (prefix == 'U' || prefix == 'A'
                        || prefix == 'I' || prefix == 'F')
                    {
                        std::string bare = cur.substr(1);
                        auto lit = lookup.find(bare);
                        if (lit != lookup.end())
                        {
                            i32 idx = lit->second->objIndex;
                            if (idx > 0) deps.insert(idx);
                        }
                    }
                    // 枚举类型以 E 前缀开头，查枚举索引表
                    // 先用去掉 E 前缀的名字查（如 ECharacterModel → CharacterModel）
                    // 再用完整名查（如 Enum_Resolution 原名就以 E 开头）
                    if (prefix == 'E')
                    {
                        auto& enumLk = GetEnumIndexLookup();
                        std::string bare = cur.substr(1);
                        auto eit = enumLk.find(bare);
                        if (eit == enumLk.end())
                        {
                            eit = enumLk.find(cur);
                        }
                        if (eit != enumLk.end()
                            && eit->second > 0)
                        {
                            deps.insert(eit->second);
                        }
                    }
                }
            }
        }
    }

    // 排除自身索引
    i32 selfIdx = GetObjectIndex(structObj);
    deps.erase(selfIdx);

    return deps;
}

// 将 struct/enum 索引集合转换为包级别依赖
// 对标 Rei-Dumper PackageManagerUtils::SetPackageDependencies
inline void SetPackageDeps(
    PackageDeps& pkgDeps,
    const std::unordered_set<i32>& objDeps,
    i32 myPkgIndex,
    bool allowSelfPkg = false)
{
    for (i32 depIdx : objDeps)
    {
        uptr depObj = GetObjectByIndex(depIdx);
        if (!depObj)
        {
            continue;
        }
        i32 depPkgIdx = GetPackageIndex(depObj);
        if (depPkgIdx < 0)
        {
            continue;
        }
        if (!allowSelfPkg && depPkgIdx == myPkgIndex)
        {
            continue;
        }
        // 区分依赖类型：struct/enum → needStructs, class → needClasses
        std::string depClassName = GetObjectClassName(depObj);
        if (depClassName == "Class")
        {
            pkgDeps.deps[depPkgIdx].needClasses = true;
        }
        else
        {
            // ScriptStruct, Enum, UserDefinedEnum 等都放在 _structs.hpp
            pkgDeps.deps[depPkgIdx].needStructs = true;
        }
    }
}

// 将 enum 依赖添加到 classes 依赖列表
// 用缓存的 UClass::CastFlags 判断是否是 Enum
inline void AddEnumPkgDeps(
    PackageDeps& pkgDeps,
    const std::unordered_set<i32>& objDeps,
    i32 myPkgIndex,
    bool allowSelfPkg = false)
{
    constexpr u64 CASTCLASS_UEnum = 0x0000000000000004ULL;
    for (i32 depIdx : objDeps)
    {
        uptr depObj = GetObjectByIndex(depIdx);
        if (!depObj)
        {
            continue;
        }
        u64 classCF = ReadUClassCastFlags(depObj);
        if (!(classCF & CASTCLASS_UEnum))
        {
            continue;
        }
        i32 depPkgIdx = GetPackageIndex(depObj);
        if (depPkgIdx < 0)
        {
            continue;
        }
        if (!allowSelfPkg && depPkgIdx == myPkgIndex)
        {
            continue;
        }
        pkgDeps.deps[depPkgIdx].needStructs = true;
    }
}

// 收集一个 struct/class 条目的完整包依赖
// 对标 Rei-Dumper PackageManager::InitDependencies
inline void CollectEntryPackageDeps(
    const StructEntry& entry,
    i32 myPkgIndex,
    PackageDeps& structsDeps,
    PackageDeps& classesDeps,
    PackageDeps& paramsDeps)
{
    bool isClass = entry.isClass;
    PackageDeps& targetDeps = isClass
        ? classesDeps : structsDeps;

    // 属性依赖
    auto objDeps = CollectStructDeps(entry.addr);
    SetPackageDeps(targetDeps, objDeps, myPkgIndex);

    // super 类型依赖
    if (!entry.superName.empty())
    {
        auto& lookup = GetEntryLookup();
        auto it = lookup.find(entry.superName);
        if (it != lookup.end())
        {
            i32 superPkgIdx = it->second->pkgIndex;
            if (superPkgIdx >= 0
                && superPkgIdx != myPkgIndex)
            {
                auto& req = targetDeps.deps[superPkgIdx];
                if (isClass)
                {
                    req.needClasses = true;
                }
                else
                {
                    req.needStructs = true;
                }
            }
        }
    }

    // 类的函数参数依赖
    // 跳过：远程遍历 Children 链太慢（每个 GetFieldNext 一次 ReadProcessMemory）
    // 改为在写 parameters 文件时合并 structs+classes 依赖
    // Rei-Dumper 的 parameters include 列表实际上也是 structs 依赖的子集
}

} // namespace detail
} // namespace xrd
