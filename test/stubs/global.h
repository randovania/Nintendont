#include "../../kernel/global.h"
#include <stdio.h>
#include "unity.h"

static inline void checkValidAddress(u32 address)
{
    if (address < P2C(0x80000000) || address >= P2C(0x82400000))
    {
        TEST_FAIL_MESSAGE("Invalid reading/writing address.");
    }
}

#define VERBOSE_PRINTS false

#undef read8
#define read8(addr) read8_stub(addr)
static inline u8 read8_stub(u32 addr)
{
    if (VERBOSE_PRINTS)
    {
        printf("read8_stub called with addr: 0x%08X\n", addr);
    }
    checkValidAddress(addr);
    return 0;
}

#undef read32
#define read32(addr) read32_stub(addr)
static inline u32 read32_stub(u32 addr)
{
    if (VERBOSE_PRINTS)
    {
        printf("read32_stub called with addr: 0x%08X\n", addr);
    }
    checkValidAddress(addr);
    return 0;
}

#undef write32
#define write32(addr, data) write32_stub(addr, data)
static inline void write32_stub(u32 addr, u32 data)
{
    if (VERBOSE_PRINTS)
    {
        printf("write32_stub called with addr: 0x%08X, data: 0x%08X\n", addr, data);
    }
    checkValidAddress(addr);
}
