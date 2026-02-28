#pragma once
// Xrd-eXternalrEsolve - UObject 搜索与属性查找
// 从 objects.hpp 拆分：对象搜索、属性链遍历、按名称查找偏移

#include "objects.hpp"
#include <functional>
#include <shared_mutex>

namespace xrd
{

// ─── 对象搜索 ───

inline void ForEachObject(const std::function<bool(uptr obj, i32 index)>& callback)
{
    i32 total = GetTotalObjectCount();
    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = GetObjectByIndex(i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }
        if (!callback(obj, i))
        {
            break;
        }
    }
}

inline uptr FindObjectByName(const std::string& name)
{
    i32 total = GetTotalObjectCount();
    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = GetObjectByIndex(i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }
        if (GetObjectName(obj) == name)
        {
            return obj;
        }
    }
    return 0;
}

inline uptr FindClassByName(const std::string& name)
{
    i32 total = GetTotalObjectCount();
    for (i32 i = 0; i < total; ++i)
    {
        uptr obj = GetObjectByIndex(i);
        if (!IsCanonicalUserPtr(obj))
        {
            continue;
        }
        if (GetObjectClassName(obj) == "Class" && GetObjectName(obj) == name)
        {
            return obj;
        }
    }
    return 0;
}

// 检查对象是否属于指定类（含继承链）
inline bool IsObjectOfClass(uptr obj, const std::string& className)
{
    uptr cls = GetObjectClass(obj);
    while (cls)
    {
        if (GetObjectName(cls) == className)
        {
            return true;
        }
        cls = GetSuperStruct(cls);
    }
    return false;
}

// 在 struct 的属性链中按名称查找属性
inline uptr FindPropertyInStruct(uptr structObj, const std::string& propName)
{
    // 优先搜索 FField 链（UE4.25+）
    if (Off().bUseFProperty)
    {
        uptr prop = GetChildProperties(structObj);
        while (prop)
        {
            if (GetFFieldName(prop) == propName)
            {
                return prop;
            }
            prop = GetFFieldNext(prop);
        }
    }

    // 回退到 UField 链
    uptr child = GetChildren(structObj);
    while (child)
    {
        if (GetObjectName(child) == propName)
        {
            return child;
        }
        child = GetFieldNext(child);
    }
    return 0;
}

// 属性偏移缓存：(classPtr, propName) → offset
// 同一个 class 的同一个属性偏移永远不变，查一次就够了
namespace detail
{
    struct PropCacheKey
    {
        uptr cls;
        std::string name;
        bool operator==(const PropCacheKey& o) const
        {
            return cls == o.cls && name == o.name;
        }
    };

    struct PropCacheKeyHash
    {
        std::size_t operator()(const PropCacheKey& k) const
        {
            return std::hash<uptr>()(k.cls) ^ (std::hash<std::string>()(k.name) << 1);
        }
    };

    inline std::unordered_map<PropCacheKey, i32, PropCacheKeyHash>& PropOffsetCache()
    {
        static std::unordered_map<PropCacheKey, i32, PropCacheKeyHash> cache;
        return cache;
    }

    inline std::shared_mutex& PropOffsetCacheMutex()
    {
        static std::shared_mutex mtx;
        return mtx;
    }
} // namespace detail

// 在类继承链中查找属性偏移（带缓存）
inline i32 GetPropertyOffsetByName(uptr cls, const std::string& propName)
{
    if (!cls)
    {
        return -1;
    }

    detail::PropCacheKey key{cls, propName};

    // 读缓存（共享锁，允许并发读）
    {
        std::shared_lock<std::shared_mutex> rlock(detail::PropOffsetCacheMutex());
        auto& cache = detail::PropOffsetCache();
        auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }
    }

    // 缓存未命中，遍历属性链
    i32 result = -1;
    uptr current = cls;
    while (current)
    {
        uptr prop = FindPropertyInStruct(current, propName);
        if (prop)
        {
            result = GetPropertyOffset(prop);
            break;
        }
        current = GetSuperStruct(current);
    }

    // 写缓存（独占锁）
    {
        std::unique_lock<std::shared_mutex> wlock(detail::PropOffsetCacheMutex());
        detail::PropOffsetCache()[key] = result;
    }

    return result;
}

} // namespace xrd
