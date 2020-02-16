#pragma once

#include "Test.h"

namespace CovCane::Tests {

class TestCppExceptionPrimitiveInt final : public Test
{
public:
    int Run() const override;
};

class TestCppExceptionPrimitiveFloat final : public Test
{
public:
    int Run() const override;
};

class TestCppExceptionPrimitiveDouble final : public Test
{
public:
    int Run() const override;
};

class TestCppExceptionClass final : public Test
{
public:
    int Run() const override;
};

} // namespace CovCane::Tests