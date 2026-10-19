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
#include <string>

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
	constexpr std::uint32_t kPresetRecord = 0x50525354;       // PRST
	constexpr std::uint32_t kPresetBindingRecord = 0x50524244; // PRBD
	struct AppearanceRecord { RE::FormID original; RE::FormID appearance; };
	struct PresetHeaderRecord { std::uint32_t slot; char name[64]; };
	struct PresetBindingRecord { std::uint32_t slot; RE::FormID original; RE::FormID appearance; };
	struct ArmorPreset { std::string name; std::map<RE::FormID, RE::FormID> appearances; };
	constexpr std::size_t kPresetCount = 10;
	std::array<ArmorPreset, kPresetCount> presets;

	std::string DefaultPresetName(std::size_t slot)
	{
		return "Favorite " + std::to_string(slot + 1);
	}

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

	bool PreviewArmorAppearanceNative(RE::StaticFunctionTag*, RE::TESForm* originalForm,
	                                  RE::TESForm* appearanceForm)
	{
		return PreviewArmorAppearance(originalForm, appearanceForm);
	}

	bool CancelArmorPreviewNative(RE::StaticFunctionTag*, RE::TESForm* originalForm)
	{
		return CancelArmorPreview(originalForm);
	}

	bool SaveArmorPresetNative(RE::StaticFunctionTag*, std::int32_t slot)
	{
		if (slot < 0 || static_cast<std::size_t>(slot) >= kPresetCount) return false;
		std::lock_guard lock(overridesMutex);
		presets[slot].name = DefaultPresetName(static_cast<std::size_t>(slot));
		presets[slot].appearances = overrides;
		return true;
	}

	bool LoadArmorPresetNative(RE::StaticFunctionTag*, std::int32_t slot)
	{
		if (slot < 0 || static_cast<std::size_t>(slot) >= kPresetCount) return false;
		std::map<RE::FormID, RE::FormID> restored;
		{
			std::lock_guard lock(overridesMutex);
			for (const auto& [original, appearance] : presets[slot].appearances) {
				auto* originalForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(original);
				auto* appearanceForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(appearance);
				if (!originalForm || !appearanceForm || originalForm->GetSlotMask() != appearanceForm->GetSlotMask()) return false;
				restored.emplace(original, appearance);
			}
			overrides = std::move(restored);
			previews.clear();
		}
		RefreshPlayerArmor();
		return true;
	}

	std::vector<std::string> GetArmorPresetNamesNative(RE::StaticFunctionTag*)
	{
		std::lock_guard lock(overridesMutex);
		std::vector<std::string> names;
		names.reserve(presets.size());
		for (std::size_t slot = 0; slot < presets.size(); ++slot)
			names.push_back(presets[slot].name.empty() ? DefaultPresetName(slot) : presets[slot].name);
		return names;
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
		for (std::size_t slot = 0; slot < presets.size(); ++slot) {
			const auto& preset = presets[slot];
			PresetHeaderRecord header{};
			header.slot = static_cast<std::uint32_t>(slot);
			const auto name = preset.name.empty() ? DefaultPresetName(slot) : preset.name;
			std::copy_n(name.data(), std::min(name.size(), sizeof(header.name) - 1), header.name);
			if (!serialization->WriteRecord(kPresetRecord, 1, header)) {
				SKSE::log::error("Failed to save armor favorite {}", slot + 1);
				continue;
			}
			for (const auto& [original, appearance] : preset.appearances) {
				const PresetBindingRecord binding{ static_cast<std::uint32_t>(slot), original, appearance };
				if (!serialization->WriteRecord(kPresetBindingRecord, 1, binding))
					SKSE::log::error("Failed to save armor favorite {} binding {:08X}", slot + 1, original);
			}
		}
	}

	void Load(SKSE::SerializationInterface* serialization)
	{
		std::map<RE::FormID, RE::FormID> loaded;
		std::array<ArmorPreset, kPresetCount> loadedPresets;
		for (std::size_t slot = 0; slot < loadedPresets.size(); ++slot)
			loadedPresets[slot].name = DefaultPresetName(slot);
		std::uint32_t type{}, version{}, length{};
		while (serialization->GetNextRecordInfo(type, version, length)) {
			if (type == kPresetRecord && version == 1 && length == sizeof(PresetHeaderRecord)) {
				PresetHeaderRecord header{};
				if (serialization->ReadRecordData(header) == sizeof(header) && header.slot < loadedPresets.size()) {
					header.name[sizeof(header.name) - 1] = '\0';
					if (header.name[0]) loadedPresets[header.slot].name = header.name;
				}
				continue;
			}
			if (type == kPresetBindingRecord && version == 1 && length == sizeof(PresetBindingRecord)) {
				PresetBindingRecord binding{};
				if (serialization->ReadRecordData(binding) == sizeof(binding) && binding.slot < loadedPresets.size()) {
					RE::FormID original{}, appearance{};
					if (serialization->ResolveFormID(binding.original, original) &&
						serialization->ResolveFormID(binding.appearance, appearance)) {
						auto* originalForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(original);
						auto* appearanceForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(appearance);
						if (originalForm && appearanceForm && originalForm->GetSlotMask() == appearanceForm->GetSlotMask())
							loadedPresets[binding.slot].appearances.insert_or_assign(original, appearance);
					}
				}
				continue;
			}
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
			presets = std::move(loadedPresets);
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
	    static_cast<std::uint32_t>(original->GetSlotMask()) == 0 ||
	    original->GetSlotMask() != appearance->GetSlotMask()) return false;
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
	for (std::size_t slot = 0; slot < presets.size(); ++slot) {
		presets[slot].name = DefaultPresetName(slot);
		presets[slot].appearances.clear();
	}
}

bool RegisterArmorAppearanceFunctions(RE::BSScript::IVirtualMachine* vm)
{
	vm->RegisterFunction("SetArmorAppearance", "JGJGTransmogNative", SetArmorAppearanceNative);
	vm->RegisterFunction("ClearArmorAppearance", "JGJGTransmogNative", ClearArmorAppearanceNative);
	vm->RegisterFunction("PreviewArmorAppearance", "JGJGTransmogNative", PreviewArmorAppearanceNative);
	vm->RegisterFunction("CancelArmorPreview", "JGJGTransmogNative", CancelArmorPreviewNative);
	vm->RegisterFunction("SaveArmorPreset", "JGJGTransmogNative", SaveArmorPresetNative);
	vm->RegisterFunction("LoadArmorPreset", "JGJGTransmogNative", LoadArmorPresetNative);
	vm->RegisterFunction("GetArmorPresetNames", "JGJGTransmogNative", GetArmorPresetNamesNative);
	return true;
}
}  // namespace jgjg::engine
