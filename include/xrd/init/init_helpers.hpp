#pragma once
// Xrd-eXternalrEsolve - 手动偏移设置与查询

#include "../core/context.hpp"

namespace xrd
{

// ─── 手动偏移设置（通过 RVA） ───

inline void SetGObjects(uptr rva)
{
    Ctx().off.GObjects = Ctx().mainModule.base + rva;
}

inline void SetGNames(uptr rva)
{
    Ctx().off.GNames = Ctx().mainModule.base + rva;
}

inline void SetGWorld(uptr rva)
{
    Ctx().off.GWorld = Ctx().mainModule.base + rva;
}

// ─── 手动偏移设置（通过 VA） ───

inline void SetGObjectsVA(uptr va) { Ctx().off.GObjects = va; }
inline void SetGNamesVA(uptr va)   { Ctx().off.GNames = va; }
inline void SetGWorldVA(uptr va)   { Ctx().off.GWorld = va; }

// ─── 查询 ───

inline bool IsUsingDoublePrecision()
{
    return Ctx().off.bUseDoublePrecision;
}

} // namespace xrd
