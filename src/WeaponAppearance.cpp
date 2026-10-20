#include "WeaponAppearance.h"
#include "ArmorAppearance.h"
#include "RenderDiagnostics.h"
#include <RE/T/TESObjectSTAT.h>

#include <cstring>
#include <mutex>

namespace jgjg::engine {
namespace {

// The player branch in Skyrim 1.6.1170's weapon part builder calls the shared
// TESModel part loader at 15683 + 0x2B1. We verify both the call opcode and
// resolved target before writing the call, so an address drift disables this
// feature instead of patching an unrelated instruction.
constexpr REL::RelocationID kWeaponPartBuilder{ 15506, 15683 };
constexpr REL::RelocationID kLoadModelPart{ 15526, 15703 };
constexpr std::ptrdiff_t kPlayerLoadPartCall = 0x2B1;

using LoadPartFn = RE::NiAVObject* (*)(RE::TESModel*, std::int32_t, RE::TESObjectREFR*,
                                       RE::BSTSmartPointer<RE::BipedAnim>*, RE::NiAVObject*);
LoadPartFn loadPartOriginal{};

WeaponAppearanceMap overrides;
WeaponAppearanceMap previews;
std::mutex stateMutex;

enum class WeaponKind : std::uint8_t {
	Sword, Dagger, Waraxe, Mace, Greatsword, Battleaxe, Warhammer, Bow, Crossbow, Staff, Invalid
};

WeaponKind KindOf(const RE::TESObjectWEAP* weapon)
{
	if (!weapon) return WeaponKind::Invalid;
	switch (static_cast<unsigned>(weapon->GetWeaponType())) {
	case 1: return WeaponKind::Sword;
	case 2: return WeaponKind::Dagger;
	case 3: return WeaponKind::Waraxe;
	case 4: return WeaponKind::Mace;
	case 5: return WeaponKind::Greatsword;
	case 6: return weapon->HasKeywordString("WeapTypeWarhammer") ? WeaponKind::Warhammer : WeaponKind::Battleaxe;
	case 7: return WeaponKind::Bow;
	case 8: return WeaponKind::Staff;
	case 9: return WeaponKind::Crossbow;
	default: return WeaponKind::Invalid;
	}
}

RE::TESModel* BaseModel(RE::TESObjectWEAP* weapon)
{
	return weapon ? static_cast<RE::TESModelTextureSwap*>(weapon) : nullptr;
}

RE::TESModel* FirstPersonModel(RE::TESObjectWEAP* weapon)
{
	return weapon && weapon->firstPersonModelObject
		? static_cast<RE::TESModelTextureSwap*>(weapon->firstPersonModelObject)
		: nullptr;
}

RE::TESModel* ResolveModel(RE::TESObjectWEAP* original, RE::TESObjectWEAP* appearance,
                           RE::TESModel* incoming)
{
	if (!AreCompatiblePlayerWeapons(original, appearance)) return nullptr;
	// The part builder passes the weapon's WNAM model for first-person biped
	// parts and the base WEAP model for third-person parts. Preserve that role;
	// if the appearance has no WNAM model, its base model is the fallback.
	if (auto* originalFirstPerson = FirstPersonModel(original);
	    originalFirstPerson && incoming == originalFirstPerson) {
		if (auto* appearanceFirstPerson = FirstPersonModel(appearance)) {
			return appearanceFirstPerson;
		}
	}
	return BaseModel(appearance);
}

RE::NiAVObject* LoadPartHook(RE::TESModel* part, std::int32_t slot,
                             RE::TESObjectREFR* reference,
                             RE::BSTSmartPointer<RE::BipedAnim>* biped,
                             RE::NiAVObject* root)
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (reference == player) diagnostics::weapon.hookCalls.fetch_add(1, std::memory_order_relaxed);
	if (part && reference == player && biped && biped->get() && slot >= 0 &&
	    slot < static_cast<std::int32_t>(RE::BIPED_OBJECTS::kTotal)) {
		auto* item = biped->get()->objects[slot].item;
		auto* original = item ? item->As<RE::TESObjectWEAP>() : nullptr;
		if (original) {
			RE::FormID appearanceID{};
			{
				std::lock_guard lock(stateMutex);
				if (const auto preview = previews.find(original->GetFormID()); preview != previews.end())
					appearanceID = preview->second;
				else if (const auto saved = overrides.find(original->GetFormID()); saved != overrides.end())
					appearanceID = saved->second;
			}
			if (appearanceID) {
				auto* appearance = RE::TESForm::LookupByID<RE::TESObjectWEAP>(appearanceID);
				if (auto* model = ResolveModel(original, appearance, part)) {
					diagnostics::weapon.replacements.fetch_add(1, std::memory_order_relaxed);
					part = model;
				}
			}
		}
	}
	return loadPartOriginal(part, slot, reference, biped, root);
}

bool IsValidWeaponForm(const RE::TESObjectWEAP* weapon)
{
	if (!weapon || weapon->IsDynamicForm() || !weapon->GetPlayable() ||
	    KindOf(weapon) == WeaponKind::Invalid) return false;
	auto* model = static_cast<const RE::TESModelTextureSwap*>(weapon);
	const auto* path = model->GetModel();
	return path && *path;
}

}  // namespace

bool AreCompatiblePlayerWeapons(const RE::TESObjectWEAP* original,
                                const RE::TESObjectWEAP* appearance)
{
	return IsValidWeaponForm(original) && IsValidWeaponForm(appearance) &&
	       KindOf(original) == KindOf(appearance);
}

bool InstallWeaponAppearanceHook()
{
	const auto parent = REL::Relocation<std::uintptr_t>{ kWeaponPartBuilder }.address();
	const auto expected = REL::Relocation<std::uintptr_t>{ kLoadModelPart }.address();
	const auto site = parent + kPlayerLoadPartCall;
	if (!parent || !expected || *reinterpret_cast<const std::uint8_t*>(site) != 0xE8) {
		SKSE::log::error("Weapon appearance hook site did not match the expected call");
		return false;
	}
	std::int32_t displacement{};
	std::memcpy(&displacement, reinterpret_cast<const void*>(site + 1), sizeof(displacement));
	if (site + 5 + displacement != expected) {
		SKSE::log::error("Weapon appearance hook target mismatch; leaving vanilla weapon rendering active");
		return false;
	}
	loadPartOriginal = reinterpret_cast<LoadPartFn>(
		SKSE::GetTrampoline().write_call<5>(site, reinterpret_cast<std::uintptr_t>(LoadPartHook)));
	if (!loadPartOriginal) return false;
	SKSE::log::info("Player weapon appearance hook installed for Skyrim 1.6.1170");
	return true;
}

bool SetWeaponAppearance(RE::TESForm* originalForm, RE::TESForm* appearanceForm)
{
	auto* original = originalForm ? originalForm->As<RE::TESObjectWEAP>() : nullptr;
	auto* appearance = appearanceForm ? appearanceForm->As<RE::TESObjectWEAP>() : nullptr;
	if (!AreCompatiblePlayerWeapons(original, appearance)) return false;
	{
		std::lock_guard lock(stateMutex);
		overrides.insert_or_assign(original->GetFormID(), appearance->GetFormID());
		previews.erase(original->GetFormID());
	}
	RefreshPlayerWeapons();
	return true;
}

bool ClearWeaponAppearance(RE::TESForm* originalForm)
{
	auto* original = originalForm ? originalForm->As<RE::TESObjectWEAP>() : nullptr;
	if (!original) return false;
	bool removed{};
	{
		std::lock_guard lock(stateMutex);
		removed = overrides.erase(original->GetFormID()) != 0;
		removed = previews.erase(original->GetFormID()) != 0 || removed;
	}
	if (removed) RefreshPlayerWeapons();
	return removed;
}

bool PreviewWeaponAppearance(RE::TESForm* originalForm, RE::TESForm* appearanceForm)
{
	auto* original = originalForm ? originalForm->As<RE::TESObjectWEAP>() : nullptr;
	auto* appearance = appearanceForm ? appearanceForm->As<RE::TESObjectWEAP>() : nullptr;
	if (!AreCompatiblePlayerWeapons(original, appearance)) return false;
	{
		std::lock_guard lock(stateMutex);
		previews.insert_or_assign(original->GetFormID(), appearance->GetFormID());
	}
	RefreshPlayerWeapons();
	return true;
}

bool CancelWeaponPreview(RE::TESForm* originalForm)
{
	auto* original = originalForm ? originalForm->As<RE::TESObjectWEAP>() : nullptr;
	if (!original) return false;
	bool removed{};
	{
		std::lock_guard lock(stateMutex);
		removed = previews.erase(original->GetFormID()) != 0;
	}
	if (removed) RefreshPlayerWeapons();
	return removed;
}

void RefreshPlayerWeapons()
{
	// The equipment update refreshes biped parts without unequipping or
	// replacing the real weapon instance.
	jgjg::engine::RefreshPlayerArmor();
}

std::string GetWeaponRenderDiagnostics()
{
	return "Player model calls " + std::to_string(diagnostics::weapon.hookCalls.load(std::memory_order_relaxed)) +
	       ", swaps " + std::to_string(diagnostics::weapon.replacements.load(std::memory_order_relaxed));
}

WeaponAppearanceMap GetWeaponAppearanceOverrides()
{
	std::lock_guard lock(stateMutex);
	return overrides;
}

void ReplaceWeaponAppearanceOverrides(WeaponAppearanceMap appearances)
{
	{
		std::lock_guard lock(stateMutex);
		overrides = std::move(appearances);
		previews.clear();
	}
	RefreshPlayerWeapons();
}

}  // namespace jgjg::engine
