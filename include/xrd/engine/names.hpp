#pragma once
// Xrd-eXternalrEsolve - FName 解析
// 通过 GNames (FNamePool 或 TNameEntryArray) 将 FName 索引解析为字符串

#include "../core/context.hpp"
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace xrd
{

namespace detail
{
    inline std::unordered_map<i32, std::string>& NameCache()
    {
        static std::unordered_map<i32, std::string> cache;
        return cache;
    }

    inline std::shared_mutex& NameCacheMutex()
    {
        static std::shared_mutex mtx;
        return mtx;
    }
} // namespace detail

// 通过 FNamePool 解析名称（UE4.23+ 的新格式）
inline bool ResolveName_NamePool(
    const IMemoryAccessor& mem,
    uptr gnames,
    i32 compIdx,
    std::string& out)
{
    if (compIdx <= 0)
    {
        out.clear();
        return false;
    }

    // FNameEntryId 编码: blockIdx = compIdx >> blockBits, offset = (compIdx & mask) * stride
    i32 blockBits = Ctx().off.FNamePoolBlockBits;
    if (blockBits <= 0)
    {
        blockBits = 16;
    }

    i32 stride = Ctx().off.FNameEntryStride;
    if (stride <= 0)
    {
        stride = 2;
    }

    i32 blockIdx = compIdx >> blockBits;
    i32 entryOffset = (compIdx & ((1 << blockBits) - 1)) * stride;

    // 读取 block 指针
    uptr blockPtr = 0;
    if (!ReadPtr(mem, gnames + 0x10 + blockIdx * sizeof(uptr), blockPtr))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(blockPtr))
    {
        return false;
    }

    uptr entryAddr = blockPtr + entryOffset;

    // 读取 entry 头部：bit[0]=宽字符标志, bit[6..15]=长度
    u16 header = 0;
    if (!ReadValue(mem, entryAddr, header))
    {
        return false;
    }

    u16 len = header >> 6;
    bool isWide = (header & 1) != 0;

    if (len == 0 || len > 1024)
    {
        return false;
    }

    if (isWide)
    {
        // UTF-16 宽字符串，转换为 UTF-8
        std::vector<wchar_t> wbuf(len + 1);
        if (!mem.Read(entryAddr + 2, wbuf.data(), len * 2))
        {
            return false;
        }
        wbuf[len] = L'\0';
        // WideCharToMultiByte 转 UTF-8
        int needed = WideCharToMultiByte(
            CP_UTF8, 0, wbuf.data(), len, nullptr, 0, nullptr, nullptr);
        if (needed > 0)
        {
            out.resize(needed);
            WideCharToMultiByte(
                CP_UTF8, 0, wbuf.data(), len,
                out.data(), needed, nullptr, nullptr);
        }
        else
        {
            // 回退：有损 ASCII 转换
            out.resize(len);
            for (u16 i = 0; i < len; ++i)
            {
                out[i] = (wbuf[i] < 128)
                    ? static_cast<char>(wbuf[i])
                    : '?';
            }
        }
    }
    else
    {
        out.resize(len);
        if (!mem.Read(entryAddr + 2, out.data(), len))
        {
            return false;
        }
    }

    return true;
}

// 通过 TNameEntryArray 解析名称（旧版 UE4）
inline bool ResolveName_Array(
    const IMemoryAccessor& mem,
    uptr gnames,
    i32 compIdx,
    std::string& out)
{
    if (compIdx <= 0)
    {
        out.clear();
        return false;
    }

    constexpr i32 kChunkSize = 16384;
    i32 chunkIdx = compIdx / kChunkSize;
    i32 withinIdx = compIdx % kChunkSize;

    uptr chunksPtr = 0;
    if (!ReadPtr(mem, gnames, chunksPtr))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(chunksPtr))
    {
        return false;
    }

    uptr chunk = 0;
    if (!ReadPtr(mem, chunksPtr + chunkIdx * sizeof(uptr), chunk))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(chunk))
    {
        return false;
    }

    uptr entry = 0;
    if (!ReadPtr(mem, chunk + withinIdx * sizeof(uptr), entry))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(entry))
    {
        return false;
    }

    // FNameEntry 的字符串偏移因版本而异，逐个尝试
    for (i32 strOff : {0x10, 0x0C, 0x08})
    {
        std::string candidate;
        if (ReadCString(mem, entry + strOff, candidate, 256) && !candidate.empty())
        {
            bool valid = true;
            for (char c : candidate)
            {
                if (c < 0x20 || c > 0x7E)
                {
                    valid = false;
                    break;
                }
            }
            if (valid)
            {
                out = std::move(candidate);
                return true;
            }
        }
    }

    return false;
}

// 主解析函数：ComparisonIndex -> 字符串，带缓存
inline std::string GetNameFromFName(i32 compIdx, i32 number = 0)
{
    if (!IsInited())
    {
        return "";
    }

    {
        std::shared_lock<std::shared_mutex> rlock(detail::NameCacheMutex());
        auto& cache = detail::NameCache();
        auto it = cache.find(compIdx);
        if (it != cache.end())
        {
            std::string result = it->second;
            if (number > 0)
            {
                result += "_" + std::to_string(number - 1);
            }
            return result;
        }
    }

    std::string name;
    bool ok = false;

    if (Off().bUseNamePool)
    {
        ok = ResolveName_NamePool(Mem(), Off().GNames, compIdx, name);
    }
    else
    {
        ok = ResolveName_Array(Mem(), Off().GNames, compIdx, name);
    }

    if (ok && !name.empty())
    {
        std::unique_lock<std::shared_mutex> wlock(detail::NameCacheMutex());
        detail::NameCache()[compIdx] = name;
        std::string result = name;
        if (number > 0)
        {
            result += "_" + std::to_string(number - 1);
        }
        return result;
    }

    return "";
}

// 从指定地址读取 FName 并解析为字符串
inline std::string ReadFNameAt(uptr address)
{
    if (!IsInited() || !address)
    {
        return "";
    }

    FName fname{};
    if (!GReadValue(address, fname))
    {
        return "";
    }
    return GetNameFromFName(fname.ComparisonIndex, fname.Number);
}

} // namespace xrd
