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
            HoshimiLocal::Log::InfoFmt("ADD_HOOK: %s at %p", #name, addr);                         \
        }                                                                                          \
    }                                                                                              \
    else HoshimiLocal::Log::ErrorFmt("Hook failed: %s is NULL", #name, addr)

void UnHookAll() {
    for (const auto i: hookedStubs) {
        int result = shadowhook_unhook(i);
        if(result != 0)
        {
            int error_num = shadowhook_get_errno();
            const char *error_msg = shadowhook_to_errmsg(error_num);
            HoshimiLocal::Log::ErrorFmt("unhook failed: %d - %s", error_num, error_msg);
        }
    }
}

namespace HoshimiLocal::HookMain {
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
                            static HoshimiLocal::Misc::FixedSizeQueue<float> recordsY(60);
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

    namespace {
        std::string HexDump(const void* ptr, size_t size) {
            const unsigned char* p = static_cast<const unsigned char*>(ptr);
            std::string res;
            char buf[4];
            for (size_t i = 0; i < size; ++i) {
                snprintf(buf, sizeof(buf), "%02X ", p[i]);
                res += buf;
            }
            return res;
        }
    }

    DEFINE_HOOK(UnityResolve::UnityType::String*, QualiArts_I18n_Get, (void* self, UnityResolve::UnityType::String* key, void* method)) {
        auto ret = QualiArts_I18n_Get_Orig(self, key, method);
        try {
            if (key != nullptr) {
                // Log::DebugFmt("QualiArts_I18n_Get hex: %s", HexDump(key, 32).c_str());
                std::string keyStr = key->ToString();
                std::string translated;
                if (!keyStr.empty() && Local::GetI18n(keyStr, &translated)) {
                    return UnityResolve::UnityType::String::New(translated);
                }
            }
        } catch (...) {
            // Ignore
        }
        return ret;
    }

    DEFINE_HOOK(UnityResolve::UnityType::String*, QualiArts_I18n_GetOrDefault, (void* self, UnityResolve::UnityType::String* key, UnityResolve::UnityType::String* defaultKey, void* method)) {
        auto ret = QualiArts_I18n_GetOrDefault_Orig(self, key, defaultKey, method);
        try {
            if (key != nullptr) {
                Log::DebugFmt("QualiArts_I18n_GetOrDefault hex: %s", HexDump(key, 32).c_str());
                std::string keyStr = key->ToString();
                std::string translated;
                if (!keyStr.empty() && Local::GetI18n(keyStr, &translated)) {
                    return UnityResolve::UnityType::String::New(translated);
                }
            }
        } catch (...) {
            // Ignore
        }
        return ret;
    }



    void* fontCache = nullptr;
    void* tmpFontCache = nullptr;

    void* GetReplaceFont(bool asTMP = false) {
        if (asTMP && tmpFontCache) {
            if (IsNativeObjectAlive(tmpFontCache)) return tmpFontCache;
        }
        if (!asTMP && fontCache) {
            if (IsNativeObjectAlive(fontCache)) return fontCache;
        }

        static auto fontName = Local::GetBasePath() / "local-files" / "gkamsZHFontMIX.otf";
        if (!std::filesystem::exists(fontName)) {
            Log::ErrorFmt("GetReplaceFont: Font file not found: %s", fontName.c_str());
            return nullptr;
        }

        static void (*CreateFontP)(void* self, Il2cppString* path) = nullptr;
        static const char* icall_variants[] = {
            "UnityEngine.Font::Internal_CreateFontFromPath(UnityEngine.Font,System.String)",
            "UnityEngine.Font::Internal_CreateFont(UnityEngine.Font,System.String)",
        };

        if (!CreateFontP) {
            for (auto variant : icall_variants) {
                CreateFontP = reinterpret_cast<void (*)(void*, Il2cppString*)>(
                    Il2cppUtils::il2cpp_resolve_icall(variant)
                );
                if (CreateFontP) break;
            }
        }

        static UnityResolve::Class* Font_klass = nullptr;
        static UnityResolve::Method* Font_ctor = nullptr;
        if (!Font_klass) {
            Font_klass = Il2cppUtils::GetClass("UnityEngine.TextRenderingModule.dll", "UnityEngine", "Font");
            if (Font_klass) Font_ctor = Font_klass->Get<UnityResolve::Method>(".ctor");
        }

        if (!CreateFontP || !Font_klass || !Font_ctor) {
             Log::Error("GetReplaceFont: Failed to resolve Font creation methods.");
             return nullptr;
        }

        const auto newFont = Font_klass->New<void*>();
        Font_ctor->Invoke<void>(newFont);
        CreateFontP(newFont, Il2cppString::New(fontName.string()));
        fontCache = newFont;

        // Create TMP Font Asset from raw font
        static UnityResolve::Class* TMP_FontAsset_klass = nullptr;
        if (!TMP_FontAsset_klass) {
            const char* assemblies[] = {"Unity.TextMeshPro.dll", "Unity.TextMeshPro", "TextMeshPro-Runtime"};
            for (auto asm_name : assemblies) {
                TMP_FontAsset_klass = Il2cppUtils::GetClass(asm_name, "TMPro", "TMP_FontAsset");
                if (TMP_FontAsset_klass) break;
            }
        }

        if (TMP_FontAsset_klass) {
            static bool mtdsLogged = false;
            if (!mtdsLogged) {
                Log::Info("GetReplaceFont: Listing TMP_FontAsset methods...");
                for (auto mtd : TMP_FontAsset_klass->methods) {
                    if (mtd && !mtd->name.empty()) {
                        Log::InfoFmt(" - %s (Args: %d)", mtd->name.c_str(), (int)mtd->args.size());
                    }
                }
                mtdsLogged = true;
            }
            
            // Try to find the correct CreateFontAsset variant
            static UnityResolve::Method* CreateFontAsset = nullptr;
            if (!CreateFontAsset) {
                CreateFontAsset = TMP_FontAsset_klass->Get<UnityResolve::Method>("CreateFontAsset", {"UnityEngine.Font"});
                if (!CreateFontAsset) CreateFontAsset = TMP_FontAsset_klass->Get<UnityResolve::Method>("CreateFontAsset", {"UnityEngine.Font", "*", "*", "*", "*", "*", "*"}); 
            }

            if (CreateFontAsset) {
                if (CreateFontAsset->args.size() == 1) {
                    tmpFontCache = CreateFontAsset->Invoke<void*>(nullptr, newFont);
                } else {
                    Log::InfoFmt("GetReplaceFont: CreateFontAsset found but has %d args, need more research.", (int)CreateFontAsset->args.size());
                }
            } else {
                Log::Error("GetReplaceFont: CreateFontAsset method not found by name.");
            }
        } else {
            Log::Error("GetReplaceFont: TMP_FontAsset class not found in any common assembly.");
        }

        if (tmpFontCache) {
            Log::Info("GetReplaceFont: Successfully created TMP_FontAsset.");
            static auto set_name = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_name");
            if (set_name) set_name->Invoke<void>(tmpFontCache, Il2cppString::New("KoreanFallbackFont"));
        }

        return asTMP ? tmpFontCache : fontCache;
    }

    void GlobalInjectFallbacks(void* koreanTMP) {
        static bool globallyInjected = false;
        if (globallyInjected) return;

        static UnityResolve::Class* settingsKlass = nullptr;
        if (!settingsKlass) {
            const char* assemblies[] = {"Unity.TextMeshPro.dll", "Unity.TextMeshPro", "TextMeshPro-Runtime"};
            for (auto asm_name : assemblies) {
                settingsKlass = Il2cppUtils::GetClass(asm_name, "TMPro", "TMP_Settings");
                if (settingsKlass) break;
            }
        }
        
        if (!settingsKlass) {
            Log::Error("GlobalInjectFallbacks: TMP_Settings class not found.");
            return;
        }

        // Try to get static field fallbackFontAssets
        static auto globalFallbackField = settingsKlass->Get<UnityResolve::Field>("fallbackFontAssets");
        if (globalFallbackField) {
            void* globalList = nullptr;
            globalFallbackField->GetValue(&globalList);
            if (globalList) {
                Il2cppUtils::Tools::CSListEditor editor(globalList);
                editor.Add(koreanTMP);
                Log::Info("GlobalInjectFallbacks: Successfully added Korean to TMP_Settings.fallbackFontAssets");
                globallyInjected = true;
            } else {
                Log::Error("GlobalInjectFallbacks: fallbackFontAssets list is null via field.");
            }
        } else {
            Log::Error("GlobalInjectFallbacks: fallbackFontAssets field not found.");
        }
    }

    std::unordered_set<void*> updatedFontPtrs{};

    void UpdateFont(void* TMP_Textself) {
        if (!Config::replaceFont) return;

        static auto get_font = Il2cppUtils::GetMethod(
            "Unity.TextMeshPro.dll", "TMPro", "TMP_Text", "get_font");
        static auto get_name = Il2cppUtils::GetMethod(
            "UnityEngine.CoreModule.dll", "UnityEngine", "Object", "get_name");
        
        static auto fontAssetKlass = Il2cppUtils::GetClass("Unity.TextMeshPro.dll", "TMPro", "TMP_FontAsset");
        static auto get_fallbackTable = fontAssetKlass ? fontAssetKlass->Get<UnityResolve::Method>("get_fallbackFontAssetTable") : nullptr;
        static auto fallbackField = fontAssetKlass ? fontAssetKlass->Get<UnityResolve::Field>("fallbackFontAssetTable") : nullptr;

        auto fontAsset = get_font->Invoke<void*>(TMP_Textself);
        if (!fontAsset) return;

        // Diagnostic Logging
        auto fontAssetNameString = get_name->Invoke<Il2cppString*>(fontAsset);
        std::string fontName = fontAssetNameString != nullptr ? fontAssetNameString->ToString() : "null";
        Log::DebugFmt("UpdateFont: Processing %s", fontName.c_str());

        auto koreanTMP = GetReplaceFont(true);
        if (!koreanTMP) {
             Log::Error("UpdateFont: Failed to get/create Korean TMP Font Asset.");
             return;
        }

        static bool globalDone = false;
        if (!globalDone) {
            GlobalInjectFallbacks(koreanTMP);
            globalDone = true;
        }

        // Try Local Injection
        void* fallbackList = nullptr;
        if (get_fallbackTable) fallbackList = get_fallbackTable->Invoke<void*>(fontAsset);
        if (!fallbackList && fallbackField) {
            fallbackList = *reinterpret_cast<void**>((uintptr_t)fontAsset + fallbackField->offset);
            Log::DebugFmt("UpdateFont: Found fallback list via field for %s", fontName.c_str());
        }

        if (fallbackList) {
            Il2cppUtils::Tools::CSListEditor<void*> editor(fallbackList);
            // Check count first
            int count = editor.get_Count();
            bool alreadyHas = false;
            for (int i = 0; i < count; i++) {
                if (editor.get_Item(i) == koreanTMP) {
                    alreadyHas = true;
                    break;
                }
            }
            if (!alreadyHas) {
                editor.Add(koreanTMP);
                Log::InfoFmt("UpdateFont: Successfully added Korean fallback to %s (Table size: %d -> %d)", 
                    fontName.c_str(), count, count + 1);
            }
        } else {
             Log::InfoFmt("UpdateFont: Could not find fallback list for %s", fontName.c_str());
        }
    }

    // ??????????????????????????????????????????????????????????????????????????
    DEFINE_HOOK(void, TMP_Text_PopulateTextBackingArray, (void* self, Il2cppString* text, int start, int length, void* mtd)) {
        if (!text) {
            return TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length, mtd);
        }

        UpdateFont(self);

        static auto Substring = Il2cppUtils::GetMethod("mscorlib.dll", "System", "String", "Substring",
                                                       {"System.Int32", "System.Int32"});

        const std::string origText = Substring->Invoke<Il2cppString*>(text, start, length)->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            return TMP_Text_PopulateTextBackingArray_Orig(self, newText, 0, newText->length, mtd);
        }

        if (Config::textTest) {
            const auto newText = UnityResolve::UnityType::String::New("[TP]" + text->ToString());
            return TMP_Text_PopulateTextBackingArray_Orig(self, newText, start, length + 4, mtd);
        }

        TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length, mtd);
    }


    DEFINE_HOOK(void, TMP_Text_set_text, (void* self, Il2cppString* value, void* mtd)) {
        if (!value) {
            return TMP_Text_set_text_Orig(self, value, mtd);
        }

        Log::DebugFmt("TMP_Text_set_text hit! text: %s", value->ToString().c_str());
        UpdateFont(self);

        const std::string origText = value->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            return TMP_Text_set_text_Orig(self, newText, mtd);
        }
        
        TMP_Text_set_text_Orig(self, value, mtd);
    }

    DEFINE_HOOK(void, UnityEngine_UI_Text_set_text, (void* self, Il2cppString* value, void* mtd)) {
        if (!value) {
            return UnityEngine_UI_Text_set_text_Orig(self, value, mtd);
        }
        Log::DebugFmt("UnityEngine_UI_Text_set_text hit! text: %s", value->ToString().c_str());
        // For standard UI Text, we might need a different font update logic if TMP logic doesn't apply.
        // But let's log it first.
        UnityEngine_UI_Text_set_text_Orig(self, value, mtd);
    }

    DEFINE_HOOK(void, TMP_Text_OnEnable, (void* self, void* mtd)) {
        Log::Debug("TMP_Text_OnEnable hit!");
        UpdateFont(self);
        TMP_Text_OnEnable_Orig(self, mtd);
    }

    DEFINE_HOOK(void, TMP_Text_SetText_1, (void* self, Il2cppString* sourceText, void* mtd)) {
        if (!sourceText) {
            return TMP_Text_SetText_1_Orig(self, sourceText, mtd);
        }

        UpdateFont(self);

        const std::string origText = sourceText->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            return TMP_Text_SetText_1_Orig(self, newText, mtd);
        }
        if (Config::textTest) {
            TMP_Text_SetText_1_Orig(self, UnityResolve::UnityType::String::New("[T1]" + origText), mtd);
            return;
        }
        
        TMP_Text_SetText_1_Orig(self, sourceText, mtd);
    }


    DEFINE_HOOK(void, TMP_Text_SetText_2, (void* self, Il2cppString* sourceText, bool syncTextInputBox, void* mtd)) {
        if (!sourceText) {
            return TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
        }

        UpdateFont(self);

        const std::string origText = sourceText->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            return TMP_Text_SetText_2_Orig(self, newText, syncTextInputBox, mtd);
        }
        if (Config::textTest) {
            TMP_Text_SetText_2_Orig(self, UnityResolve::UnityType::String::New("[TS]" + sourceText->ToString()), syncTextInputBox, mtd);
            return;
        }

        TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
    }


    DEFINE_HOOK(void, TextMeshProUGUI_Awake, (void* self, void* method)) {
        TextMeshProUGUI_Awake_Orig(self, method);
        UpdateFont(self);
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
        void* handle = xdl_open(hookInstaller->m_il2cppLibraryPath.c_str(), RTLD_NOW);
        UnityResolve::Init(handle, UnityResolve::Mode::Il2Cpp);

        struct TargetMethod {
            const char* assembly;
            const char* nameSpace;
            const char* className;
        };

        const TargetMethod targets[] = {
                {"quaunity-ui.Runtime.dll", "Qua.UI", "I18n"},
                {"Assembly-CSharp.dll", "QualiArts", "I18n"},
                {"quaunity-api.Runtime.dll", "QualiArts", "I18n"},
                {"idolypride.Runtime.dll", "QualiArts", "I18n"}
        };

        void* get_mtd_ptr = nullptr;
        void* get_def_mtd_ptr = nullptr;

        for (const auto& target : targets) {
            get_mtd_ptr = Il2cppUtils::GetMethodPointer(target.assembly, target.nameSpace, target.className, "Get");
            if (get_mtd_ptr) {
                get_def_mtd_ptr = Il2cppUtils::GetMethodPointer(target.assembly, target.nameSpace, target.className, "GetOrDefault");
                Log::InfoFmt("I18n found in %s (%s.%s)", target.assembly, target.nameSpace, target.className);
                break;
            }
        }

        if (get_mtd_ptr) {
            ADD_HOOK(QualiArts_I18n_Get, get_mtd_ptr);
            if (get_def_mtd_ptr) {
                ADD_HOOK(QualiArts_I18n_GetOrDefault, get_def_mtd_ptr);
            }
        } else {
            Log::Error("QualiArts.I18n not found in any targeted assemblies.");
        }


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
                ADD_HOOK(UnityEngine_UI_Text_set_text, uiTextPtr);
            }
        }

        {
            auto tmpOnEnablePtr = Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
                                                                 "TMP_Text", "OnEnable");
            if (tmpOnEnablePtr) {
                ADD_HOOK(TMP_Text_OnEnable, tmpOnEnablePtr);
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


namespace HoshimiLocal::Hook {
    void Install() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

        Log::Info("Installing hook");

        ADD_HOOK(HookMain::il2cpp_init,
                  Plugin::GetInstance().GetHookInstaller()->LookupSymbol("il2cpp_init"));

        Log::Info("Hook installed");
    }
}
