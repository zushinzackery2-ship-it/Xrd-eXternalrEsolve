#pragma once
// Xrd-eXternalrEsolve - Chaos 结构反射发现
// 仅把 SDK / UStruct 可见成员转成动态发现；私有物理链路仍保留扫描或稳定布局。

#include "../core/context.hpp"
#include "../engine/objects/objects_search.hpp"
#include <initializer_list>

namespace xrd
{
namespace detail
{
    inline uptr FindScriptStructByNames(std::initializer_list<const char*> names)
    {
        uptr result = 0;
        ForEachObject([&](uptr obj, i32)
        {
            const std::string className = GetObjectClassName(obj);
            if (className != "ScriptStruct")
            {
                return true;
            }

            const std::string objectName = GetObjectName(obj);
            for (const char* candidate : names)
            {
                if (candidate && objectName == candidate)
                {
                    result = obj;
                    return false;
                }
            }
            return true;
        });
        return result;
    }

    inline void ReflectClassOffset(
        const char* className,
        const char* propertyName,
        i32& outOffset,
        const char* logName)
    {
        uptr cls = FindClassByName(className ? className : "");
        if (!cls)
        {
            return;
        }

        i32 offset = GetPropertyOffsetByName(cls, propertyName ? propertyName : "");
        if (offset < 0)
        {
            return;
        }

        outOffset = offset;
        std::cerr << "[xrd][Chaos] " << logName
                  << " = +0x" << std::hex << offset << std::dec
                  << " (reflect)\n";
    }

    inline void ReflectStructOffset(
        std::initializer_list<const char*> structNames,
        const char* propertyName,
        i32& outOffset,
        const char* logName)
    {
        uptr scriptStruct = FindScriptStructByNames(structNames);
        if (!scriptStruct)
        {
            return;
        }

        i32 offset = GetPropertyOffsetByName(scriptStruct, propertyName ? propertyName : "");
        if (offset < 0)
        {
            return;
        }

        outOffset = offset;
        std::cerr << "[xrd][Chaos] " << logName
                  << " = +0x" << std::hex << offset << std::dec
                  << " (reflect)\n";
    }

    inline void ReflectStructSize(
        std::initializer_list<const char*> structNames,
        i32& outSize,
        const char* logName)
    {
        if (Off().UStruct_Size < 0)
        {
            return;
        }

        uptr scriptStruct = FindScriptStructByNames(structNames);
        if (!scriptStruct)
        {
            return;
        }

        i32 size = 0;
        if (!GReadValue(scriptStruct + Off().UStruct_Size, size) || size <= 0)
        {
            return;
        }

        outSize = size;
        std::cerr << "[xrd][Chaos] " << logName
                  << " = 0x" << std::hex << size << std::dec
                  << " (reflect)\n";
    }

    inline void ApplyChaosSdkFallbacks(Context& ctx)
    {
        auto& co = ctx.chaosOff;
        const bool isDouble = ctx.off.bUseDoublePrecision;

        if (co.BodyInstance_BodySetup < 0) co.BodyInstance_BodySetup = 0x08;

        if (co.AggGeom_SphereElems < 0)  co.AggGeom_SphereElems = 0x00;
        if (co.AggGeom_BoxElems < 0)     co.AggGeom_BoxElems = 0x10;
        if (co.AggGeom_SphylElems < 0)   co.AggGeom_SphylElems = 0x20;
        if (co.AggGeom_ConvexElems < 0)  co.AggGeom_ConvexElems = 0x30;

        if (co.ShapeElem_BaseSize < 0)   co.ShapeElem_BaseSize = 0x30;

        if (co.SphereElem_Center < 0)    co.SphereElem_Center = 0x30;
        if (co.SphereElem_Radius < 0)    co.SphereElem_Radius = isDouble ? 0x48 : 0x3C;
        if (co.SphereElem_Size < 0)      co.SphereElem_Size = isDouble ? 0x50 : 0x40;

        if (co.BoxElem_Center < 0)       co.BoxElem_Center = 0x30;
        if (co.BoxElem_Rotation < 0)     co.BoxElem_Rotation = isDouble ? 0x48 : 0x3C;
        if (co.BoxElem_X < 0)            co.BoxElem_X = isDouble ? 0x60 : 0x48;
        if (co.BoxElem_Y < 0)            co.BoxElem_Y = isDouble ? 0x64 : 0x4C;
        if (co.BoxElem_Z < 0)            co.BoxElem_Z = isDouble ? 0x68 : 0x50;
        if (co.BoxElem_Size < 0)         co.BoxElem_Size = isDouble ? 0x70 : 0x58;

        if (co.SphylElem_Center < 0)     co.SphylElem_Center = 0x30;
        if (co.SphylElem_Rotation < 0)   co.SphylElem_Rotation = isDouble ? 0x48 : 0x3C;
        if (co.SphylElem_Radius < 0)     co.SphylElem_Radius = isDouble ? 0x60 : 0x48;
        if (co.SphylElem_Length < 0)     co.SphylElem_Length = isDouble ? 0x64 : 0x4C;
        if (co.SphylElem_Size < 0)       co.SphylElem_Size = isDouble ? 0x68 : 0x50;

        if (co.ConvexElem_VertexData < 0) co.ConvexElem_VertexData = 0x30;
        if (co.ConvexElem_IndexData < 0)  co.ConvexElem_IndexData = 0x40;
        if (co.ConvexElem_Size < 0)       co.ConvexElem_Size = isDouble ? 0x100 : 0xB0;
    }

    inline void DiscoverChaosSdkOffsets(Context& ctx)
    {
        auto& co = ctx.chaosOff;

        if (co.PrimComp_BodyInstance < 0)
        {
            ReflectClassOffset(
                "PrimitiveComponent",
                "BodyInstance",
                co.PrimComp_BodyInstance,
                "PrimitiveComponent::BodyInstance");
        }

        if (co.BodySetup_AggGeom < 0)
        {
            ReflectClassOffset(
                "BodySetup",
                "AggGeom",
                co.BodySetup_AggGeom,
                "BodySetup::AggGeom");
        }

        if (co.BodyInstance_StructSize < 0)
        {
            ReflectStructSize(
                { "BodyInstance" },
                co.BodyInstance_StructSize,
                "FBodyInstance size");
        }

        i32 reflectedBodySetupOffset = -1;
        ReflectStructOffset(
            { "BodyInstance" },
            "BodySetup",
            reflectedBodySetupOffset,
            "FBodyInstance::BodySetup");
        if (reflectedBodySetupOffset >= 0)
        {
            co.BodyInstance_BodySetup = reflectedBodySetupOffset;
        }

        ReflectStructOffset(
            { "KAggregateGeom" },
            "SphereElems",
            co.AggGeom_SphereElems,
            "FKAggregateGeom::SphereElems");
        ReflectStructOffset(
            { "KAggregateGeom" },
            "BoxElems",
            co.AggGeom_BoxElems,
            "FKAggregateGeom::BoxElems");
        ReflectStructOffset(
            { "KAggregateGeom" },
            "SphylElems",
            co.AggGeom_SphylElems,
            "FKAggregateGeom::SphylElems");
        ReflectStructOffset(
            { "KAggregateGeom" },
            "ConvexElems",
            co.AggGeom_ConvexElems,
            "FKAggregateGeom::ConvexElems");

        ReflectStructSize(
            { "KShapeElem" },
            co.ShapeElem_BaseSize,
            "FKShapeElem size");

        ReflectStructOffset(
            { "KSphereElem" },
            "Center",
            co.SphereElem_Center,
            "FKSphereElem::Center");
        ReflectStructOffset(
            { "KSphereElem" },
            "Radius",
            co.SphereElem_Radius,
            "FKSphereElem::Radius");
        ReflectStructSize(
            { "KSphereElem" },
            co.SphereElem_Size,
            "FKSphereElem size");

        ReflectStructOffset(
            { "KBoxElem" },
            "Center",
            co.BoxElem_Center,
            "FKBoxElem::Center");
        ReflectStructOffset(
            { "KBoxElem" },
            "Rotation",
            co.BoxElem_Rotation,
            "FKBoxElem::Rotation");
        ReflectStructOffset(
            { "KBoxElem" },
            "X",
            co.BoxElem_X,
            "FKBoxElem::X");
        ReflectStructOffset(
            { "KBoxElem" },
            "Y",
            co.BoxElem_Y,
            "FKBoxElem::Y");
        ReflectStructOffset(
            { "KBoxElem" },
            "Z",
            co.BoxElem_Z,
            "FKBoxElem::Z");
        ReflectStructSize(
            { "KBoxElem" },
            co.BoxElem_Size,
            "FKBoxElem size");

        ReflectStructOffset(
            { "KSphylElem" },
            "Center",
            co.SphylElem_Center,
            "FKSphylElem::Center");
        ReflectStructOffset(
            { "KSphylElem" },
            "Rotation",
            co.SphylElem_Rotation,
            "FKSphylElem::Rotation");
        ReflectStructOffset(
            { "KSphylElem" },
            "Radius",
            co.SphylElem_Radius,
            "FKSphylElem::Radius");
        ReflectStructOffset(
            { "KSphylElem" },
            "Length",
            co.SphylElem_Length,
            "FKSphylElem::Length");
        ReflectStructSize(
            { "KSphylElem" },
            co.SphylElem_Size,
            "FKSphylElem size");

        ReflectStructOffset(
            { "KConvexElem" },
            "VertexData",
            co.ConvexElem_VertexData,
            "FKConvexElem::VertexData");
        ReflectStructOffset(
            { "KConvexElem" },
            "IndexData",
            co.ConvexElem_IndexData,
            "FKConvexElem::IndexData");
        ReflectStructSize(
            { "KConvexElem" },
            co.ConvexElem_Size,
            "FKConvexElem size");

        ApplyChaosSdkFallbacks(ctx);
    }
} // namespace detail
} // namespace xrd
