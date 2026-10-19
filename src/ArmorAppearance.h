#pragma once

namespace RE {
	class TESForm;
namespace BSScript { class IVirtualMachine; }
}

namespace jgjg::engine {
	bool InstallArmorAppearanceHook();
	bool InstallAppearanceSerialization();
	void RefreshPlayerArmor();
	bool SetArmorAppearance(RE::TESForm* original, RE::TESForm* appearance);
	bool ClearArmorAppearance(RE::TESForm* original);
	bool PreviewArmorAppearance(RE::TESForm* original, RE::TESForm* appearance);
	bool CancelArmorPreview(RE::TESForm* original);
	void ClearAllArmorAppearances();
	bool RegisterArmorAppearanceFunctions(RE::BSScript::IVirtualMachine* vm);
}
