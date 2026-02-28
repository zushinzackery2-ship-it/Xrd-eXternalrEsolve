#pragma once
// Xrd-eXternalrEsolve - SDK 导出：属性/函数收集
// 从 UStruct 中收集属性和函数信息，使用精确类型解析

#include "../core/context.hpp"
#include "../engine/objects.hpp"
#include "dump_type_resolve.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace xrd
{
namespace detail
{

struct PropertyInfo
{
    std::string name;
    std::string typeName;
    std::string fieldClassName; // 原始 FField 类名，用于 BitField 判断
    i32 offset   = 0;
    i32 size     = 0;
    i32 arrayDim = 1;
    u64 flags    = 0;
    // BoolProperty BitField 信息
    bool isBitField = false;
    u8 bitIndex     = 0;
    u8 bitCount     = 1;
    u8 fieldMask    = 0xFF;
};

struct FunctionParam
{
    std::string name;
    std::string typeName;       // 原始类型（参数结构体中使用）
    std::string sigTypeName;    // 签名类型（含 const/&/* 修饰）
    std::string fieldClassName; // 原始属性类名，用于判断 move 类型
    u64 flags = 0;
    i32 offset = 0;
    i32 size   = 0;
    bool isReturnParam = false;
    bool isOutParam    = false;
    bool isConstParam  = false;
    bool isRefParam    = false;  // OutParm + ReferenceParm = 引用参数
    bool isMoveType    = false;  // 大类型使用 std::move
};

struct FunctionInfo
{
    std::string name;
    std::string returnType;
    u64 functionFlags = 0;
    i32 paramStructSize = 0; // UFunction 的 ParamsSize
    std::vector<FunctionParam> params;
};

// 属性缓存：每个 struct 地址只读一次，避免重复 ReadProcessMemory
// 4592 structs × 平均10属性 × 3次调用 = 节省 ~80% 的远程读取
inline std::unordered_map<uptr, std::vector<PropertyInfo>>& GetPropertiesCache()
{
    static std::unordered_map<uptr, std::vector<PropertyInfo>> cache;
    return cache;
}

// 清空属性缓存（每次 DumpSdk 开始前调用）
inline void ClearPropertiesCache()
{
    GetPropertiesCache().clear();
}

// 读取 BoolProperty 的 BitField 信息
inline void ReadBoolPropertyBitInfo(uptr prop, PropertyInfo& pi)
{
    if (Off().BoolProperty_Base == -1)
    {
        return;
    }

    // BoolProperty 在 BoolProperty_Base 偏移处存放:
    //   +0: FieldSize (u8)
    //   +1: ByteOffset (u8)
    //   +2: ByteMask (u8)
    //   +3: FieldMask (u8)
    u8 fieldSize = 0, byteOffset = 0, byteMask = 0, fieldMask = 0;
    Mem().Read(prop + Off().BoolProperty_Base + 0, &fieldSize, 1);
    Mem().Read(prop + Off().BoolProperty_Base + 1, &byteOffset, 1);
    Mem().Read(prop + Off().BoolProperty_Base + 2, &byteMask, 1);
    Mem().Read(prop + Off().BoolProperty_Base + 3, &fieldMask, 1);

    // 如果 fieldMask != 0xFF，说明是 BitField
    if (fieldMask != 0xFF && fieldMask != 0)
    {
        pi.isBitField = true;
        pi.fieldMask = fieldMask;

        // 计算 bitIndex（fieldMask 中最低位的位置）
        u8 mask = fieldMask;
        u8 idx = 0;
        while ((mask & 1) == 0 && idx < 8)
        {
            mask >>= 1;
            idx++;
        }
        pi.bitIndex = idx;

        // 计算 bitCount
        u8 count = 0;
        while (mask & 1)
        {
            mask >>= 1;
            count++;
        }
        pi.bitCount = count;
    }
}

// 收集一个 UStruct 的所有属性（精确类型）
// 结果缓存到 GetPropertiesCache()，每个 struct 地址只读一次
inline std::vector<PropertyInfo> CollectProperties(uptr structObj)
{
    // 先查缓存
    auto& cache = GetPropertiesCache();
    auto it = cache.find(structObj);
    if (it != cache.end())
    {
        return it->second;
    }

    std::vector<PropertyInfo> props;

    // FField 链（UE4.25+）
    if (Off().bUseFProperty)
    {
        uptr prop = GetChildProperties(structObj);
        i32 propLimit = 0;
        std::unordered_set<uptr> propSeen;
        while (prop && propLimit < 2000)
        {
            if (propSeen.count(prop))
            {
                break;
            }
            propSeen.insert(prop);
            propLimit++;

            std::string piName = GetFFieldName(prop);
            std::string piClassName = GetFFieldClassName(prop);

            if (piName.empty())
            {
                prop = GetFFieldNext(prop);
                continue;
            }

            PropertyInfo pi;
            pi.name           = piName;
            pi.fieldClassName = piClassName;
            pi.typeName       = ResolvePropertyType(prop, piClassName);
            pi.offset         = GetPropertyOffset(prop);
            pi.size           = GetPropertyElementSize(prop);
            pi.arrayDim       = GetPropertyArrayDim(prop);
            pi.flags          = GetPropertyFlags(prop);

            // BoolProperty BitField 检测
            if (pi.fieldClassName == "BoolProperty")
            {
                ReadBoolPropertyBitInfo(prop, pi);
            }

            props.push_back(pi);
            prop = GetFFieldNext(prop);
        }
    }

    // UField 链（兼容旧版，仅在不使用 FProperty 时遍历）
    // UE4.25+ 使用 FField 链存储属性，Children 链只有 UFunction
    if (!Off().bUseFProperty)
    {
        uptr child = GetChildren(structObj);
        i32 childLimit = 0;
        while (child && childLimit < 2000)
        {
            childLimit++;
            std::string className = GetObjectClassName(child);
            if (className.find("Property") != std::string::npos)
            {
                PropertyInfo pi;
                pi.name           = GetObjectName(child);
                pi.fieldClassName = className;
                pi.typeName       = ResolvePropertyType(
                    child, className);
                pi.offset         = GetPropertyOffset(child);
                pi.size           = GetPropertyElementSize(child);
                pi.arrayDim       = GetPropertyArrayDim(child);

                if (pi.fieldClassName == "BoolProperty")
                {
                    ReadBoolPropertyBitInfo(child, pi);
                }

                if (!pi.name.empty())
                {
                    props.push_back(pi);
                }
            }
            child = GetFieldNext(child);
        }
    }

    // 按偏移排序
    std::sort(props.begin(), props.end(),
        [](const PropertyInfo& a, const PropertyInfo& b)
        {
            return a.offset < b.offset;
        }
    );

    // 存入缓存
    cache[structObj] = props;
    return cache[structObj];
}

// 反转函数列表，使输出顺序与 Rei-Dumper 一致
// UE 的 Children 链表是后进先出的，Rei-Dumper 内部进程遍历得到的顺序
// 和外部进程遍历一致，但 Rei-Dumper 在 MemberManager 中保持了原始顺序
// 实际对比发现 Xrd 输出是反序的，需要反转
inline void ReverseIfNeeded(std::vector<FunctionInfo>& funcs)
{
    std::reverse(funcs.begin(), funcs.end());
}

// 构建参数的签名类型（含 const/&/* 修饰）
// 对标 Rei-Dumper CppGenerator::GenerateFunctionInfo
inline void BuildSignatureType(FunctionParam& fp)
{
    std::string type = fp.typeName;

    if (fp.isReturnParam)
    {
        if (fp.isConstParam)
        {
            type = "const " + type;
        }
        fp.sigTypeName = type;
        return;
    }

    // const 修饰：ConstParm && (!isOut || isRef)
    if (fp.isConstParam
        && (!fp.isOutParam || fp.isRefParam))
    {
        type = "const " + type;
    }

    // out 参数：引用或指针
    if (fp.isOutParam)
    {
        type += fp.isRefParam ? "&" : "*";
    }
    // 非 out 的 move 类型：加 const& 传递
    else if (fp.isMoveType)
    {
        type += "&";
        if (!fp.isConstParam)
        {
            type = "const " + type;
        }
    }

    fp.sigTypeName = type;
}

// 收集函数参数（精确类型 + 方向标记 + 偏移/大小）
// 对标 Rei-Dumper CppGenerator::GenerateFunctionInfo
inline std::vector<FunctionParam> CollectFuncParams(uptr funcObj)
{
    std::vector<FunctionParam> params;

    if (Off().bUseFProperty)
    {
        uptr prop = GetChildProperties(funcObj);
        i32 limit = 0;
        std::unordered_set<uptr> seen;
        while (prop && limit < 500)
        {
            if (seen.count(prop))
            {
                break;
            }
            seen.insert(prop);
            limit++;
            u64 flags = GetPropertyFlags(prop);
            if (flags & 0x80)
            {
                FunctionParam fp;
                fp.name           = GetFFieldName(prop);
                // 清理参数名：非法 ASCII 字符替换为下划线
                for (auto& c : fp.name)
                {
                    unsigned char uc = static_cast<unsigned char>(c);
                    if (uc < 0x80 && !std::isalnum(uc) && c != '_')
                    {
                        c = '_';
                    }
                }
                if (!fp.name.empty()
                    && std::isdigit(
                        static_cast<unsigned char>(fp.name[0])))
                {
                    fp.name = "_" + fp.name;
                }
                fp.fieldClassName = GetFFieldClassName(prop);
                fp.typeName       = ResolvePropertyType(
                    prop, fp.fieldClassName);
                fp.flags          = flags;
                fp.offset         = GetPropertyOffset(prop);
                fp.size           = GetPropertyElementSize(prop);
                fp.isReturnParam = (flags & 0x400) != 0;
                fp.isConstParam  = (flags & 0x02) != 0;
                bool isRef = (flags & 0x08000000) != 0;
                fp.isOutParam = isRef
                    || ((flags & 0x100) != 0);
                fp.isRefParam = isRef;
                fp.isMoveType = (
                    fp.fieldClassName == "StructProperty" ||
                    fp.fieldClassName == "ArrayProperty" ||
                    fp.fieldClassName == "StrProperty" ||
                    fp.fieldClassName == "TextProperty" ||
                    fp.fieldClassName == "MapProperty" ||
                    fp.fieldClassName == "SetProperty");

                BuildSignatureType(fp);
                params.push_back(fp);
            }
            prop = GetFFieldNext(prop);
        }
    }

    return params;
}

// 函数缓存：每个 struct 地址只读一次
inline std::unordered_map<uptr, std::vector<FunctionInfo>>& GetFunctionsCache()
{
    static std::unordered_map<uptr, std::vector<FunctionInfo>> cache;
    return cache;
}

inline void ClearFunctionsCache()
{
    GetFunctionsCache().clear();
}

// 收集一个 UStruct 的所有函数（精确签名）
// 结果缓存到 GetFunctionsCache()，每个 struct 地址只读一次
inline std::vector<FunctionInfo> CollectFunctions(uptr structObj)
{
    auto& cache = GetFunctionsCache();
    auto it = cache.find(structObj);
    if (it != cache.end())
    {
        return it->second;
    }

    std::vector<FunctionInfo> funcs;

    uptr child = GetChildren(structObj);
    i32 safetyLimit = 0;
    std::unordered_set<uptr> visited;
    while (child && safetyLimit < 2000)
    {
        // 检测循环链表
        if (visited.count(child))
        {
            std::cerr << "[xrd] WARNING: 循环链表检测到 at "
                << std::hex << child << std::dec
                << " after " << safetyLimit << " nodes\n";
            std::cerr.flush();
            break;
        }
        visited.insert(child);
        safetyLimit++;
        if (!IsCanonicalUserPtr(child))
        {
            break;
        }
        std::string className = GetObjectClassName(child);
        if (className == "Function")
        {
            FunctionInfo fi;
            fi.name = GetObjectName(child);

            // 跳过包含控制字符的垃圾函数名
            bool hasCtrl = false;
            for (char c : fi.name)
            {
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    hasCtrl = true;
                    break;
                }
            }
            if (fi.name.empty() || hasCtrl)
            {
                child = GetFieldNext(child);
                continue;
            }

            // 清理函数名：空格等非法 ASCII 字符替换为下划线
            for (auto& c : fi.name)
            {
                unsigned char uc = static_cast<unsigned char>(c);
                if (uc < 0x80 && !std::isalnum(uc) && c != '_')
                {
                    c = '_';
                }
            }
            if (!fi.name.empty()
                && std::isdigit(
                    static_cast<unsigned char>(fi.name[0])))
            {
                fi.name = "_" + fi.name;
            }

            if (Off().UFunction_FunctionFlags != -1)
            {
                u32 rawFlags = 0;
                GReadValue(
                    child + Off().UFunction_FunctionFlags,
                    rawFlags);
                fi.functionFlags = rawFlags;
            }

            // 跳过 Delegate 函数（对标 Rei-Dumper）
            if (fi.functionFlags & 0x00100000)
            {
                child = GetFieldNext(child);
                continue;
            }

            fi.params = CollectFuncParams(child);
            fi.paramStructSize = GetStructSize(child);

            fi.returnType = "void";
            for (auto& p : fi.params)
            {
                if (p.isReturnParam)
                {
                    fi.returnType = p.sigTypeName;
                    break;
                }
            }

            funcs.push_back(fi);
        }
        child = GetFieldNext(child);
    }

    ReverseIfNeeded(funcs);

    cache[structObj] = funcs;
    return funcs;
}

} // namespace detail
} // namespace xrd
