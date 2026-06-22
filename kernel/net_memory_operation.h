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

typedef struct MemoryOperationHeader {
    bool has_read: 1;       // TODO: combine read and write into one bit
    bool has_write: 1;
    bool is_word: 1;
    bool has_offset: 1;
    u8 address_index: 4;
} MemoryOperationHeader;

typedef struct MemoryOperation {
    MemoryOperationHeader header;
    u8* data;
    // u8 byte_count;  # if !header.is_word
    // u16 offset_count; # if header.has_offset
    // u8 write_data[byte_count or 4]; # if header.has_write
} MemoryOperation;

/* TODO
 t y*pedef struct ReadArrayOperation {
 SocketOperationHeader header;  // type is always 2
 u32 address;
 u16 length;
 u16 stride;
 } ReadArrayOperation;
 */
#pragma pack(pop)

int processMemoryOperation(SocketOperation *socket_op, u8* output);
u32 read32FromGCMemory(u32 addr);
void write32ToGCMemory(u32 addr, u32 value);
void readBytesFromGCMemory(u32 addr, int byte_count, u8* output);
void writeBytesToGCMemory(u32 addr, int byte_count, u8* input);

#endif //_NET_MEMORY_OPERATION_H
