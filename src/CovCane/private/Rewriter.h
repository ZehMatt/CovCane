#pragma once

#include <stdint.h>

namespace CovCane::Rewriter {

void Initialize();

bool CreateSectionBuffer(uintptr_t startVA, uintptr_t endVA);

// Rewrites the branch from source VA and results the new address
// with the rewritten code.
uintptr_t ProcessBranch(uintptr_t sourceVA);

} // namespace CovCane::Rewriter