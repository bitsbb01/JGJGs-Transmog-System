#include "ArmorAppearance.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <safetyhook.hpp>

#include <map>
#include <mutex>

namespace jgjg::engine {
namespace {
	using ActorWeightModel = void;
	constexpr REL::RelocationID kApplyArmorAddon{ 17392, 17792 };

	std::map<RE::FormID, RE::FormID> overrides;
	std::mutex overridesMutex;
	SafetyHookInline applyArmorAddonHook;

	bool IsPlayerBiped(const ActorWeightModel* model, const RE::PlayerCharacter* player)
	{
		if (!model || !player) return false;
		return model == reinterpret_cast<const ActorWeightModel*>(std::addressof(player->GetBiped1(false))) ||
		       model == reinterpret_cast<const ActorWeightModel*>(std::addressof(player->GetBiped1(true)));
	}

	RE::TESObjectARMO* ResolveOverride(RE::TESObjectARMO* original)
	{
		if (!original) return nullptr;
		RE::FormID appearanceID{};
		{
			std::lock_guard lock(overridesMutex);
			auto found = overrides.find(original->GetFormID());
			if (found == overrides.end()) return nullptr;
			appearanceID = found->second;
		}
		auto* appearance = RE::TESForm::LookupByID<RE::TESObjectARMO>(appearanceID);
		if (!appearance || !appearance->GetPlayable() ||
		    appearance->GetSlotMask() != original->GetSlotMask()) return nullptr;
		return appearance;
	}

	bool ApplyArmorAddonHook(RE::TESObjectARMO* armor, RE::TESRace* race,
	                         ActorWeightModel* model, bool female)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (armor && IsPlayerBiped(model, player)) {
			if (auto* appearance = ResolveOverride(armor)) armor = appearance;
		}
		return applyArmorAddonHook.call<bool>(armor, race, model, female);
	}

	void RefreshPlayerArmor()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) return;
		auto* process = player->GetActorRuntimeData().currentProcess;
		if (!process) return;
		using SetFlagFn = void (*)(RE::AIProcess*, std::uint8_t);
		using UpdateEquipmentFn = void (*)(RE::AIProcess*, RE::Actor*);
		static REL::Relocation<SetFlagFn> setFlag{ REL::RelocationID(38867, 39907) };
		static REL::Relocation<UpdateEquipmentFn> updateEquipment{ REL::RelocationID(38404, 39395) };
		setFlag(process, static_cast<std::uint8_t>(1u << 0));
		updateEquipment(process, player);
	}

	bool SetArmorAppearanceNative(RE::StaticFunctionTag*, RE::TESForm* originalForm,
	                              RE::TESForm* appearanceForm)
	{
		auto* original = originalForm ? originalForm->As<RE::TESObjectARMO>() : nullptr;
		auto* appearance = appearanceForm ? appearanceForm->As<RE::TESObjectARMO>() : nullptr;
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!original || !appearance || !player || original->IsDynamicForm() ||
		    appearance->IsDynamicForm() || !original->GetPlayable() || !appearance->GetPlayable() ||
		    static_cast<std::uint32_t>(original->GetSlotMask()) == 0 ||
		    original->GetSlotMask() != appearance->GetSlotMask()) return false;
		{
			std::lock_guard lock(overridesMutex);
			overrides.insert_or_assign(original->GetFormID(), appearance->GetFormID());
		}
		RefreshPlayerArmor();
		return true;
	}

	bool ClearArmorAppearanceNative(RE::StaticFunctionTag*, RE::TESForm* originalForm)
	{
		auto* original = originalForm ? originalForm->As<RE::TESObjectARMO>() : nullptr;
		if (!original) return false;
		bool removed{};
		{
			std::lock_guard lock(overridesMutex);
			removed = overrides.erase(original->GetFormID()) != 0;
		}
		if (removed) RefreshPlayerArmor();
		return removed;
	}
}

bool InstallArmorAppearanceHook()
{
	const auto address = REL::Relocation<std::uintptr_t>{ kApplyArmorAddon }.address();
	if (!address) return false;
	applyArmorAddonHook = safetyhook::create_inline(reinterpret_cast<void*>(address),
	                                                  reinterpret_cast<void*>(ApplyArmorAddonHook));
	if (!applyArmorAddonHook) {
		SKSE::log::error("Could not install armor appearance hook");
		return false;
	}
	SKSE::log::info("Armor appearance hook installed for Skyrim 1.6.1170");
	return true;
}

bool SetArmorAppearance(RE::TESForm* original, RE::TESForm* appearance)
{
	return SetArmorAppearanceNative(nullptr, original, appearance);
}

bool ClearArmorAppearance(RE::TESForm* original)
{
	return ClearArmorAppearanceNative(nullptr, original);
}

void ClearAllArmorAppearances()
{
	std::lock_guard lock(overridesMutex);
	overrides.clear();
}

bool RegisterArmorAppearanceFunctions(RE::BSScript::IVirtualMachine* vm)
{
	vm->RegisterFunction("SetArmorAppearance", "JGJGTransmogNative", SetArmorAppearanceNative);
	vm->RegisterFunction("ClearArmorAppearance", "JGJGTransmogNative", ClearArmorAppearanceNative);
	return true;
}
}  // namespace jgjg::engine
