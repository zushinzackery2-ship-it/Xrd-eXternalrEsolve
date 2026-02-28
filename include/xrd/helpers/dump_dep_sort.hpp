#pragma once
// Xrd-eXternalrEsolve - 依赖拓扑排序
// 完全对标 Rei-Dumper DependencyManager：
// 使用 unordered_map<i32> / unordered_set<i32> 存储依赖图
// 遍历顺序由 MSVC STL 的哈希桶决定，与 Rei-Dumper 完全一致

#include "dump_prefix.hpp"
#include "dump_collect.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace xrd
{
namespace detail
{

// 从属性类型中提取依赖的结构体/类名（裸名，不含前缀）
inline std::vector<std::string> ExtractDepNames(
    const std::string& typeName)
{
    std::vector<std::string> names;
    std::string t = typeName;

    if (t.find("const ") == 0)
    {
        t = t.substr(6);
    }
    if (t.find("class ") == 0)
    {
        t = t.substr(6);
    }
    if (t.find("struct ") == 0)
    {
        t = t.substr(7);
    }
    while (!t.empty()
        && (t.back() == '*' || t.back() == '&' || t.back() == ' '))
    {
        t.pop_back();
    }

    // 处理模板类型
    auto lt = t.find('<');
    auto gt = t.rfind('>');
    if (lt != std::string::npos && gt != std::string::npos)
    {
        std::string inner = t.substr(lt + 1, gt - lt - 1);
        int depth = 0;
        size_t start = 0;
        for (size_t i = 0; i < inner.size(); ++i)
        {
            if (inner[i] == '<') depth++;
            else if (inner[i] == '>') depth--;
            else if (inner[i] == ',' && depth == 0)
            {
                auto sub = inner.substr(start, i - start);
                // 修剪前导空格（逗号分割后可能有空格）
                while (!sub.empty() && sub[0] == ' ')
                {
                    sub = sub.substr(1);
                }
                auto subNames = ExtractDepNames(sub);
                names.insert(names.end(),
                    subNames.begin(), subNames.end());
                start = i + 1;
            }
        }
        auto sub = inner.substr(start);
        // 修剪前导空格
        while (!sub.empty() && sub[0] == ' ')
        {
            sub = sub.substr(1);
        }
        auto subNames = ExtractDepNames(sub);
        names.insert(names.end(),
            subNames.begin(), subNames.end());
        return names;
    }

    if (t.length() > 1)
    {
        char prefix = t[0];
        // 排除 E 前缀：枚举类型不参与结构体拓扑排序
        // 枚举去掉 E 前缀后可能与结构体名冲突
        // 例如 ECollisionResponse → CollisionResponse = FCollisionResponse
        if (prefix == 'U' || prefix == 'A'
            || prefix == 'I' || prefix == 'F')
        {
            names.push_back(t.substr(1));
        }
    }
    return names;
}

// 收集一个结构体/类的所有依赖名（裸名）
// 使用缓存的 PropertyInfo.typeName 提取依赖
inline std::unordered_set<std::string> CollectEntryDeps(
    const StructEntry& entry)
{
    std::unordered_set<std::string> deps;

    if (!entry.superName.empty())
    {
        deps.insert(entry.superName);
    }

    auto props = CollectProperties(entry.addr);
    for (auto& prop : props)
    {
        auto depNames = ExtractDepNames(prop.typeName);
        for (auto& dn : depNames)
        {
            if (dn != entry.name)
            {
                deps.insert(dn);
            }
        }


    }


    return deps;
}

// 对标 Rei-Dumper DependencyManager：
// 使用 unordered_map<i32, unordered_set<i32>> 存储依赖图
// DFS 遍历顺序完全由 MSVC 的 unordered_map/set 哈希桶决定
// 这样编译后的迭代顺序与 Rei-Dumper 完全一致
//
// 对标 Rei-Dumper InitDependencies 的关键区别：
//   - 类（isClass=true）：只添加 super 依赖（AddDependency），不添加属性依赖
//   - 结构体（isClass=false）：添加 super + 属性依赖（SetDependencies）
// 这与 Rei-Dumper 的 "if (!bIsClass) AddStructDependencies" 逻辑完全一致
inline std::vector<const StructEntry*> TopoSortEntries(
    const std::vector<const StructEntry*>& entries)
{
    if (entries.size() <= 1)
    {
        return entries;
    }

    // 构建 objIndex → 条目指针映射
    std::unordered_map<i32, const StructEntry*> idxMap;
    // 构建 name → objIndex 映射（仅限当前列表内）
    std::unordered_map<std::string, i32> nameToIdx;
    for (auto* e : entries)
    {
        idxMap[e->objIndex] = e;
        nameToIdx[e->name] = e->objIndex;
    }

    // 对标 Rei-Dumper：用 unordered_map<i32, unordered_set<i32>>
    // 按 GObjects 索引存储依赖关系
    std::unordered_map<i32, std::unordered_set<i32>> depGraph;

    // 对标 Rei-Dumper InitDependencies：先 SetExists 确保每个条目都在图中
    for (auto* e : entries)
    {
        depGraph[e->objIndex];
    }

    for (auto* e : entries)
    {
        if (e->isClass)
        {
            // 对标 Rei-Dumper：类只添加 super 依赖（AddDependency）
            // 属性依赖不影响类的排序（属性通常是指针，只需前向声明）
            if (!e->superName.empty())
            {
                auto nit = nameToIdx.find(e->superName);
                if (nit != nameToIdx.end())
                {
                    depGraph[e->objIndex].insert(nit->second);
                }
            }
        }
        else
        {
            // 对标 Rei-Dumper：结构体添加 super + 属性依赖（SetDependencies）
            auto allDeps = CollectEntryDeps(*e);
            std::unordered_set<i32> localDeps;
            for (auto& d : allDeps)
            {
                auto nit = nameToIdx.find(d);
                if (nit != nameToIdx.end())
                {
                    localDeps.insert(nit->second);
                }
            }
            depGraph[e->objIndex] = std::move(localDeps);
        }
    }


    // 对标 Rei-Dumper VisitAllNodesWithCallback：
    // 遍历 unordered_map，对每个节点做 DFS
    std::vector<const StructEntry*> result;
    result.reserve(entries.size());
    std::unordered_set<i32> visited;

    std::function<void(i32)> visit;
    visit = [&](i32 idx)
    {
        if (visited.count(idx))
        {
            return;
        }
        visited.insert(idx);

        auto it = depGraph.find(idx);
        if (it != depGraph.end())
        {
            for (i32 dep : it->second)
            {
                visit(dep);
            }
        }

        auto mit = idxMap.find(idx);
        if (mit != idxMap.end())
        {
            result.push_back(mit->second);
        }
    };

    // 遍历 unordered_map<i32>，顺序由 MSVC 哈希桶决定
    for (const auto& [idx, deps] : depGraph)
    {
        visit(idx);
    }

    return result;
}

} // namespace detail
} // namespace xrd
