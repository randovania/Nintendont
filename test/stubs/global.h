#include "../../kernel/global.h"
#include <stdio.h>

#undef read8
#define read8(addr) read8_stub(addr)
static inline u8 read8_stub(u32 addr)
{
	printf("read8_stub called with addr: 0x%08X\n", addr);
	return 0;
}

#undef read32
#define read32(addr) read32_stub(addr)
static inline u32 read32_stub(u32 addr)
{
    printf("read32_stub called with addr: 0x%08X\n", addr);
    return 0;
}

#undef write32
#define write32(addr, data) write32_stub(addr, data)
static inline void write32_stub(u32 addr, u32 data)
{
    printf("write32_stub called with addr: 0x%08X, data: 0x%08X\n", addr, data);
}
