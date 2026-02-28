#pragma once
// Xrd-eXternalrEsolve - SDK 导出：结构体/类/枚举生成
// 生成单个 struct/class/enum 的 C++ 代码文本
// 对标 Rei-Dumper 的 _classes.hpp / _structs.hpp 格式

#include "dump_sdk_format.hpp"
#include "dump_enum.hpp"
#include "dump_prefix.hpp"
#include "dump_predefined.hpp"
#include "gen/gen_struct_predefined.hpp"
#include <string>
#include <format>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace xrd
{
namespace detail
{

// 生成函数签名字符串（用于 _classes.hpp 中的声明）
inline std::string FormatFunctionDeclaration(
    const FunctionInfo& func)
{
    // 跳过包含垃圾数据的函数签名
    if (HasBinaryGarbage(func.returnType)
        || HasBinaryGarbage(func.name))
    {
        return "";
    }
    for (auto& p : func.params)
    {
        if (HasBinaryGarbage(p.sigTypeName)
            || HasBinaryGarbage(p.name))
        {
            return "";
        }
    }

    std::string sig = "\t";
    bool isStatic = (func.functionFlags & 0x00002000) != 0;
    if (isStatic)
    {
        sig += "static ";
    }
    sig += func.returnType + " " + func.name + "(";
    bool first = true;
    for (auto& p : func.params)
    {
        if (p.isReturnParam) continue;
        if (!first) sig += ", ";
        sig += p.sigTypeName + " " + p.name;
        first = false;
    }
    sig += ");\n";
    return sig;
}

// 生成单个枚举的 C++ 代码（用于 _structs.hpp）
inline std::string GenerateEnumCode(const EnumInfo& ei)
{
    static std::unordered_map<std::string, bool> globalEnumNames;
    static std::unordered_set<std::string> illegalNames = {
        "IN", "OUT", "TRUE", "FALSE", "DELETE",
        "PF_MAX", "SW_MAX", "MM_MAX", "SIZE_MAX",
        "RELATIVE", "TRANSPARENT", "NO_ERROR",
        "EVENT_MAX", "IGNORE"
    };

    std::string out;
    out += std::format("// Enum {}.{}\n",
        StripPackagePrefix(ei.outerName), ei.name);
    out += std::format("// NumValues: 0x{:04X}\n",
        ei.members.size());
    // 根据 underlyingTypeSize 确定底层类型（对标 Rei-Dumper）
    static constexpr const char* kEnumTypeBySize[] = {
        "uint8", "uint16", "InvalidEnumSize", "uint32",
        "InvalidEnumSize", "InvalidEnumSize", "InvalidEnumSize", "uint64"
    };
    const char* enumType = (ei.underlyingTypeSize >= 1 && ei.underlyingTypeSize <= 8)
        ? kEnumTypeBySize[ei.underlyingTypeSize - 1] : "uint8";
    // 对标 Rei-Dumper：枚举名不以 E 开头时加 E 前缀
    std::string enumName = ei.name;
    if (!enumName.empty() && enumName[0] != 'E')
    {
        enumName = "E" + enumName;
    }
    out += std::format("enum class {} : {}\n{{\n",
        enumName, enumType);

    std::vector<std::pair<std::string, int>> localNames;
    for (size_t i = 0; i < ei.members.size(); ++i)
    {
        auto& m = ei.members[i];
        std::string name = m.name;
        int collisionCount = 0;

        auto git = globalEnumNames.find(name);
        bool isNewGlobal = (git == globalEnumNames.end());
        if (isNewGlobal)
        {
            globalEnumNames[name] = true;
        }
        if (!isNewGlobal)
        {
            for (auto& [ln, lc] : localNames)
            {
                if (ln == name)
                {
                    collisionCount = lc + 1;
                    break;
                }
            }
        }
        if (illegalNames.count(name))
        {
            if (collisionCount == 0)
            {
                collisionCount = 1;
            }
        }
        localNames.push_back({name, collisionCount});
        if (collisionCount > 0)
        {
            name += "_" + std::to_string(collisionCount - 1);
        }
        out += std::format("\t{:{}} = {},\n",
            name, 40, m.value);
    }
    out += "};\n\n";
    return out;
}

// 生成 StaticClass/StaticName/GetDefaultObj 内联函数体
inline std::string GenerateStaticClassFuncs(
    const std::string& qualifiedName,
    const std::string& rawName,
    const std::string& objClassName,
    bool hasCollision,
    const std::string& outerName)
{
    std::string out;
    bool isBPClass = (
        objClassName == "BlueprintGeneratedClass" ||
        objClassName == "WidgetBlueprintGeneratedClass" ||
        objClassName == "AnimBlueprintGeneratedClass");

    out += "\tstatic class UClass* StaticClass()\n";
    out += "\t{\n";
    if (hasCollision)
    {
        std::string shortOuter = StripPackagePrefix(outerName);
        std::string fullNameStr = objClassName + " "
            + shortOuter + "." + rawName;
        out += std::format(
            "\t\tBP_STATIC_CLASS_IMPL_FULLNAME(\"{}\")\n",
            fullNameStr);
    }
    else if (isBPClass)
    {
        out += std::format(
            "\t\tBP_STATIC_CLASS_IMPL(\"{}\")\n", rawName);
    }
    else
    {
        out += std::format(
            "\t\tSTATIC_CLASS_IMPL(\"{}\")\n", rawName);
    }
    out += "\t}\n";
    out += "\tstatic const class FName& StaticName()\n";
    out += "\t{\n";
    out += std::format(
        "\t\tSTATIC_NAME_IMPL(L\"{}\")\n", rawName);
    out += "\t}\n";
    out += std::format(
        "\tstatic class {}* GetDefaultObj()\n", qualifiedName);
    out += "\t{\n";
    out += std::format(
        "\t\treturn GetDefaultObjImpl<{}>();\n", qualifiedName);
    out += "\t}\n";
    return out;
}

// 统一函数条目：追踪 inline/static/const 切换
struct FuncEntry
{
    std::string text;
    bool bIsInline  = false;
    bool bIsStatic  = false;
    bool bIsConst   = false;
};

// UObject 预定义非内联函数条目
inline std::vector<FuncEntry> BuildUObjectPredefEntries()
{
    std::vector<FuncEntry> entries;
    entries.push_back({
        "\tstatic class UObject* FindObjectFastImpl("
        "const std::string& Name, "
        "EClassCastFlags RequiredType = EClassCastFlags::None);\n",
        false, true, false});
    entries.push_back({
        "\tstatic class UObject* FindObjectImpl("
        "const std::string& FullName, "
        "EClassCastFlags RequiredType = EClassCastFlags::None);\n",
        false, true, false});
    entries.push_back({
        "\tstd::string GetFullName() const;\n",
        false, false, true});
    entries.push_back({
        "\tstd::string GetName() const;\n",
        false, false, true});
    entries.push_back({
        "\tbool HasTypeFlag(EClassCastFlags TypeFlags) const;\n",
        false, false, true});
    entries.push_back({
        "\tbool IsA(EClassCastFlags TypeFlags) const;\n",
        false, false, true});
    entries.push_back({
        "\tbool IsA(const class FName& ClassName) const;\n",
        false, false, true});
    entries.push_back({
        "\tbool IsA(const class UClass* TypeClass) const;\n",
        false, false, true});
    entries.push_back({
        "\tbool IsDefaultObject() const;\n",
        false, false, true});
    return entries;
}

// UObject 预定义内联函数条目
inline std::vector<FuncEntry> BuildUObjectInlineEntries()
{
    std::vector<FuncEntry> entries;
    {
        std::string t;
        t += "\tstatic class UClass* FindClass(const std::string& ClassFullName)\n";
        t += "\t{\n";
        t += "\t\treturn FindObject<class UClass>(ClassFullName, EClassCastFlags::Class);\n";
        t += "\t}\n";
        entries.push_back({t, true, true, false});
    }
    {
        std::string t;
        t += "\tstatic class UClass* FindClassFast(const std::string& ClassName)\n";
        t += "\t{\n";
        t += "\t\treturn FindObjectFast<class UClass>(ClassName, EClassCastFlags::Class);\n";
        t += "\t}\n";
        entries.push_back({t, true, true, false});
    }
    {
        std::string t;
        t += "\t\n";
        t += "\ttemplate<typename UEType = UObject>\n";
        t += "\tstatic UEType* FindObject(const std::string& Name, EClassCastFlags RequiredType = EClassCastFlags::None)\n";
        t += "\t{\n";
        t += "\t\treturn static_cast<UEType*>(FindObjectImpl(Name, RequiredType));\n";
        t += "\t}\n";
        entries.push_back({t, true, true, false});
    }
    {
        std::string t;
        t += "\ttemplate<typename UEType = UObject>\n";
        t += "\tstatic UEType* FindObjectFast(const std::string& Name, EClassCastFlags RequiredType = EClassCastFlags::None)\n";
        t += "\t{\n";
        t += "\t\treturn static_cast<UEType*>(FindObjectFastImpl(Name, RequiredType));\n";
        t += "\t}\n";
        entries.push_back({t, true, true, false});
    }
    {
        std::string t;
        t += "\tvoid ProcessEvent(class UFunction* Function, void* Parms) const\n";
        t += "\t{\n";
        t += "\t\tInSDKUtils::CallGameFunction(InSDKUtils::GetVirtualFunction<void(*)(const UObject*, class UFunction*, void*)>(this, Offsets::ProcessEventIdx), this, Function, Parms);\n";
        t += "\t}\n";
        entries.push_back({t, true, false, true});
    }
    return entries;
}

// UStruct 预定义函数条目
inline std::vector<FuncEntry> BuildUStructPredefEntries()
{
    std::vector<FuncEntry> entries;
    entries.push_back({"\tbool IsSubclassOf(const UStruct* Base) const;\n", false, false, true});
    entries.push_back({"\tbool IsSubclassOf(const FName& baseClassName) const;\n", false, false, true});
    return entries;
}

// UClass 预定义函数条目
inline std::vector<FuncEntry> BuildUClassPredefEntries()
{
    std::vector<FuncEntry> entries;
    entries.push_back({"\tclass UFunction* GetFunction(const char* ClassName, const char* FuncName) const;\n", false, false, true});
    return entries;
}

// StaticClass 条目列表
inline std::vector<FuncEntry> BuildStaticClassEntries(
    const std::string& qualifiedName,
    const std::string& rawName,
    const std::string& objClassName,
    bool hasCollision,
    const std::string& outerName)
{
    std::vector<FuncEntry> entries;
    std::string scText = GenerateStaticClassFuncs(qualifiedName, rawName, objClassName, hasCollision, outerName);
    entries.push_back({scText, true, true, false});
    return entries;
}

// 输出统一函数列表，处理 inline/static/const 切换
inline std::string EmitFunctionEntries(const std::vector<FuncEntry>& entries)
{
    std::string out;
    bool wasLastInline = false;
    bool wasLastStatic = false;
    bool wasLastConst  = false;
    bool isFirst = true;
    bool didSwitch = false;

    for (auto& fe : entries)
    {
        if (!isFirst)
        {
            if (wasLastInline != fe.bIsInline)
            {
                out += "\npublic:\n";
                didSwitch = true;
            }
            else if ((wasLastStatic != fe.bIsStatic || wasLastConst != fe.bIsConst) && !didSwitch)
            {
                out += "\n";
            }
            if (wasLastInline == fe.bIsInline)
            {
                didSwitch = false;
            }
        }
        out += fe.text;
        wasLastInline = fe.bIsInline;
        wasLastStatic = fe.bIsStatic;
        wasLastConst  = fe.bIsConst;
        isFirst = false;
    }
    return out;
}

// 生成 _classes.hpp 中单个类的代码
// 使用统一函数条目列表，精确追踪 inline/static/const 切换
inline std::string GenerateClassCode(const StructEntry& entry)
{
    std::string out;

    std::string prefixedName = AddStructPrefix(
        entry.name, entry.isClass,
        entry.isActorChild, entry.isInterfaceChild);
    std::string prefixedSuper;
    if (!entry.superName.empty())
    {
        prefixedSuper = LookupPrefixedName(entry.superName, true);
    }

    bool hasCollision = !entry.collisionNs.empty();
    std::string qualifiedName = hasCollision
        ? (entry.collisionNs + "::" + prefixedName) : prefixedName;
    std::string assertName = hasCollision
        ? (entry.collisionNs + "__" + prefixedName) : prefixedName;

    // 对标 Rei-Dumper：注释中 superSize 使用 aligned 值
    i32 alignedEntrySize = entry.size;
    // 注释用 aligned superSize（对标 Rei-Dumper）
    i32 parentAlignForComment = 1;
    if (!entry.superName.empty())
    {
        auto& lk = GetEntryLookup();
        auto superIt = lk.find(entry.superName);
        if (superIt != lk.end())
        {
            parentAlignForComment = superIt->second->alignment > 0
                ? superIt->second->alignment : 1;
        }
    }
    i32 commentSuperSize = (entry.superSize + parentAlignForComment - 1)
        & ~(parentAlignForComment - 1);
    i32 sizeWithoutSuper = alignedEntrySize - commentSuperSize;
    bool isInterface = entry.isInterfaceChild;

    // 注释头
    std::string cleanOuter = StripPackagePrefix(entry.outerName);
    std::string objClassName = entry.objClassName;
    if (objClassName.empty() && entry.addr)
    {
        objClassName = GetObjectClassName(entry.addr);
    }
    std::string commentType = objClassName.empty()
        ? (entry.isClass ? "Class" : "ScriptStruct") : objClassName;
    out += std::format("// {} {}.{}\n", commentType, cleanOuter, entry.name);

    if (isInterface)
    {
        out += "// 0x0000 (0x0000 - 0x0000)\n";
    }
    else
    {
        out += std::format("// 0x{:04X} (0x{:04X} - 0x{:04X})\n",
            sizeWithoutSuper > 0 ? sizeWithoutSuper : 0,
            entry.size, commentSuperSize);
    }

    // 类声明
    std::string finalStr = entry.isFinal ? " final" : "";

    // 对标 Rei-Dumper：#pragma pack(push, 0x1) 只给有 alignas 的类
    // （bUseExplicitAlignment = true 时同时输出 alignas 和 #pragma pack）
    bool needPragmaPack = !isInterface
        && entry.bUseExplicitAlignment;
    if (needPragmaPack)
    {
        out += "#pragma pack(push, 0x1)\n";
    }

    out += "class ";
    if (isInterface)
    {
        out += qualifiedName + " final";
    }
    else if (!prefixedSuper.empty())
    {
        if (entry.bUseExplicitAlignment)
        {
            out += std::format("alignas(0x{:02X}) ",
                entry.alignment);
        }
        out += qualifiedName + finalStr + " : public " + prefixedSuper;
    }
    else
    {
        // 无父类：使用显式对齐或默认 0x08
        i32 align = entry.bUseExplicitAlignment
            ? entry.alignment : 0x08;
        out += std::format("alignas(0x{:02X}) {}{}",
            align, qualifiedName, finalStr);
    }

    // Interface 子类特殊处理
    if (isInterface && entry.isClass)
    {
        out += "\n{\npublic:\n";
        out += GenerateStaticClassFuncs(
            qualifiedName, entry.name, objClassName,
            hasCollision, entry.outerName);
        out += "\n";
        out += "\tclass UObject* AsUObject()\n";
        out += "\t{\n";
        out += "\t\treturn reinterpret_cast<UObject*>(this);\n";
        out += "\t}\n";
        out += "\tconst class UObject* AsUObject() const\n";
        out += "\t{\n";
        out += "\t\treturn reinterpret_cast<const UObject*>(this);\n";
        out += "\t}\n";
        out += "};\n";
        out += std::format("#ifdef DUMPER7_ASSERTS_{}\n", assertName);
        out += std::format("DUMPER7_ASSERTS_{};\n", assertName);
        out += "#endif\n\n";
        return out;
    }

    // ─── 收集成员 ───
    std::string membersStr;
    std::string predefMembers = GetPredefinedMembers(entry.name);
    if (!predefMembers.empty())
    {
        membersStr = predefMembers;
    }
    else
    {
        auto props = CollectProperties(entry.addr);
        i32 effectiveSuperSize = entry.superSize;
        auto& lk = GetEntryLookup();
        if (!entry.superName.empty())
        {
            auto superIt = lk.find(entry.superName);
            if (superIt != lk.end())
            {
                if (superIt->second->bHasReusedTrailingPadding)
                {
                    effectiveSuperSize =
                        superIt->second->unalignedSize;
                }
                else if (superIt->second->bCanSkipTrailingPad)
                {
                    // 父类有 pack 且 alignas 足以填充 trailing pad
                    // 子类从 lastMemberEnd 开始生成显式 pad
                    effectiveSuperSize =
                        superIt->second->lastMemberEnd;
                }
                else
                {
                    i32 parentAlign2 = superIt->second->alignment > 0
                        ? superIt->second->alignment : 1;
                    effectiveSuperSize =
                        (entry.superSize + parentAlign2 - 1)
                        & ~(parentAlign2 - 1);
                }
            }
        }
        i32 effectiveSize = entry.unalignedSize > 0
            ? entry.unalignedSize : entry.size;
        // bCanSkipTrailingPad：不生成 trailing pad，靠 alignas 填充
        // 子类会从 lastMemberEnd 开始生成显式 pad
        if (entry.bCanSkipTrailingPad)
        {
            effectiveSize = entry.lastMemberEnd;
        }
        membersStr = GenerateMembers(
            props, effectiveSuperSize, effectiveSize);
    }

    i32 effectiveAlign = entry.alignment > 0 ? entry.alignment : 8;
    bool bHasMembers = !membersStr.empty() || (sizeWithoutSuper >= effectiveAlign);

    // ─── 构建统一函数条目列表 ───
    std::vector<FuncEntry> funcEntries;

    // 预定义非内联函数
    if (entry.name == "Object")
    {
        auto v = BuildUObjectPredefEntries();
        funcEntries.insert(funcEntries.end(), v.begin(), v.end());
    }
    else if (entry.name == "Struct")
    {
        auto v = BuildUStructPredefEntries();
        funcEntries.insert(funcEntries.end(), v.begin(), v.end());
    }
    else if (entry.name == "Class")
    {
        auto v = BuildUClassPredefEntries();
        funcEntries.insert(funcEntries.end(), v.begin(), v.end());
    }

    // 反射函数（non-inline）
    // 收集属性名，用于检测函数名冲突
    auto allProps = CollectProperties(entry.addr);
    std::unordered_set<std::string> propNames;
    for (auto& pi : allProps)
    {
        propNames.insert(pi.name);
    }
    auto funcs = CollectFunctions(entry.addr);
    for (auto& func : funcs)
    {
        // 对标 Rei-Dumper：函数名与属性名冲突时加下划线后缀
        FunctionInfo resolved = func;
        if (propNames.count(resolved.name))
        {
            resolved.name += "_";
        }
        bool isStatic = (resolved.functionFlags & 0x00002000) != 0;
        funcEntries.push_back({
            FormatFunctionDeclaration(resolved), false, isStatic, false});
    }

    // 预定义内联函数
    if (entry.name == "Object")
    {
        auto v = BuildUObjectInlineEntries();
        funcEntries.insert(funcEntries.end(), v.begin(), v.end());
    }

    // StaticClass/StaticName/GetDefaultObj
    bool hasStaticClass = entry.isClass
        && entry.name != "Object"
        && !entry.superName.empty();
    if (hasStaticClass)
    {
        auto v = BuildStaticClassEntries(
            qualifiedName, entry.name, objClassName,
            hasCollision, entry.outerName);
        funcEntries.insert(funcEntries.end(), v.begin(), v.end());
    }

    bool bHasFunctions = !funcEntries.empty();

    // ─── 输出类体 ───
    out += "\n{\n";
    if (bHasMembers || bHasFunctions)
    {
        out += "public:\n";
    }

    if (bHasMembers)
    {
        out += membersStr;
        if (bHasFunctions)
        {
            out += "\npublic:\n";
        }
    }

    if (bHasFunctions)
    {
        out += EmitFunctionEntries(funcEntries);
    }

    out += "};\n";
    if (needPragmaPack)
    {
        out += "#pragma pack(pop)\n";
    }
    out += std::format("#ifdef DUMPER7_ASSERTS_{}\n", assertName);
    out += std::format("DUMPER7_ASSERTS_{};\n", assertName);
    out += "#endif\n\n";
    return out;
}

// 生成 _structs.hpp 中单个结构体的代码
inline std::string GenerateStructCode(const StructEntry& entry)
{
    std::string out;

    std::string prefixedName = AddStructPrefix(
        entry.name, entry.isClass,
        entry.isActorChild, entry.isInterfaceChild);
    std::string prefixedSuper;
    if (!entry.superName.empty())
    {
        prefixedSuper = LookupPrefixedName(entry.superName, false);
    }

    i32 alignedEntrySizeS = entry.size;
    i32 sizeWithoutSuper = alignedEntrySizeS - entry.superSize;

    std::string objClassName = entry.objClassName;
    if (objClassName.empty() && entry.addr)
    {
        objClassName = GetObjectClassName(entry.addr);
    }
    std::string commentType = objClassName.empty()
        ? "ScriptStruct" : objClassName;
    out += std::format("// {} {}.{}\n",
        commentType, StripPackagePrefix(entry.outerName), entry.name);
    out += std::format("// 0x{:04X} (0x{:04X} - 0x{:04X})\n",
        sizeWithoutSuper > 0 ? sizeWithoutSuper : 0,
        entry.size, entry.superSize);

    std::string finalStr = entry.isFinal ? " final" : "";

    // 对标 Rei-Dumper：
    // - #pragma pack(push, 0x1) 只在 bHasReusedTrailingPadding 时使用
    // - alignas 在 bUseExplicitAlignment 时使用（minAlign > highestMemberAlign）
    bool needPragmaPack = entry.bHasReusedTrailingPadding;
    if (needPragmaPack)
    {
        out += "#pragma pack(push, 0x1)\n";
    }

    out += "struct ";
    if (entry.bUseExplicitAlignment)
    {
        out += std::format("alignas(0x{:02X}) ", entry.alignment);
    }
    out += prefixedName + finalStr;
    if (!prefixedSuper.empty())
    {
        out += " : public " + prefixedSuper;
    }
    // 预定义 typedef（如 FVector 的 UnderlayingType）
    std::string typedefStr = GetStructPredefinedTypedefs(entry.name);

    auto props = CollectProperties(entry.addr);

    i32 effectiveSuperSize = entry.superSize;
    auto& lk = GetEntryLookup();
    if (!entry.superName.empty())
    {
        auto superIt = lk.find(entry.superName);
        if (superIt != lk.end())
        {
            if (superIt->second->bHasReusedTrailingPadding)
            {
                effectiveSuperSize =
                    superIt->second->unalignedSize;
            }
            else if (superIt->second->bCanSkipTrailingPad)
            {
                effectiveSuperSize =
                    superIt->second->lastMemberEnd;
            }
            else
            {
                i32 parentAlign2 = superIt->second->alignment > 0
                    ? superIt->second->alignment : 1;
                effectiveSuperSize =
                    (entry.superSize + parentAlign2 - 1)
                    & ~(parentAlign2 - 1);
            }
        }
    }
    i32 effectiveSize = entry.unalignedSize > 0
        ? entry.unalignedSize : entry.size;
    if (entry.bCanSkipTrailingPad)
    {
        effectiveSize = entry.lastMemberEnd;
    }
    std::string membersStr2 = GenerateMembers(
        props, effectiveSuperSize, effectiveSize);

    // 预定义函数（如 FVector 的构造函数/运算符）
    std::string predFuncs = GetStructPredefinedFunctions(entry.name);

    bool bHasContent = !typedefStr.empty()
        || !membersStr2.empty() || !predFuncs.empty();

    out += "\n{\n";
    if (bHasContent)
    {
        out += "public:\n";
    }
    out += typedefStr;
    out += membersStr2;
    out += predFuncs;

    out += "};\n";
    if (needPragmaPack)
    {
        out += "#pragma pack(pop)\n";
    }
    out += std::format("#ifdef DUMPER7_ASSERTS_{}\n", prefixedName);
    out += std::format("DUMPER7_ASSERTS_{};\n", prefixedName);
    out += "#endif\n\n";
    return out;
}

} // namespace detail
} // namespace xrd

// 函数/参数/断言生成拆分到独立文件
#include "dump_sdk_func_gen.hpp"
