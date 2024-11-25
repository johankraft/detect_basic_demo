import struct
from dataclasses import dataclass
from enum import Enum


class EntryType(Enum):
    alert = 0x1512
    payload_header = 0x4618
    payload_chunk = 0x8371


class EntryHeader:

    def __init__(self, payload: bytes):
        self._payload = payload

    def __unpack_uint16(self, input_bytes) -> int:
        unpacked_int, = struct.unpack("{}H".format(self.__get_endian_char()), input_bytes)
        return unpacked_int

    def __unpack_uint32(self, input_bytes) -> int:
        unpacked_int, = struct.unpack("{}I".format(self.__get_endian_char()), input_bytes)
        return unpacked_int

    def __unpack_string(self, input_bytes) -> str:
        unpacked_string, = struct.unpack("{}s".format(len(input_bytes)), input_bytes)
        decoded_string = unpacked_string.decode('ASCII')
        return decoded_string

    def __get_endian_char(self) -> str:
        if self.endianness == 0x0FF0:
            return "<"  # Little endian
        return ">"  # Big endian

    @property
    def is_valid(self) -> bool:
        correct_start_marker_bytes = bytes([0xD1, 0xD2, 0xD3, 0xD4])
        correct_end_marker_bytes = bytes([0xD4, 0xD3, 0xD2, 0xD1])
        try:
            return self.start_markers == correct_start_marker_bytes and self.end_markers == correct_end_marker_bytes
        except IndexError:
            return False

    @property
    def entry_type(self) -> EntryType or None:
        try:
            return EntryType(self.entry_type_value)
        except ValueError:
            return None

    @property
    def start_markers(self) -> bytes:
        return self._payload[0:4]

    @property
    def endianness(self) -> int:
        endianess: int = self._payload[4] | self._payload[5] << 8
        return endianess

    @property
    def version(self) -> int:
        return self.__unpack_uint16(self._payload[6:8])

    @property
    def entry_type_value(self) -> int:
        return self.__unpack_uint16(self._payload[8:10])

    @property
    def entry_id(self) -> int:
        return self.__unpack_uint16(self._payload[10:12])

    @property
    def chunk_index(self) -> int:
        return self.__unpack_uint16(self._payload[12:14])

    @property
    def chunk_count(self) -> int:
        return self.__unpack_uint16(self._payload[14:16])

    @property
    def alert_id(self) -> int:
        return self.__unpack_uint32(self._payload[16:20])

    @property
    def session_id_size(self) -> int:
        return self.__unpack_uint16(self._payload[20:22])

    @property
    def device_name_size(self) -> int:
        return self.__unpack_uint16(self._payload[22:24])

    @property
    def description_size(self) -> int:
        return self.__unpack_uint16(self._payload[24:26])

    # 2 reserved bytes here, won't be needed for the time being

    @property
    def data_size(self) -> int:
        return self.__unpack_uint32(self._payload[28:32])

    @property
    def session_id(self) -> str:
        return self.__unpack_string(self._payload[32:32 + self.session_id_size]).replace('\00', '')

    @property
    def device_name(self) -> str:
        start = 32 + self.session_id_size
        end = start + self.device_name_size
        return self.__unpack_string(self._payload[start:end]).replace('\00', '')

    @property
    def description(self) -> str:
        start = 32 + self.session_id_size + self.device_name_size
        end = start + self.description_size
        return self.__unpack_string(self._payload[start:end]).replace('\00', '')

    @property
    def data(self) -> bytes:
        start = 32 + self.session_id_size + self.device_name_size + self.description_size
        end = start + self.data_size
        return self._payload[start:end]

    @property
    def end_markers(self) -> bytes:
        start = 32 + self.session_id_size + self.device_name_size + self.description_size + self.data_size
        end = start + 4
        return self._payload[start:end]

