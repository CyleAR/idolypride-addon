#pragma once
#include "../deps/UnityResolve/UnityResolve.hpp"
#include "Log.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

namespace Il2cppUtils {
    using namespace HoshimiLocal;

    struct Il2CppClassHead {
        const void* image;
        void* gc_desc;
        const char* name;
        const char* namespaze;
    };

    struct Il2CppObject
    {
        union
        {
            void* klass;
            void* vtable;
        };
        void* monitor;
    };

    enum Il2CppTypeEnum
    {
        IL2CPP_TYPE_END = 0x00,
        IL2CPP_TYPE_VOID = 0x01,
        IL2CPP_TYPE_BOOLEAN = 0x02,
        IL2CPP_TYPE_CHAR = 0x03,
        IL2CPP_TYPE_I1 = 0x04,
        IL2CPP_TYPE_U1 = 0x05,
        IL2CPP_TYPE_I2 = 0x06,
        IL2CPP_TYPE_U2 = 0x07,
        IL2CPP_TYPE_I4 = 0x08,
        IL2CPP_TYPE_U4 = 0x09,
        IL2CPP_TYPE_I8 = 0x0a,
        IL2CPP_TYPE_U8 = 0x0b,
        IL2CPP_TYPE_R4 = 0x0c,
        IL2CPP_TYPE_R8 = 0x0d,
        IL2CPP_TYPE_STRING = 0x0e,
        IL2CPP_TYPE_PTR = 0x0f,
        IL2CPP_TYPE_BYREF = 0x10,
        IL2CPP_TYPE_VALUETYPE = 0x11,
        IL2CPP_TYPE_CLASS = 0x12,
        IL2CPP_TYPE_VAR = 0x13,
        IL2CPP_TYPE_ARRAY = 0x14,
        IL2CPP_TYPE_GENERICINST = 0x15,
        IL2CPP_TYPE_TYPEDBYREF = 0x16,
        IL2CPP_TYPE_I = 0x18,
        IL2CPP_TYPE_U = 0x19,
        IL2CPP_TYPE_FNPTR = 0x1b,
        IL2CPP_TYPE_OBJECT = 0x1c,
        IL2CPP_TYPE_SZARRAY = 0x1d,
        IL2CPP_TYPE_MVAR = 0x1e,
        IL2CPP_TYPE_CMOD_REQD = 0x1f,
        IL2CPP_TYPE_CMOD_OPT = 0x20,
        IL2CPP_TYPE_INTERNAL = 0x21,
        IL2CPP_TYPE_MODIFIER = 0x40,
        IL2CPP_TYPE_SENTINEL = 0x41,
        IL2CPP_TYPE_PINNED = 0x45,
        IL2CPP_TYPE_ENUM = 0x55
    };

    typedef struct Il2CppType
    {
        void* dummy;
        unsigned int attrs : 16;
        Il2CppTypeEnum type : 8;
        unsigned int num_mods : 6;
        unsigned int byref : 1;
        unsigned int pinned : 1;
    } Il2CppType;

    struct Il2CppReflectionType
    {
        Il2CppObject object;
        const Il2CppType* type;
    };

    struct MethodInfo {
        uintptr_t methodPointer;
        uintptr_t invoker_method;
        const char* name;
        uintptr_t klass;
        const Il2CppType* return_type;
        const void* parameters;
        uintptr_t methodDefinition;
        uintptr_t genericContainer;
        uint32_t token;
        uint16_t flags;
        uint16_t iflags;
        uint16_t slot;
        uint8_t parameters_count;
        uint8_t is_generic : 1;
        uint8_t is_inflated : 1;
        uint8_t wrapper_type : 1;
        uint8_t is_marshaled_from_native : 1;
    };

    struct FieldInfo {
        const char* name;
        const Il2CppType* type;
        uintptr_t parent;
        int32_t offset;
        uint32_t token;
    };

    struct Resolution_t {
        int width;
        int height;
        int herz;
    };

    static UnityResolve::Class* GetClass(const std::string& assemblyName, const std::string& nameSpaceName,
                   const std::string& className) {
        const auto assembly = UnityResolve::Get(assemblyName);
        if (!assembly) {
            Log::ErrorFmt("GetMethodPointer error: assembly %s not found.", assemblyName.c_str());
            return nullptr;
        }
        const auto pClass = assembly->Get(className, nameSpaceName);
        if (!pClass) {
            Log::ErrorFmt("GetMethodPointer error: Class %s::%s not found.", nameSpaceName.c_str(), className.c_str());
            return nullptr;
        }
        return pClass;
    }

    static UnityResolve::Method* GetMethod(const std::string& assemblyName, const std::string& nameSpaceName,
                           const std::string& className, const std::string& methodName, const std::vector<std::string>& args = {}) {
        const auto assembly = UnityResolve::Get(assemblyName);
        if (!assembly) {
            Log::ErrorFmt("GetMethod error: assembly %s not found.", assemblyName.c_str());
            return nullptr;
        }
        const auto pClass = assembly->Get(className, nameSpaceName);
        if (!pClass) {
            Log::ErrorFmt("GetMethod error: Class %s::%s not found.", nameSpaceName.c_str(), className.c_str());
            return nullptr;
        }
        auto method = pClass->Get<UnityResolve::Method>(methodName, args);
        if (!method) {
            Log::ErrorFmt("GetMethod error: method %s::%s.%s not found.", nameSpaceName.c_str(), className.c_str(), methodName.c_str());
            return nullptr;
        }
        return method;
    }

    static void* GetMethodPointer(const std::string& assemblyName, const std::string& nameSpaceName,
                           const std::string& className, const std::string& methodName, const std::vector<std::string>& args = {}) {
        auto method = GetMethod(assemblyName, nameSpaceName, className, methodName, args);
        if (method) {
            return method->function;
        }
        return nullptr;
    }

    static void* il2cpp_resolve_icall(const char* s) {
        return UnityResolve::Invoke<void*>("il2cpp_resolve_icall", s);
    }

    static Il2CppClassHead* get_class_from_instance(const void* instance) {
        return static_cast<Il2CppClassHead*>(*static_cast<void* const*>(std::assume_aligned<alignof(void*)>(instance)));
    }

    static MethodInfo* il2cpp_class_get_method_from_name(void* klass, const char* name, int argsCount) {
        return UnityResolve::Invoke<MethodInfo*>("il2cpp_class_get_method_from_name", klass, name, argsCount);
    }

    static void* find_nested_class(void* klass, std::function<bool(void*)> predicate)
    {
        void* iter{};
        while (const auto curNestedClass = UnityResolve::Invoke<void*>("il2cpp_class_get_nested_types", klass, &iter))
        {
            if (predicate(curNestedClass))
            {
                return curNestedClass;
            }
        }
        return nullptr;
    }

    static void* find_nested_class_from_name(void* klass, const char* name)
    {
        return find_nested_class(klass, [name = std::string(name)](void* nestedClass) {
            return std::string(static_cast<Il2CppClassHead*>(nestedClass)->name) == name;
        });
    }

    template <typename RType>
    static auto ClassGetFieldValue(void* obj, UnityResolve::Field* field) -> RType {
        return *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(obj) + field->offset);
    }

    template <typename RType>
    static auto ClassGetFieldValue(void* obj, FieldInfo* field) -> RType {
        return *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(obj) + field->offset);
    }

    template <typename RType>
    static auto ClassSetFieldValue(void* obj, UnityResolve::Field* field, RType value) -> void {
        *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(obj) + field->offset) = value;
    }

    template <typename RType>
    static auto ClassSetFieldValue(void* obj, FieldInfo* field, RType value) -> void {
        *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(obj) + field->offset) = value;
    }

    static void* get_system_class_from_reflection_type_str(const char* typeStr, const char* assemblyName = "mscorlib") {
        using Il2cppString = UnityResolve::UnityType::String;
        static auto assemblyLoad = reinterpret_cast<void* (*)(Il2cppString*)>(
                GetMethodPointer("mscorlib.dll", "System.Reflection",
                                 "Assembly", "Load", {"*"})
        );
        static auto assemblyGetType = reinterpret_cast<Il2CppReflectionType * (*)(void*, Il2cppString*)>(
                GetMethodPointer("mscorlib.dll", "System.Reflection",
                                 "Assembly", "GetType", {"*"})
        );
        static auto reflectionAssembly = assemblyLoad(Il2cppString::New(assemblyName));
        auto reflectionType = assemblyGetType(reflectionAssembly, Il2cppString::New(typeStr));
        return UnityResolve::Invoke<void*>("il2cpp_class_from_system_type", reflectionType);
    }

    static std::unordered_map<std::string, std::unordered_map<int, std::string>> enumToValueMapCache{};
    static std::unordered_map<int, std::string> EnumToValueMap(Il2CppClassHead* enumClass, bool useCache) {
        std::unordered_map<int, std::string> ret{};
        auto isEnum = UnityResolve::Invoke<bool>("il2cpp_class_is_enum", enumClass);
        if (isEnum) {
            FieldInfo* field = nullptr;
            void* iter = nullptr;
            std::string cacheName = std::string(enumClass->namespaze) + "::" + enumClass->name;
            if (useCache) {
                if (auto it = enumToValueMapCache.find(cacheName); it != enumToValueMapCache.end()) {
                    return it->second;
                }
            }
            while ((field = UnityResolve::Invoke<FieldInfo*>("il2cpp_class_get_fields", enumClass, &iter))) {
                if (field->offset > 0) continue;
                if (strcmp(field->name, "value__") == 0) continue;
                int value;
                UnityResolve::Invoke<void>("il2cpp_field_static_get_value", field, &value);
                std::string itemName = std::string(enumClass->name) + "_" + field->name;
                ret.emplace(value, std::move(itemName));
            }
            if (useCache) {
                enumToValueMapCache.emplace(std::move(cacheName), ret);
            }
        }
        return ret;
    }

    namespace Tools {
        template <typename T = void*>
        class CSListEditor {
        public:
            CSListEditor(void* list) {
                list_klass = get_class_from_instance(list);
                lst = list;
                lst_get_Count_method = il2cpp_class_get_method_from_name(list_klass, "get_Count", 0);
                lst_get_Item_method = il2cpp_class_get_method_from_name(list_klass, "get_Item", 1);
                lst_set_Item_method = il2cpp_class_get_method_from_name(list_klass, "set_Item", 2);
                lst_Add_method = il2cpp_class_get_method_from_name(list_klass, "Add", 1);
                lst_get_Count = reinterpret_cast<int(*)(void*, void*)>(lst_get_Count_method->methodPointer);
                lst_get_Item = reinterpret_cast<T(*)(void*, int, void*)>(lst_get_Item_method->methodPointer);
                lst_set_Item = reinterpret_cast<void(*)(void*, int, T, void*)>(lst_set_Item_method->methodPointer);
                lst_Add = reinterpret_cast<void(*)(void*, T, void*)>(lst_Add_method->methodPointer);
            }
            void Add(T value) { lst_Add(lst, value, lst_Add_method); }
            T get_Item(int index) { return lst_get_Item(lst, index, lst_get_Item_method); }
            void set_Item(int index, T value) { lst_set_Item(lst, index, value, lst_set_Item_method); }
            int get_Count() { return lst_get_Count(lst, lst_get_Count_method); }
            void* lst;
            void* list_klass;
        private:
            MethodInfo* lst_get_Item_method;
            MethodInfo* lst_Add_method;
            MethodInfo* lst_get_Count_method;
            MethodInfo* lst_set_Item_method;
            int(*lst_get_Count)(void*, void*);
            T(*lst_get_Item)(void*, int, void*);
            void(*lst_set_Item)(void*, int, T, void*);
            void(*lst_Add)(void*, T, void*);
        };
    }
}
