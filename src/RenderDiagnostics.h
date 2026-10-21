#pragma once

#include <atomic>
#include <cstdint>

namespace jgjg::engine::diagnostics {
	struct ArmorCounters {
		std::atomic<std::uint32_t> refreshRequests{};
		std::atomic<std::uint32_t> equipmentUpdates{};
		std::atomic<std::uint32_t> hookCalls{};
		std::atomic<std::uint32_t> mapMatches{};
		std::atomic<std::uint32_t> validAppearances{};
		std::atomic<std::uint32_t> replacements{};
		std::atomic<std::uint32_t> lastSetOriginal{};
		std::atomic<std::uint32_t> lastSetAppearance{};
		std::atomic<std::uint32_t> lastHookInput{};
		std::atomic<std::uint32_t> lastHookTemplate{};
		std::atomic<std::uint32_t> lastHookSlots{};
	};
	struct WeaponCounters {
		std::atomic<std::uint32_t> hookCalls{};
		std::atomic<std::uint32_t> validSlots{};
		std::atomic<std::uint32_t> items{};
		std::atomic<std::uint32_t> weapons{};
		std::atomic<std::uint32_t> mapMatches{};
		std::atomic<std::uint32_t> compatible{};
		std::atomic<std::uint32_t> replacements{};
		std::atomic<std::uint32_t> lastSetOriginal{};
		std::atomic<std::uint32_t> lastSetAppearance{};
		std::atomic<std::uint32_t> lastSlotItem{};
		std::atomic<std::uint32_t> lastWeapon{};
	};
	inline ArmorCounters armor;
	inline WeaponCounters weapon;

	// Clear the previous operation's measurements immediately before a user
	// initiated Apply/Preview/Clear. Refreshes triggered during load keep their
	// normal counters and cannot interfere with an in-progress game startup.
	inline void ResetForAppearanceOperation()
	{
		armor.refreshRequests.store(0, std::memory_order_relaxed);
		armor.equipmentUpdates.store(0, std::memory_order_relaxed);
		armor.hookCalls.store(0, std::memory_order_relaxed);
		armor.mapMatches.store(0, std::memory_order_relaxed);
		armor.validAppearances.store(0, std::memory_order_relaxed);
		armor.replacements.store(0, std::memory_order_relaxed);
		armor.lastHookInput.store(0, std::memory_order_relaxed);
		armor.lastHookTemplate.store(0, std::memory_order_relaxed);
		armor.lastHookSlots.store(0, std::memory_order_relaxed);
		weapon.hookCalls.store(0, std::memory_order_relaxed);
		weapon.validSlots.store(0, std::memory_order_relaxed);
		weapon.items.store(0, std::memory_order_relaxed);
		weapon.weapons.store(0, std::memory_order_relaxed);
		weapon.mapMatches.store(0, std::memory_order_relaxed);
		weapon.compatible.store(0, std::memory_order_relaxed);
		weapon.replacements.store(0, std::memory_order_relaxed);
		weapon.lastSlotItem.store(0, std::memory_order_relaxed);
		weapon.lastWeapon.store(0, std::memory_order_relaxed);
	}
}
