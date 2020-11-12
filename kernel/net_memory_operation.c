//
// Created by pwootage on 8/19/16.
//

#include "net_memory_operation.h"

#define GET_PTR(value) (P2C(value))
#define MEM_HIGH (0x2400000)
#define INVALID_PTR(value) (value) > MEM_HIGH

#define PRIME_1_GAMEID (0x474D3845)
#define PRIME_2_GAMEID (0x47324d45)
#define PRIME_1_MAKERID (0x3031)

#define CHECK_INPUT(input) if ((input) > MAX_INPUT_BYTES) { output[0] = 0xFF; return 1; }

int processReadCommands(MemoryOperation *memory_op, u8* output);

u32 get32FromBuffer(u8* buffer, int *index) {
  int i = *index;
  *index = i + 4;
  return (buffer[i] << 24) + (buffer[i + 1] << 16) + (buffer[i + 2] << 8) + buffer[i + 3];
}

u16 get16FromBuffer(u8* buffer, int *index) {
  int i = *index;
  *index = i + 2;
  return (buffer[i + 0] << 8) + buffer[i + 1];
}

void write32ToBuffer(u8* output, u32 value, int *index) {
  int i = *index;
  *index = i + 4;
  output[i + 0] = (u8)((value >> 24) & 0xFF);
  output[i + 1] = (u8)((value >> 16) & 0xFF);
  output[i + 2] = (u8)((value >> 8) & 0xFF);
  output[i + 3] = (u8)((value) & 0xFF);
}

int processReadCommands(MemoryOperation *memory_op, u8* output) {
  u32 addresses[MAX_ABSOLUTE_ADDRESSES];
  int i, result_index = 0, input_index = 0;

  for (i = 0; i < memory_op->absolute_addresses_count && i < MAX_ABSOLUTE_ADDRESSES; ++i) {
    addresses[i] = get32FromBuffer(memory_op->data, &input_index);
  }

  // Special case 0 ops, since it'd break the num_op_success_bytes calculation
  if (memory_op->operations_count == 0) {
    return 0;
  }

  int num_op_success_bytes = 1 + (memory_op->operations_count - 1) / 8;
  for (i = 0; i < num_op_success_bytes; ++i) {
    // initialize these with 0, since we fill these incrementally
    output[i] = 0;
  }
  // Skip the bytes reserved for informing valid addresses
  result_index += num_op_success_bytes;

  for (i = 0; i < memory_op->operations_count; ++i) {
    CHECK_INPUT(input_index)
    struct OperationHeader *op = (struct OperationHeader *)&memory_op->data[input_index++];

    u8 addr_index = op->address_index;
    u32 addr = addresses[addr_index];

    //dbgprintf("[Net] [processReadCommands] %d - Address: %x - Index: %d - is_word: %d - has_offset: %d - has_read: %d - has_write: %d\r\n", i, addr, addr_index, op->is_word, op->has_offset, op->has_read, op->has_write);

    u8 byte_count = 4;
    if (!op->is_word) {
      byte_count = memory_op->data[input_index++];
    }

    if (op->has_offset) {
      CHECK_INPUT(input_index + 2)
      s16 offset = (s16) get16FromBuffer(memory_op->data, &input_index);
      addr = (u32)((s32)read32FromGCMemory(addr) + (s32)offset);
    }
    bool is_valid_addr = !(INVALID_PTR(P2C(addr)));

    if (op->has_read && is_valid_addr) {
      if (result_index > MAX_OUTPUT_BYTES) {
        return 0;
      }

      readBytesFromGCMemory(addr, byte_count, output + result_index);
      result_index += byte_count;
    }

    if (op->has_write) {
      CHECK_INPUT(input_index + byte_count)
      if (is_valid_addr) {
        writeBytesToGCMemory(addr, byte_count, &memory_op->data[input_index]);
      }
      input_index += byte_count;
    }
    output[i / 8] |= is_valid_addr << (i % 8);
  }

  return result_index;
}

int processRequestVersion(MemoryOperation *memory_op, u8* output) {
  int result_index = 0;
  write32ToBuffer(output, API_VERSION, &result_index);
  write32ToBuffer(output, MAX_INPUT_BYTES, &result_index);
  write32ToBuffer(output, MAX_OUTPUT_BYTES, &result_index);
  write32ToBuffer(output, MAX_ABSOLUTE_ADDRESSES, &result_index);
  return result_index;
}

int processMemoryOperation(MemoryOperation *memory_op, u8* output) {
  switch(memory_op->type) {
    case 0:
      return processReadCommands(memory_op, output);
    case 1:
      return processRequestVersion(memory_op, output);
    default:
      return 0;
  }
}

u32 read32FromGCMemory(u32 addr) {
  u32 actualPtr = P2C(addr);
  if (INVALID_PTR(actualPtr)) {
    return 0;
  }
  u32 res = read32(GET_PTR(actualPtr));
  return res;
}

void write32ToGCMemory(u32 addr, u32 value) {
  u32 actualPtr = P2C(addr);
  if (INVALID_PTR(actualPtr)) {
    return;
  }
  write32(GET_PTR(actualPtr), value);
}

void readBytesFromGCMemory(u32 addr, int byte_count, u8* output) {
  int index = 0;
  while (byte_count >= 4) {
    u32 result = read32FromGCMemory(addr + index);
    write32ToBuffer(output, result, &index);
    byte_count -= 4;
  }
  while (byte_count >= 0) {
    u8 result = read8(P2C(addr + index));
    output[index++] = result;
    byte_count -= 1;
  }
}

void writeBytesToGCMemory(u32 addr, int byte_count, u8* input) {
  int i, index = 0, bytes_left = byte_count;
  u32 current_address = addr & (~3);
  if (addr > current_address) {
    u32 value = read32(P2C(current_address));
    for (i = addr & 3; i < 4 && bytes_left >= 0; ++i) {
      int shift = ((3 - i) * 8);
      value &= ~(0xFF << shift);
      value |= input[index++] << shift;
      bytes_left -= 1;
    }
    write32(P2C(current_address), value);
    current_address += 4;
  }
  while (bytes_left >= 4) {
    u32 value = get32FromBuffer(input, &index);
    write32ToGCMemory(current_address, value);
    bytes_left -= 4;
    current_address += 4;
  }
  if (bytes_left >= 0) {
    u32 value = read32(P2C(current_address));
    for (i = 0; i < bytes_left; ++i) {
      int shift = ((3 - i) * 8);
      value &= ~(0xFF << shift);
      value |= input[index++] << shift;
      bytes_left -= 1;
    }
    write32(P2C(current_address), value);
    current_address += 1;
  }
  sync_after_write_align32((void*)(P2C(addr)), byte_count);
}