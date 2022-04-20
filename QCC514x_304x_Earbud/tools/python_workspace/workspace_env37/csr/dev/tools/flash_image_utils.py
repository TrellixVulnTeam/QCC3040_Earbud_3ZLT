############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2016-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
#############################################################################
""" 
Utils for XUV/ELF related low-level manipulations
"""

import subprocess
import os
import tempfile
from csr.wheels.global_streams import iprint
from csr.dev.fw.meta.xuv_stream_decoder import XUVStreamDecoder
from csr.dev.fw.meta.xuv_stream_encoder import XUVStreamEncoder
from csr.dev.fw.meta.read_only_fs import PackfileReadOnlyFS
from csr.wheels.bitsandbobs import bytes_to_words

def loadable_to_bytes(loadable, verbose=False):
    '''
    Read loadable sections (e.g. from a filesystem or an elf) into a 
    bytearray padding as necessary.
    Note that this needs the sections to be in address order otherwise
    the padding will go very wrong.
    '''
    last_paddr = 0
    output_bytes = bytearray()
    for section in loadable:
        addr = section.paddr
        data = section.data
        if addr != last_paddr:
            pad_len = addr - last_paddr
            if verbose:
                iprint("  Pad %d bytes" % pad_len) 
            output_bytes += bytearray([0xff] * pad_len)
        if verbose:
            iprint("  %s block starting at 0x%08x, %d bytes" %
                                        (section.name, addr, len(data)))
        output_bytes += bytearray(data)
        last_paddr = addr + len(data)
    return output_bytes

def pack_dir(dir_to_pack, out, packfile, offset, appsfs):
    """
    Create a filesystem image starting at the specified address offset
    """
    #1. Run packfile on the specified directory
    iprint("Packing directory...")
    handle, tmp_packed_img = tempfile.mkstemp(suffix=".fs")
    os.close(handle) # We close this since we need packfile to write to it first

    if subprocess.call([packfile, dir_to_pack, tmp_packed_img],
                       stderr=subprocess.STDOUT) != 0:
        raise RuntimeError("%s failed" % packfile)

    #2. Translate the XUV file to the offset addresses
    iprint("Translating filesystem image by 0x%06x" % (offset))
    with open(tmp_packed_img) as raw_img:
        decoder = XUVStreamDecoder(raw_img)
        encoder = XUVStreamEncoder(out)
        if appsfs:
            profs = PackfileReadOnlyFS([(a, v) for a, v in decoder.address_value_pairs])
            data = loadable_to_bytes(profs.loadable_le32, verbose=True)
            if len(data) & 1:
                iprint("padding odd length with a single byte")
                data += bytearray([0xff])
            addr = 0
            for val in bytes_to_words(data):
                encoder.address_value_pair(addr + offset, val)
                addr += 1
        else:
            for addr, val in decoder.address_value_pairs:
                encoder.address_value_pair(addr+offset, val)
    os.remove(tmp_packed_img)
