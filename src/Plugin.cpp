// Read-only native catalogue prototype. No engine hooks or inventory writes.
#include "jgjg/Core.hpp"
#include <mutex>
#include <thread>
#include <memory>
#include <atomic>
#include <spdlog/sinks/basic_file_sink.h>

namespace {
std::mutex gate;
std::shared_ptr<const jgjg::Catalogue> catalogue;
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
            std::vector<jgjg::Item> snapshot;
            for (auto* f : data->GetFormArray<RE::TESObjectWEAP>()) if (auto item = describe(f, player)) snapshot.push_back(std::move(*item));
            for (auto* f : data->GetFormArray<RE::TESObjectARMO>()) if (auto item = describe(f, player)) snapshot.push_back(std::move(*item));
            // Only detached, copied values cross threads. Never touch game forms on this worker.
            std::thread([epoch, input = std::move(snapshot)]() mutable {
                try {
                    auto result = std::make_shared<jgjg::Catalogue>(); result->rebuild(std::move(input));
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
    return catalogue ? static_cast<std::int32_t>(catalogue->size()) : 0;
}
bool IsBuilding(RE::StaticFunctionTag*) { return building.load(); }
std::string Status(RE::StaticFunctionTag*) {
    return "Development catalogue only: appearance application and 3D preview are not implemented.";
}
std::vector<std::string> QueryNames(RE::StaticFunctionTag*, RE::TESForm* target, std::string search, std::string plugin, std::int32_t page) {
    // Ordinary Papyrus native call: do not register this function as tasklet-callable.
    auto original = describe(target, RE::PlayerCharacter::GetSingleton());
    if (!original) return {};
    std::shared_ptr<const jgjg::Catalogue> current;
    { std::lock_guard lock(gate); current = catalogue; }
    if (!current) return {};
    auto found = current->query(*original, std::move(search), std::move(plugin), static_cast<std::size_t>(std::max(0, page)));
    std::vector<std::string> names;
    for (auto& key : found.keys) if (auto* item = current->find(key)) names.push_back(item->name + " [" + key.plugin + "]");
    return names;
}
bool Register(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("RequestCatalogue", "JGJGTransmogNative", RequestCatalogue);
    vm->RegisterFunction("CatalogueCount", "JGJGTransmogNative", CatalogueCount);
    vm->RegisterFunction("IsBuilding", "JGJGTransmogNative", IsBuilding);
    vm->RegisterFunction("Status", "JGJGTransmogNative", Status);
    vm->RegisterFunction("QueryNames", "JGJGTransmogNative", QueryNames);
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
    SKSE::log::info("JGJGs Transmog System 0.1.0: read-only development catalogue, Steam 1.6.1170");
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kPreLoadGame || message->type == SKSE::MessagingInterface::kNewGame) {
            std::lock_guard lock(gate); ++generation; catalogue.reset();
        }
    });
    return SKSE::GetPapyrusInterface()->Register(Register);
}
