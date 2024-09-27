#!/usr/bin/python3

import argparse
import os
import re
import sys
import time
from binascii import unhexlify
import enum
from typing import Tuple
from DevAlertCommon import DfmEntryParser, DfmEntryParserException
from pathlib import Path

class ChunkResultType(enum.Enum):
    Start = enum.auto()
    Data = enum.auto()
    End = enum.auto()
    NotDfm = enum.auto()


class DataBlockParseState(enum.Enum):
    NotRunning = enum.auto()
    Parsing = enum.auto()


def crc16_ccit(data: bytearray):
    seed = 0x0000
    for i in range(0, len(data)):
        cur_byte = data[i]
        e = (seed ^ cur_byte) & 0xFF
        f = (e ^ (e << 4)) & 0xFF
        seed = (seed >> 8) ^ (f << 8) ^ (f << 3) ^ (f >> 4)

    return seed


def info_log(message):
    sys.stderr.write("{}\n".format(message))


class DatablockParser:

    @staticmethod
    def process_line(line: str) -> Tuple[ChunkResultType, None or bytes or int]:
        """
        Process a line and return what it was so that the state machine can handle it properly
        :param line:
        :return:
        """

        line = line.strip()
        # Normal log line, ignore
        if not (line.startswith("[[ ") and line.endswith(" ]]")):
            return ChunkResultType.NotDfm, None

        line = re.sub(r"^\[\[\s", "", line)
        line = re.sub(r"\s\]\]$", "", line)

        if line == "DevAlert Data Begins":
            return ChunkResultType.Start, None
        elif line.startswith("DATA: "):
            line = re.sub(r"^DATA:\s", "", line)
            return ChunkResultType.Data, unhexlify(line.replace(" ", ""))
        elif matches := re.match(r'^DevAlert\sData\sEnded.\sChecksum:\s(\d{1,5})', line):
            return ChunkResultType.End, int(matches.group(1))
        else:
            return ChunkResultType.NotDfm, None

class LineParser:

    def __init__(self):
        self.line_buffer = ""
        self.parsing = False

    def process(self, line_data) -> bool:
        stripped_line = line_data.replace("\n", "")
        if not self.parsing:
            self.line_buffer = ""
            if stripped_line.startswith("[[") and stripped_line.endswith("]]"):
                self.line_buffer = stripped_line
                return True
            elif stripped_line.startswith("[["):
                self.line_buffer += stripped_line
                self.parsing = True
                return False
            elif stripped_line.startswith("[") and len(stripped_line) < 2:
                self.line_buffer += stripped_line
                self.parsing = True
                return False
            else:
                return False
        elif self.parsing:
            # Apparently we've encountered a new line
            if stripped_line.startswith("[[") and stripped_line.endswith("]]"):
                self.line_buffer = stripped_line
                self.parsing = False
                return True
            elif stripped_line.startswith("[["):
                self.line_buffer = stripped_line
                return False
            elif stripped_line.endswith("]]"):
                self.line_buffer += stripped_line
                self.parsing = False
                return True
            else:
                self.line_buffer += stripped_line
                # Invalid line which started with [, drop it
                if not self.line_buffer.startswith("[["):
                    self.line_buffer = ""
                    self.parsing = False
                return False

from argparse import RawTextHelpFormatter

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog='percepio_receiver',
        description='Extracts DFM data from device log files (text files) and converts the DFM data for Percepio Detect or Percepio DevAlert.\nFor Percepio Detect, use --upload file --eof wait and provide the ALERTS_DIR path as --folder.\nExample: python percepio_receiver.py device.log --upload file --folder alerts_dir --eof wait.',
        formatter_class=RawTextHelpFormatter     
    )
    parser.add_argument('inputfile', type=str, help='The log file to read, containing DFM data.')
    parser.add_argument('--upload', type=str, help="Where to output the data:\n    file: store data as files (for Detect server).\n    s3: Amazon s3 bucket (requires the devalerts3 tool in the same folder).\n    sandbox: DevAlert evaluation account storage (requires the devalerthttps tool in the same folder)",  required=True)
    parser.add_argument('--folder', type=str, help='The folder where the output data should be saved. Only needed if upload is s3 or file.')
    parser.add_argument('--eof', type=str, help="What to do at end of file:\n    wait: keeps waiting for more data (exit using Ctrl-C).\n    exit: exits directly at end of file (default).",  required=False)

    args = parser.parse_args()

    if args.upload != "s3" and args.upload != "sandbox" and args.upload != "file":
        info_log("Invalid upload destination specified: {}".format(args.upload))

    if args.upload == "s3":
        if not os.path.isdir(args.folder):
            info_log("Invalid dump folder specified: {}".format(args.folder))

    binary_name_s3 = "devalerts3"
    if sys.platform == "win32":
        binary_name_s3 = "devalerts3.exe"

    line_parser = LineParser()
    block_parse_state = DataBlockParseState.NotRunning
    accumulated_payload = bytes([])
    with open(args.inputfile, "r", buffering=1) as fh:
        while 1:
            line = fh.readline()

            # We've reached the end of the file, just sleep and try reading again.
            # This makes it possible to continuously read the file
            if line == "":
                if args.upload == "file" and args.eof!="wait":
                    # We've reached the end of the file, and we just wanted to do that
                    break
                time.sleep(1)
                continue

            # If parsing in the middle of data being written to file, try to get the whole line
            if not line_parser.process(line):
                continue

            parse_result, payload = DatablockParser.process_line(line_parser.line_buffer)

            if block_parse_state == DataBlockParseState.NotRunning:
                if parse_result == ChunkResultType.Start:
                    block_parse_state = DataBlockParseState.Parsing
                    accumulated_payload = bytes([])
                elif parse_result == ChunkResultType.NotDfm:
                    info_log("Got Notdfm data: {}".format(line_parser.line_buffer))
                    continue
                else:
                    info_log("Got {} without having received a start".format(parse_result.name))
            elif block_parse_state == DataBlockParseState.Parsing:

                if parse_result == ChunkResultType.Data:
                    accumulated_payload += payload                    
                    
                elif parse_result == ChunkResultType.End:
                    block_parse_state = DataBlockParseState.NotRunning

                    if len(accumulated_payload) == 0:
                        info_log("Got empty message")
                    else:
                        # No checksum provided, skip check
                        if payload != 0:
                           calculated_crc = crc16_ccit(bytearray(accumulated_payload))
                           if calculated_crc != payload:
                               info_log("Got crc mismatch, calculated: {}, got: {}, payload length: {}".format(
                                   calculated_crc, payload, len(accumulated_payload)))
                               continue

                        
                        if args.upload == "file":      

                            # This is for generating raw alert files, intended for the local server. 
                            # Note: Not compatible with the DevAlert upload tools.

                            parsed_payload: bytes
                            try:
                                parsed_payload = DfmEntryParser.get_entry_data(accumulated_payload)
                            except DfmEntryParserException as e:
                                info_log("Got DfmParserException: {}".format(e))
                                continue
						                        
                            topic = DfmEntryParser.get_topic(accumulated_payload)
                            file_path = args.folder + "/" + topic;
                            
                            folder = str(Path(file_path).parent.resolve())
                            
                            Path(folder).mkdir(parents=True, exist_ok=True)

                            info_log("Generating " + file_path)
                                                    
                            with open(file_path, 'wb') as dump_fh:
                               dump_fh.write(parsed_payload)

                        else:
                            # Try to parse the payload to generate a proper devalerts3/devalerthttps payload
                            parsed_payload: bytes
                            try:
                                parsed_payload = DfmEntryParser.parse(accumulated_payload)
                            except DfmEntryParserException as e:
                                info_log("Got DfmParserException: {}".format(e))
                                continue
						    
                            if args.upload == "s3":
                                with open('{}/dumpfile.bin'.format(args.folder), 'wb') as dump_fh:
                                    dump_fh.write(parsed_payload)
                                os.system('./{} store-trace --file {}/dumpfile.bin'.format(binary_name_s3, args.folder))
                            else:
                                os.write(sys.stdout.fileno(), parsed_payload)

                elif parse_result == ChunkResultType.Start:
                    info_log("Got a start while parsing, resetting payload")
                    accumulated_payload = bytes([])
