#pragma once

#include <string>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <map>

namespace jgjg::engine {

using WeaponAppearanceMap = std::map<RE::FormID, RE::FormID>;

bool InstallWeaponAppearanceHook();
bool SetWeaponAppearance(RE::TESForm* original, RE::TESForm* appearance);
bool ClearWeaponAppearance(RE::TESForm* original);
bool PreviewWeaponAppearance(RE::TESForm* original, RE::TESForm* appearance);
bool CancelWeaponPreview(RE::TESForm* original);
void RefreshPlayerWeapons();
std::string GetWeaponRenderDiagnostics();
std::string GetWeaponRenderFormIDs();
WeaponAppearanceMap GetWeaponAppearanceOverrides();
void ReplaceWeaponAppearanceOverrides(WeaponAppearanceMap appearances);
bool AreCompatiblePlayerWeapons(const RE::TESObjectWEAP* original,
                                const RE::TESObjectWEAP* appearance);

}  // namespace jgjg::engine
