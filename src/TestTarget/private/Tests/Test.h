#pragma once

#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <utility>

namespace CovCane::Tests {

// Interface
class Test
{
public:
    virtual int Run() const = 0;
};

} // namespace CovCane::Tests
