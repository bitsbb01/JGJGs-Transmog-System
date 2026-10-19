#include "jgjg/Core.hpp"
#include <iostream>
#include <limits>
#include <chrono>
using namespace jgjg;
#define CHECK(x) do { if (!(x)) throw std::runtime_error("Failed: " #x); } while (false)
Item item(unsigned id, std::string name, Kind kind = Kind::sword, unsigned slots = 0) {
    return {{"Skyrim.esm", id}, std::move(name), kind, slots};
}
int main() {
    auto sword = item(1, "Iron sword"), axe = item(2, "Steel axe", Kind::waraxe);
    CHECK(!compatible(sword, axe));
    CHECK(!compatible(item(1,"Battleaxe",Kind::battleaxe), item(2,"Warhammer",Kind::warhammer)));
    CHECK(!compatible(item(1,"Bow",Kind::bow), item(2,"Crossbow",Kind::crossbow)));
    CHECK(compatible(item(1,"Iron cuirass",Kind::armor,4), item(2,"Robe",Kind::armor,4)));
    CHECK(!compatible(item(1,"Iron cuirass",Kind::armor,4), item(2,"Robe and boots",Kind::armor,132)));
    auto hidden = item(3,"Hidden"); hidden.playable = false;
    auto badRace = item(4,"Race mismatch"); badRace.validForPlayer = false;
    Catalogue c; c.rebuild({sword, axe, hidden, badRace, item(5,"Daedric sword")});
    CHECK(c.size() == 3);
    auto results = c.query(sword, "SWORD", "skyrim.esm", 0);
    CHECK(results.total == 2 && results.keys.front().localID == 5);
    CHECK(c.query(sword,"missing","",0).keys.empty());
    auto p = plan(c, {{"hand:right",sword}}, {{"hand:right",{"Skyrim.esm",5}}, {"hand:left",axe.key}});
    CHECK(p.changes.size() == 1 && p.skipped.size() == 1);
    AppearanceState looks;
    CHECK(looks.resolve("hand:right", sword.key) == nullptr);
    CHECK(looks.commit(p));
    auto* selectedLook = looks.resolve("hand:right", sword.key);
    CHECK(selectedLook && selectedLook->localID == 5);
    // The real equipped form remains authoritative; changing gear stops the
    // old slot appearance from being applied to a different item.
    CHECK(looks.resolve("hand:right", axe.key) == nullptr);
    Plan invalid = p;
    invalid.changes.push_back(invalid.changes.front());
    CHECK(!looks.commit(invalid));
    CHECK(looks.resolve("hand:right", sword.key) && looks.clear("hand:right"));
    CHECK(looks.resolve("hand:right", sword.key) == nullptr);
    struct Backend { int calls{}; bool succeeds{true}; bool commit(const Plan&) { ++calls; return succeeds; } } backend;
    Selection selection; selection.stage(p);
    CHECK(backend.calls == 0);
    CHECK(!selection.apply(std::vector<Equipped>{{"hand:right",axe}}, backend));
    CHECK(backend.calls == 0 && selection.pending());
    backend.succeeds = false;
    CHECK(!selection.apply(std::vector<Equipped>{{"hand:right",sword}}, backend));
    CHECK(selection.pending());
    backend.succeeds = true;
    CHECK(selection.apply(std::vector<Equipped>{{"hand:right",sword}}, backend));
    CHECK(!selection.pending());
    selection.stage(p); selection.cancel(); CHECK(!selection.pending());
    Presets presets; CHECK(presets.get(0).name == "Favourite 1");
    presets.save(0, {"Dragon outfit", {{"hand:right", sword.key}}});
    CHECK(presets.get(0).bindings.size() == 1 && presets.get(1).bindings.empty());
    bool caught = false;
    try { presets.save(0, {"Bad", {{"hand:right", sword.key},{"hand:right",axe.key}}}); } catch (const std::invalid_argument&) { caught = true; }
    CHECK(caught && presets.get(0).name == "Dragon outfit");
    std::vector<Item> large; for (unsigned i = 0; i < 100000; ++i) large.push_back(item(i, "Sword " + std::to_string(i)));
    auto start = std::chrono::steady_clock::now(); c.rebuild(std::move(large));
    auto page = c.query(sword, "", "", std::numeric_limits<std::size_t>::max(), 500);
    CHECK(page.total == 100000 && page.keys.size() == 50 && page.page == 1999);
    std::cout << "Core checks passed; 100,000 synthetic items built and queried in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()
              << " ms (not an in-game benchmark).\n";
}
