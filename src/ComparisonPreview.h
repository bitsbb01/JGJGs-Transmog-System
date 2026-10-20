#pragma once

#include <RE/Skyrim.h>

namespace jgjg::engine {

bool InstallComparisonPreviewListener();
bool StartSideBySideComparison(RE::TESForm* original, RE::TESForm* appearance);
void EndSideBySideComparison();

}  // namespace jgjg::engine
