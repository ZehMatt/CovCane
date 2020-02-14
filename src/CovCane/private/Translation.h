#pragma once

#include <asmjit/asmjit.h>
#include <Zydis/Zydis.h>

namespace CovCane::Translation {

bool convertInstruction(
    const ZydisDecodedInstruction& instr, asmjit::x86::Assembler& cb);

}