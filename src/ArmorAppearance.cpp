#include "ArmorAppearance.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <safetyhook.hpp>

#include <map>
#include <mutex>
#include <memory>
#include <array>
#include <algorithm>
#include <cstddef>

namespace jgjg::engine {
namespace {
	using ActorWeightModel = void;
	constexpr REL::RelocationID kApplyArmorAddon{ 17392, 17792 };

	std::map<RE::FormID, RE::FormID> overrides;
	std::map<RE::FormID, RE::FormID> previews;
	std::mutex overridesMutex;
	SafetyHookInline applyArmorAddonHook;
	constexpr std::uint32_t kSerializationUID = 0x4A475453;  // JGTS
	constexpr std::uint32_t kAppearanceRecord = 0x41524D4F;  // ARMO
	struct AppearanceRecord { RE::FormID original; RE::FormID appearance; };

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
			if (const auto preview = previews.find(original->GetFormID()); preview != previews.end()) {
				appearanceID = preview->second;
			} else if (const auto saved = overrides.find(original->GetFormID()); saved != overrides.end()) {
				appearanceID = saved->second;
			} else {
				return nullptr;
			}
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

	void RefreshPlayerArmorImpl()
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
			previews.erase(original->GetFormID());
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
			previews.erase(original->GetFormID());
		}
		if (removed) RefreshPlayerArmor();
		return removed;
	}

	void Save(SKSE::SerializationInterface* serialization)
	{
		std::lock_guard lock(overridesMutex);
		for (const auto& [original, appearance] : overrides) {
			const AppearanceRecord record{ original, appearance };
			if (!serialization->WriteRecord(kAppearanceRecord, 1, record)) {
				SKSE::log::error("Failed to save armor appearance {:08X}", original);
			}
		}
	}

	void Load(SKSE::SerializationInterface* serialization)
	{
		std::map<RE::FormID, RE::FormID> loaded;
		std::uint32_t type{}, version{}, length{};
		while (serialization->GetNextRecordInfo(type, version, length)) {
			if (type != kAppearanceRecord || version != 1 || length != sizeof(AppearanceRecord)) {
				std::array<std::byte, 256> discard{};
				while (length > 0) {
					const auto chunk = std::min<std::uint32_t>(length, static_cast<std::uint32_t>(discard.size()));
					const auto read = serialization->ReadRecordData(discard.data(), chunk);
					if (read == 0) break;
					length -= read;
				}
				continue;
			}
			AppearanceRecord record{};
			if (serialization->ReadRecordData(record) != sizeof(record)) continue;
			RE::FormID original{}, appearance{};
			if (!serialization->ResolveFormID(record.original, original) ||
			    !serialization->ResolveFormID(record.appearance, appearance)) continue;
			auto* originalForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(original);
			auto* appearanceForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(appearance);
			if (originalForm && appearanceForm && originalForm->GetSlotMask() == appearanceForm->GetSlotMask())
				loaded.insert_or_assign(original, appearance);
		}
		{
			std::lock_guard lock(overridesMutex);
			overrides = std::move(loaded);
			previews.clear();
		}
	}

	void Revert(SKSE::SerializationInterface*)
	{
		ClearAllArmorAppearances();
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

bool InstallAppearanceSerialization()
{
	auto* serialization = SKSE::GetSerializationInterface();
	if (!serialization) return false;
	serialization->SetUniqueID(kSerializationUID);
	serialization->SetSaveCallback(Save);
	serialization->SetLoadCallback(Load);
	serialization->SetRevertCallback(Revert);
	return true;
}

void RefreshPlayerArmor() { RefreshPlayerArmorImpl(); }

bool SetArmorAppearance(RE::TESForm* original, RE::TESForm* appearance)
{
	return SetArmorAppearanceNative(nullptr, original, appearance);
}

bool ClearArmorAppearance(RE::TESForm* original)
{
	return ClearArmorAppearanceNative(nullptr, original);
}

bool PreviewArmorAppearance(RE::TESForm* originalForm, RE::TESForm* appearanceForm)
{
	auto* original = originalForm ? originalForm->As<RE::TESObjectARMO>() : nullptr;
	auto* appearance = appearanceForm ? appearanceForm->As<RE::TESObjectARMO>() : nullptr;
	if (!original || !appearance || !original->GetPlayable() || !appearance->GetPlayable() ||
	    !original->GetSlotMask() || original->GetSlotMask() != appearance->GetSlotMask()) return false;
	{
		std::lock_guard lock(overridesMutex);
		previews.insert_or_assign(original->GetFormID(), appearance->GetFormID());
	}
	RefreshPlayerArmorImpl();
	return true;
}

bool CancelArmorPreview(RE::TESForm* originalForm)
{
	auto* original = originalForm ? originalForm->As<RE::TESObjectARMO>() : nullptr;
	if (!original) return false;
	bool removed{};
	{
		std::lock_guard lock(overridesMutex);
		removed = previews.erase(original->GetFormID()) != 0;
	}
	if (removed) RefreshPlayerArmorImpl();
	return removed;
}

void ClearAllArmorAppearances()
{
	std::lock_guard lock(overridesMutex);
	overrides.clear();
	previews.clear();
}

bool RegisterArmorAppearanceFunctions(RE::BSScript::IVirtualMachine* vm)
{
	vm->RegisterFunction("SetArmorAppearance", "JGJGTransmogNative", SetArmorAppearanceNative);
	vm->RegisterFunction("ClearArmorAppearance", "JGJGTransmogNative", ClearArmorAppearanceNative);
	vm->RegisterFunction("PreviewArmorAppearance", "JGJGTransmogNative", [](RE::StaticFunctionTag*, RE::TESForm* original, RE::TESForm* appearance) { return PreviewArmorAppearance(original, appearance); });
	vm->RegisterFunction("CancelArmorPreview", "JGJGTransmogNative", [](RE::StaticFunctionTag*, RE::TESForm* original) { return CancelArmorPreview(original); });
	return true;
}
}  // namespace jgjg::engine
