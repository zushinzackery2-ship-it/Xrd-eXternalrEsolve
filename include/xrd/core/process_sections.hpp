#pragma once
// Xrd-eXternalrEsolve - PE 段缓存
// 从 process.hpp 拆分：远程读取 PE 头并缓存各段数据

#include "process.hpp"

namespace xrd
{

// ─── PE 段缓存（远程读取 PE 头并缓存各段数据） ───

inline bool CacheSections(
    const IMemoryAccessor& mem,
    uptr moduleBase,
    u32 /*moduleSize*/,
    std::vector<SectionCache>& sections)
{
    sections.clear();

    // 读取 DOS 头
    IMAGE_DOS_HEADER dos{};
    if (!mem.Read(moduleBase, &dos, sizeof(dos)))
    {
        return false;
    }
    if (dos.e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    // 读取 NT 头
    IMAGE_NT_HEADERS64 nt{};
    if (!mem.Read(moduleBase + dos.e_lfanew, &nt, sizeof(nt)))
    {
        return false;
    }
    if (nt.Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    u16 numSections = nt.FileHeader.NumberOfSections;
    uptr sectionHeaderAddr = moduleBase + dos.e_lfanew + sizeof(IMAGE_NT_HEADERS64);

    for (u16 i = 0; i < numSections; ++i)
    {
        IMAGE_SECTION_HEADER sh{};
        if (!mem.Read(sectionHeaderAddr + i * sizeof(IMAGE_SECTION_HEADER), &sh, sizeof(sh)))
        {
            continue;
        }

        SectionCache sc;
        sc.name.assign(reinterpret_cast<const char*>(sh.Name), 8);

        // 去掉尾部的 null 字节
        while (!sc.name.empty() && sc.name.back() == '\0')
        {
            sc.name.pop_back();
        }

        sc.va   = moduleBase + sh.VirtualAddress;
        sc.size = sh.Misc.VirtualSize;

        // 缓存段数据到本地
        sc.data.resize(sc.size);
        if (mem.Read(sc.va, sc.data.data(), sc.size))
        {
            sections.push_back(std::move(sc));
        }
    }

    return !sections.empty();
}

inline const SectionCache* FindSection(
    const std::vector<SectionCache>& sections,
    const std::string& name)
{
    for (auto& s : sections)
    {
        if (s.name == name)
        {
            return &s;
        }
    }
    return nullptr;
}

} // namespace xrd
