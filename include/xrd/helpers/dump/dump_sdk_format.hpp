#pragma once
// Xrd-eXternalrEsolve - SDK 导出：格式化工具
// Padding 生成、成员字符串对齐、BitField 格式化
// 对标 Dumper7 CppGenerator 的输出格式

#include "dump_collect.hpp"
#include "dump_property_flags.hpp"
#include <string>
#include <format>

namespace xrd
{
namespace detail
{

// 检查字符串是否包含非法字符：
// 1. 控制字符（< 0x20，排除 \t \n \r）
// 2. 非法高字节（> 0x7F 但不是有效 UTF-8 多字节序列的起始字节）
inline bool HasBinaryGarbage(const std::string& s)
{
    size_t i = 0;
    while (i < s.size())
    {
        unsigned char uc = static_cast<unsigned char>(s[i]);
        if (uc < 0x20 && s[i] != '\t' && s[i] != '\n'
            && s[i] != '\r')
        {
            return true;
        }
        if (uc >= 0x80)
        {
            // 验证 UTF-8 多字节序列
            int expected = 0;
            if ((uc & 0xE0) == 0xC0) expected = 1;
            else if ((uc & 0xF0) == 0xE0) expected = 2;
            else if ((uc & 0xF8) == 0xF0) expected = 3;
            else return true; // 非法起始字节
            if (i + expected >= s.size()) return true;
            for (int j = 1; j <= expected; j++)
            {
                unsigned char cb =
                    static_cast<unsigned char>(s[i + j]);
                if ((cb & 0xC0) != 0x80) return true;
            }
            i += expected + 1;
            continue;
        }
        i++;
    }
    return false;
}

// 生成对齐的成员字符串：Type(45列) Name;(50列) // Comment
// 对标 Rei-Dumper: 使用 Tab 缩进
inline std::string MakeMemberString(
    const std::string& type,
    const std::string& name,
    const std::string& comment)
{
    // 对标 Dumper7: <tab><--45 chars--><-------50 chars----->
    int nameFieldWidth = 50;
    if (type.length() >= 45)
    {
        if (type.length() + name.length() > 95)
        {
            nameFieldWidth = 1;
        }
        else
        {
            nameFieldWidth = 50 - static_cast<int>(type.length() - 45);
        }
    }

    return std::format("\t{:{}} {:{}} // {}\n",
        type, 45,
        name + ";", nameFieldWidth,
        comment);
}

// 生成字节填充（属性之间的间隙）
// 对标 Rei-Dumper: padding 注释末尾加 [ Rei-SdkDumper ]
inline std::string GenerateBytePadding(
    i32 offset,
    i32 padSize,
    const std::string& reason)
{
    return MakeMemberString(
        "uint8",
        std::format("Pad_{:X}[0x{:X}]", offset, padSize),
        std::format("0x{:04X}(0x{:04X})({} [ Rei-SdkDumper ])",
            offset, padSize, reason));
}

// 生成位填充（BitField 之间的间隙）
inline std::string GenerateBitPadding(
    i32 underlayingSize,
    i32 prevEndBit,
    i32 offset,
    i32 padBits,
    const std::string& reason)
{
    return MakeMemberString(
        GetTypeFromSize(underlayingSize),
        std::format("BitPad_{:X}_{:X} : {:d}",
            offset, prevEndBit, padBits),
        std::format("0x{:04X}(0x{:04X})({} [ Rei-SdkDumper ])",
            offset, underlayingSize, reason));
}

// 生成单个属性的成员字符串（含 BitField 处理）
inline std::string FormatProperty(const PropertyInfo& prop)
{
    // 对标 Rei-Dumper：属性名中 ASCII 范围的非法标识符字符替换为下划线
    // 保留非 ASCII 字符（如中文），MSVC 支持 UTF-8 标识符
    std::string memberName = prop.name;
    for (auto& c : memberName)
    {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 0x80
            && !std::isalnum(uc) && c != '_')
        {
            c = '_';
        }
    }
    // 首字符为数字时加下划线前缀
    if (!memberName.empty() && std::isdigit(
        static_cast<unsigned char>(memberName[0])))
    {
        memberName = "_" + memberName;
    }

    if (prop.arrayDim > 1)
    {
        memberName += std::format("[0x{:X}]", prop.arrayDim);
    }
    else if (prop.isBitField)
    {
        memberName += std::format(" : {:d}", prop.bitCount);
    }

    // 属性标志字符串（对标 Rei-Dumper 格式）
    std::string flagStr = StringifyPropertyFlags(prop.flags);

    std::string comment;
    if (prop.isBitField)
    {
        std::string bitInfo = std::format(
            "BitIndex: 0x{:02X}, PropSize: 0x{:04X}",
            prop.bitIndex, prop.size);
        if (!flagStr.empty())
        {
            bitInfo += " (" + flagStr + ")";
        }
        comment = std::format(
            "0x{:04X}(0x{:04X})({})",
            prop.offset, prop.size, bitInfo);
    }
    else
    {
        comment = std::format("0x{:04X}(0x{:04X})({})",
            prop.offset, prop.size, flagStr);
    }

    return MakeMemberString(prop.typeName, memberName, comment);
}

// 生成带 padding 的完整成员列表
// 对标 Dumper7 的 GenerateMembers：处理字节填充和位填充
inline std::string GenerateMembers(
    const std::vector<PropertyInfo>& props,
    i32 superSize,
    i32 structSize)
{
    std::string out;
    out.reserve(props.size() * 128);

    i32 prevEnd = superSize;
    bool lastWasBitField = false;
    i32 prevBitEndBit = 0;
    i32 prevBitSize = 1;
    i32 prevBitOffset = 0;

    for (auto& prop : props)
    {
        i32 memberEnd = prop.offset + prop.size;

        // 两个不同偏移的 BitField 之间，先补齐前一个的剩余位
        if (memberEnd > prevEnd && lastWasBitField && prop.isBitField)
        {
            i32 totalBits = prevBitSize * 8;
            if (prevBitEndBit < totalBits)
            {
                out += GenerateBitPadding(
                    prevBitSize, prevBitEndBit, prevBitOffset,
                    totalBits - prevBitEndBit,
                    "Fixing Bit-Field Size For New Byte");
            }
            prevBitEndBit = 0;
        }

        // 字节填充
        if (prop.offset > prevEnd)
        {
            out += GenerateBytePadding(
                prevEnd, prop.offset - prevEnd,
                "Fixing Size After Last Property");
        }

        // BitField 位填充
        if (prop.isBitField)
        {
            if (memberEnd > prevEnd)
            {
                prevBitEndBit = 0;
            }

            if (prevBitEndBit < prop.bitIndex)
            {
                out += GenerateBitPadding(
                    prop.size, prevBitEndBit, prop.offset,
                    prop.bitIndex - prevBitEndBit,
                    "Fixing Bit-Field Size Between Bits");
            }

            prevBitEndBit = prop.bitIndex + prop.bitCount;
            prevBitSize = prop.size;
            prevBitOffset = prop.offset;
        }

        lastWasBitField = prop.isBitField;

        // 更新 prevEnd（BitField 不推进 prevEnd，除非跨越了新字节）
        if (!prop.isBitField)
        {
            prevEnd = prop.offset + (prop.size * prop.arrayDim);
        }
        else if (memberEnd > prevEnd)
        {
            prevEnd = memberEnd;
        }

        out += FormatProperty(prop);
    }

    // 结构体尾部填充
    if (structSize > prevEnd)
    {
        out += GenerateBytePadding(
            prevEnd, structSize - prevEnd,
            "Fixing Struct Size After Last Property");
    }

    return out;
}

} // namespace detail
} // namespace xrd
