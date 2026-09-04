#include "global.h"
#include "../../kernel/syscalls.h"
#include <stdio.h>

void sync_before_read(void *ptr, int len)
{
    printf("sync_before_read_stub called with ptr: %p, len: %d\n", ptr, len);
}

void sync_after_write(void *ptr, int len)
{
    printf("sync_after_write_stub called with ptr: %p, len: %d\n", ptr, len);
}