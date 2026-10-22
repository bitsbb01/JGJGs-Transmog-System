#pragma once

#include <string>

namespace RE {
	class TESForm;
namespace BSScript { class IVirtualMachine; }
}

namespace jgjg::engine {
	bool InstallArmorAppearanceHook();
	bool InstallAppearanceSerialization();
	void RefreshPlayerArmor();
	void RebuildPendingPlayerArmor();
	std::string GetArmorRenderDiagnostics();
	std::string GetArmorRenderFormIDs();
	std::string GetArmorRenderContext();
	std::string GetArmorSourceAddonMasks();
	std::string GetArmorAppearanceAddonMasks();
	std::string GetArmorHookAddonMasks();
	std::string GetPlayerWornArmorFormIDs();
	std::string GetArmorOverrideStatus();
	bool SetArmorAppearance(RE::TESForm* original, RE::TESForm* appearance);
	bool ClearArmorAppearance(RE::TESForm* original);
	bool PreviewArmorAppearance(RE::TESForm* original, RE::TESForm* appearance);
	bool CancelArmorPreview(RE::TESForm* original);
	void ClearAllArmorAppearances();
	bool RegisterArmorAppearanceFunctions(RE::BSScript::IVirtualMachine* vm);
}
