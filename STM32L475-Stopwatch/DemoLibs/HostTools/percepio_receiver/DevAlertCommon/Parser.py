from .Exceptions import DfmEntryParserException
from .EntryHeader import EntryHeader, EntryType
from .MqttTopic import generate_mqtt_topic
from .BinaryGenerator import BinaryGenerator, DaToolsPayload


class DfmEntryParser:

    @staticmethod
    def parse(raw_entry_header: bytes) -> bytes:
        entry_header = EntryHeader(raw_entry_header)

        if not entry_header.is_valid:
            raise DfmEntryParserException("Invalid entry header provided")

        mqtt_topic = generate_mqtt_topic(entry_header)
        payload = BinaryGenerator.generate_output(mqtt_topic, entry_header.data)

        return payload

    def get_entry_data(raw_entry_header: bytes) -> bytes:
        entry_header = EntryHeader(raw_entry_header)

        if not entry_header.is_valid:
            raise DfmEntryParserException("Invalid entry header provided")
        
        return entry_header.data

    @staticmethod
    def get_topic(raw_entry_header: bytes) -> bytes:
        entry_header = EntryHeader(raw_entry_header)

        if not entry_header.is_valid:
            raise DfmEntryParserException("Invalid entry header provided")

        mqtt_topic = generate_mqtt_topic(entry_header)

        return mqtt_topic
