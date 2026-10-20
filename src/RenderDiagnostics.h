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
}
