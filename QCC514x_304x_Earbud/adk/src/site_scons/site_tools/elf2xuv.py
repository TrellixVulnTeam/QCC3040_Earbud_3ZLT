"""
SCons Tool to convert ELF format to XUV.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import os
import sys
import logging
import subprocess

# Comment out the following to force use of SecurityCmd for local signing
try:
    from Crypto.Cipher import AES
except ImportError:
    pass

# Uncomment to display debug messages
#logging.getLogger().setLevel(logging.DEBUG)

def _loadElf(elf_filename):
    """ Load a .elf file into memory """
    from csr.dev.fw.meta.elf_code_reader import ElfCodeReader
    from csr.dev.hw.core.meta.i_core_info import Kalimba32CoreInfo
    from csr.dev.tools.flash_image_utils import loadable_to_bytes

    logging.debug("Loading elf symbols from {}".format(elf_filename))
    layout_info = Kalimba32CoreInfo().layout_info
    elfcode = ElfCodeReader(elf_filename, layout_info)
    return loadable_to_bytes(elfcode.sections, verbose=True)


def _loadXuv(xuv_filename):
    """Load a .xuv file into memory """
    logging.debug("Loading image from {}".format(xuv_filename))

    import re
    _av_re = re.compile('^@([0-9a-zA-Z]+) +([0-9a-zA-Z]+)')
    image = bytearray()
    with open(xuv_filename, 'r') as infile:
        for line in infile:
            m = _av_re.match(line)
            if m:
                image += bytearray.fromhex(m.group(2)[2:] + m.group(2)[:2])

    return image


def _convertElfToXuv(target, source, env):
    """Convert a .elf/.xuv file to a signed .xuv file"""
    # Temporarily define bytes_to_words inline to reduce dependencies
    #from csr.wheels.bitsandbobs import bytes_to_words

    def bytes_to_words(bytes): # pylint: disable=redefined-builtin
        ''' Convert a list of bytes into a list of words assuming little-endian
        packing
        '''
        from itertools import zip_longest as zip_longest # pylint: disable=useless-import-alias
        return [a[0] | a[1]<<8 for a in zip_longest(*[iter(bytes)]*2, fillvalue=0)]


    # Setup temporary files
    input = str(source[0])
    output = str(target[0])
    output_stem = os.path.splitext(output)[0]
    raw_xuv = output_stem + '_raw.xuv'
    hash_xuv = output_stem + '_hash.xuv'

    def _sign_image_natively(image):
        """Sign the image using PyCrypto module"""
        logging.debug('Sign image using PyCrypto')

        BLOCKSIZE = 16
        iv = bytearray([0]*BLOCKSIZE)
        with open(env['KEY_FILE'], 'r') as infile:
            key = bytearray.fromhex(infile.readline())
        for i in range(BLOCKSIZE//2):
            tmp = key[i]
            key[i] = key[BLOCKSIZE-1-i]
            key[BLOCKSIZE-1-i] = tmp
        cipher = AES.new(key, AES.MODE_CBC, iv)
        text = cipher.encrypt(image)
        hash = text[-BLOCKSIZE:]
        return hash

    def _sign_image_locally(image):
        """Sign the image using SecurityCmd"""
        logging.debug('Sign image using SecurityCmd')

        # Write the raw image
        with open(raw_xuv, 'w', newline='') as outfile:
            addr = 0
            words = bytes_to_words(image)
            for value in words:
                outfile.write("@%06X   %04X\r\n" % (addr, value))
                addr += 1

        # Sign the image
        cmd_line = [env['securitycmd'], "createcbcmac", raw_xuv,
                    hash_xuv, env['KEY_FILE'], "-product", "hyd",
                    "-endian", "L"]
        retval = subprocess.call(cmd_line)
        if retval:
            return None

        # Read in the hash
        return _loadXuv(hash_xuv)

    def _sign_image_http(image):
        """Sign the image using a remote signing server"""
        logging.debug('Sign image with Signing Server over HTTP')
        import requests

        # Initialise return value
        hash = None

        # Environment variable holds the server URL in this case
        SERVER_TIMEOUT = 5 # Timeout if no reply in 5s
        print('Connecting to {}'.format(env['KEY_FILE']))
        try:
            response = requests.post(env['KEY_FILE'], data=image, timeout=SERVER_TIMEOUT)
            response.raise_for_status()
            hash = response.content
        except requests.exceptions.HTTPError as err:
            print("HTTP Error: {}".format(err))
        except requests.exceptions.ConnectionError as err:
            print("Connection error: {}".format(err))
        except requests.exceptions.Timeout as err:
            print('No reply received within {}s: {}'.format(SERVER_TIMEOUT, err))
        except requests.exceptions.RequestException as err:
            print('Error: {}'.format(err))

        return hash

    # Load .elf file
    if input.endswith('.elf'):
        image = _loadElf(input)
    else: # '.xuv'
        image = _loadXuv(input)

    # Pad the xuv file to an 8-word alignment
    BLOCKSIZE = 16
    length_of_padding = len(image) % BLOCKSIZE
    if length_of_padding > 0:
        length_of_padding = BLOCKSIZE - length_of_padding
        # Add deterministic padding
        import random
        state = random.getstate()
        # Uncomment the following to reproduce SecurityCmd output:
        # This is to verify the signing mechanism is working correctly
        random.seed(length_of_padding//2)
        padding = bytearray([random.randint(0, 255)
                            for dummy in range(length_of_padding)])
        for i in range(0, length_of_padding, 2):
            tmp  = padding[i]
            padding[i] = padding[i+1]
            padding[i+1] = tmp
        image += padding
        # Uncomment the following for more efficient padding when we're
        # satisfied the signing mechanism is working correctly
        #random.seed(length_of_padding)
        #image += bytearray([random.randint(0, 255)
        #                    for dummy in range(length_of_padding)])

    # Calculate the hash on the xuv file
    if env['KEY_FILE'].startswith('http'):
        hash = _sign_image_http(image)
    elif 'Crypto.Cipher.AES' in sys.modules:
        hash = _sign_image_natively(image)
    else:
        hash = _sign_image_locally(image)

    if not hash:
        return 1

    # Debug code - output hash to file
    # To be removed when we're happy the signing mechanism is working
    # properly
    if env['KEY_FILE'].startswith('http') or 'Crypto.Cipher.AES' in sys.modules:
        with open(hash_xuv, 'w', newline='') as outfile:
            addr = 0
            words = bytes_to_words(hash)
            for value in words:
                outfile.write("@%06X   %04X\r\n" % (addr, value))
                addr += 1

    # Append the hash to the image file
    image += hash
    with open(output, 'w', newline='') as outfile:
        addr = 0
        words = bytes_to_words(image)
        for value in words:
            outfile.write("@%06X   %04X\r\n" % (addr, value))
            addr += 1
    print("Created {}".format(output))


def main():
    "Command line interface to the script"
    import argparse

    helptext = ('Convert a .elf/.xuv format file into a signed .xuv file.')

    parser = argparse.ArgumentParser(description=helptext)
    parser.add_argument('input_file',
                        help=(' Input .elf file to convert.'))
    parser.add_argument('key_file', nargs='?',
                        default = os.environ.get('QCC3042_KEY_FILE'),
                        help=(' Key/URL of server used to sign .xuv file.'))
    parser.add_argument('output_file', nargs='?',
                        help=(' Output .xuv file name.'
                              ' Default is the input file name with the .elf extension'
                              ' replaced by .xuv.'))
    parser.add_argument('-t', '--TOOLS_ROOT',
                        help=(' Root directory of the tool kit.'))

    options = parser.parse_args()

    # Perform basic input checks
    if not os.path.exists(options.input_file):
        print("Failed to find input file {}".format(options.input_file))
        return 1
    if not (options.input_file.endswith('.elf') or
            options.input_file.endswith('.xuv')):
        print("Input file should be .elf or .xuv format")
        return 1
    if not options.key_file.startswith('http'):
        if not os.path.exists(options.key_file):
            print("Failed to find key file {}".format(options.key_file))
            return 1

    if not options.output_file:
        options.output_file = os.path.splitext(options.input_file)[0] + '.xuv'

    # Locate the signing tool
    securitycmd = None
    if not options.key_file.startswith('http') and \
       'Crypto.Cipher.AES' not in sys.modules:
        signing_tool = 'SecurityCmd.exe'
        if os.path.exists(signing_tool):
            securitycmd = signing_tool
        elif options.TOOLS_ROOT:
            tool_path = os.path.join(options.TOOLS_ROOT, 'tools', 'bin', signing_tool)
            if os.path.exists(tool_path):
                securitycmd = tool_path
        if not securitycmd:
            for path in os.environ['PATH'].split(';'):
                tool_path = os.path.join(path, signing_tool)
                if os.path.exists(tool_path):
                    securitycmd = tool_path
                    break
        if not securitycmd:
            print("Could not find signing tool {}".format(signing_tool))
            return 1

    # Perform the conversion
    source = [options.input_file]
    target = [options.output_file]
    env = {'securitycmd': securitycmd, 'KEY_FILE': options.key_file}
    return _convertElfToXuv(target, source, env)

if __name__ == "__main__":
    sys.exit(main())
else:
    # SCons specific code
    import SCons.Builder

    class Elf2XuvGenWarning(SCons.Warnings.Warning):
        pass

    class Elf2XuvGenNotFound(Elf2XuvGenWarning):
        pass

    SCons.Warnings.enableWarningClass(Elf2XuvGenWarning)

    _Elf2XuvBuilder = SCons.Builder.Builder(
        action=_convertElfToXuv,
        suffix='.xuv',
        src_suffix='.elf')

    def _detect(env):
        """Try to detect the presence of the signing tool."""
        if env.get('KEY_FILE', '').startswith('http'):
            # Using remote signing server so just return
            return True
        if 'Crypto.Cipher.AES' in sys.modules:
            # Using PyCrypto so just return
            return True

        try:
            return env['securitycmd']
        except KeyError:
            pass

        securitycmd = env.WhereIs('SecurityCmd', env['TOOLS_ROOT'] + '/tools/bin')
        if securitycmd:
            return securitycmd

        raise SCons.Errors.StopError(
            Elf2XuvGenNotFound,
            "Could not find Signing and Encryption Tool (SecurityCmd.exe) "
            "or the PyCrypto module.")

    def generate(env):
        """Add Builders and construction variables to the Environment."""

        env['securitycmd'] = _detect(env)
        env['BUILDERS']['XuvObject'] = _Elf2XuvBuilder

    def exists(env):
        return _detect(env)
