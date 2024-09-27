from .EntryHeader import EntryHeader
from .Exceptions import DfmEntryParserException
from dataclasses import dataclass
import struct

@dataclass
class DaToolsPayload:
    start_marker: bytes
    mqtt_key: str
    data: bytes

    def pack(self) -> bytes:
        if len(self.start_marker) != 4:
            raise DfmEntryParserException("Invalid start marker length, detected length: {}".format(len(self.start_marker)))

        payload = struct.pack(
            "<4sHH{}s{}s".format(len(self.mqtt_key), len(self.data)),
            self.start_marker,
            len(self.mqtt_key),
            len(self.data),
            self.mqtt_key.encode('ASCII'),
            self.data
        )

        return payload


class BinaryGenerator:

    @staticmethod
    def generate_output(mqtt_topic, data) -> bytes:
        return DaToolsPayload(
            start_marker=bytes([0xA1, 0x1A, 0xF9, 0x9F]),
            data=data,
            mqtt_key=mqtt_topic
        ).pack()
