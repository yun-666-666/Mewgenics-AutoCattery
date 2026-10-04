"""Reuse native effective stats only during immutable partner selection.

Current EXE D2850 calls C1820 at D2981 with the same all-minus-one context,
include-passives flag and zero final flag as NativeCatReference.effective_stats.
Other callers and all physiological writes continue through the native path.
"""

from contextlib import contextmanager
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RSP


@contextmanager
def mating_stat_cache(native, cats):
    if not getattr(native, "mating_cache_enabled", True):
        yield
        return
    values = {cat: struct.pack("<7i", *native.effective_stats(cat)) for cat in set(cats)}

    def cached(uc, address, size, data):
        stack = uc.reg_read(UC_X86_REG_RSP)
        caller, = struct.unpack("<Q", uc.mem_read(stack, 8))
        cat = uc.reg_read(UC_X86_REG_RCX)
        if caller == native.base + 0xD2986 and cat in values:
            output = uc.reg_read(UC_X86_REG_RDX)
            uc.mem_write(output, values[cat])
            native._return(output)

    hook = native.uc.hook_add(UC_HOOK_CODE, cached,
                              begin=native.base + 0xC1820, end=native.base + 0xC1820)
    try:
        yield
    finally:
        native.uc.hook_del(hook)
