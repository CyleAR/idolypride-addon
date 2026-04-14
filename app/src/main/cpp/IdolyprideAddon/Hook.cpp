#include <android/log.h>
#include "Hook.h"
#include "Plugin.h"
#include "Log.h"
#include "../deps/UnityResolve/UnityResolve.hpp"
#include "Il2cppUtils.hpp"
#include "Local.h"
#include "MasterLocal.h"
#include <unordered_set>
#include "camera/camera.hpp"
#include "config/Config.hpp"
#include "shadowhook.h"
#include <jni.h>
#include <thread>
#include <map>
#include <algorithm>
#include <cctype>


std::unordered_set<void*> hookedStubs{};

#define DEFINE_HOOK(returnType, name, params)                                                      \
	using name##_Type = returnType(*) params;                                                      \
	name##_Type name##_Addr = nullptr;                                                             \
	name##_Type name##_Orig = nullptr;                                                             \
	returnType name##_Hook params


#define ADD_HOOK(name, addr)                                                                       \
	name##_Addr = reinterpret_cast<name##_Type>(addr);                                             \
	if (addr) {                                                                                    \
    	auto stub = hookInstaller->InstallHook(reinterpret_cast<void*>(addr),                      \
                                               reinterpret_cast<void*>(name##_Hook),               \
                                               reinterpret_cast<void**>(&name##_Orig));            \
        if (stub == NULL) {                                                                        \
            int error_num = shadowhook_get_errno();                                                \
            const char *error_msg = shadowhook_to_errmsg(error_num);                               \
            Log::ErrorFmt("ADD_HOOK: %s at %p failed: %s", #name, addr, error_msg);                \
        }                                                                                          \
        else {                                                                                     \
            hookedStubs.emplace(stub);                                                             \
            IdolyprideLocal::Log::InfoFmt("ADD_HOOK: %s at %p", #name, addr);                         \
        }                                                                                          \
    }                                                                                              \
    else IdolyprideLocal::Log::ErrorFmt("Hook failed: %s is NULL", #name, addr)

void UnHookAll() {
    for (const auto i: hookedStubs) {
        int result = shadowhook_unhook(i);
        if(result != 0)
        {
            int error_num = shadowhook_get_errno();
            const char *error_msg = shadowhook_to_errmsg(error_num);
            IdolyprideLocal::Log::ErrorFmt("unhook failed: %d - %s", error_num, error_msg);
        }
    }
}

namespace IdolyprideLocal::HookMain {
    using Il2cppString = UnityResolve::UnityType::String;

    UnityResolve::UnityType::String* environment_get_stacktrace() {
        /*
        static auto mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System",
                                                 "Environment", "get_StackTrace");
        return mtd->Invoke<UnityResolve::UnityType::String*>();*/
        const auto pClass = Il2cppUtils::GetClass("mscorlib.dll", "System.Diagnostics",
                                                  "StackTrace");

        const auto ctor_mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System.Diagnostics",
                                                     "StackTrace", ".ctor");
        const auto toString_mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System.Diagnostics",
                                                         "StackTrace", "ToString");

        const auto klassInstance = pClass->New<void*>();
        ctor_mtd->Invoke<void>(klassInstance);
        return toString_mtd->Invoke<Il2cppString*>(klassInstance);
    }

    DEFINE_HOOK(void, Internal_LogException, (void* ex, void* obj)) {
        Internal_LogException_Orig(ex, obj);
        static auto Exception_ToString = Il2cppUtils::GetMethod("mscorlib.dll", "System", "Exception", "ToString");
        Log::LogUnityLog(ANDROID_LOG_ERROR, "UnityLog - Internal_LogException:\n%s", Exception_ToString->Invoke<Il2cppString*>(ex)->ToString().c_str());
    }

    DEFINE_HOOK(void, Internal_Log, (int logType, int logOption, UnityResolve::UnityType::String* content, void* context)) {
        Internal_Log_Orig(logType, logOption, content, context);
        // 2022.3.21f1
        Log::LogUnityLog(ANDROID_LOG_VERBOSE, "Internal_Log:\n%s", content->ToString().c_str());
    }

    bool IsNativeObjectAlive(void* obj) {
        static UnityResolve::Method* IsNativeObjectAliveMtd = nullptr;
        if (!IsNativeObjectAliveMtd) IsNativeObjectAliveMtd = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                                     "Object", "IsNativeObjectAlive");
        return IsNativeObjectAliveMtd->Invoke<bool>(obj);
    }

    UnityResolve::UnityType::Camera* mainCameraCache = nullptr;
    UnityResolve::UnityType::Transform* cameraTransformCache = nullptr;
    void CheckAndUpdateMainCamera() {
        if (!Config::enableFreeCamera) return;
        if (IsNativeObjectAlive(mainCameraCache)) return;

        mainCameraCache = UnityResolve::UnityType::Camera::GetMain();
        cameraTransformCache = mainCameraCache->GetTransform();
    }

    Il2cppUtils::Resolution_t GetResolution() {
        static auto GetResolution = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                           "Screen", "get_currentResolution");
        return GetResolution->Invoke<Il2cppUtils::Resolution_t>();
    }

    DEFINE_HOOK(void, Unity_set_fieldOfView, (UnityResolve::UnityType::Camera* self, float value)) {
        if (Config::enableFreeCamera) {
            if (self == mainCameraCache) {
                value = IPCamera::baseCamera.fov;
            }
        }
        Unity_set_fieldOfView_Orig(self, value);
    }

    DEFINE_HOOK(float, Unity_get_fieldOfView, (UnityResolve::UnityType::Camera* self)) {
        if (Config::enableFreeCamera) {
            if (self == mainCameraCache) {
                static auto get_orthographic = reinterpret_cast<bool (*)(void*)>(Il2cppUtils::il2cpp_resolve_icall(
                        "UnityEngine.Camera::get_orthographic()"
                ));
                static auto set_orthographic = reinterpret_cast<bool (*)(void*, bool)>(Il2cppUtils::il2cpp_resolve_icall(
                        "UnityEngine.Camera::set_orthographic(System.Boolean)"
                ));

                for (const auto& i : UnityResolve::UnityType::Camera::GetAllCamera()) {
                    // Log::DebugFmt("get_orthographic: %d", get_orthographic(i));
                    // set_orthographic(i, false);
                    Unity_set_fieldOfView_Orig(i, IPCamera::baseCamera.fov);
                }
                Unity_set_fieldOfView_Orig(self, IPCamera::baseCamera.fov);

                // Log::DebugFmt("main - get_orthographic: %d", get_orthographic(self));
                return IPCamera::baseCamera.fov;
            }
        }
        return Unity_get_fieldOfView_Orig(self);
    }

    UnityResolve::UnityType::Transform* cacheTrans = nullptr;
    UnityResolve::UnityType::Quaternion cacheRotation{};
    UnityResolve::UnityType::Vector3 cachePosition{};
    UnityResolve::UnityType::Vector3 cacheForward{};
    UnityResolve::UnityType::Vector3 cacheLookAt{};

    DEFINE_HOOK(void, Unity_set_rotation_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Quaternion* value)) {
        if (Config::enableFreeCamera) {
            static auto lookat_injected = reinterpret_cast<void (*)(void*self,
                                                                    UnityResolve::UnityType::Vector3* worldPosition, UnityResolve::UnityType::Vector3* worldUp)>(
                    Il2cppUtils::il2cpp_resolve_icall(
                            "UnityEngine.Transform::Internal_LookAt_Injected(UnityEngine.Vector3&,UnityEngine.Vector3&)"));
            static auto worldUp = UnityResolve::UnityType::Vector3(0, 1, 0);

            if (cameraTransformCache == self) {
                const auto cameraMode = IPCamera::GetCameraMode();
                if (cameraMode == IPCamera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && IsNativeObjectAlive(cacheTrans)) {
                        if (IPCamera::GetFirstPersonRoll() == IPCamera::FirstPersonRoll::ENABLE_ROLL) {
                            *value = cacheRotation;
                        }
                        else {
                            static IdolyprideLocal::Misc::FixedSizeQueue<float> recordsY(60);
                            const auto newY = IPCamera::CheckNewY(cacheLookAt, true, recordsY);
                            UnityResolve::UnityType::Vector3 newCacheLookAt{cacheLookAt.x, newY, cacheLookAt.z};
                            lookat_injected(self, &newCacheLookAt, &worldUp);
                            return;
                        }
                    }
                }
                else if (cameraMode == IPCamera::CameraMode::FOLLOW) {
                    auto newLookAtPos = IPCamera::CalcFollowModeLookAt(cachePosition,
                                                                       IPCamera::followPosOffset, true);
                    lookat_injected(self, &newLookAtPos, &worldUp);
                    return;
                }
                else {
                    auto& origCameraLookat = IPCamera::baseCamera.lookAt;
                    lookat_injected(self, &origCameraLookat, &worldUp);
                    // Log::DebugFmt("fov: %f, target: %f", Unity_get_fieldOfView_Orig(mainCameraCache), IPCamera::baseCamera.fov);
                    return;
                }
            }
        }
        return Unity_set_rotation_Injected_Orig(self, value);
    }

    DEFINE_HOOK(void, Unity_set_position_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Vector3* data)) {
        if (Config::enableFreeCamera) {
            CheckAndUpdateMainCamera();

            if (cameraTransformCache == self) {
                const auto cameraMode = IPCamera::GetCameraMode();
                if (cameraMode == IPCamera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && IsNativeObjectAlive(cacheTrans)) {
                        *data = IPCamera::CalcFirstPersonPosition(cachePosition, cacheForward, IPCamera::firstPersonPosOffset);
                    }

                }
                else if (cameraMode == IPCamera::CameraMode::FOLLOW) {
                    auto newLookAtPos = IPCamera::CalcFollowModeLookAt(cachePosition, IPCamera::followPosOffset);
                    auto pos = IPCamera::CalcPositionFromLookAt(newLookAtPos, IPCamera::followPosOffset);
                    data->x = pos.x;
                    data->y = pos.y;
                    data->z = pos.z;
                }
                else {
                    //Log::DebugFmt("MainCamera set pos: %f, %f, %f", data->x, data->y, data->z);
                    auto& origCameraPos = IPCamera::baseCamera.pos;
                    data->x = origCameraPos.x;
                    data->y = origCameraPos.y;
                    data->z = origCameraPos.z;
                }
            }
        }

        return Unity_set_position_Injected_Orig(self, data);
    }

    DEFINE_HOOK(void*, InternalSetOrientationAsync, (void* self, int type, void* c, void* tc, void* mtd)) {
        switch (Config::gameOrientation) {
            case 1: type = 0x2; break;  // FixedPortrait
            case 2: type = 0x3; break;  // FixedLandscape
            default: break;
        }
        return InternalSetOrientationAsync_Orig(self, type, c, tc, mtd);
    }

    DEFINE_HOOK(void, EndCameraRendering, (void* ctx, void* camera, void* method)) {
        EndCameraRendering_Orig(ctx, camera, method);

        if (Config::enableFreeCamera && mainCameraCache) {
            Unity_set_fieldOfView_Orig(mainCameraCache, IPCamera::baseCamera.fov);
            if (IPCamera::GetCameraMode() == IPCamera::CameraMode::FIRST_PERSON) {
                mainCameraCache->SetNearClipPlane(0.001f);
            }
        }
    }

    DEFINE_HOOK(void, Unity_set_targetFrameRate, (int value)) {
        const auto configFps = Config::targetFrameRate;
        return Unity_set_targetFrameRate_Orig(configFps == 0 ? value: configFps);
    }

    std::unordered_map<void*, std::string> loadHistory{};

    DEFINE_HOOK(void*, AssetBundle_LoadAssetAsync, (void* self, Il2cppString* name, void* type)) {
        auto ret = AssetBundle_LoadAssetAsync_Orig(self, name, type);
        loadHistory.emplace(ret, name->ToString());
        return ret;
    }

    DEFINE_HOOK(void*, AssetBundleRequest_GetResult, (void* self)) {
        auto result = AssetBundleRequest_GetResult_Orig(self);
        if (const auto iter = loadHistory.find(self); iter != loadHistory.end()) {
            const auto name = iter->second;
            loadHistory.erase(iter);
        }
        return result;
    }

    DEFINE_HOOK(void*, Resources_Load, (Il2cppString* path, void* systemTypeInstance)) {
        auto ret = Resources_Load_Orig(path, systemTypeInstance);
        return ret;
    }

    DEFINE_HOOK(UnityResolve::UnityType::String*, QualiArts_I18n_GetOrDefault, (UnityResolve::UnityType::String* key, UnityResolve::UnityType::String* defaultKey, void* method)) {
        auto ret = QualiArts_I18n_GetOrDefault_Orig(key, defaultKey, method);

        if (key != nullptr) {
            std::string keyStr = key->ToString();
            std::string translated;
            if (Local::GetI18n(keyStr, &translated)) {
                return UnityResolve::UnityType::String::New(translated);
            } else {
                Local::DumpI18nItem(keyStr, ret != nullptr ? ret->ToString() : "");
            }
        }

        return ret;
    }

    void* fontCache = nullptr;
    bool hasTriedInitFont = false;
    void* GetReplaceFont() {
        static bool hasTriedLoad = false;
        static void* replaceFont = nullptr;
        if (hasTriedLoad) {
            if (replaceFont && IsNativeObjectAlive(replaceFont)) return replaceFont;
            return nullptr;
        }
        hasTriedLoad = true;

        static auto bundlePath = Local::GetBasePath() / "local-files" / "gakumasassets";
        if (!std::filesystem::exists(bundlePath)) {
            std::ofstream debugLog(Local::GetBasePath() / "font_err.txt", std::ios::app);
            debugLog << "gakumasassets not found\n";
            return nullptr;
        }

        static auto AssetBundleClass = Il2cppUtils::GetClass("UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle");
        if (!AssetBundleClass) {
             AssetBundleClass = Il2cppUtils::GetClass("UnityEngine.dll", "UnityEngine", "AssetBundle");
        }

        if (AssetBundleClass) {
            std::ofstream debugLog(Local::GetBasePath() / "font_err.txt", std::ios::app);
            debugLog << "AssetBundle Methods:\n";
            void* iter = nullptr;
            while (auto method = UnityResolve::Invoke<Il2cppUtils::MethodInfo*>("il2cpp_class_get_methods", AssetBundleClass->address, &iter)) {
                debugLog << " - " << method->name << " (args: " << (int)method->parameters_count << ")\n";
            }
        }

        static auto AssetBundle_LoadFromFile_Internal = reinterpret_cast<void* (*)(UnityResolve::UnityType::String*, uint32_t, uint64_t)>(
            Il2cppUtils::il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadFromFile_Internal(System.String,System.UInt32,System.UInt64)")
        );

        if (!AssetBundle_LoadFromFile_Internal) {
            AssetBundle_LoadFromFile_Internal = reinterpret_cast<void* (*)(UnityResolve::UnityType::String*, uint32_t, uint64_t)>(
                Il2cppUtils::il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadFromFile_Internal")
            );
        }

        if (!AssetBundle_LoadFromFile_Internal) {
            std::ofstream debugLog(Local::GetBasePath() / "font_err.txt", std::ios::app);
            debugLog << "LoadFromFile_Internal icall not found\n";
            return nullptr;
        }

        auto bundle = AssetBundle_LoadFromFile_Internal(UnityResolve::UnityType::String::New(bundlePath.string().c_str()), 0, 0);
        if (!bundle) {
            std::ofstream debugLog(Local::GetBasePath() / "font_err.txt", std::ios::app);
            debugLog << "Failed to load bundle via LoadFromFile_Internal\n";
            return nullptr;
        }

        static auto FontClass = Il2cppUtils::GetClass("UnityEngine.TextRenderingModule.dll", "UnityEngine", "Font");
        static auto Font_Type = UnityResolve::Invoke<Il2cppUtils::Il2CppReflectionType*>("il2cpp_type_get_object", 
            UnityResolve::Invoke<void*>("il2cpp_class_get_type", FontClass->address));

        static auto AssetBundle_GetAllAssetNames = reinterpret_cast<UnityResolve::UnityType::Array<UnityResolve::UnityType::String*>* (*)(void*)>(
            Il2cppUtils::il2cpp_resolve_icall("UnityEngine.AssetBundle::GetAllAssetNames()")
        );

        if (AssetBundle_GetAllAssetNames) {
            auto names = AssetBundle_GetAllAssetNames(bundle);
            if (names) {
                std::ofstream debugLog(Local::GetBasePath() / "font_err.txt", std::ios::app);
                debugLog << "Bundle Asset Names:\n";
                for (int i = 0; i < names->max_length; i++) {
                    debugLog << " - " << names->At(i)->ToString() << "\n";
                }
            }
        }

        static auto AssetBundle_LoadAsset = reinterpret_cast<void* (*)(void* _this, UnityResolve::UnityType::String* name, Il2cppUtils::Il2CppReflectionType* type)>(
            Il2cppUtils::il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadAsset_Internal(System.String,System.Type)")
        );

        if (!AssetBundle_LoadAsset) {
            std::ofstream debugLog(Local::GetBasePath() / "font_err.txt", std::ios::app);
            debugLog << "LoadAsset_Internal icall not found\n";
            return nullptr;
        }

        auto fontPath = UnityResolve::UnityType::String::New("assets/fonts/gkamszhfontmix.otf");
        replaceFont = AssetBundle_LoadAsset(bundle, fontPath, Font_Type);
        
        if (!replaceFont) {
            std::ofstream debugLog(Local::GetBasePath() / "font_err.txt", std::ios::app);
            debugLog << "replaceFont is null after LoadAsset\n";
        }
        
        return replaceFont;
    }

    std::unordered_set<void*> updatedFontPtrs{};
    void UpdateFont(void* TMP_Textself) {
        if (!Config::replaceFont) return;
        static auto get_font = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll",
                                                      "TMPro", "TMP_Text", "get_font");
        static auto set_font = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll",
                                                      "TMPro", "TMP_Text", "set_font");
        static auto get_name = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll",
                                                      "UnityEngine", "Object", "get_name");

        static auto set_sourceFontFile = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll", "TMPro",
                                                                "TMP_FontAsset", "set_sourceFontFile");
        static auto UpdateFontAssetData = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll", "TMPro",
                                                                 "TMP_FontAsset", "UpdateFontAssetData");

        auto fontAsset = get_font->Invoke<void*>(TMP_Textself);
        if (!fontAsset) {
            return;
        }

        auto fontAssetName = get_name->Invoke<Il2cppString*>(fontAsset);

        auto newFont = GetReplaceFont();
        if (!newFont) {
            std::ofstream debugLog(Local::GetBasePath() / "font_err.txt", std::ios::app);
            debugLog << "newFont is nullptr\n";
            return;
        }

        set_sourceFontFile->Invoke<void>(fontAsset, newFont);
        if (!updatedFontPtrs.contains(fontAsset)) {
            updatedFontPtrs.emplace(fontAsset);
            UpdateFontAssetData->Invoke<void>(fontAsset);
        }
        if (updatedFontPtrs.size() > 200) updatedFontPtrs.clear();
        
        set_font->Invoke<void>(TMP_Textself, nullptr);
        set_font->Invoke<void>(TMP_Textself, fontAsset);
    }

    DEFINE_HOOK(void, TMP_Text_PopulateTextBackingArray, (void* self, UnityResolve::UnityType::String* text, int start, int length)) {
        if (!text) {
            return TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length);
        }

        static auto Substring = Il2cppUtils::GetMethod("mscorlib.dll", "System", "String", "Substring",
                                                       {"System.Int32", "System.Int32"});

        const std::string origText = Substring->Invoke<Il2cppString*>(text, start, length)->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            UpdateFont(self);
            return TMP_Text_PopulateTextBackingArray_Orig(self, newText, 0, newText->length);
        }

        if (Config::textTest) {
            const auto newText = UnityResolve::UnityType::String::New("[TP]" + text->ToString());
            UpdateFont(self);
            return TMP_Text_PopulateTextBackingArray_Orig(self, newText, start, length + 4);
        }

        TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length);
    }

    DEFINE_HOOK(void, TMP_Text_set_text, (void* self, Il2cppString* value, void* mtd)) {
        if (!value) {
            return TMP_Text_set_text_Orig(self, value, mtd);
        }
        const std::string origText = value->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            UpdateFont(self);
            return TMP_Text_set_text_Orig(self, newText, mtd);
        }
        if (Config::textTest) {
            const auto newText = UnityResolve::UnityType::String::New("[TT]" + origText + "가나다");
            UpdateFont(self);
            return TMP_Text_set_text_Orig(self, newText, mtd);
        }
        
        TMP_Text_set_text_Orig(self, value, mtd);
    }

    DEFINE_HOOK(void, TMP_Text_SetText_1, (void* self, Il2cppString* sourceText, void* mtd)) {
        if (!sourceText) {
            return TMP_Text_SetText_1_Orig(self, sourceText, mtd);
        }
        const std::string origText = sourceText->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            UpdateFont(self);
            return TMP_Text_SetText_1_Orig(self, newText, mtd);
        }
        if (Config::textTest) {
            TMP_Text_SetText_1_Orig(self, UnityResolve::UnityType::String::New("[T1]" + origText), mtd);
        }
        else {
            TMP_Text_SetText_1_Orig(self, sourceText, mtd);
        }
        UpdateFont(self);
    }

    DEFINE_HOOK(void, TMP_Text_SetText_2, (void* self, Il2cppString* sourceText, bool syncTextInputBox, void* mtd)) {
        if (!sourceText) {
            return TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
        }
        const std::string origText = sourceText->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            UpdateFont(self);
            return TMP_Text_SetText_2_Orig(self, newText, syncTextInputBox, mtd);
        }
        if (Config::textTest) {
            TMP_Text_SetText_2_Orig(self, UnityResolve::UnityType::String::New("[TS]" + sourceText->ToString()), syncTextInputBox, mtd);
        }
        else {
            TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
        }
        UpdateFont(self);
    }

    DEFINE_HOOK(void, TextMeshProUGUI_Awake, (void* self, void* method)) {
        const auto TMP_Text_klass = Il2cppUtils::GetClass("Unity.TextMeshPro.dll",
                                                                     "TMPro", "TMP_Text");
        const auto get_Text_method = TMP_Text_klass->Get<UnityResolve::Method>("get_text");
        const auto set_Text_method = TMP_Text_klass->Get<UnityResolve::Method>("set_text");
        const auto currText = get_Text_method->Invoke<UnityResolve::UnityType::String*>(self);
        if (currText) {
            std::string transText;
            if (Local::GetGenericText(currText->ToString(), &transText)) {
                if (Config::textTest) {
                    set_Text_method->Invoke<void>(self, UnityResolve::UnityType::String::New("[TA]" + transText));
                }
                else {
                    set_Text_method->Invoke<void>(self, UnityResolve::UnityType::String::New(transText));
                }
            }
        }

        UpdateFont(self);
        TextMeshProUGUI_Awake_Orig(self, method);
    }

    DEFINE_HOOK(void, UIText_set_text, (void* self, Il2cppString* value)) {
        if (!value) {
            return UIText_set_text_Orig(self, value);
        }
        const std::string origText = value->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            return UIText_set_text_Orig(self, newText);
        }
        if (Config::textTest) {
            UIText_set_text_Orig(self, UnityResolve::UnityType::String::New("[UI]" + origText));
        }
        else {
            UIText_set_text_Orig(self, value);
        }
    }

    DEFINE_HOOK(void, TMP_Text_SetCharArray, (void* self, void* charArray, int start, int count, void* mtd)) {
        if (charArray && start >= 0 && count > 0) {
            auto arr = reinterpret_cast<UnityResolve::UnityType::Array<uint16_t>*>(charArray);
            if (static_cast<uintptr_t>(start + count) <= arr->max_length) {
                auto rawData = arr->GetData();
                if (rawData) {
                    const std::u16string u16(
                        reinterpret_cast<const char16_t*>(rawData + static_cast<uintptr_t>(start) * sizeof(char16_t)),
                        static_cast<size_t>(count));
                    const std::string origText = Misc::ToUTF8(u16);
                    std::string transText;
                    if (Local::GetGenericText(origText, &transText)) {
                        UpdateFont(self);
                        TMP_Text_set_text_Orig(self, Il2cppString::New(transText), nullptr);
                        return;
                    }
                    if (Config::textTest) {
                        UpdateFont(self);
                        TMP_Text_set_text_Orig(self, Il2cppString::New("[CA]" + origText), nullptr);
                        return;
                    }
                }
            }
        }
        TMP_Text_SetCharArray_Orig(self, charArray, start, count, mtd);
    }

    DEFINE_HOOK(void, TextField_set_value, (void* self, Il2cppString* value)) {
        if (value) {
            std::string transText;
            if (Local::GetGenericText(value->ToString(), &transText)) {
                return TextField_set_value_Orig(self, UnityResolve::UnityType::String::New(transText));
            }
        }
        TextField_set_value_Orig(self, value);
    }

    DEFINE_HOOK(void, MessageExtensions_MergeFrom, (void* message, void* span, void* mtd)) {
        MessageExtensions_MergeFrom_Orig(message, span, mtd);
        if (message) {
            auto ret_klass = Il2cppUtils::get_class_from_instance(message);
            if (ret_klass) {
                MasterLocal::LocalizeMasterItem(message, ret_klass->name);
            }
        }
    }

    void StartInjectFunctions() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();
        UnityResolve::Init(xdl_open(hookInstaller->m_il2cppLibraryPath.c_str(), RTLD_NOW), UnityResolve::Mode::Il2Cpp);


        ADD_HOOK(QualiArts_I18n_GetOrDefault,
                 Il2cppUtils::GetMethodPointer("idolypride.Runtime.dll", "QualiArts",
                                               "I18n", "GetOrDefault"));

        ADD_HOOK(AssetBundle_LoadAssetAsync, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.AssetBundle::LoadAssetAsync_Internal(System.String,System.Type)"));
        ADD_HOOK(AssetBundleRequest_GetResult, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.AssetBundleRequest::GetResult()"));
        ADD_HOOK(Resources_Load, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.ResourcesAPIInternal::Load(System.String,System.Type)"));

        ADD_HOOK(TextMeshProUGUI_Awake, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
                                                                      "TextMeshProUGUI", "Awake"));
        ADD_HOOK(TMP_Text_set_text, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
                                                                  "TMP_Text", "set_text"));
        ADD_HOOK(TMP_Text_SetText_1, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
                                                                  "TMP_Text", "SetText",
                                                                  {"System.String"}));
        ADD_HOOK(TMP_Text_PopulateTextBackingArray, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
                                                                  "TMP_Text", "PopulateTextBackingArray",
                                                                  {"System.String", "System.Int32", "System.Int32"}));
        ADD_HOOK(TMP_Text_SetText_2, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
            "TMP_Text", "SetText",
            { "System.String", "System.Boolean" }));

        ADD_HOOK(TextField_set_value, Il2cppUtils::GetMethodPointer("UnityEngine.UIElementsModule.dll", "UnityEngine.UIElements",
                                                                  "TextField", "set_value"));

        {
            auto uiTextPtr = Il2cppUtils::GetMethodPointer("UnityEngine.UI.dll", "UnityEngine.UI",
                                                           "Text", "set_text");
            if (uiTextPtr) {
                ADD_HOOK(UIText_set_text, uiTextPtr);
            }
        }

        ADD_HOOK(TMP_Text_SetCharArray, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
            "TMP_Text", "SetCharArray", {"System.Char[]", "System.Int32", "System.Int32"}));

        ADD_HOOK(MessageExtensions_MergeFrom, Il2cppUtils::GetMethodPointer("Google.Protobuf.dll", "Google.Protobuf",
                                                                            "MessageExtensions", "MergeFrom", {"Google.Protobuf.IMessage", "System.ReadOnlySpan<System.Byte>"}));

        ADD_HOOK(Internal_LogException, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_LogException(System.Exception,UnityEngine.Object)"));
        ADD_HOOK(Internal_Log, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_Log(UnityEngine.LogType,UnityEngine.LogOption,System.String,UnityEngine.Object)"));

        ADD_HOOK(Unity_set_position_Injected, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)"));
        ADD_HOOK(Unity_set_rotation_Injected, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)"));
        ADD_HOOK(Unity_get_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                      "Camera", "get_fieldOfView"));
        ADD_HOOK(Unity_set_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                      "Camera", "set_fieldOfView"));
        ADD_HOOK(Unity_set_targetFrameRate, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Application::set_targetFrameRate(System.Int32)"));
        ADD_HOOK(EndCameraRendering, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine.Rendering",
                                                                     "RenderPipeline", "EndCameraRendering"));
    }
    // 77 2640 5000

    DEFINE_HOOK(int, il2cpp_init, (const char* domain_name)) {
        const auto ret = il2cpp_init_Orig(domain_name);
        // InjectFunctions();

        Log::Info("Waiting for config...");

        while (!Config::isConfigInit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!Config::enabled) {
            Log::Info("Plugin not enabled");
            return ret;
        }

        Log::Info("Start init plugin...");

        StartInjectFunctions();
        IPCamera::initCameraSettings();
        Local::LoadData();
        MasterLocal::LoadData();

        Log::Info("Plugin init finished.");
        return ret;
    }
}


namespace IdolyprideLocal::Hook {
    void Install() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

        Log::Info("Installing hook");

        ADD_HOOK(HookMain::il2cpp_init,
                  Plugin::GetInstance().GetHookInstaller()->LookupSymbol("il2cpp_init"));

        Log::Info("Hook installed");
    }
}
