// SKSE native catalogue and player-only visual appearance prototype.
#include "jgjg/Core.hpp"
#include "ArmorAppearance.h"
#include <mutex>
#include <thread>
#include <memory>
#include <atomic>
#include <spdlog/sinks/basic_file_sink.h>

namespace {
std::mutex gate;
struct Snapshot {
    jgjg::Catalogue catalogue;
    std::map<RE::FormID, jgjg::Item> items;
};
std::shared_ptr<const Snapshot> catalogue;
std::atomic<bool> building{false};
std::atomic<unsigned> generation{0};
std::optional<jgjg::Kind> weaponKind(RE::TESObjectWEAP* weapon) {
    using K = jgjg::Kind;
    switch (static_cast<unsigned>(weapon->GetWeaponType())) {
    case 1: return K::sword; case 2: return K::dagger; case 3: return K::waraxe;
    case 4: return K::mace; case 5: return K::greatsword;
    case 6: return weapon->HasKeywordString("WeapTypeWarhammer") ? K::warhammer : K::battleaxe;
    case 7: return K::bow; case 8: return K::staff; case 9: return K::crossbow;
    default: return std::nullopt;
    }
}
std::optional<jgjg::Item> describe(RE::TESForm* form, RE::PlayerCharacter* player) {
    if (!form || form->IsDynamicForm() || !form->GetFile(0)) return {};
    const char* name = form->GetName();
    if (!name || !*name) return {};
    jgjg::Item item;
    item.key = {std::string(form->GetFile(0)->GetFilename()), form->GetLocalFormID()};
    item.name = name;
    if (auto* weapon = form->As<RE::TESObjectWEAP>()) {
        auto kind = weaponKind(weapon); if (!kind) return {};
        item.kind = *kind;
        const char* model = weapon->GetModel();
        item.hasModel = model && *model;
        // TESForm::GetPlayable is virtual and covers the non-playable record flag.
        item.playable = weapon->GetPlayable();
    } else if (auto* armor = form->As<RE::TESObjectARMO>()) {
        item.slots = static_cast<std::uint32_t>(armor->GetSlotMask());
        item.kind = (item.slots & (1u << 9)) ? jgjg::Kind::shield : jgjg::Kind::armor;
        item.playable = armor->GetPlayable();
        item.hasModel = false; item.validForPlayer = false;
        if (!player || !player->GetActorBase()) return {};
        auto sex = static_cast<unsigned>(player->GetActorBase()->GetSex());
        if (sex > 1) return {};
        for (auto* addon : armor->armorAddons) {
            if (!addon || !addon->IsValidRace(player->GetRace())) continue;
            const char* model = addon->bipedModels[sex].GetModel();
            if (model && *model) { item.hasModel = true; item.validForPlayer = true; }
        }
    } else return {};
    return item;
}
bool RequestCatalogue(RE::StaticFunctionTag*) {
    if (building.exchange(true)) return false;
    const auto epoch = generation.load();
    SKSE::GetTaskInterface()->AddTask([epoch] {
        try {
            auto* data = RE::TESDataHandler::GetSingleton();
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!data || !player || !player->GetActorBase()) { building = false; return; }
            std::map<RE::FormID, jgjg::Item> snapshot;
            for (auto* f : data->GetFormArray<RE::TESObjectWEAP>()) if (auto item = describe(f, player)) snapshot.emplace(f->GetFormID(), std::move(*item));
            for (auto* f : data->GetFormArray<RE::TESObjectARMO>()) if (auto item = describe(f, player)) snapshot.emplace(f->GetFormID(), std::move(*item));
            // Only detached, copied values cross threads. Never touch game forms on this worker.
            std::thread([epoch, input = std::move(snapshot)]() mutable {
                try {
                    auto result = std::make_shared<Snapshot>();
                    result->items = std::move(input);
                    std::vector<jgjg::Item> entries; entries.reserve(result->items.size());
                    for (const auto& [id, item] : result->items) entries.push_back(item);
                    result->catalogue.rebuild(std::move(entries));
                    std::lock_guard lock(gate);
                    if (generation.load() == epoch) catalogue = std::move(result);
                } catch (...) { SKSE::log::error("Catalogue construction failed"); }
                building = false;
            }).detach();
        } catch (...) { building = false; SKSE::log::error("Catalogue snapshot failed"); }
    });
    return true;
}
std::int32_t CatalogueCount(RE::StaticFunctionTag*) {
    std::lock_guard lock(gate);
    return catalogue ? static_cast<std::int32_t>(catalogue->catalogue.size()) : 0;
}
bool IsBuilding(RE::StaticFunctionTag*) { return building.load(); }
std::string Status(RE::StaticFunctionTag*) {
	return "Player armor appearances with save support and temporary on-character preview. Side-by-side 3D viewer and weapon appearances are still in development.";
}
std::vector<std::string> QueryNames(RE::StaticFunctionTag*, RE::TESForm* target, std::string search, std::string plugin, std::int32_t page) {
    // Only immutable cached metadata is queried here. No player/model access
    // from the Papyrus VM thread. Form ID is stable while the VM holds its form.
    if (!target) return {};
    std::shared_ptr<const Snapshot> current;
    { std::lock_guard lock(gate); current = catalogue; }
    if (!current) return {};
    auto original = current->items.find(target->GetFormID());
    if (original == current->items.end()) return {};
    auto found = current->catalogue.query(original->second, std::move(search), std::move(plugin), static_cast<std::size_t>(std::max(0, page)));
    std::vector<std::string> names;
    for (auto& key : found.keys) if (auto* item = current->catalogue.find(key)) names.push_back(item->name + " [" + key.plugin + "]");
    return names;
}
std::vector<std::string> QueryPlugins(RE::StaticFunctionTag*, RE::TESForm* target) {
    if (!target) return {};
    std::shared_ptr<const Snapshot> current;
    { std::lock_guard lock(gate); current = catalogue; }
    if (!current) return {};
    auto original = current->items.find(target->GetFormID());
    if (original == current->items.end()) return {};
    std::map<std::string, std::string> sorted;
    for (const auto& [id, item] : current->items) {
        if (jgjg::compatible(original->second, item))
            sorted.try_emplace(jgjg::folded(item.key.plugin), item.key.plugin);
    }
    std::vector<std::string> names;
    names.reserve(sorted.size() + 1);
    names.push_back("All mods");
    for (auto& [foldedName, displayName] : sorted) names.push_back(displayName);
    return names;
}
std::int32_t QueryPageCount(RE::StaticFunctionTag*, RE::TESForm* target, std::string plugin) {
    if (!target) return 0;
    std::shared_ptr<const Snapshot> current;
    { std::lock_guard lock(gate); current = catalogue; }
    if (!current) return 0;
    auto original = current->items.find(target->GetFormID());
    if (original == current->items.end()) return 0;
    const auto total = current->catalogue.query(original->second, "", std::move(plugin), 0).total;
    return static_cast<std::int32_t>((total + 24) / 25);
}
std::vector<RE::TESForm*> GetPlayerWornArmors(RE::StaticFunctionTag*) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return {};
    std::map<RE::FormID, RE::TESObjectARMO*> worn;
    for (std::uint32_t bit = 0; bit < 32; ++bit) {
        auto* armor = player->GetWornArmor(
            static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(1u << bit), true);
        if (armor) worn.try_emplace(armor->GetFormID(), armor);
    }
    std::vector<RE::TESForm*> result;
    result.reserve(worn.size());
    for (const auto& [id, armor] : worn) result.push_back(armor);
    return result;
}
std::vector<std::string> GetPlayerWornArmorNames(RE::StaticFunctionTag*) {
    std::vector<std::string> names;
    for (auto* form : GetPlayerWornArmors(nullptr)) {
        const char* name = form ? form->GetName() : nullptr;
        names.emplace_back(name && *name ? name : "Unnamed armor");
    }
    return names;
}
std::vector<RE::TESForm*> QueryForms(RE::StaticFunctionTag*, RE::TESForm* target,
                                     std::string search, std::string plugin, std::int32_t page) {
    if (!target) return {};
    std::shared_ptr<const Snapshot> current;
    { std::lock_guard lock(gate); current = catalogue; }
    if (!current) return {};
    auto original = current->items.find(target->GetFormID());
    if (original == current->items.end()) return {};
    auto result = current->catalogue.query(original->second, std::move(search), std::move(plugin),
                                           static_cast<std::size_t>(std::max(0, page)));
    std::vector<RE::TESForm*> forms;
    forms.reserve(result.keys.size());
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return {};
    for (const auto& key : result.keys) {
        if (auto* form = data->LookupForm(key.localID, key.plugin)) forms.push_back(form);
    }
    return forms;
}
bool Register(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("RequestCatalogue", "JGJGTransmogNative", RequestCatalogue);
    vm->RegisterFunction("CatalogueCount", "JGJGTransmogNative", CatalogueCount);
    vm->RegisterFunction("IsBuilding", "JGJGTransmogNative", IsBuilding);
    vm->RegisterFunction("Status", "JGJGTransmogNative", Status);
    vm->RegisterFunction("QueryNames", "JGJGTransmogNative", QueryNames);
    vm->RegisterFunction("QueryPlugins", "JGJGTransmogNative", QueryPlugins);
    vm->RegisterFunction("QueryPageCount", "JGJGTransmogNative", QueryPageCount);
    vm->RegisterFunction("QueryForms", "JGJGTransmogNative", QueryForms);
    vm->RegisterFunction("GetPlayerWornArmors", "JGJGTransmogNative", GetPlayerWornArmors);
    vm->RegisterFunction("GetPlayerWornArmorNames", "JGJGTransmogNative", GetPlayerWornArmorNames);
    jgjg::engine::RegisterArmorAppearanceFunctions(vm);
    return true;
}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    // Check before initializing CommonLib or resolving any game address.
    if (skse->RuntimeVersion() != REL::Version{1,6,1170,0}) return false;
    SKSE::Init(skse);
    if (auto dir = SKSE::log::log_directory()) {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / "JGJGsTransmogSystem.log").string(), true);
        spdlog::set_default_logger(std::make_shared<spdlog::logger>("JGJG", sink));
        spdlog::flush_on(spdlog::level::info);
    }
    SKSE::log::info("JGJGs Transmog System 0.1.0: player armor appearance prototype, Steam 1.6.1170");
    if (!jgjg::engine::InstallArmorAppearanceHook()) return false;
    if (!jgjg::engine::InstallAppearanceSerialization()) return false;
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kPreLoadGame || message->type == SKSE::MessagingInterface::kNewGame) {
            std::lock_guard lock(gate); ++generation; catalogue.reset();
            jgjg::engine::ClearAllArmorAppearances();
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            jgjg::engine::RefreshPlayerArmor();
        }
    });
    return SKSE::GetPapyrusInterface()->Register(Register);
}
