# /bin/env python2

import random
import struct
import socket
import datetime
from typing import NamedTuple, Optional, List

host = '192.168.5.107'
port = 43673


sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print("Connecting to Nintendont...")
sock.connect((host, port))
print("Connected")


class Op(NamedTuple):
    address_index: int
    write_value: Optional[int]

    @property
    def has_read(self):
        return True

    @property
    def has_write(self):
        return self.write_value is not None

    @property
    def is_word(self):
        return True


def write_to_socket(data: bytes) -> bytes:
    print("\nSent:", sock.send(data), data.hex())
    response = sock.recv(1024)
    print(f"Response: {response.hex()}")
    return response



def request_meta_info():
    data = struct.pack(f">BBBB", 1, 0, 0, 1)
    result = write_to_socket(data)

    api_version, max_input, max_output, max_addresses = struct.unpack(">IIII", result)
    print(f"> API version: {api_version}")
    print(f"> Max input bytes: {max_input}")
    print(f"> Max output bytes: {max_output}")
    print(f"> Max addresses: {max_addresses}")


def send_socket(addresses: List[int], ops: List[Op]):
    data = struct.pack(f">BBBB{len(addresses)}I", 0, len(ops), len(addresses), 1, *addresses)
    for op in ops:
        op_byte = op.address_index
        if op.has_read:
            op_byte = op_byte | 0x80
        if op.has_write:
            op_byte = op_byte | 0x40
        if op.is_word:
            op_byte = op_byte | 0x20

        data += struct.pack(">B", op_byte)
        if op.write_value is not None:
            data += struct.pack(">I", op.write_value)

    response = write_to_socket(data)

    last_validation_index = 0
    for i in range(len(ops)):
        last_validation_index = i // 8
        if not response[last_validation_index] & (1 << (i % 8)):
            raise RuntimeError(f"Op {i} used an invalid address")

    return response[last_validation_index + 1:]


def send_message_alert(message: str):
    encoded_message = message.encode("utf-16_be")[:0x58]
    if random.randint(0, 1) == 0:
        encoded_message += b'\x00 '
    encoded_message += b"\x00\x00"

    string_address = 0x803a6380
    has_message_address = 0x803db6e0 + 0x2

    data = struct.pack(f">BBBB2I", 0, 2, 2, 1, string_address, has_message_address)
    data += struct.pack(f">BB", 0x40 | 0, len(encoded_message)) + encoded_message
    data += struct.pack(f">BBB", 0x40 | 1, 1, 1)

    write_to_socket(data)


def read_memory(ops):
    return send_socket(ops, [Op(i, None) for i, _ in enumerate(ops)])


def write_memory(ops):
    return send_socket(
        [addr for addr, _ in ops],
        [Op(i, value) for i, (_, value) in enumerate(ops)])


request_meta_info()

game_id, cplayer, cstatemanager = struct.unpack_from(">4sII", read_memory([0x0, 0x803CA740, 0x803C5C9C]))
print(">> Game ID:", game_id)
print(f">> CPlayer address: {hex(cplayer)}")
print(f">> CStateManager address: {hex(cstatemanager)}")

cplayer_state, timer = struct.unpack_from(">Id", read_memory([cplayer + 0x1314, cstatemanager + 0x48, cstatemanager + 0x4C]))
print(f">> CPlayerState address: {hex(cplayer_state)}")
print(f">> Timer: {timer}")

health, = struct.unpack_from(">f", read_memory([cplayer_state + 0x14]))
print(">> Health:", health)

base_inventory = 0x5C
dark_beam = struct.unpack_from(">IIII", write_memory([
    (cplayer_state + 0x68, 1), (cplayer_state + 0x6C, 1),
    (cplayer_state + base_inventory + 45 * 0xC + 0, 50), (cplayer_state + base_inventory + 45 * 0xC + 4, 50),
]))
print(">> Dark Beam Before:", dark_beam)

send_message_alert("Hello World!")

sock.close()
print("Closed connection.")