#pragma once
#include "x64_emulator.hpp"

namespace cpu_context
{
    void save(x64_emulator& emu, CONTEXT64& context);
    void restore(x64_emulator& emu, const CONTEXT64& context);
}
