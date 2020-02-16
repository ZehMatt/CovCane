#pragma once

#include "Test.h"

namespace CovCane::Tests {

class TestLongJmp final : public Test
{
public:
    int Run() const override;
};

} // namespace CovCane::Tests