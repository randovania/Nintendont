#ifndef _NET_MEMORY_OPERATION_H
#define _NET_MEMORY_OPERATION_H

#include "global.h"

#define API_VERSION 1
#define MAX_INPUT_BYTES 100
#define MAX_OUTPUT_BYTES 100
#define MAX_ABSOLUTE_ADDRESSES 8
#define MINIMUM_MESSAGE_SIZE 4

#pragma pack(push,1)
typedef struct MemoryOperation {
    u8 type;
    u8 operations_count;
    u8 absolute_addresses_count;
    u8 keep_alive;
    u8 data[MAX_INPUT_BYTES];
} MemoryOperation;

typedef struct OperationHeader {
    bool has_read: 1;
    bool has_write: 1;
    bool is_word: 1;
    bool has_offset: 1;
    u8 address_index: 4;
} OperationHeader;
#pragma pack(pop)

int processMemoryOperation(MemoryOperation *memory_op, u8* output);
u32 read32FromGCMemory(u32 addr);
void write32ToGCMemory(u32 addr, u32 value);
void readBytesFromGCMemory(u32 addr, int byte_count, u8* output);
void writeBytesToGCMemory(u32 addr, int byte_count, u8* input);

#endif //_NET_MEMORY_OPERATION_H
