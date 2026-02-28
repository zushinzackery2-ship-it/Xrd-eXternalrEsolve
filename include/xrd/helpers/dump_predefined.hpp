#pragma once
// Xrd-eXternalrEsolve - 预定义成员生成
// 对标 Rei-Dumper 的 PredefinedMembers：
// UObject/UField/UStruct/UClass/UFunction/UEnum/IInterface
// 这些核心类的成员不是通过反射获取的，而是硬编码的

#include "dump_sdk_format.hpp"
#include "dump_prefix.hpp"
#include "../core/context.hpp"
#include <string>
#include <format>
#include <unordered_set>

namespace xrd
{
namespace detail
{

// 预定义成员的格式化辅助（NOT AUTO-GENERATED 注释）
inline std::string PredefinedMember(
    const std::string& type,
    const std::string& name,
    i32 offset,
    i32 size,
    const std::string& comment = "NOT AUTO-GENERATED PROPERTY")
{
    return MakeMemberString(type, name,
        std::format("0x{:04X}(0x{:04X})({})", offset, size, comment));
}

// 预定义静态成员
inline std::string PredefinedStaticMember(
    const std::string& type,
    const std::string& name,
    i32 offset,
    i32 size,
    const std::string& comment = "NOT AUTO-GENERATED PROPERTY")
{
    return MakeMemberString(
        "static inline " + type, name,
        std::format("0x{:04X}(0x{:04X})({})", offset, size, comment));
}

// 判断是否是需要预定义成员的核心类
inline bool IsPredefinedClass(const std::string& name)
{
    static const std::unordered_set<std::string> predefined = {
        "Object", "Field", "Struct", "Class",
        "Function", "Enum", "Interface",
        "ScriptStruct"
    };
    return predefined.count(name) > 0;
}

// 生成 UObject 的预定义成员（对标 Rei-Dumper CoreUObject_classes.hpp）
inline std::string GenerateUObjectPredefined()
{
    std::string out;
    out += PredefinedStaticMember(
        "class TUObjectArrayWrapper", "GObjects", 0x00, 0x08);
    out += "\n";
    out += PredefinedMember("void*", "VTable", 0x00, 0x08);
    out += PredefinedMember("EObjectFlags", "Flags", 0x08, 0x04);
    out += PredefinedMember("int32", "Index", 0x0C, 0x04);
    out += PredefinedMember("class UClass*", "Class", 0x10, 0x08);
    out += PredefinedMember("class FName", "Name", 0x18, 0x08);
    out += PredefinedMember("class UObject*", "Outer", 0x20, 0x08);
    return out;
}

// 生成 UObject 的预定义函数声明
// 不含开头的 \npublic:\n，由 GenerateClassCode 的 bHasMembers&&bHasFunctions 逻辑统一插入
inline std::string GenerateUObjectPredefinedFuncDecls()
{
    std::string out;
    // 静态查找函数
    out += "\tstatic class UObject* FindObjectFastImpl("
           "const std::string& Name, "
           "EClassCastFlags RequiredType = EClassCastFlags::None);\n";
    out += "\tstatic class UObject* FindObjectImpl("
           "const std::string& FullName, "
           "EClassCastFlags RequiredType = EClassCastFlags::None);\n";
    out += "\n";
    // 实例方法
    out += "\tstd::string GetFullName() const;\n";
    out += "\tstd::string GetName() const;\n";
    out += "\tbool HasTypeFlag(EClassCastFlags TypeFlags) const;\n";
    out += "\tbool IsA(EClassCastFlags TypeFlags) const;\n";
    out += "\tbool IsA(const class FName& ClassName) const;\n";
    out += "\tbool IsA(const class UClass* TypeClass) const;\n";
    out += "\tbool IsDefaultObject() const;\n";
    // 对标 Rei-Dumper：预定义函数声明末尾加空行，与反射函数之间有间隔
    out += "\n";
    return out;
}

// 生成 UObject 的内联函数体（放在反射函数之后）
inline std::string GenerateUObjectPredefinedInlineFuncs()
{
    std::string out;
    out += "\npublic:\n";
    out += "\tstatic class UClass* FindClass("
           "const std::string& ClassFullName)\n";
    out += "\t{\n";
    out += "\t\treturn FindObject<class UClass>("
           "ClassFullName, EClassCastFlags::Class);\n";
    out += "\t}\n";
    out += "\tstatic class UClass* FindClassFast("
           "const std::string& ClassName)\n";
    out += "\t{\n";
    out += "\t\treturn FindObjectFast<class UClass>("
           "ClassName, EClassCastFlags::Class);\n";
    out += "\t}\n";
    out += "\t\n";
    out += "\ttemplate<typename UEType = UObject>\n";
    out += "\tstatic UEType* FindObject("
           "const std::string& Name, "
           "EClassCastFlags RequiredType = EClassCastFlags::None)\n";
    out += "\t{\n";
    out += "\t\treturn static_cast<UEType*>("
           "FindObjectImpl(Name, RequiredType));\n";
    out += "\t}\n";
    out += "\ttemplate<typename UEType = UObject>\n";
    out += "\tstatic UEType* FindObjectFast("
           "const std::string& Name, "
           "EClassCastFlags RequiredType = EClassCastFlags::None)\n";
    out += "\t{\n";
    out += "\t\treturn static_cast<UEType*>("
           "FindObjectFastImpl(Name, RequiredType));\n";
    out += "\t}\n";
    out += "\n";
    out += "\tvoid ProcessEvent("
           "class UFunction* Function, void* Parms) const\n";
    out += "\t{\n";
    out += "\t\tInSDKUtils::CallGameFunction("
           "InSDKUtils::GetVirtualFunction<void(*)"
           "(const UObject*, class UFunction*, void*)>"
           "(this, Offsets::ProcessEventIdx), "
           "this, Function, Parms);\n";
    out += "\t}\n";
    return out;
}

// 生成 UField 的预定义成员
inline std::string GenerateUFieldPredefined()
{
    std::string out;
    out += PredefinedMember("class UField*", "Next", 0x28, 0x08);
    return out;
}

// 生成 UStruct 的预定义成员
inline std::string GenerateUStructPredefined()
{
    std::string out;
    // 0x30~0x3F 是 padding
    out += GenerateBytePadding(0x30, 0x10,
        "Fixing Size After Last Property");
    out += PredefinedMember("class UStruct*", "Super", 0x40, 0x08);
    out += PredefinedMember("class UField*", "Children", 0x48, 0x08);
    out += PredefinedMember("class FField*", "ChildProperties", 0x50, 0x08);
    out += PredefinedMember("int32", "Size", 0x58, 0x04);
    out += PredefinedMember("int16", "MinAlignment", 0x5C, 0x02);
    out += GenerateBytePadding(0x5E, 0x52,
        "Fixing Struct Size After Last Property");
    return out;
}

// 生成 UStruct 的预定义函数声明
// 不含开头的 \npublic:\n，由 GenerateClassCode 统一处理
inline std::string GenerateUStructPredefinedFunctions()
{
    std::string out;
    out += "\tbool IsSubclassOf(const UStruct* Base) const;\n";
    out += "\tbool IsSubclassOf(const FName& baseClassName) const;\n";
    return out;
}

// 生成 UClass 的预定义成员
inline std::string GenerateUClassPredefined()
{
    std::string out;
    out += GenerateBytePadding(0xB0, 0x20,
        "Fixing Size After Last Property");
    out += PredefinedMember("enum class EClassCastFlags", "CastFlags",
        0xD0, 0x08);
    out += GenerateBytePadding(0xD8, 0x40,
        "Fixing Size After Last Property");
    out += PredefinedMember("class UObject*", "DefaultObject",
        0x118, 0x08);
    out += GenerateBytePadding(0x120, 0x110,
        "Fixing Struct Size After Last Property");
    return out;
}

// 生成 UClass 的预定义函数声明
// 不含开头的 \npublic:\n，由 GenerateClassCode 统一处理
inline std::string GenerateUClassPredefinedFunctions()
{
    std::string out;
    out += "\tclass UFunction* GetFunction("
           "const char* ClassName, const char* FuncName) const;\n";
    return out;
}

// 生成 UFunction 的预定义成员
inline std::string GenerateUFunctionPredefined()
{
    std::string out;
    out += "\tusing FNativeFuncPtr = void (*)"
           "(void* Context, void* TheStack, void* Result);\n\n";
    out += PredefinedMember("uint32", "FunctionFlags", 0xB0, 0x04);
    out += GenerateBytePadding(0xB4, 0x24,
        "Fixing Size After Last Property");
    out += PredefinedMember("FNativeFuncPtr", "ExecFunction", 0xD8, 0x08);
    return out;
}

// 生成 UEnum 的预定义成员
inline std::string GenerateUEnumPredefined()
{
    std::string out;
    out += GenerateBytePadding(0x30, 0x10,
        "Fixing Size After Last Property");
    out += PredefinedMember(
        "class TArray<class TPair<class FName, int64>>", "Names",
        0x40, 0x10);
    out += GenerateBytePadding(0x50, 0x10,
        "Fixing Struct Size After Last Property");
    return out;
}

// IInterface 不再通过 GetPredefinedFunctions 输出
// 其 StaticClass/StaticName/GetDefaultObj/AsUObject 由 GenerateClassCode 统一处理

// 获取预定义成员代码（替换自动收集的属性）
// 返回空字符串表示该类不需要预定义成员
inline std::string GetPredefinedMembers(const std::string& name)
{
    if (name == "Object")   return GenerateUObjectPredefined();
    if (name == "Field")    return GenerateUFieldPredefined();
    if (name == "Struct")   return GenerateUStructPredefined();
    if (name == "Class")    return GenerateUClassPredefined();
    if (name == "Function") return GenerateUFunctionPredefined();
    if (name == "Enum")     return GenerateUEnumPredefined();
    return "";
}

// 获取预定义函数声明（追加到类体内，在反射函数之前）
inline std::string GetPredefinedFunctions(const std::string& name)
{
    if (name == "Object")    return GenerateUObjectPredefinedFuncDecls();
    if (name == "Struct")    return GenerateUStructPredefinedFunctions();
    if (name == "Class")     return GenerateUClassPredefinedFunctions();
    return "";
}

// 获取预定义内联函数体（追加到类体内，在反射函数之后）
inline std::string GetPredefinedInlineFunctions(const std::string& name)
{
    if (name == "Object") return GenerateUObjectPredefinedInlineFuncs();
    return "";
}

} // namespace detail
} // namespace xrd
