# Remote Memory Protocol

This document describes the remote memory protocol that is used by Nintendont. It's defined in `kernel/network/net_memory_operation.[h/c]` .

The following explains the current API, which is on version 2.

## Request Packet Header

Each request packet begins with these two fields:

| Type |    Name    |
|------|------------|
|  u8  | type       |
|  u8  | keep_alive |

- `type` shows what type of operation it is.
- `keep_alive` shows whether to keep the connection open after the operation was acted on. 

The valid operation types are:

- `0`, for the Request Version operation.
- `1`, for the Bulk Memory operation.
- `2`, for the Read Array operation.

If an invalid type gets used, then no response is given.

## Request Version

The Request Version operation shows several capabilities of the program.

### Request

It has no additional fields.

### Response

The response is the following:

|  Type  |         Name           |
|--------|------------------------|
|  u32   |  api_version           |
|  u32   | max_input_bytes        |
|  u32   | max_output_bytes       |
|  u32   | max_absolute_addresses |
|  u32   | major_version          |
|  u32   | minor_version          |

- `api_version` shows the api version of the protocol this program supports.
- `max_input_bytes` shows the maximum amount bytes on how large a packet can be. It's undefined behaviour if a request is bigger than `max_input_bytes`. It's recommended that the program supports at least 16 bytes to be able to handle all operations.
- `max_output_bytes` shows the maximum amount of bytes on how large a response from the program can be. The behaviour on what happens if a response would be bigger than this number is different per operation. It's recommended that the program supports at least word-sized bytes. But supporting 0 bytes would work too, as it would mean that the program only supports write operations.
- `max_absolute_addresses` shows how many addresses this program can operate on at the same time. This is only used for the Bulk Memory operation. See the description for `absolute_address_count`. Due to how the Bulk Memory operation is defined, this number has to be lower or equal to 16. It should at minimum be 1 in order to support the Bulk Memory Operation.
- `major_version` shows the major version of this program.
- `minor version` shows the minor version of this program.

## Bulk Memory

The Bulk Memory operation allows multiple reads from/writes to addresses.

### Request

It has the following additional fields:

|                Type                 |           Name           |
|-------------------------------------|--------------------------|
|    u8                               | operations_count         |
|    u8                               | absolute_addresses_count |
|    u32[`absolute_addresses_count`]  | absolute_addresses       |
| MemoryOperation[`operations_count`] | operations               |

- `operations_count` shows how many read/write Memory Operations this Bulk Memory operation has.
- `absolute_addresses_count` shows how many unique memory addresses are operated on. This number should not be greater than `max_absolute_addresses`. If it is, then only addresses up to `max_absolute_addresses` will be taken into account.
- `absolute_addresses` is an array with `absolute_addresses_count`-elements. Each element is one address that is being operated on. These addresses do not have to be sorted in any way, and could even be duplicated. It's undefined behaviour if the length of this array does not match with `absolute_address_count`.
- `operations` is an array with `operations_count`-elements. Each element is one MemoryOperation.

##### Memory Operation

A Memory Operation is structured the following way:

|             Type           |                     Name                    |
|----------------------------|---------------------------------------------|
|       u1                   |      has_read                               |
|       u1                   |      has_write                              |
|       u1                   |      is_word                                |
|       u1                   |      has_offset                             |
|       u4                   |   address_index                             |
|       u8                   | byte_count (only if `is_word` is false)     |
|       u16                  | offset_count (only if `has_offset` is true) |
|   u8[`byte_count` or word] | write_data (only if `has_write` is true)    |

- `has_read` shows whether the operation reads from the memory address.
- `has_write` shows whether the operation writes to the memory address.
- `is_word` shows whether this operation's read/write byte count is that of a word.
- `has_offset` shows whether the memory address is a pointer that should get dereferenced before reading/writing to it.
- `address_index` is an index to the `absolute_addresses` array.
- `byte_count` shows how many bytes to read/write. This field is only present if `is_word` is false.
- `offset_count` shows the offset that gets applied after the memory address got dereferenced. This field is only present if `has_offset` is true.
- `write_data` is an array of data should should be written to the memory address. It has `byte_count`-elements if `is_word` is false, and word-elements if `is_word` is true. It's undefined behaviour if the length of this array does not match with the `byte_count`/word-size.

If both `has_read` and `has_write` are given, then it will first read the bytes from the address, and then write to that address.

### Examples

Here are a few examples:
```
operations_count=1
absolute_addresses_count=1
absolute_addresses=[0x1234]
operations=[
    MemoryOperation(
        has_read=true
        has_write=false
        is_word=true
        has_offset=false
        address_index=0
    )
]
```
This will read a word from 0x1234.

```
operations_count=2
absolute_addresses_count=2
absolute_addresses=[0x1234, 0x1234]
operations=[
    MemoryOperation(
        has_read=true
        has_write=false
        is_word=true
        has_offset=false
        address_index=0
    )
    MemoryOperation(
        has_read=true
        has_write=false
        is_word=true
        has_offset=false
        address_index=1
    )
]
```
This will have both MemoryOperations reading a word from 0x1234. However, this could also be shortened to only include one address and have both `address_index` as 0, since both MemoryOperations operate on the same address.
Similarly, you could also specify the byte_count manually. E.g. If a word is 4 bytes long, have `is_word=false, bytes_count=4`.

```
operations_count=1
absolute_addresses_count=1
absolute_addresses=[0x1234]
operations=[
    MemoryOperation(
        has_read=true
        has_write=false
        is_word=false
        has_offset=true
        address_index=0
        byte_count=20
        offset_count=12
    )
]
```
This will do the following:
- read an address at 0x1234. 
- dereference said address
- add 12 to the new address
- read 20 bytes from the address that was calculated in the above step.

Consequently
```
operations_count=1
absolute_addresses_count=1
absolute_addresses=[0x1234]
operations=[
    MemoryOperation(
        has_read=false
        has_write=true
        is_word=false
        has_offset=true
        address_index=0
        byte_count=5
        offset_count=12
        write_data=[9, 8, 7, 6, 5]
    )
]
```
Will do the same as above, but instead of reading 20 bytes will write 09, 08, 07, 06, 05. (This is not the same as 98765.)

### Response

The response is split into two parts: success bytes and read bytes.

The amount of success bytes is determined by this: `1 + ('operations_count' - 1) / 8`. Note that this is integer division.
In other words, each Memory Operation corresponds to one bit, and they're padded to one byte.
The success-bit is `1` if the Memory Operation had a valid memory address or if the `address_index` of the Memory Operation is within `absolute_address_count`. 
It's `0` if the memory address was invalid.
Note that the order of Memory Operations in the success bytes is the same as it was given in the request.

After the success bytes follow the bytes that were read. The order of read bytes also matches the order of the Memory Operations given in the request.
If a Memory Operation didn't read anything, then nothing is appended for it. 
Take a request 3 Memory Operations: the first reading 1 byte, the second writing 1 byte and the third reading 2 bytes. In that example, this read-byte-section would contain the 1 read byte of the first Memory Operation followed by the 2 bytes of the third Memory Operation.


On fatal errors the response will be completely empty. This happens in the following cases:
- `operations_count` is 0.
- The response would be bigger than `max_output_bytes`
- A Memory Operation was noted to dereference a pointer, but the given memory address was not aligned as such. E.g. providing 0x0001 on a machine where memory addresses are 4-byte aligned.

## Read Array

The Read Array operation allows you to read an array in an easier and  more efficient way.

### Request

It has the following additional fields:

|  Type  |   Name   |
|--------|----------|
|   u32  | address  |
|   u32  | count    |
|   u32  | size     |
|   u32  | stride   |

- `address` shows the starting memory address of the array
- `count` shows how many elements of the array to read.
- `size` shows how much to read from each element, in bytes.
- `stride` shows how many bytes are between elements.

For example, reading an array where each element has this structure:

`| u32 a | u32 b | u8 c | u24 padding |`

Might have this request:
- address = 0x1234 (arbitrary address)
- count = 3 (arbitrary count)
- size = 72 (only cares about `a`, `b` and `c`, so 32+32+8)
- stride = 96 (full element is 32+32+8+24)

### Response

The response begins with one status byte:
- `1`, for when the operation was successfull.
- `0`, for when the operation failed.

After that are the read data blocks.


The operation is marked as a failure when:
- the address to be read from is outside a valid range.
- the output would be bigger than `max_output_bytes`.

The server returns success when the read completes within the limit of `max_output_bytes`.
For `count = 0` or `size = 0`, the program still returns a success byte, although no data since none was read.

## Invalid

If an invalid type is specified, then the program responds with nothing.

# Nintendont Specific

The following things are not part of the protocol. They're Nintendont-specific quirks.

- Nintendont listens on port 43673.
- Nintendont / the GameCube uses big-endian ordering.
- Valid addresses to read/write from are in the GameCube memory region. This means that any address smaller than `0x80000000` is invalid, and any address bigger or equal to `0x82400000` is also invalid.
- The word size is 4.
- Reading lots of data from an address that is not 4-byte-aligned is slow and should be avoided.
- Nintendont allows up to 4 simultaneous connections.

TODOs for version 3:
- rename "request version" since it does more than request the version?
- close connection if a bigger input is send than what can be handled (e.g. sending 400 when max_input_bytes is 200)
- clarify what to do if keep_alive doesnt make sense in the context.
- change response for invalid types in request packets?
- request version: bitfield for indicating which types are supported?
- maybe standardize response for when response would exceed output?
- should probably get rid of being able to both read and write at the same time in one op in bulk memory op
- bulk memory operation: should we change the response when trying to dereference a pointer from an misaligned address?
- bulk memory operation: have visible error if address array is bigger than max_addresses?
- read array: disallow stride being smaller than size?
- change response from invalid type so that we get some kind of feedback?