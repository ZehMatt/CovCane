#include "Tests/LongJmp.h"
#include <csetjmp>

namespace CovCane::Tests {

int TestLongJmp::Run() const
{
    jmp_buf ctx{};
    int state;
    int state2;

    state = 0xBADF00D;

    state2 = setjmp(ctx);
    if (state2 == 0)
    {
        state = 0xDEADBEEF;
        longjmp(ctx, 0xBAD);
    }
    else if (state2 == 0xBAD)
    {
        state = 0xBADF00D;
    }

    if (state != 0xBADF00D)
        return EXIT_FAILURE;
    if (state2 != 0xBAD)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

} // namespace CovCane::Tests
