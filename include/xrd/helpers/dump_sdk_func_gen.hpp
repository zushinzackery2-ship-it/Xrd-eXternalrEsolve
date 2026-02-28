#pragma once
// Xrd-eXternalrEsolve - SDK 导出：函数/参数/断言 代码生成
// 从 dump_sdk_struct.hpp 拆分，保持单文件 300 行以内
// 对标 Rei-Dumper CppGenerator 的函数实现格式

#include "dump_collect.hpp"
#include "dump_property_flags.hpp"
#include "dump_function_flags.hpp"
#include "dump_sdk_format.hpp"
#include <string>
#include <format>

namespace xrd
{
namespace detail
{

// 生成 _parameters.hpp 中单个函数的参数结构体
// 对标 Rei-Dumper：带偏移/大小注释、padding
inline std::string GenerateParamStruct(
    const std::string& pkgName,
    const std::string& className,
    const FunctionInfo& func)
{
    // 跳过包含垃圾数据的函数
    if (HasBinaryGarbage(func.name))
    {
        return "";
    }
    for (auto& p : func.params)
    {
        if (HasBinaryGarbage(p.typeName)
            || HasBinaryGarbage(p.name))
        {
            return "";
        }
    }

    std::string out;
    std::string structName = className + "_" + func.name;

    // 函数注释头（对标 Rei-Dumper: Package.ClassName.FuncName）
    out += std::format("// Function {}.{}.{}\n",
        pkgName, className, func.name);
    out += std::format("// 0x{:04X} (0x{:04X} - 0x{:04X})\n",
        func.paramStructSize, func.paramStructSize, 0);
    out += std::format("struct {} final\n{{\npublic:\n",
        structName);

    // 使用 GenerateMembers 风格的 padding 生成
    i32 prevEnd = 0;
    for (auto& p : func.params)
    {
        // 字节填充
        if (p.offset > prevEnd)
        {
            out += GenerateBytePadding(
                prevEnd, p.offset - prevEnd,
                "Fixing Size After Last Property");
        }

        std::string flagStr = StringifyPropertyFlags(p.flags);
        std::string comment = std::format(
            "0x{:04X}(0x{:04X})({})",
            p.offset, p.size, flagStr);

        // 参数结构体中使用原始类型（不加 const/&/*）
        out += MakeMemberString(p.typeName, p.name, comment);
        prevEnd = p.offset + p.size;
    }

    // 结构体尾部填充
    if (func.paramStructSize > prevEnd
        && func.paramStructSize > 0)
    {
        out += GenerateBytePadding(
            prevEnd, func.paramStructSize - prevEnd,
            "Fixing Struct Size After Last Property");
    }

    out += "};\n";
    out += std::format("DUMPER7_ASSERTS_{};\n\n", structName);

    return out;
}

// 生成 _functions.cpp 中单个函数的实现
// 对标 Rei-Dumper：完整注释头、参数描述、Native 标志保存/恢复
inline std::string GenerateFunctionImpl(
    const std::string& pkgName,
    const std::string& className,
    const std::string& prefixedClassName,
    const FunctionInfo& func,
    bool isInterface,
    const std::string& collisionNs = "")
{
    // 跳过包含垃圾数据的函数
    if (HasBinaryGarbage(func.name)
        || HasBinaryGarbage(func.returnType))
    {
        return "";
    }
    for (auto& p : func.params)
    {
        if (HasBinaryGarbage(p.sigTypeName)
            || HasBinaryGarbage(p.name)
            || HasBinaryGarbage(p.typeName))
        {
            return "";
        }
    }

    std::string out;
    bool isNative = (func.functionFlags & 0x00000400) != 0;
    bool isStatic = (func.functionFlags & 0x00002000) != 0;
    bool hasParams = !func.params.empty();

    // 冲突包使用命名空间限定名
    std::string qualifiedClass = collisionNs.empty()
        ? prefixedClassName
        : (collisionNs + "::" + prefixedClassName);

    // 函数全名注释（对标 Rei-Dumper: Package.ClassName.FuncName）
    out += std::format("// Function {}.{}.{}\n",
        pkgName, className, func.name);
    // 函数标志注释
    out += std::format("// ({})\n",
        StringifyFunctionFlags(func.functionFlags));

    // 参数描述注释（对标 Rei-Dumper 的 40/55 列对齐）
    if (hasParams)
    {
        out += "// Parameters:\n";
        for (auto& p : func.params)
        {
            std::string flagStr =
                StringifyPropertyFlags(p.flags);
            // 对标 Rei-Dumper：签名中的类型（含 const/&/*）
            std::string dispType = p.sigTypeName;
            out += std::format("// {:{}}{:{}}({})\n",
                dispType, 40, p.name, 55, flagStr);
        }
    }
    out += "\n";

    // 函数签名（对标 Rei-Dumper 格式）
    out += func.returnType + " " + qualifiedClass
        + "::" + func.name + "(";

    bool first = true;
    for (auto& p : func.params)
    {
        if (p.isReturnParam) continue;
        if (!first) out += ", ";
        out += p.sigTypeName + " " + p.name;
        first = false;
    }

    out += ")\n{\n";

    // 函数体（对标 Rei-Dumper：tab 缩进，brace-less if）
    out += "\tstatic class UFunction* Func = nullptr;\n\n";
    out += "\tif (Func == nullptr)\n";

    // GetFunction 调用
    if (isInterface)
    {
        out += std::format(
            "\t\tFunc = AsUObject()->Class->"
            "GetFunction(\"{}\", \"{}\");\n",
            className, func.name);
    }
    else if (isStatic)
    {
        out += std::format(
            "\t\tFunc = StaticClass()->"
            "GetFunction(\"{}\", \"{}\");\n",
            className, func.name);
    }
    else
    {
        out += std::format(
            "\t\tFunc = Class->"
            "GetFunction(\"{}\", \"{}\");\n",
            className, func.name);
    }

    // 参数结构体创建
    bool hasParamsToInit = false;
    std::string paramAssignments;
    std::string outRefAssignments;
    std::string outPtrAssignments;

    if (hasParams)
    {
        std::string paramStructName =
            className + "_" + func.name;
        out += std::format(
            "\n\tParams::{} Parms{{}};\n",
            paramStructName);

        for (auto& p : func.params)
        {
            if (p.isReturnParam) continue;

            if (p.isOutParam && !p.isRefParam)
            {
                // out-ptr 参数在调用后处理
                if (p.isMoveType)
                {
                    outPtrAssignments += std::format(
                        R"(

	if ({0} != nullptr)
		*{0} = std::move(Parms.{0});)",
                        p.name);
                }
                else
                {
                    outPtrAssignments += std::format(
                        R"(

	if ({0} != nullptr)
		*{0} = Parms.{0};)",
                        p.name);
                }
                continue;
            }

            // 普通参数赋值
            if (p.isMoveType)
            {
                paramAssignments += std::format(
                    "\tParms.{0} = std::move({0});\n",
                    p.name);
            }
            else
            {
                paramAssignments += std::format(
                    "\tParms.{0} = {0};\n", p.name);
            }
            hasParamsToInit = true;

            // out-ref 参数回写
            if (p.isOutParam && p.isRefParam && !p.isConstParam)
            {
                if (p.isMoveType)
                {
                    outRefAssignments += std::format(
                        "\n\t{0} = std::move(Parms.{0});",
                        p.name);
                }
                else
                {
                    outRefAssignments += std::format(
                        "\n\t{0} = Parms.{0};", p.name);
                }
            }
        }

        if (hasParamsToInit)
        {
            out += "\n" + paramAssignments;
        }
    }

    // Native 函数：保存/恢复 FunctionFlags
    if (isNative)
    {
        out += "\n\tauto Flgs = Func->FunctionFlags;\n";
        out += "\tFunc->FunctionFlags |= 0x400;\n";
    }

    // ProcessEvent 调用
    out += "\n\t";
    if (isStatic)
    {
        out += "GetDefaultObj()->";
    }
    else if (isInterface)
    {
        out += "AsUObject()->";
    }
    else
    {
        out += "UObject::";
    }
    out += std::format("ProcessEvent(Func, {});\n",
        hasParams ? "&Parms" : "nullptr");

    // 恢复 FunctionFlags
    if (isNative)
    {
        out += "\n\tFunc->FunctionFlags = Flgs;\n";
    }

    // out-ref 参数回写
    if (!outRefAssignments.empty())
    {
        out += outRefAssignments + "\n";
    }

    // out-ptr 参数回写
    if (!outPtrAssignments.empty())
    {
        out += outPtrAssignments + "\n";
    }

    // 返回值
    for (auto& p : func.params)
    {
        if (p.isReturnParam)
        {
            out += "\n\treturn Parms.ReturnValue;\n";
            break;
        }
    }

    out += "}\n\n";
    return out;
}

// 生成断言宏定义（追加到 Assertions.inl）
// assertName 可以是 "UClassName" 或 "PkgName__UClassName"
// typeName 是实际 C++ 类型名（可能含 ::）
inline std::string GenerateAssertionMacro(
    const std::string& assertName,
    const std::string& typeName,
    i32 size,
    i32 alignment = 0x08)
{
    std::string out;
    out += std::format(
        "#define DUMPER7_ASSERTS_{} \\\n", assertName);
    out += std::format(
        "static_assert(alignof({}) == 0x{:06X}, "
        "\"Wrong alignment on {}\"); \\\n",
        typeName, alignment, assertName);
    out += std::format(
        "static_assert(sizeof({}) == 0x{:06X}, "
        "\"Wrong size on {}\"); \\\n",
        typeName, size > 0 ? size : 1, assertName);
    out += "\n";
    return out;
}

} // namespace detail
} // namespace xrd
