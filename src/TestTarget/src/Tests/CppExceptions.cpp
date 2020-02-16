#include "Tests/CppExceptions.h"

namespace CovCane::Tests {

template<typename T> T TestTypeException(const T&& initial, const T&& passBy)
{
    T state = initial;
    try
    {
        throw passBy;
    }
    catch (const T& val)
    {
        state = val;
    }
    return state;
}

template<typename T> int TestException(const T&& initial, const T&& passBy)
{
    auto res = TestTypeException<T>(std::move(initial), std::move(passBy));
    if (res != passBy)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int TestCppExceptionPrimitiveInt::Run() const
{
    return TestException<int>(0xBADF00D, 0xDEADBEEF);
}

#define E_PI 3.14159265358979323846264338327950

int TestCppExceptionPrimitiveFloat::Run() const
{
    return TestException<float>(0.3f, float(E_PI));
}

int TestCppExceptionPrimitiveDouble::Run() const
{
    return TestException<double>(0.1666, double(E_PI));
}

} // namespace CovCane::Tests
