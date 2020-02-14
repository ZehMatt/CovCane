#pragma once

#include <vector>
#include <asmjit/asmjit.h>

namespace CovCane {

class Runtime final : public asmjit::Target
{
    struct Buffer
    {
        uintptr_t base;
        uintptr_t end;
        uintptr_t cur;
    };

    std::vector<Buffer> _buffers;

public:
    Runtime() noexcept;
    virtual ~Runtime() noexcept = default;

    template<typename Func>
    inline asmjit::Error add(
        Func* dst, asmjit::CodeHolder* code, uintptr_t sourceVA) noexcept
    {
        return _add(
            asmjit::Support::ptr_cast_impl<void**, Func*>(dst), code, sourceVA);
    }

    template<typename Func> inline asmjit::Error release(Func p) noexcept
    {
        return _release(asmjit::Support::ptr_cast_impl<void*, Func>(p));
    }

    asmjit::Error _add(
        void** dst, asmjit::CodeHolder* code, uintptr_t sourceVA) noexcept;
    asmjit::Error _release(void* p) noexcept;

    void flush(const void* p, size_t size) noexcept;

    void* createBuffer(uintptr_t startVA, uintptr_t endVA);

private:
    void* alloc(size_t len, uintptr_t sourceVA);
};

} // namespace CovCane