#include "Plugin.h"
#include "Hook.h"

namespace IdolyprideLocal {
    HookInstaller::~HookInstaller() {
    }

    Plugin &Plugin::GetInstance() {
        static Plugin instance;
        return instance;
    }

    void Plugin::InstallHook(std::unique_ptr<HookInstaller>&& hookInstaller)
    {
        m_HookInstaller = std::move(hookInstaller);
        Hook::Install();
    }

    HookInstaller* Plugin::GetHookInstaller() const
    {
        return m_HookInstaller.get();
    }

}