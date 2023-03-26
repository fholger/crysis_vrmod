#include "StdAfx.h"

#include "Hooks.h"

#include "MinHook.h"

#include <unordered_map>

namespace {
	struct HookInfo {
		intptr_t target;
		intptr_t original;
		intptr_t hook;
	};
	std::unordered_map<intptr_t, HookInfo> g_hooksToOriginal;
}

namespace hooks {

	void Init() {
		if (MH_Initialize() != MH_OK) {
			CryError("Failed to initialize MinHook");
		}
	}

	void Shutdown() {
		MH_Uninitialize();
		g_hooksToOriginal.clear();
	}

	void InstallVirtualFunctionHook(const std::string &name, void *instance, uint32_t methodPos, void *detour) {
		CryLog("Installing virtual function hook for %s", name.c_str());
		LPVOID *vtable = *((LPVOID**)instance);
		LPVOID pTarget = vtable[methodPos];

		LPVOID pOriginal = nullptr;
		MH_STATUS result = MH_CreateHook(pTarget, detour, &pOriginal);
		if (result != MH_OK || MH_EnableHook(pTarget) != MH_OK) {
			if (result == MH_ERROR_ALREADY_CREATED) {
				CryLog("  Hook already installed.");
			} else {
				CryError("Failed to install hook for %s!", name);
			}
			return;
		}

		g_hooksToOriginal[reinterpret_cast<intptr_t>(detour)] = HookInfo {
			reinterpret_cast<intptr_t>(pTarget),
			reinterpret_cast<intptr_t>(pOriginal),
			reinterpret_cast<intptr_t>(detour),
		};
	}

	void RemoveHook(void *detour) {
		auto entry = g_hooksToOriginal.find(reinterpret_cast<intptr_t>(detour));
		if (entry != g_hooksToOriginal.end()) {
			void *target = reinterpret_cast<void *>(entry->second.target);
			CryLog("Removing hook to %ul", target);
			MH_STATUS status = MH_DisableHook(target);
			if (status != MH_OK) {
				CryError("Error when disabling hook to %ul: %i", target, status);
			}
			status = MH_RemoveHook(target);
			if (status != MH_OK) {
				CryError("Error when removing hook to %ul: %i", target, status);
			}
			g_hooksToOriginal.erase(entry);
		}
	}

	void InstallHook(const std::string &name, void *target, void *detour) {
		CryLog("Installing hook for %s from %ul to %ul", name, target, detour);
		LPVOID pOriginal = nullptr;
		if (MH_CreateHook(target, detour, &pOriginal) != MH_OK || MH_EnableHook(target) != MH_OK) {
			CryError("Failed to install hook for %s", name);
			return;
		}

		g_hooksToOriginal[reinterpret_cast<intptr_t>(detour)] = HookInfo {
			reinterpret_cast<intptr_t>(target),
			reinterpret_cast<intptr_t>(pOriginal),
			reinterpret_cast<intptr_t>(detour),
		};
	}

	void InstallHookInDll(const std::string &name, HMODULE module, void *detour) {
		LPVOID target = GetProcAddress(module, name.c_str());
		if (target != nullptr) {
			InstallHook(name, target, detour);
		}
	}

	intptr_t HookToOriginal(intptr_t hook) {
		return g_hooksToOriginal[hook].original;
	}
}
