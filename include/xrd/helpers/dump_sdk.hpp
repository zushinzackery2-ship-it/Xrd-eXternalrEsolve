#pragma once
// Xrd-eXternalrEsolve - SDK 导出主入口
// 对标 Dumper7 品质的 C++ SDK 导出
// 目录结构：CppSDK/SDK/*.hpp + CppSDK/*.hpp（辅助文件）
// 每个包生成 4 个文件：_classes.hpp / _structs.hpp / _functions.cpp / _parameters.hpp

#include "../core/context.hpp"
#include "../engine/objects.hpp"
#include "dump_sdk_struct.hpp"
#include "dump_sdk_infra.hpp"
#include "dump_sdk_writer.hpp"
#include "dump_dep_sort.hpp"
#include <fstream>
#include <filesystem>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <functional>
#include <chrono>
#include <iomanip>
#include <climits>

namespace xrd
{

// 清理包名：去除 /Script/ 等前缀，取最后一个路径组件
inline std::string CleanPackageName(const std::string& raw)
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
    auto lastSlash = name.rfind('/');
    if (lastSlash != std::string::npos)
    {
        name = name.substr(lastSlash + 1);
    }
    return name.empty() ? "Unknown" : name;
}

// 对标 Dumper7 GetValidName：数字开头的名字替换首字符为英文
inline std::string MakeValidCppName(const std::string& name)
{
    if (name.empty())
    {
        return name;
    }
    if (name[0] >= '0' && name[0] <= '9')
    {
        static const char* digits[] = {
            "Zero", "One", "Two", "Three", "Four",
            "Five", "Six", "Seven", "Eight", "Nine"
        };
        return std::string(digits[name[0] - '0']) + name.substr(1);
    }
    return name;
}

// 清理包名为合法文件名
inline std::string SanitizePackageName(const std::string& raw)
{
    std::string name = CleanPackageName(raw);
    name = MakeValidCppName(name);
    for (auto& c : name)
    {
        if (c == '/' || c == '\\')
        {
            c = '_';
        }
    }
    if (name.empty())
    {
        name = "Unknown";
    }
    return name;
}

// 检查一个类是否继承自指定的祖先类
inline bool IsChildOf(uptr cls, uptr ancestor)
{
    if (!ancestor) return false;
    uptr s = GetSuperStruct(cls);
    int depth = 0;
    while (s && depth < 64)
    {
        if (s == ancestor) return true;
        s = GetSuperStruct(s);
        depth++;
    }
    return false;
}

// 收集所有需要导出的结构体/类
inline std::vector<detail::StructEntry> CollectAllStructEntries()
{
    std::vector<detail::StructEntry> entries;
    i32 total = GetTotalObjectCount();

    uptr actorClass = 0;
    uptr interfaceClass = 0;
    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = GetObjectByIndex(i);
        if (!IsCanonicalUserPtr(obj)) continue;
        if (GetObjectClassName(obj) != "Class") continue;
        std::string name = GetObjectName(obj);
        if (name == "Actor" && !actorClass)
        {
            actorClass = obj;
        }
        if (name == "Interface" && !interfaceClass)
        {
            interfaceClass = obj;
        }
        if (actorClass && interfaceClass) break;
    }

    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = GetObjectByIndex(i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }

        std::string className = GetObjectClassName(obj);
        bool isClass  = (className == "Class" ||
                         className == "BlueprintGeneratedClass" ||
                         className == "WidgetBlueprintGeneratedClass" ||
                         className == "AnimBlueprintGeneratedClass" ||
                         className == "DynamicClass");
        bool isStruct = (className == "ScriptStruct"
                         || className == "Struct"
                         || className == "UserDefinedStruct");
        if (!isClass && !isStruct)
        {
            continue;
        }

        detail::StructEntry entry;
        entry.addr    = obj;
        entry.objIndex = i;  // GObjects 索引
        entry.name    = GetObjectName(obj);
        entry.size    = GetStructSize(obj);
        entry.isClass = isClass;

        if (entry.name.empty() || entry.size <= 0)
        {
            continue;
        }

        uptr super = GetSuperStruct(obj);
        if (super)
        {
            entry.superName = GetObjectName(super);
            entry.superSize = GetStructSize(super);
        }

        if (isClass)
        {
            entry.isActorChild =
                (obj == actorClass)
                || IsChildOf(obj, actorClass);
            entry.isInterfaceChild =
                !entry.isActorChild &&
                ((obj == interfaceClass)
                 || IsChildOf(obj, interfaceClass));
        }

        entry.fullName = GetObjectFullName(obj);

        // 缓存 UE 类型名（避免后续重复读取）
        entry.objClassName = className;

        uptr pkg = GetOutermostOuter(obj);
        entry.outerName = pkg ? GetObjectName(pkg) : "Global";
        entry.pkgIndex = pkg ? GetObjectIndex(pkg) : -1;

        entries.push_back(std::move(entry));
    }

    // 对标 Rei-Dumper InitSizesAndIsFinal：
    // 被其他结构体/类继承的条目标记为非 final
    std::unordered_set<std::string> superNames;
    for (auto& e : entries)
    {
        if (!e.superName.empty())
        {
            superNames.insert(e.superName);
        }
    }
    for (auto& e : entries)
    {
        if (superNames.count(e.name))
        {
            e.isFinal = false;
        }
    }

    // 对标 Rei-Dumper InitAlignmentsAndNames：
    // 计算每个 struct/class 的对齐值
    constexpr i32 defaultClassAlign = sizeof(void*); // 0x8
    std::cerr << "[xrd] 计算对齐值...\n";
    for (auto& e : entries)
    {
        // Interface 子类：alignment=1, size=0, superSize=0
        // 对标 Rei-Dumper：接口类不继承 UObject，视为空类
        if (e.isInterfaceChild)
        {
            e.alignment = 1;
            e.size = 0;
            e.superSize = 0;
            continue;
        }

        i32 minAlign = GetStructMinAlignment(e.addr);
        i32 highestMemberAlign = 1;

        // 遍历 FField 属性链，找最大对齐
        // GetPropertyAlignment 内部使用 CastFlags 缓存，无字符串读取开销
        if (Off().bUseFProperty)
        {
            uptr prop = GetChildProperties(e.addr);
            while (prop)
            {
                i32 propAlign = GetPropertyAlignment(prop);
                if (propAlign > highestMemberAlign)
                {
                    highestMemberAlign = propAlign;
                }
                prop = GetFFieldNext(prop);
            }
        }

        bool hasSuperClass = !e.superName.empty();

        // 保存自身属性的最高成员对齐
        e.highestMemberAlign = highestMemberAlign;

        // class: minAlign > defaultClassAlign(8) 时加 #pragma pack + alignas
        // struct: minAlign > highestMemberAlign 时加
        i32 effectiveAlign = std::max(minAlign, highestMemberAlign);
        if (e.isClass && hasSuperClass
            && effectiveAlign < defaultClassAlign)
        {
            effectiveAlign = defaultClassAlign;
        }
        e.alignment = effectiveAlign;
        if (e.isClass)
        {
            e.bUseExplicitAlignment =
                (minAlign > defaultClassAlign);
        }
        else
        {
            e.bUseExplicitAlignment =
                (minAlign > highestMemberAlign);
        }
    }

    // 第二遍：沿继承链向下传播 alignment（不传播 highestMemberAlign）
    // bUseExplicitAlignment 只看本类自身成员的最大对齐
    std::cerr << "[xrd] 传播继承链对齐值...\n";
    std::unordered_map<std::string, detail::StructEntry*>
        nameToEntry;
    for (auto& e : entries)
    {
        nameToEntry[e.name] = &e;
    }
    for (auto& e : entries)
    {
        if (e.isInterfaceChild)
        {
            continue;
        }
        // 收集继承链（从当前到最顶层）
        std::vector<detail::StructEntry*> chain;
        chain.push_back(&e);
        std::string cur = e.superName;
        int depth = 0;
        while (!cur.empty() && depth < 64)
        {
            auto it = nameToEntry.find(cur);
            if (it == nameToEntry.end()) break;
            chain.push_back(it->second);
            cur = it->second->superName;
            depth++;
        }
        // 从顶层向下传播：只传播 alignment
        i32 curHighestAlign = 0;
        for (int i = (int)chain.size() - 1; i >= 0; i--)
        {
            if (curHighestAlign < chain[i]->alignment)
            {
                curHighestAlign = chain[i]->alignment;
            }
            else if (curHighestAlign > chain[i]->alignment)
            {
                chain[i]->alignment = curHighestAlign;
            }
        }
    }

    return entries;
}

// 按包索引分组（对标 Rei-Dumper：使用 unordered_map<i32>）
inline std::unordered_map<i32,
    std::vector<const detail::StructEntry*>>
GroupByPackageIndex(
    const std::vector<detail::StructEntry>& entries)
{
    std::unordered_map<i32,
        std::vector<const detail::StructEntry*>> pkgMap;
    for (auto& e : entries)
    {
        pkgMap[e.pkgIndex].push_back(&e);
    }
    return pkgMap;
}

// 对包列表进行依赖排序（DFS 拓扑排序）
// 对标 Rei-Dumper IterateDependencies：
// 使用 unordered_map<i32> 存储包依赖图
// 遍历顺序由 MSVC 哈希桶决定，与 Rei-Dumper 一致
inline std::vector<i32> TopoSortPackagesByIndex(
    const std::unordered_map<i32, detail::PackageDeps>& cDeps,
    const std::unordered_map<i32, detail::PackageDeps>& sDeps,
    const std::unordered_set<i32>& allPkgIndices)
{
    if (allPkgIndices.size() <= 1)
    {
        return std::vector<i32>(
            allPkgIndices.begin(), allPkgIndices.end());
    }

    // 构建依赖图：unordered_map<i32, unordered_set<i32>>
    // 对标 Rei-Dumper PackageInfos 的遍历
    std::unordered_map<i32,
        std::unordered_set<i32>> depGraph;

    for (i32 pkg : allPkgIndices)
    {
        depGraph[pkg]; // SetExists
    }

    for (i32 pkg : allPkgIndices)
    {
        auto cit = cDeps.find(pkg);
        if (cit != cDeps.end())
        {
            for (auto& [dep, info] : cit->second.deps)
            {
                if (allPkgIndices.count(dep) && dep != pkg)
                {
                    depGraph[pkg].insert(dep);
                }
            }
        }
        auto sit = sDeps.find(pkg);
        if (sit != sDeps.end())
        {
            for (auto& [dep, info] : sit->second.deps)
            {
                if (allPkgIndices.count(dep) && dep != pkg)
                {
                    depGraph[pkg].insert(dep);
                }
            }
        }
    }

    std::vector<i32> result;
    result.reserve(allPkgIndices.size());
    std::unordered_set<i32> visited;
    std::unordered_set<i32> inStack; // 循环检测

    std::function<void(i32)> visit;
    visit = [&](i32 idx)
    {
        if (visited.count(idx))
        {
            return;
        }
        if (inStack.count(idx))
        {
            // 检测到循环依赖，跳过
            return;
        }
        inStack.insert(idx);
        auto it = depGraph.find(idx);
        if (it != depGraph.end())
        {
            for (i32 dep : it->second)
            {
                visit(dep);
            }
        }
        inStack.erase(idx);
        visited.insert(idx);
        result.push_back(idx);
    };

    // 遍历 unordered_map<i32>，顺序由 MSVC 哈希桶决定
    for (const auto& [idx, deps] : depGraph)
    {
        visit(idx);
    }
    return result;
}

// ─── 主导出函数：Dumper7 品质 C++ SDK ───
inline bool DumpCppSdk(const std::wstring& outputPath)
{
    if (!IsInited())
    {
        std::cerr << "[xrd] 未初始化，无法导出 SDK\n";
        return false;
    }

    namespace fs = std::filesystem;
    using Clock = std::chrono::steady_clock;
    auto t0 = Clock::now();
    auto elapsed = [&]() -> double {
        return std::chrono::duration<double>(Clock::now() - t0).count();
    };

    std::wstring cppSdkDir = outputPath + L"/CppSDK";
    std::wstring sdkDir    = cppSdkDir + L"/SDK";
    fs::create_directories(sdkDir);

    std::cerr << "[xrd] 开始导出 Dumper7 品质 C++ SDK...\n";

    // 收集枚举（按包索引分组）
    auto allEnums = detail::CollectAllEnums();
    std::unordered_map<i32,
        std::vector<detail::EnumInfo>> enumsByPkgIdx;
    for (auto& ei : allEnums)
    {
        enumsByPkgIdx[ei.pkgIndex].push_back(ei);
    }
    // 填充枚举名→GObjects索引查找表（供依赖收集缓存路径使用）
    {
        auto& enumLk = detail::GetEnumIndexLookup();
        enumLk.clear();
        for (auto& ei : allEnums)
        {
            i32 idx = GetObjectIndex(ei.addr);
            if (idx > 0)
            {
                enumLk[ei.name] = idx;
            }
        }
    }
    std::cerr << std::fixed << std::setprecision(2)
              << "[xrd] 枚举收集完成 (" << elapsed() << "s)\n";
    std::cerr.flush();

    // 收集结构体/类
    auto entries = CollectAllStructEntries();
    auto pkgMap = GroupByPackageIndex(entries);

    std::cerr << "[xrd] 找到 " << entries.size()
              << " 个类/结构体, "
              << allEnums.size() << " 个枚举, "
              << pkgMap.size() << " 个包 ("
              << elapsed() << "s)\n";
    std::cerr.flush();    // 填充全局查找表
    auto& lookup = detail::GetEntryLookup();
    lookup.clear();
    for (auto& e : entries)
    {
        lookup[e.name] = &e;
    }
    std::cerr << "[xrd] 查找表填充完成 (" << elapsed() << "s)\n";
    std::cerr.flush();

    // 预热属性缓存
    std::cerr << "[xrd] 预热缓存...\n";
    std::cerr.flush();
    detail::ClearPropertiesCache();
    detail::ClearFunctionsCache();
    i32 warmupCount = 0;
    for (auto& e : entries)
    {
        detail::CollectProperties(e.addr);
        if (e.isClass)
        {
            detail::CollectFunctions(e.addr);
        }
        warmupCount++;
        if (warmupCount % 1000 == 0)
        {
            std::cerr << "[xrd] 预热: "
                << warmupCount << "/" << entries.size()
                << " (" << elapsed() << "s)\n";
            std::cerr.flush();
        }
    }
    std::cerr << "[xrd] 缓存预热完成 ("
              << elapsed() << "s)\n";
    std::cerr.flush();

    // 对标 Rei-Dumper InitSizesAndIsFinal：
    // 计算 lastMemberEnd / unalignedSize / bHasReusedTrailingPadding
    // 当子类属性侵入父类尾部 padding 时，需要用 #pragma pack(push, 0x1) + alignas
    {
        auto& propCache = detail::GetPropertiesCache();
        // 第一遍：计算每个 entry 的 lastMemberEnd
        for (auto& e : entries)
        {
            if (e.isInterfaceChild) continue;

            auto cit = propCache.find(e.addr);
            if (cit == propCache.end() || cit->second.empty())
            {
                e.lastMemberEnd = e.superSize;
                e.unalignedSize = e.size;
                continue;
            }

            i32 maxEnd = e.superSize;
            for (auto& pi : cit->second)
            {
                i32 memberEnd = pi.offset + (pi.size * pi.arrayDim);
                if (memberEnd > maxEnd)
                {
                    maxEnd = memberEnd;
                }
            }
            e.lastMemberEnd = maxEnd;
            // unalignedSize 初始等于 aligned size，
            // 只有被检测到 bHasReusedTrailingPadding 时才被缩短
            e.unalignedSize = e.size;
        }

        // 建立可修改的 name → StructEntry* 映射
        std::unordered_map<std::string, detail::StructEntry*>
            mutableLookup;
        for (auto& e : entries)
        {
            mutableLookup[e.name] = &e;
        }

        // 第二遍：检测子类是否侵入父类尾部 padding
        for (auto& e : entries)
        {
            if (e.isInterfaceChild || e.superName.empty()) continue;

            auto cit = propCache.find(e.addr);
            if (cit == propCache.end() || cit->second.empty()) continue;

            // 找到最小的属性偏移
            i32 minPropOffset = e.size;
            for (auto& pi : cit->second)
            {
                if (pi.offset < minPropOffset)
                {
                    minPropOffset = pi.offset;
                }
            }

            // 如果子类最小属性偏移 < 父类 aligned size，说明侵入了父类尾部 padding
            if (minPropOffset < e.superSize)
            {
                // 在父类 entry 中标记 bHasReusedTrailingPadding
                auto superIt = mutableLookup.find(e.superName);
                if (superIt != mutableLookup.end())
                {
                    auto* superEntry = superIt->second;
                    // 父类的 unalignedSize 应缩短到子类最小属性偏移处
                    if (!superEntry->bHasReusedTrailingPadding
                        || minPropOffset < superEntry->unalignedSize)
                    {
                        superEntry->bHasReusedTrailingPadding = true;
                        superEntry->unalignedSize = minPropOffset;
                    }
                }
            }
        }
        // 第三遍：计算 bCanSkipTrailingPad
        // packed 类如果 Align(lastMemberEnd, alignment) == size，
        // 说明 alignas 足以提供 trailing pad，不需要显式 trailing pad 成员
        // 此时子类可从 lastMemberEnd 开始生成显式 pad
        for (auto& e : entries)
        {
            if (e.isInterfaceChild || e.bHasReusedTrailingPadding)
                continue;
            // 只对 class 生效：class 的 bUseExplicitAlignment 同时控制 #pragma pack
            // struct 的 pack 由 bHasReusedTrailingPadding 控制，不在此处理
            if (!e.isClass || !e.bUseExplicitAlignment)
                continue;
            i32 a = e.alignment > 0 ? e.alignment : 1;
            i32 aligned = (e.lastMemberEnd + a - 1) & ~(a - 1);
            if (aligned == e.size)
            {
                e.bCanSkipTrailingPad = true;
            }
        }

        std::cerr << "[xrd] 尾部 padding 重用检测完成 ("
            << elapsed() << "s)\n";
        std::cerr.flush();
    }

    // 从属性缓存中检测枚举底层类型大小
    // 对标 Rei-Dumper：遍历所有 ByteProperty/EnumProperty，用 ElementSize 确定枚举大小
    {
        // 建立 enum 名 → EnumInfo* 的查找表
        std::unordered_map<std::string, std::vector<detail::EnumInfo*>> enumByName;
        for (auto& ei : allEnums)
        {
            enumByName[ei.name].push_back(&ei);
        }

        auto& propCache = detail::GetPropertiesCache();
        for (auto& [structAddr, props] : propCache)
        {
            for (auto& pi : props)
            {
                // EnumProperty: typeName 就是枚举名
                if (pi.fieldClassName == "EnumProperty")
                {
                    auto eit = enumByName.find(pi.typeName);
                    if (eit != enumByName.end())
                    {
                        for (auto* ep : eit->second)
                        {
                            if (pi.size > 0 && (u8)pi.size > ep->underlyingTypeSize)
                            {
                                ep->underlyingTypeSize = (u8)pi.size;
                            }
                        }
                    }
                }
                // ByteProperty 关联枚举: typeName 是枚举名（非 "uint8"）
                else if (pi.fieldClassName == "ByteProperty" && pi.typeName != "uint8")
                {
                    auto eit = enumByName.find(pi.typeName);
                    if (eit != enumByName.end())
                    {
                        for (auto* ep : eit->second)
                        {
                            if (pi.size > 0 && (u8)pi.size > ep->underlyingTypeSize)
                            {
                                ep->underlyingTypeSize = (u8)pi.size;
                            }
                        }
                    }
                }
            }
        }
        std::cerr << "[xrd] 枚举底层类型检测完成 ("
            << elapsed() << "s)\n";
        std::cerr.flush();

        // 检测完成后重建 enumsByPkgIdx（之前的是检测前的副本）
        enumsByPkgIdx.clear();
        for (auto& ei : allEnums)
        {
            enumsByPkgIdx[ei.pkgIndex].push_back(ei);
        }
    }

    // 合并所有包索引
    std::unordered_set<i32> allPkgIndices;
    for (auto& [k, v] : pkgMap)
    {
        allPkgIndices.insert(k);
    }
    for (auto& [k, v] : enumsByPkgIdx)
    {
        allPkgIndices.insert(k);
    }

    // 构建 pkgIndex → outerName 映射
    std::unordered_map<i32, std::string> pkgIdxToOuter;
    for (auto& e : entries)
    {
        pkgIdxToOuter[e.pkgIndex] = e.outerName;
    }
    for (auto& ei : allEnums)
    {
        if (pkgIdxToOuter.find(ei.pkgIndex)
            == pkgIdxToOuter.end())
        {
            pkgIdxToOuter[ei.pkgIndex] = ei.outerName;
        }
    }

    // 构建 pkgIndex → sanitizedName 映射
    // 对标 Rei-Dumper：检测包名冲突，冲突时加 _N 后缀
    std::unordered_map<i32, std::string> pkgIdxToSanitized;
    {
        // 先统计每个 sanitized 名出现的次数
        std::map<std::string, std::vector<i32>> sanToIndices;
        for (i32 idx : allPkgIndices)
        {
            auto oit = pkgIdxToOuter.find(idx);
            std::string outer = (oit != pkgIdxToOuter.end())
                ? oit->second : "Unknown";
            std::string san = SanitizePackageName(outer);
            sanToIndices[san].push_back(idx);
        }
        for (auto& [san, indices] : sanToIndices)
        {
            if (indices.size() == 1)
            {
                pkgIdxToSanitized[indices[0]] = san;
            }
            else
            {
                pkgIdxToSanitized[indices[0]] = san;
                for (size_t i = 1; i < indices.size(); ++i)
                {
                    pkgIdxToSanitized[indices[i]] =
                        san + "_" + std::to_string(i - 1);
                }
            }
        }
    }

    // 构建冲突包的命名空间映射
    std::map<std::string, std::string> collisionNsMap;
    std::unordered_map<i32, std::string> collisionNsByIdx;
    {
        std::map<std::string, int> sanCount;
        for (auto& [idx, san] : pkgIdxToSanitized)
        {
            auto oit = pkgIdxToOuter.find(idx);
            std::string outer = (oit != pkgIdxToOuter.end())
                ? oit->second : "Unknown";
            std::string baseSan = SanitizePackageName(outer);
            sanCount[baseSan]++;
        }
        for (auto& [idx, san] : pkgIdxToSanitized)
        {
            auto oit = pkgIdxToOuter.find(idx);
            std::string outer = (oit != pkgIdxToOuter.end())
                ? oit->second : "Unknown";
            std::string baseSan = SanitizePackageName(outer);
            if (sanCount[baseSan] > 1)
            {
                collisionNsMap[outer] = san;
                collisionNsByIdx[idx] = san;
            }
        }
    }

    // 将冲突信息写入 entries
    for (auto& e : entries)
    {
        auto cit = collisionNsByIdx.find(e.pkgIndex);
        if (cit != collisionNsByIdx.end())
        {
            e.collisionNs = cit->second;
        }
    }

    // 生成辅助文件到 CppSDK/ 层级
    detail::GenerateAuxiliaryFiles(cppSdkDir, collisionNsMap,
        entries);
    std::cerr << "[xrd] 辅助文件生成完成 (" << elapsed() << "s)\n";
    std::cerr.flush();

    // 收集跨包依赖（key 为包的 GObjects 索引）
    std::cerr << "[xrd] 收集跨包依赖 ("
        << entries.size() << " 条目)...\n";
    std::cerr.flush();
    std::unordered_map<i32, detail::PackageDeps> classesDeps;
    std::unordered_map<i32, detail::PackageDeps> structsDeps;

    i32 depProgress = 0;
    i32 entryProgress = 0;
    i32 totalEntries = (i32)entries.size();
    for (i32 pkgIdx : allPkgIndices)
    {
        auto structIt = pkgMap.find(pkgIdx);
        if (structIt == pkgMap.end())
        {
            depProgress++;
            continue;
        }

        // paramsDeps 参数传入一个临时占位，实际不使用
        detail::PackageDeps unusedParams;
        for (auto* entry : structIt->second)
        {
            try
            {
                detail::CollectEntryPackageDeps(
                    *entry, pkgIdx,
                    structsDeps[pkgIdx],
                    classesDeps[pkgIdx],
                    unusedParams);
            }
            catch (...)
            {
                std::cerr << "[xrd] 依赖收集异常: "
                    << entry->name << " (idx="
                    << entry->objIndex << ")\n";
            }
            entryProgress++;
            if (entryProgress % 500 == 0)
            {
                std::cerr << "[xrd] 依赖进度: "
                    << entryProgress << "/"
                    << totalEntries << " 条目\n";
                std::cerr.flush();
            }
        }
        depProgress++;
    }
    std::cerr << "[xrd] 依赖收集完成 ("
        << entryProgress << " 条目)\n";
    std::cerr.flush();

    // 打开 Assertions.inl
    std::wstring assertPath = cppSdkDir + L"/Assertions.inl";
    std::ofstream assertFile(assertPath, std::ios::app);

    // SDK.hpp 的 include 列表
    std::vector<std::string> sdkIncludes;

    // 对标 Rei-Dumper：按依赖顺序排列包
    std::cerr << "[xrd] 开始拓扑排序 ("
        << allPkgIndices.size() << " 个包)...\n";
    std::cerr.flush();
    auto sortedPkgs = TopoSortPackagesByIndex(
        classesDeps, structsDeps, allPkgIndices);
    std::cerr << "[xrd] 拓扑排序完成 ("
        << sortedPkgs.size() << " 个包)\n";
    std::cerr.flush();

    // 预计算哪些包有 structs 文件（有非 class 条目或有枚举）
    std::unordered_set<i32> pkgsWithStructs;
    for (auto& [pi, ents] : pkgMap)
    {
        for (auto* e : ents)
        {
            if (!e->isClass)
            {
                pkgsWithStructs.insert(pi);
                break;
            }
        }
    }
    for (auto& [pi, _] : enumsByPkgIdx)
    {
        pkgsWithStructs.insert(pi);
    }

    i32 pkgCount = 0;
    i32 totalPkgs = (i32)sortedPkgs.size();
    for (i32 pkgIdx : sortedPkgs)
    {
        std::string sanitized = pkgIdxToSanitized[pkgIdx];
        auto oit = pkgIdxToOuter.find(pkgIdx);
        std::string outerName = (oit != pkgIdxToOuter.end())
            ? oit->second : "Unknown";
        std::string cleanPkg = CleanPackageName(outerName);

        auto structIt = pkgMap.find(pkgIdx);
        auto enumIt   = enumsByPkgIdx.find(pkgIdx);

        std::vector<const detail::StructEntry*> classes;
        std::vector<const detail::StructEntry*> structs;
        if (structIt != pkgMap.end())
        {
            for (auto* e : structIt->second)
            {
                if (e->isClass)
                {
                    classes.push_back(e);
                }
                else
                {
                    structs.push_back(e);
                }
            }
        }

        if (pkgCount % 50 == 0)
        {
            std::cerr << "[xrd] 写入进度: "
                << pkgCount << "/" << totalPkgs
                << " (" << elapsed() << "s)\n";
            std::cerr.flush();
        }

        // 对标 Rei-Dumper：包内按依赖拓扑排序
        structs = detail::TopoSortEntries(structs);
        classes = detail::TopoSortEntries(classes);

        std::vector<detail::EnumInfo> emptyEnums;
        const auto& pkgEnums = (enumIt != enumsByPkgIdx.end())
            ? enumIt->second : emptyEnums;

        // 获取依赖
        detail::PackageDeps sDeps, cDeps, pDeps;
        auto sit = structsDeps.find(pkgIdx);
        if (sit != structsDeps.end())
        {
            sDeps = sit->second;
        }
        auto cit = classesDeps.find(pkgIdx);
        if (cit != classesDeps.end())
        {
            cDeps = cit->second;
        }
        // parameters 文件的依赖 = structs 依赖 ∪ classes 依赖
        // 因为函数参数可能引用任意 struct/class/enum
        // 对标 Rei-Dumper：parameters include 是 structs+classes 的超集
        pDeps = sDeps;
        for (auto& [depIdx, info] : cDeps.deps)
        {
            auto& pInfo = pDeps.deps[depIdx];
            if (info.needStructs) pInfo.needStructs = true;
            if (info.needClasses) pInfo.needClasses = true;
        }

        bool hasStructsFile = !structs.empty()
            || !pkgEnums.empty();

        // 检查类是否依赖同包的结构体或枚举
        // _structs.hpp 同时包含结构体和枚举定义
        // 需要同时检查两者的依赖
        bool classesNeedOwnStructs = false;
        if (hasStructsFile && !classes.empty())
        {
            // 收集同包的结构体裸名
            std::unordered_set<std::string> structNames;
            for (auto* s : structs)
            {
                structNames.insert(s->name);
            }
            // 收集同包的枚举名（枚举也在 _structs.hpp 中）
            std::unordered_set<std::string> enumNames;
            for (auto& ei : pkgEnums)
            {
                enumNames.insert(ei.name);
            }
            for (auto* cls : classes)
            {
                // 检查结构体依赖
                auto deps2 = detail::CollectEntryDeps(*cls);
                for (auto& d : deps2)
                {
                    if (structNames.count(d))
                    {
                        classesNeedOwnStructs = true;
                        break;
                    }
                }
                if (classesNeedOwnStructs) break;
                // 检查枚举依赖：遍历属性的 typeName
                auto props = detail::CollectProperties(cls->addr);
                for (auto& p : props)
                {
                    if (enumNames.count(p.typeName))
                    {
                        classesNeedOwnStructs = true;
                        break;
                    }
                }
                if (classesNeedOwnStructs) break;
            }
        }

        detail::WriteStructsFile(sdkDir, sanitized, cleanPkg,
            structs, pkgEnums, sDeps, pkgIdxToSanitized,
            &pkgsWithStructs);
        detail::WriteClassesFile(sdkDir, sanitized, cleanPkg,
            classes, cDeps, pkgIdxToSanitized,
            hasStructsFile, classesNeedOwnStructs,
            &pkgsWithStructs);
        detail::WriteFunctionsFile(sdkDir, sanitized, cleanPkg,
            classes);
        detail::WriteParametersFile(sdkDir, sanitized, cleanPkg,
            classes, pDeps, pkgIdxToSanitized);

        detail::AppendAssertions(assertFile, structs, classes);

        bool hasClasses = !classes.empty();

        if (hasStructsFile)
        {
            sdkIncludes.push_back(
                sanitized + "_structs.hpp");
        }
        if (hasClasses)
        {
            sdkIncludes.push_back(
                sanitized + "_classes.hpp");
        }

        pkgCount++;
        if (pkgCount % 50 == 0)
        {
            std::cerr << "[xrd] 写入进度: "
                << pkgCount << "/" << totalPkgs << " 包\n";
            std::cerr.flush();
        }
    }

    // 关闭 Assertions.inl 中的 #ifndef DUMPER7_DISABLE_ASSERTS
    assertFile << "\n#endif // DUMPER7_DISABLE_ASSERTS\n";
    assertFile.close();

    detail::GenerateBasicHpp(sdkDir);
    detail::GenerateBasicCpp(sdkDir);
    detail::GenerateSdkHpp(cppSdkDir, sdkIncludes);

    std::cerr << "[xrd] C++ SDK 导出完成: " << pkgCount
              << " 个包, " << sdkIncludes.size()
              << " 个文件\n";
    return true;
}

// 兼容旧接口
inline bool DumpSdk(const std::wstring& outputPath)
{
    return DumpCppSdk(outputPath);
}

} // namespace xrd
