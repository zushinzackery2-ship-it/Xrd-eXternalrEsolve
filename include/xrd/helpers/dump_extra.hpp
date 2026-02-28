#pragma once
// Xrd-eXternalrEsolve - SDK 导出：附加格式
// OffsetTable 和 Mapping 导出
// 从 dump_sdk.hpp 拆分，保持单文件 300 行以内

#include "dump_sdk.hpp"
#include <fstream>
#include <filesystem>
#include <set>

namespace xrd
{

// ─── 导出偏移表格式 ───
inline bool DumpOffsetTable(const std::wstring& outputPath)
{
    if (!IsInited()) return false;

    namespace fs = std::filesystem;
    fs::create_directories(outputPath);

    std::wstring filePath = outputPath + L"/OffsetsTable.txt";
    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << "// Offset dump by Xrd-eXternalrEsolve\n\n";

    auto entries = CollectAllStructEntries();
    for (auto& entry : entries)
    {
        auto props = detail::CollectProperties(entry.addr);
        file << "[" << entry.name << "] // Size: 0x"
             << std::hex << entry.size << std::dec << "\n";
        for (auto& prop : props)
        {
            file << "  0x" << std::hex << prop.offset << std::dec
                 << " " << prop.typeName << " " << prop.name
                 << " // Size: 0x" << std::hex << prop.size
                 << std::dec << "\n";
        }
        file << "\n";
    }

    file.close();
    std::cerr << "[xrd] 偏移表导出完成\n";
    return true;
}

// ─── 导出 Mapping ───
inline bool DumpMapping(const std::wstring& outputPath)
{
    if (!IsInited()) return false;

    namespace fs = std::filesystem;
    fs::create_directories(outputPath);

    std::wstring filePath = outputPath + L"/Mapping.txt";
    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << "// Mapping dump by Xrd-eXternalrEsolve\n\n";

    std::set<std::string> written;
    auto entries = CollectAllStructEntries();
    for (auto& entry : entries)
    {
        if (written.count(entry.name)) continue;
        written.insert(entry.name);

        auto props = detail::CollectProperties(entry.addr);
        for (auto& prop : props)
        {
            file << entry.name << "." << prop.name << " "
                 << prop.typeName << " 0x" << std::hex
                 << prop.offset << std::dec << "\n";
        }
    }

    file.close();
    std::cerr << "[xrd] Mapping 导出完成\n";
    return true;
}

// ─── 导出 GObjects-Dump 格式（对标 Rei-Dumper 的 Dumpspace 格式） ───
inline bool DumpSpaceSdk(const std::wstring& outputPath)
{
    if (!IsInited()) return false;

    namespace fs = std::filesystem;
    fs::create_directories(outputPath);

    i32 total = GetTotalObjectCount();

    // GObjects-Dump.txt — 不带属性
    {
        std::wstring filePath = outputPath + L"/GObjects-Dump.txt";
        std::ofstream file(filePath);
        if (!file.is_open()) return false;

        file << "Object dump by Xrd-eXternalrEsolve\n\n";
        file << "Count: " << total << "\n\n\n";

        for (i32 i = 0; i < total; ++i)
        {
            uptr obj = GetObjectByIndex(i);
            if (!IsCanonicalUserPtr(obj))
            {
                continue;
            }

            std::string className = GetObjectClassName(obj);
            std::string objName = GetObjectName(obj);
            std::string outerName;

            uptr outer = GetObjectOuter(obj);
            if (outer)
            {
                outerName = GetObjectName(outer);
            }

            // 格式：[Index] {Address} ClassName OuterName.ObjectName
            file << std::format("[{:08X}] {{0x{:x}}} {} {}.{}\n",
                i, obj, className, outerName, objName);
        }

        file.close();
    }

    // GObjects-Dump-WithProperties.txt — 带属性
    {
        std::wstring filePath = outputPath
            + L"/GObjects-Dump-WithProperties.txt";
        std::ofstream file(filePath);
        if (!file.is_open()) return false;

        file << "Object dump by Xrd-eXternalrEsolve\n\n";
        file << "Count: " << total << "\n\n\n";

        for (i32 i = 0; i < total; ++i)
        {
            uptr obj = GetObjectByIndex(i);
            if (!IsCanonicalUserPtr(obj))
            {
                continue;
            }

            std::string className = GetObjectClassName(obj);
            std::string objName = GetObjectName(obj);
            std::string outerName;

            uptr outer = GetObjectOuter(obj);
            if (outer)
            {
                outerName = GetObjectName(outer);
            }

            file << std::format("[{:08X}] {{0x{:x}}} {} {}.{}\n",
                i, obj, className, outerName, objName);

            // 如果是 Class/ScriptStruct，输出其属性
            bool isClassOrStruct =
                (className == "Class" ||
                 className == "ScriptStruct" ||
                 className == "Struct");

            if (isClassOrStruct)
            {
                auto props = detail::CollectProperties(obj);
                for (auto& prop : props)
                {
                    file << std::format(
                        "[{:08X}]     {} {}\n",
                        prop.offset,
                        prop.typeName, prop.name);
                }
            }
        }

        file.close();
    }

    std::cerr << "[xrd] GObjects-Dump 导出完成: "
              << total << " 个对象\n";
    return true;
}

} // namespace xrd
