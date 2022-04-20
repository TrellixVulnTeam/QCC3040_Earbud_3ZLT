############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Simple script to decode a raw Apps P1 log contained in a plain binary file, and 
write it to a text file (in the default encoding).

Note: this is pretty quick and dirty.  In particular, 
 - there's no way to check that the ELF file that is passed is consistent with 
 the application that generated the log, because the raw log file has no 
 firmware version information.
 - the messages normally written during a Pydbg log decode to indicate that
 log entries were unexpectedly not valid addresses of debug strings have been
 suppressed if they contain the value 0xfffffff, because of the large number of 
 occurrences of 0xffffffff in the raw data to indicate gaps in the log content.
 - none of the presentation enhancements available in a normal Pydbg sesion are
 exposed - colouration, white/blacklisting, etc.
 - there's no facility to read from stdin or write to stdout - it's limited to 
 named files.
"""

import sys
import os
from argparse import ArgumentParser

my_dir = os.path.abspath(os.path.dirname(__file__))    
pylib_dir = os.path.join(my_dir, "pylib")
if pylib_dir not in sys.path:
    sys.path.insert(0, pylib_dir)

from csr.dev.fw.meta.i_firmware_build_info import HydraAppsP1FirmwareBuildInfo
from csr.dev.fw.apps_firmware import AppsP1Firmware
from csr.dev.env.standalone_env import StandaloneFirmwareEnvironment
from csr.dev.hw.core.base_core import BaseCore
from csr.dev.hw.core.meta.i_layout_info import Kalimba32DataInfo
from csr.dev.fw.debug_log import Apps1LogDecoder
from csr.wheels import redirected_stdout
from csr.dev.hw.address_space import NullAccessCache

class DummyCore(BaseCore):

    access_cache_type = NullAccessCache

    """
    StandaloneFirmwareEnvironment requires a bit of metadata from the core at
    construction time, although we don't need to use the core in anger here.
    """
    def __init__(self):
        BaseCore.__init__(self)

    @property
    def nicknames(self):
        return ["apps1"]
    @property
    def data(self):
        return None 

        

def get_args():
    parser = ArgumentParser()
    parser.add_argument("-i","--input", help="Path to input file containing binary raw log data, assumed to be little-endian")
    parser.add_argument("-o","--output", help="Path to output file to contain the decoded log data")
    parser.add_argument("-f","--firmware-path", help="Path to application ELF file")
    return parser.parse_args()
    

def get_decoder(firmware_path, layout_info):
    """
    Construct an Apps1 decoder instance
    """
    build_info = HydraAppsP1FirmwareBuildInfo(firmware_path, data_layout_info=layout_info)
    core = DummyCore()
    env = StandaloneFirmwareEnvironment(build_info, core, layout_info)
    
    return Apps1LogDecoder(AppsP1Firmware(env, core))

def main(input, output, firmware_path):
    """
    Drive the process: construct a decoder, read in the raw data, convert to
    words, run the decoder over them, and write out again.
    """
    layout_info = Kalimba32DataInfo()
    
    decoder = get_decoder(firmware_path, layout_info)
    
    with open(input, "rb") as data_in:
        raw_bytes = data_in.read()
        
    if bytes is str:
        # Py2
        raw_bytes = [ord(byte) for byte in raw_bytes]
    raw_words = list(layout_info.word_stream_from_octet_stream(raw_bytes))
    
    with redirected_stdout() as stdout:
        decoded_lines = decoder.decode(raw_words)
    stdout_msgs = stdout.getvalue().split("\n")
    interesting_stdout_msgs = [msg for msg in stdout_msgs if "0xffffffff" not in msg.lower()]
    print("\n".join(interesting_stdout_msgs) + "\n")
        
    print("Writing decoded log to %s" % output)
    with open(output, "w") as data_out:
        data_out.write("\n".join(decoded_lines))
        

if __name__ == "__main__":
    
    args = get_args()
    
    main(args.input, args.output, args.firmware_path)
