#ifndef _NET_MEMORY_OPERATION_H
#define _NET_MEMORY_OPERATION_H

#include "global.h"
#include "../common/include/NintendontVersion.h"

#define API_VERSION 2
#define MAX_INPUT_BYTES 256
#define MAX_OUTPUT_BYTES 256
#define MAX_ABSOLUTE_ADDRESSES 16
#define MINIMUM_MESSAGE_SIZE 4

#pragma pack(push,1)
typedef struct SocketOperationHeader {
    u8 type;
    u8 keep_alive;
} SocketOperationHeader;

typedef struct SocketOperation {
    SocketOperationHeader header;
    u8 data[MAX_INPUT_BYTES];
} SocketOperation;

// Different operations based off of versioning

typedef struct RequestVersionOperation {
    SocketOperationHeader header;  // type is always 0
} RequestVersionOperation;


typedef struct BulkMemoryOperation {
    SocketOperationHeader header;  // type is always 1
    u8 operations_count;
    u8 absolute_addresses_count;
    u8* data;
    // u32 absolute_addresses[absolute_addresses_count];
    // MemoryOperation operations[operations_count];
} BulkMemoryOperation;