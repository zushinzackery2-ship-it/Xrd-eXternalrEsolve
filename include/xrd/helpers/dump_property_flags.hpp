#pragma once
// Xrd-eXternalrEsolve - 属性标志字符串化
// 对标 Rei-Dumper 的 StringifyPropertyFlags
// 将 u64 属性标志转换为人类可读的逗号分隔字符串

#include "../core/types.hpp"
#include <string>

namespace xrd
{
namespace detail
{

// EPropertyFlags 位定义（对标 UE4/5 的 EPropertyFlags）
namespace EPropFlags
{
    constexpr u64 Edit                          = 0x0000000000000001ULL;
    constexpr u64 ConstParm                     = 0x0000000000000002ULL;
    constexpr u64 BlueprintVisible              = 0x0000000000000004ULL;
    constexpr u64 ExportObject                  = 0x0000000000000008ULL;
    constexpr u64 BlueprintReadOnly             = 0x0000000000000010ULL;
    constexpr u64 Net                           = 0x0000000000000020ULL;
    constexpr u64 EditFixedSize                 = 0x0000000000000040ULL;
    constexpr u64 Parm                          = 0x0000000000000080ULL;
    constexpr u64 OutParm                       = 0x0000000000000100ULL;
    constexpr u64 ZeroConstructor               = 0x0000000000000200ULL;
    constexpr u64 ReturnParm                    = 0x0000000000000400ULL;
    constexpr u64 DisableEditOnTemplate         = 0x0000000000000800ULL;
    constexpr u64 Transient                     = 0x0000000000002000ULL;
    constexpr u64 Config                        = 0x0000000000004000ULL;
    constexpr u64 DisableEditOnInstance         = 0x0000000000010000ULL;
    constexpr u64 EditConst                     = 0x0000000000020000ULL;
    constexpr u64 GlobalConfig                  = 0x0000000000040000ULL;
    constexpr u64 InstancedReference            = 0x0000000000080000ULL;
    constexpr u64 DuplicateTransient            = 0x0000000000200000ULL;
    constexpr u64 SubobjectReference            = 0x0000000000400000ULL;
    constexpr u64 SaveGame                      = 0x0000000001000000ULL;
    constexpr u64 NoClear                       = 0x0000000002000000ULL;
    constexpr u64 ReferenceParm                 = 0x0000000008000000ULL;
    constexpr u64 BlueprintAssignable           = 0x0000000010000000ULL;
    constexpr u64 Deprecated                    = 0x0000000020000000ULL;
    constexpr u64 IsPlainOldData                = 0x0000000040000000ULL;
    constexpr u64 RepSkip                       = 0x0000000080000000ULL;
    constexpr u64 RepNotify                     = 0x0000000100000000ULL;
    constexpr u64 Interp                        = 0x0000000200000000ULL;
    constexpr u64 NonTransactional              = 0x0000000400000000ULL;
    constexpr u64 EditorOnly                    = 0x0000000800000000ULL;
    constexpr u64 NoDestructor                  = 0x0000001000000000ULL;
    constexpr u64 AutoWeak                       = 0x0000004000000000ULL;
    constexpr u64 ContainsInstancedReference    = 0x0000008000000000ULL;
    constexpr u64 AssetRegistrySearchable       = 0x0000010000000000ULL;
    constexpr u64 SimpleDisplay                 = 0x0000020000000000ULL;
    constexpr u64 AdvancedDisplay               = 0x0000040000000000ULL;
    constexpr u64 Protected                     = 0x0000080000000000ULL;
    constexpr u64 BlueprintCallable             = 0x0000100000000000ULL;
    constexpr u64 BlueprintAuthorityOnly        = 0x0000200000000000ULL;
    constexpr u64 TextExportTransient           = 0x0000400000000000ULL;
    constexpr u64 NonPIEDuplicateTransient      = 0x0000800000000000ULL;
    constexpr u64 ExposeOnSpawn                 = 0x0001000000000000ULL;
    constexpr u64 PersistentInstance            = 0x0002000000000000ULL;
    constexpr u64 UObjectWrapper                = 0x0004000000000000ULL;
    constexpr u64 HasGetValueTypeHash           = 0x0008000000000000ULL;
    constexpr u64 NativeAccessSpecifierPublic   = 0x0010000000000000ULL;
    constexpr u64 NativeAccessSpecifierProtected= 0x0020000000000000ULL;
    constexpr u64 NativeAccessSpecifierPrivate  = 0x0040000000000000ULL;
} // namespace EPropFlags

// 将属性标志转为逗号分隔的字符串（对标 Rei-Dumper 格式）
inline std::string StringifyPropertyFlags(u64 flags)
{
    std::string ret;

    // 按 Rei-Dumper 的顺序逐位检查
    if (flags & EPropFlags::Edit)                         ret += "Edit, ";
    if (flags & EPropFlags::ConstParm)                    ret += "ConstParm, ";
    if (flags & EPropFlags::BlueprintVisible)             ret += "BlueprintVisible, ";
    if (flags & EPropFlags::ExportObject)                 ret += "ExportObject, ";
    if (flags & EPropFlags::BlueprintReadOnly)            ret += "BlueprintReadOnly, ";
    if (flags & EPropFlags::Net)                          ret += "Net, ";
    if (flags & EPropFlags::EditFixedSize)                ret += "EditFixedSize, ";
    if (flags & EPropFlags::Parm)                         ret += "Parm, ";
    if (flags & EPropFlags::OutParm)                      ret += "OutParm, ";
    if (flags & EPropFlags::ZeroConstructor)              ret += "ZeroConstructor, ";
    if (flags & EPropFlags::ReturnParm)                   ret += "ReturnParm, ";
    if (flags & EPropFlags::DisableEditOnTemplate)        ret += "DisableEditOnTemplate, ";
    if (flags & EPropFlags::Transient)                    ret += "Transient, ";
    if (flags & EPropFlags::Config)                       ret += "Config, ";
    if (flags & EPropFlags::DisableEditOnInstance)        ret += "DisableEditOnInstance, ";
    if (flags & EPropFlags::EditConst)                    ret += "EditConst, ";
    if (flags & EPropFlags::GlobalConfig)                 ret += "GlobalConfig, ";
    if (flags & EPropFlags::InstancedReference)           ret += "InstancedReference, ";
    if (flags & EPropFlags::DuplicateTransient)           ret += "DuplicateTransient, ";
    if (flags & EPropFlags::SubobjectReference)           ret += "SubobjectReference, ";
    if (flags & EPropFlags::SaveGame)                     ret += "SaveGame, ";
    if (flags & EPropFlags::NoClear)                      ret += "NoClear, ";
    if (flags & EPropFlags::ReferenceParm)                ret += "ReferenceParm, ";
    if (flags & EPropFlags::BlueprintAssignable)          ret += "BlueprintAssignable, ";
    if (flags & EPropFlags::Deprecated)                   ret += "Deprecated, ";
    if (flags & EPropFlags::IsPlainOldData)               ret += "IsPlainOldData, ";
    if (flags & EPropFlags::RepSkip)                      ret += "RepSkip, ";
    if (flags & EPropFlags::RepNotify)                    ret += "RepNotify, ";
    if (flags & EPropFlags::Interp)                       ret += "Interp, ";
    if (flags & EPropFlags::NonTransactional)             ret += "NonTransactional, ";
    if (flags & EPropFlags::EditorOnly)                   ret += "EditorOnly, ";
    if (flags & EPropFlags::NoDestructor)                 ret += "NoDestructor, ";
    if (flags & EPropFlags::AutoWeak)                     ret += "AutoWeak, ";
    if (flags & EPropFlags::ContainsInstancedReference)   ret += "ContainsInstancedReference, ";
    if (flags & EPropFlags::AssetRegistrySearchable)      ret += "AssetRegistrySearchable, ";
    if (flags & EPropFlags::SimpleDisplay)                ret += "SimpleDisplay, ";
    if (flags & EPropFlags::AdvancedDisplay)              ret += "AdvancedDisplay, ";
    if (flags & EPropFlags::Protected)                    ret += "Protected, ";
    if (flags & EPropFlags::BlueprintCallable)            ret += "BlueprintCallable, ";
    if (flags & EPropFlags::BlueprintAuthorityOnly)       ret += "BlueprintAuthorityOnly, ";
    if (flags & EPropFlags::TextExportTransient)          ret += "TextExportTransient, ";
    if (flags & EPropFlags::NonPIEDuplicateTransient)     ret += "NonPIEDuplicateTransient, ";
    if (flags & EPropFlags::ExposeOnSpawn)                ret += "ExposeOnSpawn, ";
    if (flags & EPropFlags::PersistentInstance)           ret += "PersistentInstance, ";
    if (flags & EPropFlags::UObjectWrapper)               ret += "UObjectWrapper, ";
    if (flags & EPropFlags::HasGetValueTypeHash)          ret += "HasGetValueTypeHash, ";
    if (flags & EPropFlags::NativeAccessSpecifierPublic)  ret += "NativeAccessSpecifierPublic, ";
    if (flags & EPropFlags::NativeAccessSpecifierProtected) ret += "NativeAccessSpecifierProtected, ";
    if (flags & EPropFlags::NativeAccessSpecifierPrivate) ret += "NativeAccessSpecifierPrivate, ";

    // 去掉末尾 ", "
    if (ret.size() > 2)
    {
        ret.erase(ret.size() - 2);
    }
    return ret;
}

} // namespace detail
} // namespace xrd
