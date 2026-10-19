#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace jgjg {
enum class Kind { sword, dagger, waraxe, mace, greatsword, battleaxe, warhammer, bow, crossbow, staff, armor, shield };
struct Key {
    std::string plugin;
    std::uint32_t localID{};
    auto operator<=>(const Key&) const = default;
};
struct Item {
    Key key;
    std::string name;
    Kind kind{};
    std::uint32_t slots{};
    bool playable{true};
    bool hasModel{true};
    bool validForPlayer{true};
};
inline bool compatible(const Item& real, const Item& skin) {
    if (!skin.playable || !skin.hasModel || !skin.validForPlayer || real.kind != skin.kind) return false;
    if (real.kind == Kind::armor || real.kind == Kind::shield)
        return real.slots != 0 && real.slots == skin.slots;
    return true;
}
inline std::string folded(std::string s) {
    // Deterministic ASCII case folding; non-ASCII UTF-8 bytes remain intact.
    for (auto& c : s) if (static_cast<unsigned char>(c) < 128)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
struct Results { std::vector<Key> keys; std::size_t total{}; std::size_t page{}; };
class Catalogue {
    std::map<Key, Item> items_;
    std::map<Kind, std::vector<Key>> byKind_;
public:
    void rebuild(std::vector<Item> input) {
        items_.clear(); byKind_.clear();
        for (auto& item : input) {
            if (item.name.empty() || !item.playable || !item.hasModel || !item.validForPlayer) continue;
            items_.insert_or_assign(item.key, item);
        }
        for (auto& [key, item] : items_) byKind_[item.kind].push_back(key);
        for (auto& [kind, keys] : byKind_) std::sort(keys.begin(), keys.end(), [&](const Key& a, const Key& b) {
            auto an = folded(items_.at(a).name), bn = folded(items_.at(b).name);
            return an == bn ? a < b : an < bn;
        });
    }
    const Item* find(const Key& key) const {
        auto i = items_.find(key); return i == items_.end() ? nullptr : &i->second;
    }
    std::size_t size() const { return items_.size(); }
    Results query(const Item& target, std::string search, std::string plugin,
                  std::size_t page, std::size_t pageSize = 25) const {
        pageSize = std::clamp<std::size_t>(pageSize, 1, 50);
        search = folded(search); plugin = folded(plugin);
        std::vector<Key> matches;
        auto group = byKind_.find(target.kind);
        if (group == byKind_.end()) return {};
        for (auto& key : group->second) {
            auto& skin = items_.at(key);
            if (compatible(target, skin) && (plugin.empty() || folded(key.plugin) == plugin) &&
                (search.empty() || folded(skin.name).find(search) != std::string::npos)) matches.push_back(key);
        }
        Results result;
        result.total = matches.size();
        result.page = matches.empty() ? 0 : std::min(page, (matches.size() - 1) / pageSize);
        auto start = result.page * pageSize;
        for (auto i = start; i < std::min(start + pageSize, matches.size()); ++i) result.keys.push_back(matches[i]);
        return result;
    }
};
// Binding locations are equipment positions, not source inventory instances.
// Weapons: hand:left / hand:right. Armour: armor:<full slot-mask>.
struct Binding { std::string location; Key skin; };
struct Equipped { std::string location; Item item; };
struct Change { std::string location; Key expectedOriginal; Key appearance; };
struct Plan { std::vector<Change> changes; std::vector<std::string> skipped; };
inline Plan plan(const Catalogue& catalogue, const std::vector<Equipped>& equipped,
                 const std::vector<Binding>& preset) {
    Plan result;
    std::map<std::string, bool> seen;
    for (auto& binding : preset) {
        if (seen.contains(binding.location)) throw std::invalid_argument("Duplicate preset location");
        seen[binding.location] = true;
        auto slot = std::find_if(equipped.begin(), equipped.end(), [&](const Equipped& e) { return e.location == binding.location; });
        auto* skin = catalogue.find(binding.skin);
        if (slot == equipped.end() || !skin || !compatible(slot->item, *skin)) {
            result.skipped.push_back(binding.location); continue;
        }
        result.changes.push_back({binding.location, slot->item.key, skin->key});
    }
    return result;
}
// Visual overrides are keyed by the currently occupied equipment location and
// retain the real form key. They are independent of inventory ownership,
// enchantments, tempering, charge, and item statistics.
struct VisualOverride { Key original; Key appearance; };
class AppearanceState {
    std::map<std::string, VisualOverride> active_;
public:
    bool commit(const Plan& plan) {
        auto next = active_;
        std::map<std::string, bool> seen;
        for (const auto& change : plan.changes) {
            if (change.location.empty() || change.expectedOriginal.plugin.empty() ||
                change.appearance.plugin.empty() || seen.contains(change.location)) return false;
            seen[change.location] = true;
            next.insert_or_assign(change.location, VisualOverride{change.expectedOriginal, change.appearance});
        }
        active_.swap(next);
        return true;
    }
    const Key* resolve(std::string_view location, const Key& equippedOriginal) const {
        auto found = active_.find(std::string(location));
        if (found == active_.end() || found->second.original != equippedOriginal) return nullptr;
        return &found->second.appearance;
    }
    bool clear(std::string_view location) { return active_.erase(std::string(location)) != 0; }
    void reset() { active_.clear(); }
    const std::map<std::string, VisualOverride>& entries() const { return active_; }
};
// A preview is only a plan. It never changes equipment or the applied map.
class Selection {
    std::optional<Plan> pending_;
public:
    void stage(Plan value) { pending_ = std::move(value); }
    const std::optional<Plan>& pending() const { return pending_; }
    void cancel() { pending_.reset(); }
    template<class Backend> bool apply(const std::vector<Equipped>& now, Backend& backend) {
        if (!pending_) return false;
        for (auto& change : pending_->changes) {
            auto slot = std::find_if(now.begin(), now.end(), [&](const Equipped& e) { return e.location == change.location; });
            if (slot == now.end() || slot->item.key != change.expectedOriginal) return false;
        }
        // Backend must commit the complete visual override map atomically, or fail unchanged.
        if (!backend.commit(*pending_)) return false;
        pending_.reset(); return true;
    }
};
struct Preset { std::string name; std::vector<Binding> bindings; };
class Presets {
    std::vector<Preset> slots_{10};
public:
    Presets() { for (std::size_t i = 0; i < slots_.size(); ++i) slots_[i].name = "Favourite " + std::to_string(i + 1); }
    const Preset& get(std::size_t slot) const { return slots_.at(slot); }
    void save(std::size_t slot, Preset value) {
        if (value.name.empty() || value.name.size() > 128 || value.bindings.size() > 34) throw std::invalid_argument("Invalid preset");
        std::map<std::string, bool> seen;
        for (auto& b : value.bindings) {
            if (b.location.empty() || b.skin.plugin.empty() || seen.contains(b.location)) throw std::invalid_argument("Invalid binding");
            seen[b.location] = true;
        }
        slots_.at(slot) = std::move(value);
    }
};
} // namespace jgjg
