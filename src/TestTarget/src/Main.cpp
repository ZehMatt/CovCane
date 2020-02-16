#include <iostream>
#include <vector>
#include <chrono>

#include "Tests/CppExceptions.h"
#include "Tests/LongJmp.h"

namespace CovCane::Tests {

using TestCase = std::pair<const char*, std::unique_ptr<Test>>;
using TestCases = std::vector<TestCase>;

template<typename F> double Measure(const F&& f)
{
    auto start = std::chrono::high_resolution_clock::now();
    f();
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
                   end - start)
                   .count();
    return (double)dur / 1000000.0;
}

TestCases LoadTests()
{
    TestCases res;

#define ADD_TEST(T) res.emplace_back(#T, std::make_unique<T>())
    {
        ADD_TEST(TestCppExceptionPrimitiveInt);
        ADD_TEST(TestCppExceptionPrimitiveFloat);
        ADD_TEST(TestCppExceptionPrimitiveDouble);
        ADD_TEST(TestLongJmp);
    }
#undef ADD_TEST

    return res;
}

int RunTest(const TestCase& test)
{
    printf("Test '%s' ...\n", test.first);

    int res;
    double elapsed = Measure([&]() { res = test.second->Run(); });
    if (res == EXIT_FAILURE)
        printf(".... Fail: %08X\n", res);
    else
        printf(".... OK, Elapsed in: %.08fs\n", elapsed);

    return res;
}

int RunTests()
{
    int res = EXIT_SUCCESS;
    int failed = 0;
    int succeded = 0;

    auto testCases = CovCane::Tests::LoadTests();
    for (auto&& test : testCases)
    {
        if (RunTest(test) != EXIT_SUCCESS)
        {
            res = EXIT_FAILURE;
            failed++;
        }
        else
        {
            succeded++;
        }
    }

    return res;
}

} // namespace CovCane::Tests

int main()
{
    return CovCane::Tests::RunTests();
}