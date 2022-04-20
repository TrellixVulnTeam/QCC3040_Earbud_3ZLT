#!/usr/bin/env python
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function
import sys
import os


class Result(object):
    def __init__(self, value=None, error_msg=None):
        self.value = value
        self.err = error_msg

    @property
    def is_success(self):
        return self.err is None

    @property
    def is_error(self):
        return not self.is_success


class Success(Result):
    def __init__(self, value=None):
        super(Success, self).__init__(value=value)


class Error(Result):
    def __init__(self, value=None, msg=''):
        super(Error, self).__init__(value=value, error_msg=msg)


def checksum_valid(capability_elf, prebuilt_info_file, adk_toolkit):
    sys.path.insert(0, os.path.join(adk_toolkit, 'tools', 'pythontools'))
    import kalaccess
    kal = kalaccess.Kalaccess()
    kal.sym.load(capability_elf)
    elf_checksum = "0x{:X}".format(kal.sym.static_dm[kal.sym.anysymbolfind("$__devtools_image_checksum").address_or_value])

    with open(prebuilt_info_file) as f:
        lines = f.readlines()

    prebuilt_checksum = ''
    for line in lines:
        if line.startswith("ELF File ID"):
            prebuilt_checksum = line.strip().split(" = ")[1]
            break

    if prebuilt_checksum == elf_checksum:
        return Success()
    else:
        return Error(msg="ELF File ID mismatch for {}.\n\nExpected {}, got {}".format(capability_elf, prebuilt_checksum, elf_checksum))


def sign(output_bundle, prebuilt_folder, adk_toolkit):
    cap_name = os.path.basename(output_bundle)
    cap_elf = os.path.join(output_bundle, cap_name + '.elf')
    cap_info_txt = os.path.join(prebuilt_folder, cap_name + '.txt')

    result = checksum_valid(cap_elf, cap_info_txt, adk_toolkit)
    if result.is_success:
        dkcs_infile = os.path.join(output_bundle, cap_name + '.dkcs')
        edkcs_outfile = os.path.join(output_bundle, cap_name + '.edkcs')
        esign_infile = os.path.join(prebuilt_folder, cap_name + '.esign')

        if not os.path.isfile(esign_infile):
            return Error(msg="Signature file not available for {}".format(dkcs_infile))

        with open(dkcs_infile, 'rb') as dkcs, open(esign_infile, 'rb') as esign, open(edkcs_outfile, 'wb') as outfile:
            outfile.write(esign.read() + dkcs.read())

        return Success()
    else:
        return result
