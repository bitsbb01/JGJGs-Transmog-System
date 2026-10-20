#pragma once

#include <atomic>
#include <cstdint>

namespace jgjg::engine::diagnostics {
	struct ArmorCounters {
		std::atomic<std::uint32_t> refreshRequests{};
		std::atomic<std::uint32_t> equipmentUpdates{};
		std::atomic<std::uint32_t> hookCalls{};
		std::atomic<std::uint32_t> replacements{};
	};
	struct WeaponCounters {
		std::atomic<std::uint32_t> hookCalls{};
		std::atomic<std::uint32_t> replacements{};
	};
	inline ArmorCounters armor;
	inline WeaponCounters weapon;
}
