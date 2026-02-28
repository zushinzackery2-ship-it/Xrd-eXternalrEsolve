#pragma once
// Xrd-eXternalrEsolve - 预定义函数实现体生成
// 对标 Rei-Dumper CoreUObject_functions.cpp 中的预定义函数
// UObject: FindObjectFastImpl, FindObjectImpl, GetFullName, GetName,
//          HasTypeFlag, IsA(x3), IsDefaultObject
// UStruct: IsSubclassOf(x2)
// UClass:  GetFunction

#include <string>

namespace xrd
{
namespace detail
{

// 生成 UObject 的预定义函数实现
inline std::string GenerateUObjectPredefinedFuncImpls()
{
    std::string s;

    // FindObjectFastImpl
    s += "\n// Predefined Function\n";
    s += "// Finds a UObject in the global object array by name, "
         "optionally with ECastFlags to reduce heavy string "
         "comparison\n\n";
    s += "class UObject* UObject::FindObjectFastImpl("
         "const std::string& Name, "
         "EClassCastFlags RequiredType)\n";
    s += "{\n";
    s += "\tfor (int i = 0; i < GObjects->Num(); ++i)\n";
    s += "\t{\n";
    s += "\t\tUObject* Object = GObjects->GetByIndex(i);\n";
    s += "\t\n";
    s += "\t\tif (!Object)\n";
    s += "\t\t\tcontinue;\n";
    s += "\t\t\n";
    s += "\t\tif (Object->HasTypeFlag(RequiredType) "
         "&& Object->GetName() == Name)\n";
    s += "\t\t\treturn Object;\n";
    s += "\t}\n";
    s += "\n";
    s += "\treturn nullptr;\n";
    s += "}\n";

    // FindObjectImpl
    s += "\n\n// Predefined Function\n";
    s += "// Finds a UObject in the global object array by "
         "full-name, optionally with ECastFlags to reduce heavy "
         "string comparison\n\n";
    s += "class UObject* UObject::FindObjectImpl("
         "const std::string& FullName, "
         "EClassCastFlags RequiredType)\n";
    s += "{\n";
    s += "\tfor (int i = 0; i < GObjects->Num(); ++i)\n";
    s += "\t{\n";
    s += "\t\tUObject* Object = GObjects->GetByIndex(i);\n";
    s += "\t\n";
    s += "\t\tif (!Object)\n";
    s += "\t\t\tcontinue;\n";
    s += "\t\t\n";
    s += "\t\tif (Object->HasTypeFlag(RequiredType) "
         "&& Object->GetFullName() == FullName)\n";
    s += "\t\t\treturn Object;\n";
    s += "\t}\n";
    s += "\n";
    s += "\treturn nullptr;\n";
    s += "}\n";

    // GetFullName
    s += "\n\n// Predefined Function\n";
    s += "// Returns the name of this object in the format "
         "'Class Package.Outer.Object'\n\n";
    s += "std::string UObject::GetFullName() const\n";
    s += "{\n";
    s += "\tif (this && Class)\n";
    s += "\t{\n";
    s += "\t\tstd::string Temp;\n";
    s += "\n";
    s += "\t\tfor (UObject* NextOuter = Outer; "
         "NextOuter; NextOuter = NextOuter->Outer)\n";
    s += "\t\t{\n";
    s += "\t\t\tTemp = NextOuter->GetName() + \".\" + Temp;\n";
    s += "\t\t}\n";
    s += "\n";
    s += "\t\tstd::string Name = Class->GetName();\n";
    s += "\t\tName += \" \";\n";
    s += "\t\tName += Temp;\n";
    s += "\t\tName += GetName();\n";
    s += "\n";
    s += "\t\treturn Name;\n";
    s += "\t}\n";
    s += "\n";
    s += "\treturn \"None\";\n";
    s += "}\n";

    // GetName
    s += "\n\n// Predefined Function\n";
    s += "// Retuns the name of this object\n\n";
    s += "std::string UObject::GetName() const\n";
    s += "{\n";
    s += "\treturn this ? Name.ToString() : \"None\";\n";
    s += "}\n";

    // HasTypeFlag
    s += "\n\n// Predefined Function\n";
    s += "// Checks Class->FunctionFlags for TypeFlags\n\n";
    s += "bool UObject::HasTypeFlag("
         "EClassCastFlags TypeFlags) const\n";
    s += "{\n";
    s += "\treturn (Class->CastFlags & TypeFlags);\n";
    s += "}\n";

    // IsA (EClassCastFlags)
    s += "\n\n// Predefined Function\n";
    s += "// Checks a UObjects' type by TypeFlags\n\n";
    s += "bool UObject::IsA("
         "EClassCastFlags TypeFlags) const\n";
    s += "{\n";
    s += "\treturn (Class->CastFlags & TypeFlags);\n";
    s += "}\n";

    // IsA (FName)
    s += "\n\n// Predefined Function\n";
    s += "// Checks a UObjects' type by Class name\n\n";
    s += "bool UObject::IsA("
         "const class FName& ClassName) const\n";
    s += "{\n";
    s += "\treturn Class->IsSubclassOf(ClassName);\n";
    s += "}\n";

    // IsA (UClass*)
    s += "\n\n// Predefined Function\n";
    s += "// Checks a UObjects' type by Class\n\n";
    s += "bool UObject::IsA("
         "const class UClass* TypeClass) const\n";
    s += "{\n";
    s += "\treturn Class->IsSubclassOf(TypeClass);\n";
    s += "}\n";

    // IsDefaultObject
    s += "\n\n// Predefined Function\n";
    s += "// Checks whether this object is a classes' "
         "default-object\n\n";
    s += "bool UObject::IsDefaultObject() const\n";
    s += "{\n";
    s += "\treturn (Flags & EObjectFlags::ClassDefaultObject);\n";
    s += "}\n";

    return s;
}

// 生成 UStruct 的预定义函数实现
inline std::string GenerateUStructPredefinedFuncImpls()
{
    std::string s;

    // IsSubclassOf (const UStruct*)
    s += "\n// Predefined Function\n";
    s += "// Checks if this class has a certain base\n\n";
    s += "bool UStruct::IsSubclassOf("
         "const UStruct* Base) const\n";
    s += "{\n";
    s += "\tif (!Base)\n";
    s += "\t\treturn false;\n";
    s += "\n";
    s += "\tfor (const UStruct* Struct = this; "
         "Struct; Struct = Struct->Super)\n";
    s += "\t{\n";
    s += "\t\tif (Struct == Base)\n";
    s += "\t\t\treturn true;\n";
    s += "\t}\n";
    s += "\n";
    s += "\treturn false;\n";
    s += "}\n";

    // IsSubclassOf (const FName&)
    s += "\n\n// Predefined Function\n";
    s += "// Checks if this class has a certain base\n\n";
    s += "bool UStruct::IsSubclassOf("
         "const FName& baseClassName) const\n";
    s += "{\n";
    s += "\tif (baseClassName.IsNone())\n";
    s += "\t\treturn false;\n";
    s += "\n";
    s += "\tfor (const UStruct* Struct = this; "
         "Struct; Struct = Struct->Super)\n";
    s += "\t{\n";
    s += "\t\tif (Struct->Name == baseClassName)\n";
    s += "\t\t\treturn true;\n";
    s += "\t}\n";
    s += "\n";
    s += "\treturn false;\n";
    s += "}\n";

    return s;
}

// 生成 UClass 的预定义函数实现
inline std::string GenerateUClassPredefinedFuncImpls()
{
    std::string s;

    s += "\n// Predefined Function\n";
    s += "// Gets a UFunction from this UClasses' "
         "'Children' list\n\n";
    s += "class UFunction* UClass::GetFunction("
         "const char* ClassName, "
         "const char* FuncName) const\n";
    s += "{\n";
    s += "\tfor(const UStruct* Clss = this; "
         "Clss; Clss = Clss->Super)\n";
    s += "\t{\n";
    s += "\t\tif (Clss->GetName() != ClassName)\n";
    s += "\t\t\tcontinue;\n";
    s += "\t\t\t\n";
    s += "\t\tfor (UField* Field = Clss->Children; "
         "Field; Field = Field->Next)\n";
    s += "\t\t{\n";
    s += "\t\t\tif(Field->HasTypeFlag("
         "EClassCastFlags::Function) "
         "&& Field->GetName() == FuncName)\n";
    s += "\t\t\t\treturn static_cast"
         "<class UFunction*>(Field);\n";
    s += "\t\t}\n";
    s += "\t}\n";
    s += "\n";
    s += "\treturn nullptr;\n";
    s += "}\n";

    return s;
}

// 获取指定类名的预定义函数实现
// 返回空字符串表示该类没有预定义函数实现
inline std::string GetPredefinedFuncImpls(
    const std::string& className)
{
    if (className == "Object") return GenerateUObjectPredefinedFuncImpls();
    if (className == "Struct") return GenerateUStructPredefinedFuncImpls();
    if (className == "Class")  return GenerateUClassPredefinedFuncImpls();
    return "";
}

} // namespace detail
} // namespace xrd
