#pragma once
// Xrd-eXternalrEsolve - UObject 封装
// UObject/UStruct/UClass/FProperty 的外部内存读取封装

#include "../core/context.hpp"
#include "../resolve/scan_offsets.hpp"
#include "names.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace xrd
{

// ─── 对象数组基础操作 ───

inline i32 GetTotalObjectCount()
{
    if (!IsInited())
    {
        return 0;
    }
    return resolve::GetObjectCount(Mem(), Off());
}

inline uptr GetObjectByIndex(i32 index)
{
    if (!IsInited())
    {
        return 0;
    }
    return resolve::ReadObjectAt(Mem(), Off(), index);
}

// ─── UObject 字段读取 ───

// 名称缓存：避免重复 ReadProcessMemory
namespace detail
{
    inline std::unordered_map<uptr, std::string>& GetNameCache()
    {
        static std::unordered_map<uptr, std::string> cache;
        return cache;
    }

    inline std::shared_mutex& GetNameCacheMutex()
    {
        static std::shared_mutex mtx;
        return mtx;
    }

    inline std::unordered_map<uptr, std::string>& GetClassNameCache()
    {
        static std::unordered_map<uptr, std::string> cache;
        return cache;
    }

    inline std::shared_mutex& GetClassNameCacheMutex()
    {
        static std::shared_mutex mtx;
        return mtx;
    }
} // namespace detail

inline void ClearNameCaches()
{
    {
        std::unique_lock<std::shared_mutex> wlock(detail::GetNameCacheMutex());
        detail::GetNameCache().clear();
    }
    {
        std::unique_lock<std::shared_mutex> wlock(detail::GetClassNameCacheMutex());
        detail::GetClassNameCache().clear();
    }
}

inline std::string GetObjectName(uptr obj)
{
    if (!obj)
    {
        return "";
    }
    {
        std::shared_lock<std::shared_mutex> rlock(detail::GetNameCacheMutex());
        auto& cache = detail::GetNameCache();
        auto it = cache.find(obj);
        if (it != cache.end())
        {
            return it->second;
        }
    }
    // 读取在锁外做，避免持锁时做内存读取
    std::string name = ReadFNameAt(obj + Off().UObject_Name);
    {
        std::unique_lock<std::shared_mutex> wlock(detail::GetNameCacheMutex());
        detail::GetNameCache()[obj] = name;
    }
    return name;
}

inline uptr GetObjectClass(uptr obj)
{
    if (!obj)
    {
        return 0;
    }
    uptr cls = 0;
    GReadPtr(obj + Off().UObject_Class, cls);
    return cls;
}

inline uptr GetObjectOuter(uptr obj)
{
    if (!obj || Off().UObject_Outer == -1)
    {
        return 0;
    }
    uptr outer = 0;
    GReadPtr(obj + Off().UObject_Outer, outer);
    return outer;
}

// 获取最外层 Package 对象（沿 Outer 链一直走到顶）
inline uptr GetOutermostOuter(uptr obj)
{
    if (!obj)
    {
        return 0;
    }
    uptr cur = obj;
    uptr outer = GetObjectOuter(cur);
    int depth = 0;
    while (outer && depth < 64)
    {
        cur = outer;
        outer = GetObjectOuter(cur);
        depth++;
    }
    // cur 现在是最外层对象（通常是 UPackage）
    return (cur != obj) ? cur : 0;
}

inline i32 GetObjectIndex(uptr obj)
{
    if (!obj || Off().UObject_Index == -1)
    {
        return -1;
    }
    i32 idx = -1;
    GReadI32(obj + Off().UObject_Index, idx);
    return idx;
}

// 获取对象所属包的 GObjects 索引
// 对标 Rei-Dumper UEObject::GetPackageIndex
inline i32 GetPackageIndex(uptr obj)
{
    uptr pkg = GetOutermostOuter(obj);
    if (!pkg)
    {
        return -1;
    }
    return GetObjectIndex(pkg);
}

inline std::string GetObjectClassName(uptr obj)
{
    if (!obj)
    {
        return "";
    }
    {
        std::shared_lock<std::shared_mutex> rlock(detail::GetClassNameCacheMutex());
        auto& cache = detail::GetClassNameCache();
        auto it = cache.find(obj);
        if (it != cache.end())
        {
            return it->second;
        }
    }
    std::string name = GetObjectName(GetObjectClass(obj));
    {
        std::unique_lock<std::shared_mutex> wlock(detail::GetClassNameCacheMutex());
        detail::GetClassNameCache()[obj] = name;
    }
    return name;
}

inline std::string GetObjectFullName(uptr obj)
{
    if (!obj)
    {
        return "";
    }
    std::string name = GetObjectName(obj);
    uptr outer = GetObjectOuter(obj);
    while (outer)
    {
        name = GetObjectName(outer) + "." + name;
        outer = GetObjectOuter(outer);
    }
    return GetObjectClassName(obj) + " " + name;
}

// ─── UStruct 遍历 ───

inline uptr GetSuperStruct(uptr structObj)
{
    if (!structObj || Off().UStruct_SuperStruct == -1)
    {
        return 0;
    }
    uptr super = 0;
    GReadPtr(structObj + Off().UStruct_SuperStruct, super);
    return super;
}

inline uptr GetChildren(uptr structObj)
{
    if (!structObj || Off().UStruct_Children == -1)
    {
        return 0;
    }
    uptr children = 0;
    GReadPtr(structObj + Off().UStruct_Children, children);
    return children;
}

inline uptr GetChildProperties(uptr structObj)
{
    if (!structObj || Off().UStruct_ChildProperties == -1)
    {
        return 0;
    }
    uptr cp = 0;
    GReadPtr(structObj + Off().UStruct_ChildProperties, cp);
    return cp;
}

inline i32 GetStructSize(uptr structObj)
{
    if (!structObj || Off().UStruct_Size == -1)
    {
        return 0;
    }
    i32 size = 0;
    GReadI32(structObj + Off().UStruct_Size, size);
    return size;
}

// 读取 UStruct::MinAlignment（i16，紧跟在 Size 之后）
// 对标 Rei-Dumper UEStruct::GetMinAlignment
inline i32 GetStructMinAlignment(uptr structObj)
{
    if (!structObj || Off().UStruct_Size == -1)
    {
        return 1;
    }
    i16 align = 1;
    GReadValue(structObj + Off().UStruct_Size + 4, align);
    return (align > 0) ? align : 1;
}

// ─── UField 链表 ───

inline uptr GetFieldNext(uptr field)
{
    if (!field || Off().UField_Next == -1)
    {
        return 0;
    }
    uptr next = 0;
    GReadPtr(field + Off().UField_Next, next);
    return next;
}

// ─── FField (UE4.25+) 链表 ───

inline uptr GetFFieldNext(uptr ffield)
{
    if (!ffield)
    {
        return 0;
    }
    uptr next = 0;
    GReadPtr(ffield + Off().FField_Next, next);
    return next;
}

inline std::string GetFFieldName(uptr ffield)
{
    if (!ffield)
    {
        return "";
    }
    {
        std::shared_lock<std::shared_mutex> rlock(detail::GetNameCacheMutex());
        auto& cache = detail::GetNameCache();
        auto it = cache.find(ffield);
        if (it != cache.end())
        {
            return it->second;
        }
    }
    std::string name = ReadFNameAt(ffield + Off().FField_Name);
    {
        std::unique_lock<std::shared_mutex> wlock(detail::GetNameCacheMutex());
        detail::GetNameCache()[ffield] = name;
    }
    return name;
}

inline uptr GetFFieldClass(uptr ffield)
{
    if (!ffield)
    {
        return 0;
    }
    uptr cls = 0;
    GReadPtr(ffield + Off().FField_Class, cls);
    return cls;
}

inline std::string GetFFieldClassName(uptr ffield)
{
    if (!ffield)
    {
        return "";
    }
    {
        std::shared_lock<std::shared_mutex> rlock(detail::GetClassNameCacheMutex());
        auto& cache = detail::GetClassNameCache();
        auto it = cache.find(ffield);
        if (it != cache.end())
        {
            return it->second;
        }
    }
    uptr cls = GetFFieldClass(ffield);
    std::string name;
    if (!cls)
    {
        name = "";
    }
    else
    {
        name = ReadFNameAt(cls + Off().FFieldClass_Name);
    }
    {
        std::unique_lock<std::shared_mutex> wlock(detail::GetClassNameCacheMutex());
        detail::GetClassNameCache()[ffield] = name;
    }
    return name;
}

// ─── Property 读取 ───

inline i32 GetPropertyOffset(uptr prop)
{
    if (!prop || Off().Property_Offset == -1)
    {
        return -1;
    }
    i32 offset = 0;
    GReadI32(prop + Off().Property_Offset, offset);
    return offset;
}

inline i32 GetPropertyElementSize(uptr prop)
{
    if (!prop || Off().Property_ElementSize == -1)
    {
        return 0;
    }
    i32 size = 0;
    GReadI32(prop + Off().Property_ElementSize, size);
    return size;
}

inline i32 GetPropertyArrayDim(uptr prop)
{
    if (!prop || Off().Property_ArrayDim == -1)
    {
        return 1;
    }
    i32 dim = 0;
    GReadI32(prop + Off().Property_ArrayDim, dim);
    return (dim > 0) ? dim : 1;
}

inline u64 GetPropertyFlags(uptr prop)
{
    if (!prop || Off().Property_PropertyFlags == -1)
    {
        return 0;
    }
    u64 flags = 0;
    GReadValue(prop + Off().Property_PropertyFlags, flags);
    return flags;
}

// 读取 FProperty 的 alignment
// 对标 Rei-Dumper UEProperty::GetAlignment：使用 EClassCastFlags 精确判断类型
inline i32 GetPropertyAlignment(uptr prop)
{
    auto& off = Off();

    // 读取 FFieldClass::CastFlags（带本地缓存）
    static std::unordered_map<uptr, u64> fieldClassCache;
    u64 cf = 0;
    {
        uptr cls = 0;
        GReadPtr(prop + off.FField_Class, cls);
        if (cls)
        {
            auto cit = fieldClassCache.find(cls);
            if (cit != fieldClassCache.end())
            {
                cf = cit->second;
            }
            else
            {
                GReadValue(cls + off.FFieldClass_CastFlags, cf);
                fieldClassCache[cls] = cf;
            }
        }
    }

    constexpr i32 ptrAlign = sizeof(void*); // 8

    // EClassCastFlags 正确值（对标 Rei-Dumper Enums.h）
    constexpr u64 CF_Int8       = 0x0000000000000002ULL;
    constexpr u64 CF_Byte       = 0x0000000000000040ULL;
    constexpr u64 CF_Int        = 0x0000000000000080ULL;
    constexpr u64 CF_Float      = 0x0000000000000100ULL;
    constexpr u64 CF_UInt64     = 0x0000000000000200ULL;
    constexpr u64 CF_Class      = 0x0000000000000400ULL;
    constexpr u64 CF_UInt32     = 0x0000000000000800ULL;
    constexpr u64 CF_Interface  = 0x0000000000001000ULL;
    constexpr u64 CF_Name       = 0x0000000000002000ULL;
    constexpr u64 CF_Str        = 0x0000000000004000ULL;
    constexpr u64 CF_Object     = 0x0000000000010000ULL;
    constexpr u64 CF_Bool       = 0x0000000000020000ULL;
    constexpr u64 CF_UInt16     = 0x0000000000040000ULL;
    constexpr u64 CF_Struct     = 0x0000000000100000ULL;
    constexpr u64 CF_Array      = 0x0000000000200000ULL;
    constexpr u64 CF_Int64      = 0x0000000000400000ULL;
    constexpr u64 CF_Delegate   = 0x0000000000800000ULL;
    constexpr u64 CF_MCDelegate = 0x0000000002000000ULL;
    constexpr u64 CF_ObjBase    = 0x0000000004000000ULL;
    constexpr u64 CF_Weak       = 0x0000000008000000ULL;
    constexpr u64 CF_Lazy       = 0x0000000010000000ULL;
    constexpr u64 CF_SoftObj    = 0x0000000020000000ULL;
    constexpr u64 CF_Text       = 0x0000000040000000ULL;
    constexpr u64 CF_Int16      = 0x0000000080000000ULL;
    constexpr u64 CF_Double     = 0x0000000100000000ULL;
    constexpr u64 CF_SoftClass  = 0x0000000200000000ULL;
    constexpr u64 CF_Map        = 0x0000400000000000ULL;
    constexpr u64 CF_Set        = 0x0000800000000000ULL;
    constexpr u64 CF_Enum       = 0x0001000000000000ULL;
    constexpr u64 CF_MCInline   = 0x0004000000000000ULL;
    constexpr u64 CF_MCSparse   = 0x0008000000000000ULL;
    constexpr u64 CF_FieldPath  = 0x0010000000000000ULL;
    constexpr u64 CF_Optional   = 0x0100000000000000ULL;

    // ByteProperty / BoolProperty / Int8Property → 1
    if (cf & (CF_Byte | CF_Bool | CF_Int8))
        return 1;

    // UInt16Property / Int16Property → 2
    if (cf & (CF_UInt16 | CF_Int16))
        return 2;

    // IntProperty / UInt32Property / FloatProperty → 4
    if (cf & (CF_Int | CF_UInt32 | CF_Float))
        return 4;

    // UInt64Property / Int64Property / DoubleProperty → sizeof(void*) = 8
    if (cf & (CF_UInt64 | CF_Int64 | CF_Double))
        return ptrAlign;

    // NameProperty → alignof(int32) = 4（FName 内部是 int32 数组）
    if (cf & CF_Name)
        return 4;

    // StrProperty / TextProperty / SoftClassProperty / SoftObjectProperty → 8
    if (cf & (CF_Str | CF_Text | CF_SoftClass | CF_SoftObj))
        return ptrAlign;

    // StructProperty → 底层 struct 的 MinAlignment
    if (cf & CF_Struct)
    {
        if (off.StructProperty_Struct != -1)
        {
            uptr structPtr = 0;
            GReadPtr(prop + off.StructProperty_Struct, structPtr);
            if (IsCanonicalUserPtr(structPtr))
                return GetStructMinAlignment(structPtr);
        }
        return 1;
    }

    // ArrayProperty / MapProperty / SetProperty / FieldPathProperty → 8
    if (cf & (CF_Array | CF_Map | CF_Set | CF_FieldPath))
        return ptrAlign;

    // DelegateProperty → alignof(int32) = 4
    if (cf & CF_Delegate)
        return 4;

    // MulticastInlineDelegateProperty → 8
    if (cf & CF_MCInline)
        return ptrAlign;

    // MulticastSparseDelegateProperty → 1
    if (cf & CF_MCSparse)
        return 1;

    // WeakObjectProperty / LazyObjectProperty → alignof(int32) = 4
    if (cf & (CF_Weak | CF_Lazy))
        return 4;

    // ClassProperty / ObjectProperty / InterfaceProperty → alignof(void*) = 8
    if (cf & (CF_Class | CF_Object | CF_Interface | CF_ObjBase))
        return ptrAlign;

    // EnumProperty → 底层属性的对齐
    if (cf & CF_Enum)
    {
        if (off.EnumProperty_Base != -1)
        {
            uptr underProp = 0;
            GReadPtr(prop + off.EnumProperty_Base, underProp);
            if (IsCanonicalUserPtr(underProp))
                return GetPropertyAlignment(underProp);
        }
        return 1;
    }

    // OptionalProperty
    if (cf & CF_Optional)
    {
        if (off.StructProperty_Struct != -1)
        {
            uptr valProp = 0;
            GReadPtr(prop + off.StructProperty_Struct, valProp);
            if (IsCanonicalUserPtr(valProp))
            {
                i32 valSize = GetPropertyElementSize(valProp);
                i32 totalSize = GetPropertyElementSize(prop);
                if (valSize == totalSize)
                    return GetPropertyAlignment(valProp);
                return totalSize - valSize;
            }
        }
        return 1;
    }

    // 未知类型：回退到 min(elementSize, 8)
    i32 elemSize = GetPropertyElementSize(prop);
    if (elemSize <= 0) return 1;
    if (elemSize >= 8) return 8;
    if (elemSize >= 4) return 4;
    if (elemSize >= 2) return 2;
    return 1;
}

} // namespace xrd
