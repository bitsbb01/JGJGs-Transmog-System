#include "ArmorAppearance.h"
#include "WeaponAppearance.h"
#include "RenderDiagnostics.h"

#include <RE/Skyrim.h>
#include <RE/I/InventoryChanges.h>
#include <SKSE/SKSE.h>
#include <safetyhook.hpp>

#include <map>
#include <mutex>
#include <memory>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace jgjg::engine {
namespace {
	constexpr REL::RelocationID kApplyArmorAddon{ 17392, 17792 };

	std::map<RE::FormID, RE::FormID> overrides;
	std::map<RE::FormID, RE::FormID> previews;
	std::mutex overridesMutex;
	SafetyHookInline applyArmorAddonHook;
	thread_local bool refreshingPlayerAppearance{};
	std::atomic<bool> pendingEquippedArmorRebuild{};
	constexpr std::uint32_t kSerializationUID = 0x4A475453;  // JGTS
	constexpr std::uint32_t kAppearanceRecord = 0x41524D4F;  // ARMO
	constexpr std::uint32_t kPresetRecord = 0x50525354;       // PRST
	constexpr std::uint32_t kPresetBindingRecord = 0x50524244; // PRBD
	constexpr std::uint32_t kWeaponRecord = 0x5745504E; // WEPN
	constexpr std::uint32_t kWeaponPresetBindingRecord = 0x57504244; // WPBD
	struct AppearanceRecord { RE::FormID original; RE::FormID appearance; };
	using WeaponRecord = AppearanceRecord;
	struct PresetHeaderRecord { std::uint32_t slot; char name[64]; };
	struct PresetBindingRecord { std::uint32_t slot; RE::FormID original; RE::FormID appearance; };
	using WeaponPresetBindingRecord = PresetBindingRecord;
	struct ArmorPreset {
		std::string name;
		std::map<RE::FormID, RE::FormID> appearances;
		WeaponAppearanceMap weapons;
	};
	constexpr std::size_t kPresetCount = 10;
	std::array<ArmorPreset, kPresetCount> presets;
	std::array<std::uint32_t, kPresetCount> presetInvalidCounts{};
	std::uint32_t lastPresetSkipped{};

	std::string DefaultPresetName(std::size_t slot)
	{
		return "Favorite " + std::to_string(slot + 1);
	}

	bool HasPlayerArmorModel(const RE::TESObjectARMO* armor)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!armor || !player || !player->GetActorBase() || !player->GetRace()) return false;
		const auto sex = static_cast<unsigned>(player->GetActorBase()->GetSex());
		if (sex > 1) return false;
		for (auto* addon : armor->armorAddons) {
			if (!addon || !addon->IsValidRace(player->GetRace())) continue;
			const auto* model = addon->bipedModels[sex].GetModel();
			if (model && *model) return true;
		}
		return false;
	}

	bool IsWornArmorForm(RE::FormID formID)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* changes = player ? player->GetInventoryChanges() : nullptr;
		if (!changes || !changes->entryList) return false;
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || entry->object->GetFormID() != formID || !entry->extraLists) continue;
			for (auto* extra : *entry->extraLists) {
				if (extra && (extra->HasType(RE::ExtraDataType::kWorn) ||
				              extra->HasType(RE::ExtraDataType::kWornLeft))) return true;
			}
		}
		return false;
	}

	RE::FormID FindUniqueWornAppearanceForSlots(RE::TESObjectARMO* hookArmor)
	{
		if (!hookArmor) return 0;
		const auto slots = hookArmor->GetSlotMask();
		std::vector<RE::FormID> candidates;
		{
			std::lock_guard lock(overridesMutex);
			auto addCandidates = [&](const auto& mappings) {
				for (const auto& [sourceID, appearanceID] : mappings) {
					auto* source = RE::TESForm::LookupByID<RE::TESObjectARMO>(sourceID);
					if (source && source->GetSlotMask() == slots && IsWornArmorForm(sourceID))
						candidates.push_back(appearanceID);
				}
			};
			addCandidates(overrides);
			addCandidates(previews);
		}
		std::sort(candidates.begin(), candidates.end());
		candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
		return candidates.size() == 1 ? candidates.front() : 0;
	}

	RE::TESObjectARMO* ResolveOverride(RE::TESObjectARMO* original)
	{
		if (!original) return nullptr;
		diagnostics::armor.lastHookInput.store(original->GetFormID(), std::memory_order_relaxed);
		RE::FormID appearanceID{};
		{
			std::lock_guard lock(overridesMutex);
			auto findAppearance = [&](RE::FormID id) {
				if (const auto preview = previews.find(id); preview != previews.end()) {
					appearanceID = preview->second;
					return true;
				}
				if (const auto saved = overrides.find(id); saved != overrides.end()) {
					appearanceID = saved->second;
					return true;
				}
				return false;
			};
			// Some equipped armor is represented by a runtime/template form while
			// the MCM stores the stable inventory ARMO form. Resolve both identities.
			if (!findAppearance(original->GetFormID()) &&
			    original->templateArmor && !findAppearance(original->templateArmor->GetFormID()))
				appearanceID = 0;
		}
		// Some runtime armor proxies have neither the saved item's ID nor a
		// template link. In that case only use a mapping when exactly one
		// currently worn source has this exact biped slot mask.
		if (!appearanceID) appearanceID = FindUniqueWornAppearanceForSlots(original);
		if (!appearanceID) return nullptr;
		diagnostics::armor.mapMatches.fetch_add(1, std::memory_order_relaxed);
		auto* appearance = RE::TESForm::LookupByID<RE::TESObjectARMO>(appearanceID);
		if (!appearance || !appearance->GetPlayable() ||
		    appearance->GetSlotMask() != original->GetSlotMask() ||
		    !HasPlayerArmorModel(appearance)) return nullptr;
		diagnostics::armor.validAppearances.fetch_add(1, std::memory_order_relaxed);
		return appearance;
	}

	bool ApplyArmorAddonHook(RE::TESObjectARMO* armor, RE::TESRace* race,
	                         void* model, bool female)
	{
		if (refreshingPlayerAppearance)
			diagnostics::armor.hookCalls.fetch_add(1, std::memory_order_relaxed);
		if (armor && refreshingPlayerAppearance) {
			if (auto* appearance = ResolveOverride(armor)) {
				diagnostics::armor.replacements.fetch_add(1, std::memory_order_relaxed);
				armor = appearance;
			}
		}
		return applyArmorAddonHook.call<bool>(armor, race, model, female);
	}

	void RefreshPlayerArmorImpl()
	{
		diagnostics::armor.refreshRequests.fetch_add(1, std::memory_order_relaxed);
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) return;
		auto* process = player->GetActorRuntimeData().currentProcess;
		if (!process) return;
		using SetFlagFn = void (*)(RE::AIProcess*, std::uint8_t);
		using UpdateEquipmentFn = void (*)(RE::AIProcess*, RE::Actor*);
		static REL::Relocation<SetFlagFn> setFlag{ REL::RelocationID(38867, 39907) };
		static REL::Relocation<UpdateEquipmentFn> updateEquipment{ REL::RelocationID(38404, 39395) };
		setFlag(process, static_cast<std::uint8_t>(1u << 0));
		const bool wasRefreshing = refreshingPlayerAppearance;
		refreshingPlayerAppearance = true;
		updateEquipment(process, player);
		// QueueNiNodeUpdate is asynchronous and can be deferred while the MCM
		// pauses the game. Rebuild the player's 3D here, while the player-only
		// armor hook context is active, so armor and weapon model hooks can see
		// the mappings written by Apply before the menu returns control.
		player->DoReset3D(false);
		refreshingPlayerAppearance = wasRefreshing;
		diagnostics::armor.equipmentUpdates.fetch_add(1, std::memory_order_relaxed);
	}

	void RefreshPlayerArmorForAppearanceChange()
	{
		// The Journal menu can defer the player's worn-mesh rebuild. Re-equip the
		// exact same worn armor instance after the menu closes to refresh it.
		pendingEquippedArmorRebuild.store(true, std::memory_order_release);
		RefreshPlayerArmorImpl();
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
		    !HasPlayerArmorModel(appearance) ||
		    original->GetSlotMask() != appearance->GetSlotMask()) return false;
		diagnostics::ResetForAppearanceOperation();
		{
			std::lock_guard lock(overridesMutex);
			overrides.insert_or_assign(original->GetFormID(), appearance->GetFormID());
			previews.erase(original->GetFormID());
		}
		diagnostics::armor.lastSetOriginal.store(original->GetFormID(), std::memory_order_relaxed);
		diagnostics::armor.lastSetAppearance.store(appearance->GetFormID(), std::memory_order_relaxed);
		RefreshPlayerArmorForAppearanceChange();
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
			removed = previews.erase(original->GetFormID()) != 0 || removed;
		}
		if (removed) {
			diagnostics::ResetForAppearanceOperation();
		RefreshPlayerArmorForAppearanceChange();
		}
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

	bool SetWeaponAppearanceNative(RE::StaticFunctionTag*, RE::TESForm* original, RE::TESForm* appearance)
	{
		return SetWeaponAppearance(original, appearance);
	}

	bool ClearWeaponAppearanceNative(RE::StaticFunctionTag*, RE::TESForm* original)
	{
		return ClearWeaponAppearance(original);
	}

	bool PreviewWeaponAppearanceNative(RE::StaticFunctionTag*, RE::TESForm* original, RE::TESForm* appearance)
	{
		return PreviewWeaponAppearance(original, appearance);
	}

	bool CancelWeaponPreviewNative(RE::StaticFunctionTag*, RE::TESForm* original)
	{
		return CancelWeaponPreview(original);
	}

	bool SaveArmorPresetNative(RE::StaticFunctionTag*, std::int32_t slot)
	{
		if (slot < 0 || static_cast<std::size_t>(slot) >= kPresetCount) return false;
		std::lock_guard lock(overridesMutex);
		presets[slot].name = DefaultPresetName(static_cast<std::size_t>(slot));
		presets[slot].appearances = overrides;
		presets[slot].weapons = GetWeaponAppearanceOverrides();
		presetInvalidCounts[slot] = 0;
		return true;
	}

	bool LoadArmorPresetNative(RE::StaticFunctionTag*, std::int32_t slot)
	{
		if (slot < 0 || static_cast<std::size_t>(slot) >= kPresetCount) return false;
		std::map<RE::FormID, RE::FormID> restored;
		WeaponAppearanceMap restoredWeapons;
		std::uint32_t skipped{};
		{
			std::lock_guard lock(overridesMutex);
			skipped = presetInvalidCounts[slot];
			for (const auto& [original, appearance] : presets[slot].appearances) {
				auto* originalForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(original);
				auto* appearanceForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(appearance);
				if (!originalForm || !appearanceForm || !originalForm->GetPlayable() ||
				    !appearanceForm->GetPlayable() ||
				    originalForm->GetSlotMask() != appearanceForm->GetSlotMask() ||
				    !HasPlayerArmorModel(appearanceForm)) {
					++skipped;
					continue;
				}
				restored.emplace(original, appearance);
			}
			for (const auto& [original, appearance] : presets[slot].weapons) {
				auto* originalForm = RE::TESForm::LookupByID<RE::TESObjectWEAP>(original);
				auto* appearanceForm = RE::TESForm::LookupByID<RE::TESObjectWEAP>(appearance);
				if (!AreCompatiblePlayerWeapons(originalForm, appearanceForm)) {
					++skipped;
					continue;
				}
				restoredWeapons.emplace(original, appearance);
			}
			overrides = std::move(restored);
			previews.clear();
			lastPresetSkipped = skipped;
		}
		ReplaceWeaponAppearanceOverrides(std::move(restoredWeapons));
		RefreshPlayerArmorForAppearanceChange();
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

	std::int32_t GetLastPresetSkippedNative(RE::StaticFunctionTag*)
	{
		std::lock_guard lock(overridesMutex);
		return static_cast<std::int32_t>(lastPresetSkipped);
	}

	void Save(SKSE::SerializationInterface* serialization)
	{
		std::lock_guard lock(overridesMutex);
		const auto weaponOverrides = GetWeaponAppearanceOverrides();
		for (const auto& [original, appearance] : overrides) {
			const AppearanceRecord record{ original, appearance };
			if (!serialization->WriteRecord(kAppearanceRecord, 1, record)) {
				SKSE::log::error("Failed to save armor appearance {:08X}", original);
			}
		}
		for (const auto& [original, appearance] : weaponOverrides) {
			const WeaponRecord record{ original, appearance };
			if (!serialization->WriteRecord(kWeaponRecord, 1, record))
				SKSE::log::error("Failed to save weapon appearance {:08X}", original);
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
			for (const auto& [original, appearance] : preset.weapons) {
				const WeaponPresetBindingRecord binding{ static_cast<std::uint32_t>(slot), original, appearance };
				if (!serialization->WriteRecord(kWeaponPresetBindingRecord, 1, binding))
					SKSE::log::error("Failed to save outfit favorite {} weapon binding {:08X}", slot + 1, original);
			}
		}
	}

	void Load(SKSE::SerializationInterface* serialization)
	{
		std::map<RE::FormID, RE::FormID> loaded;
		WeaponAppearanceMap loadedWeapons;
		std::array<ArmorPreset, kPresetCount> loadedPresets;
		std::array<std::uint32_t, kPresetCount> invalidBindings{};
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
						else
							++invalidBindings[binding.slot];
					} else {
						++invalidBindings[binding.slot];
					}
				}
				continue;
			}
			if (type == kWeaponPresetBindingRecord && version == 1 && length == sizeof(WeaponPresetBindingRecord)) {
				WeaponPresetBindingRecord binding{};
				if (serialization->ReadRecordData(binding) == sizeof(binding) && binding.slot < loadedPresets.size()) {
					RE::FormID original{}, appearance{};
					if (serialization->ResolveFormID(binding.original, original) &&
						serialization->ResolveFormID(binding.appearance, appearance) &&
						AreCompatiblePlayerWeapons(RE::TESForm::LookupByID<RE::TESObjectWEAP>(original),
							RE::TESForm::LookupByID<RE::TESObjectWEAP>(appearance)))
						loadedPresets[binding.slot].weapons.insert_or_assign(original, appearance);
					else
						++invalidBindings[binding.slot];
				}
				continue;
			}
			if (type == kWeaponRecord && version == 1 && length == sizeof(WeaponRecord)) {
				WeaponRecord record{};
				if (serialization->ReadRecordData(record) == sizeof(record)) {
					RE::FormID original{}, appearance{};
					if (serialization->ResolveFormID(record.original, original) &&
						serialization->ResolveFormID(record.appearance, appearance) &&
						AreCompatiblePlayerWeapons(RE::TESForm::LookupByID<RE::TESObjectWEAP>(original),
							RE::TESForm::LookupByID<RE::TESObjectWEAP>(appearance)))
						loadedWeapons.insert_or_assign(original, appearance);
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
			presetInvalidCounts = invalidBindings;
			previews.clear();
			lastPresetSkipped = 0;
		}
		ReplaceWeaponAppearanceOverrides(std::move(loadedWeapons));
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

void RebuildPendingPlayerArmor()
{
	if (!pendingEquippedArmorRebuild.load(std::memory_order_acquire)) return;
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* equipManager = RE::ActorEquipManager::GetSingleton();
	if (!player || !equipManager) return;
	auto* inventoryChanges = player->GetInventoryChanges();
	if (!inventoryChanges || !inventoryChanges->entryList) return;
	if (!pendingEquippedArmorRebuild.exchange(false, std::memory_order_acq_rel)) return;
	const bool wasRefreshing = refreshingPlayerAppearance;
	refreshingPlayerAppearance = true;
	for (auto* entry : *inventoryChanges->entryList) {
		auto* armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
		if (!armor || !entry) continue;
		{
			std::lock_guard lock(overridesMutex);
			if (!overrides.contains(armor->GetFormID()) && !previews.contains(armor->GetFormID())) continue;
		}

		RE::ExtraDataList* wornExtra{};
		if (entry->extraLists) {
			for (auto* extra : *entry->extraLists) {
				if (extra && (extra->HasType(RE::ExtraDataType::kWorn) ||
				              extra->HasType(RE::ExtraDataType::kWornLeft))) {
					wornExtra = extra;
					break;
				}
			}
		}
		// If we cannot identify the worn instance, leave it alone. This avoids
		// guessing which duplicate item carries its enchantments or other extras.
		auto* equipSlot = armor->GetEquipSlot();
		if (!wornExtra || !equipSlot) continue;
		if (equipManager->UnequipObject(player, armor, wornExtra, 1, equipSlot,
		                                true, false, false, true)) {
			equipManager->EquipObject(player, armor, wornExtra, 1, equipSlot,
			                         true, false, false, true);
		}
	}
	player->DoReset3D(false);
	refreshingPlayerAppearance = wasRefreshing;
}

std::string GetArmorRenderDiagnostics()
{
	const auto setOriginal = diagnostics::armor.lastSetOriginal.load(std::memory_order_relaxed);
	bool liveOverride{};
	{
		std::lock_guard lock(overridesMutex);
		liveOverride = setOriginal && (overrides.contains(setOriginal) || previews.contains(setOriginal));
	}
	return std::to_string(diagnostics::armor.refreshRequests.load(std::memory_order_relaxed)) + "/" +
	       std::to_string(diagnostics::armor.equipmentUpdates.load(std::memory_order_relaxed)) + "/" +
	       std::to_string(diagnostics::armor.hookCalls.load(std::memory_order_relaxed)) + "/" +
	       std::to_string(diagnostics::armor.mapMatches.load(std::memory_order_relaxed)) + "/" +
	       std::to_string(diagnostics::armor.validAppearances.load(std::memory_order_relaxed)) + "/" +
	       std::to_string(diagnostics::armor.replacements.load(std::memory_order_relaxed)) + "/" +
	       (liveOverride ? "1" : "0");
}

std::string GetArmorRenderFormIDs()
{
	std::ostringstream out;
	out << std::hex << std::uppercase << std::setfill('0')
	    << std::setw(8) << diagnostics::armor.lastSetOriginal.load(std::memory_order_relaxed) << ">"
	    << std::setw(8) << diagnostics::armor.lastSetAppearance.load(std::memory_order_relaxed) << " / "
	    << std::setw(8) << diagnostics::armor.lastHookInput.load(std::memory_order_relaxed);
	return out.str();
}

std::string GetArmorOverrideStatus()
{
	const auto setOriginal = diagnostics::armor.lastSetOriginal.load(std::memory_order_relaxed);
	if (!setOriginal) return "No recent Apply";
	std::lock_guard lock(overridesMutex);
	return overrides.contains(setOriginal) ? "Stored" : previews.contains(setOriginal) ? "Preview only" : "Cleared/missing";
}

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
	    !HasPlayerArmorModel(appearance) ||
	    original->GetSlotMask() != appearance->GetSlotMask()) return false;
	diagnostics::ResetForAppearanceOperation();
	{
		std::lock_guard lock(overridesMutex);
		previews.insert_or_assign(original->GetFormID(), appearance->GetFormID());
	}
	RefreshPlayerArmorForAppearanceChange();
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
	if (removed) {
		diagnostics::ResetForAppearanceOperation();
		RefreshPlayerArmorForAppearanceChange();
	}
	return removed;
}

void ClearAllArmorAppearances()
{
	{
		std::lock_guard lock(overridesMutex);
		overrides.clear();
		previews.clear();
		lastPresetSkipped = 0;
		presetInvalidCounts.fill(0);
		for (std::size_t slot = 0; slot < presets.size(); ++slot) {
			presets[slot].name = DefaultPresetName(slot);
			presets[slot].appearances.clear();
			presets[slot].weapons.clear();
		}
	}
	ReplaceWeaponAppearanceOverrides({});
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
	vm->RegisterFunction("GetLastPresetSkipped", "JGJGTransmogNative", GetLastPresetSkippedNative);
	vm->RegisterFunction("SetWeaponAppearance", "JGJGTransmogNative", SetWeaponAppearanceNative);
	vm->RegisterFunction("ClearWeaponAppearance", "JGJGTransmogNative", ClearWeaponAppearanceNative);
	vm->RegisterFunction("PreviewWeaponAppearance", "JGJGTransmogNative", PreviewWeaponAppearanceNative);
	vm->RegisterFunction("CancelWeaponPreview", "JGJGTransmogNative", CancelWeaponPreviewNative);
	return true;
}
}  // namespace jgjg::engine
