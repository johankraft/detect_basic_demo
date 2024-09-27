from dataclasses import dataclass
from .Exceptions import DfmEntryParserException
from .EntryHeader import EntryHeader, EntryType


def generate_mqtt_topic(entry_header: EntryHeader) -> str:
    mqtt_topic_string: str
    if entry_header.entry_type == EntryType.alert:
        mqtt_topic_string = "DevAlert/{}/{}/{}/{}-{}_da_header".format(
            entry_header.device_name,
            entry_header.session_id,
            entry_header.alert_id,
            entry_header.chunk_index,
            entry_header.chunk_count
        )
    elif entry_header.entry_type == EntryType.payload_header:
        mqtt_topic_string = "DevAlert/{}/{}/{}/{}-{}_da_payload{}_header".format(
            entry_header.device_name,
            entry_header.session_id,
            entry_header.alert_id,
            entry_header.chunk_index,
            entry_header.chunk_count,
            entry_header.entry_id
        )
    elif entry_header.entry_type == EntryType.payload_chunk:
        mqtt_topic_string = "DevAlert/{}/{}/{}/{}-{}_da_payload{}".format(
            entry_header.device_name,
            entry_header.session_id,
            entry_header.alert_id,
            entry_header.chunk_index,
            entry_header.chunk_count,
            entry_header.entry_id
        )
    else:
        raise DfmEntryParserException("Invalid entry type specified: {}".format(entry_header.entry_type_value))

    return mqtt_topic_string
