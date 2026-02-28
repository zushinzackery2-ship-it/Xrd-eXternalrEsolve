#pragma once
// Xrd-eXternalrEsolve - 函数标志字符串化
// 对标 Rei-Dumper 的 StringifyFunctionFlags
// 将 u64 函数标志转换为人类可读的逗号分隔字符串

#include "../core/types.hpp"
#include <string>

namespace xrd
{
namespace detail
{

// EFunctionFlags 位定义（对标 UE4/5）
namespace EFuncFlags
{
    constexpr u32 Final                  = 0x00000001;
    constexpr u32 RequiredAPI            = 0x00000002;
    constexpr u32 BlueprintAuthorityOnly = 0x00000004;
    constexpr u32 BlueprintCosmetic      = 0x00000008;
    constexpr u32 Net                    = 0x00000040;
    constexpr u32 NetReliable            = 0x00000080;
    constexpr u32 NetRequest             = 0x00000100;
    constexpr u32 Exec                   = 0x00000200;
    constexpr u32 Native                 = 0x00000400;
    constexpr u32 Event                  = 0x00000800;
    constexpr u32 NetResponse            = 0x00001000;
    constexpr u32 Static                 = 0x00002000;
    constexpr u32 NetMulticast           = 0x00004000;
    constexpr u32 UbergraphFunction      = 0x00008000;
    constexpr u32 MulticastDelegate      = 0x00010000;
    constexpr u32 Public                 = 0x00020000;
    constexpr u32 Private                = 0x00040000;
    constexpr u32 Protected              = 0x00080000;
    constexpr u32 Delegate               = 0x00100000;
    constexpr u32 NetServer              = 0x00200000;
    constexpr u32 HasOutParms            = 0x00400000;
    constexpr u32 HasDefaults            = 0x00800000;
    constexpr u32 NetClient              = 0x01000000;
    constexpr u32 DLLImport              = 0x02000000;
    constexpr u32 BlueprintCallable      = 0x04000000;
    constexpr u32 BlueprintEvent         = 0x08000000;
    constexpr u32 BlueprintPure          = 0x10000000;
    constexpr u32 EditorOnly             = 0x20000000;
    constexpr u32 Const                  = 0x40000000;
    constexpr u32 NetValidate            = 0x80000000;
} // namespace EFuncFlags

// 将函数标志转为逗号分隔的字符串（对标 Rei-Dumper 格式和顺序）
inline std::string StringifyFunctionFlags(u64 flags)
{
    std::string ret;

    if (flags & EFuncFlags::Final)                  ret += "Final, ";
    if (flags & EFuncFlags::RequiredAPI)            ret += "RequiredAPI, ";
    if (flags & EFuncFlags::BlueprintAuthorityOnly) ret += "BlueprintAuthorityOnly, ";
    if (flags & EFuncFlags::BlueprintCosmetic)      ret += "BlueprintCosmetic, ";
    if (flags & EFuncFlags::Net)                    ret += "Net, ";
    if (flags & EFuncFlags::NetReliable)            ret += "NetReliable, ";
    if (flags & EFuncFlags::NetRequest)             ret += "NetRequest, ";
    if (flags & EFuncFlags::Exec)                   ret += "Exec, ";
    if (flags & EFuncFlags::Native)                 ret += "Native, ";
    if (flags & EFuncFlags::Event)                  ret += "Event, ";
    if (flags & EFuncFlags::NetResponse)            ret += "NetResponse, ";
    if (flags & EFuncFlags::Static)                 ret += "Static, ";
    if (flags & EFuncFlags::NetMulticast)           ret += "NetMulticast, ";
    if (flags & EFuncFlags::UbergraphFunction)      ret += "UbergraphFunction, ";
    if (flags & EFuncFlags::MulticastDelegate)      ret += "MulticastDelegate, ";
    if (flags & EFuncFlags::Public)                 ret += "Public, ";
    if (flags & EFuncFlags::Private)                ret += "Private, ";
    if (flags & EFuncFlags::Protected)              ret += "Protected, ";
    if (flags & EFuncFlags::Delegate)               ret += "Delegate, ";
    if (flags & EFuncFlags::NetServer)              ret += "NetServer, ";
    if (flags & EFuncFlags::HasOutParms)            ret += "HasOutParams, ";
    if (flags & EFuncFlags::HasDefaults)            ret += "HasDefaults, ";
    if (flags & EFuncFlags::NetClient)              ret += "NetClient, ";
    if (flags & EFuncFlags::DLLImport)              ret += "DLLImport, ";
    if (flags & EFuncFlags::BlueprintCallable)      ret += "BlueprintCallable, ";
    if (flags & EFuncFlags::BlueprintEvent)         ret += "BlueprintEvent, ";
    if (flags & EFuncFlags::BlueprintPure)          ret += "BlueprintPure, ";
    if (flags & EFuncFlags::EditorOnly)             ret += "EditorOnly, ";
    if (flags & EFuncFlags::Const)                  ret += "Const, ";
    if (flags & EFuncFlags::NetValidate)            ret += "NetValidate, ";

    // 去掉末尾 ", "
    if (ret.size() > 2)
    {
        ret.erase(ret.size() - 2);
    }
    return ret;
}

} // namespace detail
} // namespace xrd
