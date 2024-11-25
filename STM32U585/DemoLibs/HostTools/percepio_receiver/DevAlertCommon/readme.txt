This python library can be used to parse a DfmEntry_t and generate
an MQTT topic with a binary data package.

To use this package in your own scripts, you'll need to:

1. Import the package:
	from DevAlertCommon import DfmEntryParser, DfmEntryParserException
2. To have the library parse the binary DfmEntry_t data,
   you can use this library in the following manner:
	devalert_ready_binary_data = DfmEntryParser.parse(binary_payload)

For further examples on how this library is used, check the devalertserial.py script.
