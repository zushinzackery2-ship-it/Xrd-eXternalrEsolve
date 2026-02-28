#pragma once
// Xrd-eXternalrEsolve - 属性类型精确解析
// 通过读取类型化属性的关联指针，解析出精确的 C++ 类型字符串
// 如 TArray<FVector>、UStaticMeshComponent*、ECollisionChannel 等

#include "../core/context.hpp"
#include "../engine/objects.hpp"
#include "../engine/names.hpp"
#include "dump_prefix.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>

namespace xrd
{
namespace detail
{

// 根据字节大小返回对应的整数类型名（BoolProperty non-native / BitPadding 使用）
inline std::string GetTypeFromSize(i32 size)
{
    switch (size)
    {
        case 1: return "uint8";
        case 2: return "uint16";
        case 4: return "uint32";
        case 8: return "uint64";
        default: return "uint8";
    }
}

// 获取 UStruct/UClass 的带前缀名称（F/U/A/I 前缀）
// 优先使用全局查找表获取正确前缀（Actor→A, Interface→I）
inline std::string GetStructPrefixedName(uptr structObj)
{
    if (!structObj)
    {
        return "void";
    }

    std::string name = GetObjectName(structObj);
    if (name.empty())
    {
        return "void";
    }

    std::string className = GetObjectClassName(structObj);
    bool isClass = (className == "Class");

    // 优先查找全局条目表（包含 Actor/Interface 继承信息）
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
    if (isClass)
    {
        return "U" + name;
    }
    if (className == "ScriptStruct" || className == "Struct")
    {
        return "F" + name;
    }
    return name;
}

// 读取 FField 的 FFieldClass::CastFlags
inline u64 GetFFieldCastFlags(uptr ffield)
{
    if (!ffield)
    {
        return 0;
    }
    uptr cls = 0;
    GReadPtr(ffield + Off().FField_Class, cls);
    if (!cls || Off().FFieldClass_CastFlags == -1)
    {
        return 0;
    }
    u64 flags = 0;
    GReadValue(cls + Off().FFieldClass_CastFlags, flags);
    return flags;
}

// 前向声明，GetDelegateFunctionSignature 需要调用它
inline std::string ResolvePropertyType(uptr prop, const std::string& fieldClassName);

// 验证指针是否指向一个 UFunction 或 DelegateFunction 对象
inline bool IsValidUFunction(uptr ptr)
{
    if (!ptr || !IsCanonicalUserPtr(ptr))
    {
        return false;
    }
    uptr cls = 0;
    GReadPtr(ptr + Off().UObject_Class, cls);
    if (!cls || !IsCanonicalUserPtr(cls))
    {
        return false;
    }
    std::string className = GetObjectName(cls);
    return (className == "Function"
        || className == "DelegateFunction"
        || className == "SparseDelegateFunction");
}

// 生成 Delegate 签名字符串，对标 Rei-Dumper GetFunctionSignature
// 格式：RetType(Param1Type Param1Name, ...)
inline std::string GetDelegateFunctionSignature(uptr funcObj)
{
    // 防止 delegate 签名无限递归
    static thread_local int g_delegateDepth = 0;
    if (!funcObj || g_delegateDepth > 4)
    {
        return "void()";
    }
    g_delegateDepth++;
    struct DelegateDepthGuard { ~DelegateDepthGuard() { g_delegateDepth--; } } delegateGuard;

    auto& off = Off();
    std::string retType = "void";
    std::string params;
    bool isFirstParam = true;

    // 遍历 UFunction 的 FField 属性链，收集 Parm 参数
    if (off.bUseFProperty && off.UStruct_ChildProperties != -1)
    {
        uptr prop = 0;
        GReadPtr(funcObj + off.UStruct_ChildProperties, prop);

        // 先收集所有参数，按 offset 排序（对标 Rei-Dumper CompareUnrealProperties）
        struct ParamEntry
        {
            std::string type;
            std::string name;
            u64 flags;
            i32 offset;
        };
        std::vector<ParamEntry> entries;

        // 安全限制：delegate 签名参数通常不超过 20 个
        i32 delegateLimit = 0;
        std::unordered_set<uptr> delegateSeen;
        while (IsCanonicalUserPtr(prop))
        {
            if (delegateLimit++ > 50 || delegateSeen.count(prop))
            {
                break;
            }
            delegateSeen.insert(prop);

            u64 flags = 0;
            if (off.Property_PropertyFlags != -1)
            {
                GReadValue(prop + off.Property_PropertyFlags, flags);
            }

            // 只处理 Parm 标记的参数
            if (flags & 0x80)
            {
                std::string fcName = GetFFieldClassName(prop);
                std::string type = ResolvePropertyType(prop, fcName);
                std::string name = GetFFieldName(prop);
                i32 offset = 0;
                if (off.Property_Offset != -1)
                {
                    GReadValue(prop + off.Property_Offset, offset);
                }

                bool isConst = (flags & 0x02) != 0;
                bool isRef   = (flags & 0x08000000) != 0;
                bool isOut   = isRef || ((flags & 0x100) != 0);
                bool isRet   = (flags & 0x400) != 0;
                bool isMoveType = (
                    fcName == "StructProperty" ||
                    fcName == "ArrayProperty"  ||
                    fcName == "StrProperty"    ||
                    fcName == "MapProperty"    ||
                    fcName == "SetProperty");

                if (isRet)
                {
                    if (isConst)
                    {
                        retType = "const " + type;
                    }
                    else
                    {
                        retType = type;
                    }
                    prop = GetFFieldNext(prop);
                    continue;
                }

                // 修饰符处理（对标 Rei-Dumper GetFunctionSignature）
                if (isConst && (!isOut || isRef))
                {
                    type = "const " + type;
                }
                if (isOut)
                {
                    type += isRef ? "&" : "*";
                }
                else if (isMoveType)
                {
                    type += "&";
                    if (!isConst)
                    {
                        type = "const " + type;
                    }
                }

                entries.push_back({type, name, flags, offset});
            }
            prop = GetFFieldNext(prop);
        }

        // 按 offset 排序
        std::sort(entries.begin(), entries.end(),
            [](const ParamEntry& a, const ParamEntry& b)
            {
                return a.offset < b.offset;
            });

        for (auto& e : entries)
        {
            if (!isFirstParam)
            {
                params += ", ";
            }
            params += e.type + " " + e.name;
            isFirstParam = false;
        }
    }

    return retType + "(" + params + ")";
}

// 递归深度保护：防止 delegate 参数中嵌套 delegate 导致无限递归
inline thread_local int g_resolveDepth = 0;

// 通过 FField 类名获取精确类型字符串
// 这是核心函数：根据属性类型读取关联的 UClass/UStruct/UEnum 指针
inline std::string ResolvePropertyType(uptr prop, const std::string& fieldClassName)
{
    // 递归深度保护
    if (g_resolveDepth > 32)
    {
        return fieldClassName;
    }
    g_resolveDepth++;
    struct DepthGuard { ~DepthGuard() { g_resolveDepth--; } } guard;
    auto& off = Off();

    // ─── ObjectProperty / ObjectPropertyBase ───
    if (fieldClassName == "ObjectProperty" || fieldClassName == "ObjectPropertyBase")
    {
        if (off.ObjectProperty_Class != -1)
        {
            uptr classPtr = 0;
            GReadPtr(prop + off.ObjectProperty_Class, classPtr);
            if (IsCanonicalUserPtr(classPtr))
            {
                std::string name = GetStructPrefixedName(classPtr);
                return "class " + name + "*";
            }
        }
        return "class UObject*";
    }

    // ─── ClassProperty ───
    // 对标 Rei-Dumper：有 UObjectWrapper 标志时输出 TSubclassOf<T>，否则 UClass*
    if (fieldClassName == "ClassProperty")
    {
        // UObjectWrapper flag = 0x0000000100000000
        // 读取 PropertyFlags 判断是否有 UObjectWrapper
        u64 propFlags = 0;
        if (off.Property_PropertyFlags != -1)
        {
            GReadValue(prop + off.Property_PropertyFlags, propFlags);
        }
        bool hasUObjectWrapper = (propFlags & 0x0000000100000000ULL) != 0;
        if (hasUObjectWrapper && off.ClassProperty_MetaClass != -1)
        {
            uptr metaClass = 0;
            GReadPtr(prop + off.ClassProperty_MetaClass, metaClass);
            if (IsCanonicalUserPtr(metaClass))
            {
                std::string name = GetStructPrefixedName(metaClass);
                return "TSubclassOf<class " + name + ">";
            }
            return "TSubclassOf<class UObject>";
        }
        return "class UClass*";
    }

    // ─── WeakObjectProperty ───
    if (fieldClassName == "WeakObjectProperty")
    {
        if (off.ObjectProperty_Class != -1)
        {
            uptr classPtr = 0;
            GReadPtr(prop + off.ObjectProperty_Class, classPtr);
            if (IsCanonicalUserPtr(classPtr))
            {
                std::string name = GetStructPrefixedName(classPtr);
                return "TWeakObjectPtr<class " + name + ">";
            }
        }
        return "TWeakObjectPtr<UObject>";
    }

    // ─── LazyObjectProperty ───
    // 对标 Rei-Dumper：输出 TLazyObjectPtr<class T>
    if (fieldClassName == "LazyObjectProperty")
    {
        if (off.ObjectProperty_Class != -1)
        {
            uptr classPtr = 0;
            GReadPtr(prop + off.ObjectProperty_Class, classPtr);
            if (IsCanonicalUserPtr(classPtr))
            {
                std::string name = GetStructPrefixedName(classPtr);
                return "TLazyObjectPtr<class " + name + ">";
            }
        }
        return "TLazyObjectPtr<class UObject>";
    }

    // ─── SoftObjectProperty ───
    // 对标 Rei-Dumper：读取 PropertyClass 获取精确类型
    if (fieldClassName == "SoftObjectProperty")
    {
        if (off.ObjectProperty_Class != -1)
        {
            uptr classPtr = 0;
            GReadPtr(prop + off.ObjectProperty_Class, classPtr);
            if (IsCanonicalUserPtr(classPtr))
            {
                std::string name = GetStructPrefixedName(classPtr);
                return "TSoftObjectPtr<class " + name + ">";
            }
        }
        return "TSoftObjectPtr<class UObject>";
    }

    // ─── SoftClassProperty ───
    // 对标 Rei-Dumper：SoftClassProperty 继承自 SoftObjectProperty
    // MetaClass 在 ClassProperty_MetaClass 偏移处
    if (fieldClassName == "SoftClassProperty")
    {
        if (off.ClassProperty_MetaClass != -1)
        {
            uptr metaClass = 0;
            GReadPtr(prop + off.ClassProperty_MetaClass, metaClass);
            if (IsCanonicalUserPtr(metaClass))
            {
                std::string name = GetStructPrefixedName(metaClass);
                return "TSoftClassPtr<class " + name + ">";
            }
        }
        // 回退：尝试 PropertyClass
        if (off.ObjectProperty_Class != -1)
        {
            uptr classPtr = 0;
            GReadPtr(prop + off.ObjectProperty_Class, classPtr);
            if (IsCanonicalUserPtr(classPtr))
            {
                std::string name = GetStructPrefixedName(classPtr);
                return "TSoftClassPtr<class " + name + ">";
            }
        }
        return "TSoftClassPtr<class UObject>";
    }

    // ─── InterfaceProperty ───
    if (fieldClassName == "InterfaceProperty")
    {
        if (off.ObjectProperty_Class != -1)
        {
            uptr classPtr = 0;
            GReadPtr(prop + off.ObjectProperty_Class, classPtr);
            if (IsCanonicalUserPtr(classPtr))
            {
                std::string name = GetObjectName(classPtr);
                if (!name.empty())
                {
                    return "TScriptInterface<class I" + name + ">";
                }
            }
        }
        return "TScriptInterface<class IInterface>";
    }

    // ─── StructProperty ───
    // 对标 Rei-Dumper：StructProperty 类型始终带 struct 前缀
    if (fieldClassName == "StructProperty")
    {
        if (off.StructProperty_Struct != -1)
        {
            uptr structPtr = 0;
            GReadPtr(prop + off.StructProperty_Struct, structPtr);
            if (IsCanonicalUserPtr(structPtr))
            {
                return "struct " + GetStructPrefixedName(structPtr);
            }
        }
        return "struct FUnknownStruct";
    }

    // ─── ArrayProperty ───
    if (fieldClassName == "ArrayProperty")
    {
        if (off.ArrayProperty_Inner != -1)
        {
            uptr innerProp = 0;
            GReadPtr(prop + off.ArrayProperty_Inner, innerProp);
            if (IsCanonicalUserPtr(innerProp))
            {
                std::string innerClassName = GetFFieldClassName(innerProp);
                std::string innerType = ResolvePropertyType(
                    innerProp, innerClassName);
                return "TArray<" + innerType + ">";
            }
        }
        return "TArray<uint8>";
    }

    // ─── SetProperty ───
    if (fieldClassName == "SetProperty")
    {
        if (off.SetProperty_ElementProp != -1)
        {
            uptr elemProp = 0;
            GReadPtr(prop + off.SetProperty_ElementProp, elemProp);
            if (IsCanonicalUserPtr(elemProp))
            {
                std::string elemClassName = GetFFieldClassName(elemProp);
                std::string elemType = ResolvePropertyType(
                    elemProp, elemClassName);
                return "TSet<" + elemType + ">";
            }
        }
        return "TSet<uint8>";
    }

    // ─── MapProperty ───
    if (fieldClassName == "MapProperty")
    {
        if (off.MapProperty_Base != -1)
        {
            uptr keyProp = 0, valProp = 0;
            GReadPtr(prop + off.MapProperty_Base, keyProp);
            GReadPtr(prop + off.MapProperty_Base + 8, valProp);

            std::string keyType = "uint8";
            std::string valType = "uint8";

            if (IsCanonicalUserPtr(keyProp))
            {
                keyType = ResolvePropertyType(
                    keyProp, GetFFieldClassName(keyProp));
            }
            if (IsCanonicalUserPtr(valProp))
            {
                valType = ResolvePropertyType(
                    valProp, GetFFieldClassName(valProp));
            }
            return "TMap<" + keyType + ", " + valType + ">";
        }
        return "TMap<uint8, uint8>";
    }

    // ─── EnumProperty ───
    if (fieldClassName == "EnumProperty")
    {
        if (off.EnumProperty_Base != -1)
        {
            uptr enumPtr = 0;
            // Enum 指针在 Base+8（Base+0 是 UnderlayingProperty）
            GReadPtr(prop + off.EnumProperty_Base + 8, enumPtr);
            if (IsCanonicalUserPtr(enumPtr))
            {
                std::string name = GetObjectName(enumPtr);
                if (!name.empty())
                {
                    // 对标 Rei-Dumper：枚举名不以 E 开头时加 E 前缀
                    if (name[0] != 'E')
                    {
                        name = "E" + name;
                    }
                    return name;
                }
            }
        }
        return "uint8";
    }

    // ─── ByteProperty（可能关联 Enum） ───
    if (fieldClassName == "ByteProperty")
    {
        if (off.ByteProperty_Enum != -1)
        {
            uptr enumPtr = 0;
            GReadPtr(prop + off.ByteProperty_Enum, enumPtr);
            if (IsCanonicalUserPtr(enumPtr))
            {
                std::string name = GetObjectName(enumPtr);
                if (!name.empty())
                {
                    // 对标 Rei-Dumper：枚举名不以 E 开头时加 E 前缀
                    if (name[0] != 'E')
                    {
                        name = "E" + name;
                    }
                    return name;
                }
            }
        }
        return "uint8";
    }

    // ─── DelegateProperty ───
    if (fieldClassName == "DelegateProperty")
    {
        if (off.DelegateProperty_Sig != -1)
        {
            uptr sigFunc = 0;
            GReadPtr(prop + off.DelegateProperty_Sig, sigFunc);
            if (IsValidUFunction(sigFunc))
            {
                std::string sig = GetDelegateFunctionSignature(sigFunc);
                return "TDelegate<" + sig + ">";
            }
        }
        return "TDelegate<void()>";
    }
    if (fieldClassName == "MulticastDelegateProperty" ||
        fieldClassName == "MulticastInlineDelegateProperty" ||
        fieldClassName == "MulticastSparseDelegateProperty")
    {
        // MulticastSparseDelegateProperty 使用特殊类型名
        // 对标 Rei-Dumper：sparse delegate 输出 FMulticastSparseDelegateProperty_
        if (fieldClassName == "MulticastSparseDelegateProperty")
        {
            return "FMulticastSparseDelegateProperty_";
        }

        if (off.DelegateProperty_Sig != -1)
        {
            uptr sigFunc = 0;
            GReadPtr(prop + off.DelegateProperty_Sig, sigFunc);
            if (IsValidUFunction(sigFunc))
            {
                std::string sig = GetDelegateFunctionSignature(sigFunc);
                return "TMulticastInlineDelegate<" + sig + ">";
            }
        }
        return "TMulticastInlineDelegate<void()>";
    }

    // ─── 基础类型直接映射 ───
    // 对标 Rei-Dumper：BoolProperty 非 native 时输出 uint8（BitField 类型）
    if (fieldClassName == "BoolProperty")
    {
        // 读取 FieldMask 判断是否是 native bool
        // native bool: FieldMask == 0xFF（整个字节）
        if (off.BoolProperty_Base != -1)
        {
            u8 fieldMask = 0;
            Mem().Read(prop + off.BoolProperty_Base + 3, &fieldMask, 1);
            if (fieldMask == 0xFF)
            {
                return "bool";
            }
            // 非 native：返回对应大小的整数类型
            i32 propSize = 0;
            if (off.Property_ElementSize != -1)
            {
                GReadValue(prop + off.Property_ElementSize, propSize);
            }
            return GetTypeFromSize(propSize > 0 ? propSize : 1);
        }
        return "bool";
    }
    if (fieldClassName == "Int8Property")    return "int8";
    if (fieldClassName == "Int16Property")   return "int16";
    if (fieldClassName == "IntProperty")     return "int32";
    if (fieldClassName == "Int64Property")   return "int64";
    if (fieldClassName == "UInt16Property")  return "uint16";
    if (fieldClassName == "UInt32Property")  return "uint32";
    if (fieldClassName == "UInt64Property")  return "uint64";
    if (fieldClassName == "FloatProperty")   return "float";
    if (fieldClassName == "DoubleProperty")  return "double";
    if (fieldClassName == "NameProperty")    return "class FName";
    if (fieldClassName == "StrProperty")     return "class FString";
    if (fieldClassName == "TextProperty")    return "class FText";
    if (fieldClassName == "FieldPathProperty") return "struct FFieldPath";

    return fieldClassName;
}

} // namespace detail
} // namespace xrd
