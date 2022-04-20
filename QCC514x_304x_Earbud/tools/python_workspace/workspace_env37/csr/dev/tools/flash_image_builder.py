# Copyright (c) 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
# This tool creates an xuv image for programming into the flash memory
# device. It combines images for filesystems, and firmware into a
# single image.
#
import copy
import os
import random
import struct
import subprocess
import sys
import tempfile

scriptdir = os.path.dirname(os.path.abspath(__file__))
pylibdir = os.path.join(scriptdir, "..", "pylib")
if not pylibdir in sys.path:
    sys.path.insert(0,pylibdir)

from ..fw.meta.xuv_stream_decoder import XUVStreamDecoder
from ..fw.meta.xuv_stream_encoder import XUVStreamEncoder
from ..fw.meta.elf_code_reader import ElfCodeReader
from ..hw.core.meta.i_core_info import Kalimba32CoreInfo
from ...wheels.bitsandbobs import create_reverse_lookup
from ...wheels.bitsandbobs import flatten_le
from .flash_image_utils import loadable_to_bytes
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint


class CapacityError(ValueError):
    pass


def decode_boot_image(boot_image):
    '''
    Parse an array of bytes read from the start of flash into
    the boot image so we can find the locations of the images. 
    '''
    boot_image = bytearray(boot_image)
    if boot_image[0:4] != ImageBuilder.magic_numbers["BOOT_IMAGE_START"]:
        raise ImageBuilder.BootImageError("Invalid boot image tag '%s'" % boot_image[0:4])
    pri_image_offset = struct.unpack(ImageBuilder.pack_fmt, boot_image[4:8])[0]
    alt_image_offset = struct.unpack(ImageBuilder.pack_fmt, boot_image[8:12])[0]
    boot_image_size = pri_image_offset
    
    return {
        "offset"        : 0,
        "size"          : boot_image_size,
        "content"       : boot_image,
        "primary_image_offset" : pri_image_offset,
        "alternate_image_offset" : alt_image_offset
        }

class ImageBuilder(object):
    magic_numbers = {  "BOOT_IMAGE_START"  : b"IMGE",
                       "BOOT_IMAGE_END"    : b"END\0",
                       "IMAGE_HEADER_START": b"Imag",
                       "IMAGE_HEADER_END"  : b"END\0" }

    #Note: "total_image" and "dfu_header_hash_len" are not actual image sections,
    #only the value of these two entries are added in the image header.
    section_ids = {
        "vmodel_partition"      : 0xfe00, # Virtual partition. Does not exist on flash
        "p1_header_ptr"         : 0x0f00,
        "curator_fs"            : 0x0e00,
        "apps_p0"               : 0x0d00,
        "apps_p1"               : 0x0c00,
        "ro_fs"                 : 0x0b00,
        "debug_partition"       : 0x0a00,
        "device_ro_fs"          : 0x0900,
        "rw_config"             : 0x0800,
        "rw_fs"                 : 0x0700,
        "total_image"           : 0x0600,
        "ro_cfg_fs"             : 0x0500,
        "dfu_header_hash_len"   : 0x0400,
        "audio"                 : 0x0300,
        "ra_partition"          : 0x0200,

        # Used to mask the section id from the key
        "mask"                  : 0xff00
        }
    element_ids = {
        # Unused
        None           : 0x00,
        # An offset from the start of the flash image to this section.
        "offset"       : 0x01,
        # The amount of supplied data stored in this section in bytes.
        # i.e. The size of the image if "src_image" is specified.
        "size"         : 0x02,
        # Offset pointing to where this section's hash is stored.
        "hash_ptr"     : 0x03,
        # The number of bytes in this section that have been hashed.
        "hash_size"    : 0x04,
        # The entire size of this section, including used and unused bytes.
        "capacity"     : 0x05,
        # The 'id' of this partition - used for RA partitions only at the moment
        "id"           : 0x06,
        # Offset pointing to where this section's OEM hash is stored.
        "oem_hash_ptr" : 0x07,
        # Used to mask the element id from the key
        "mask"         : 0xff
        }
    
    # Partitions may have an alias name e.g. named ra_partitions included as a file system project
    partition_aliases = {'va_fs':'ra_partition'}
    
    # List of sections which can't be encrypted  
    non_encryptable_sections = ["rw_config", "rw_fs", "ra_partition", "debug_partition", "vmodel_partition"]
    
    default_flash_device_parameters = {
        "block_size": 64 * 1024,
        "boot_block_size": None,
        "alt_image_offset": 128 * 64 * 1024
    }

    default_flash0 = {
        "flash_device": default_flash_device_parameters,
        "encrypt": False,
        "layout": [
            ("curator_fs",      {"capacity": 1024 * 64, "authenticate": False, "src_file_signed": False,
                                 "custom_hash": False}),
            ("apps_p0",         {"capacity": 1024 * 768, "authenticate": False, "src_file_signed": False}),
            ("apps_p1",         {"capacity": 1024 * 512, "authenticate": False}),
            # Device config filesystem size limited by size of production test buffer.
            ("device_ro_fs",    {"capacity": 1024 * 2 - 10, "authenticate": False, "inline_auth_hash": True }),
            ("rw_config",       {"capacity": 1024 * 128}),
            ("rw_fs",           {"capacity": 1024 * 256}),
            ("ra_partition",    {"capacity": 1024 * 128}),
            ("ra_partition",    {"capacity": 1024 * 128, "id": "VMdl"}),
            ("debug_partition", {"capacity": 1024 * 128}),
            ("ro_cfg_fs",       {"capacity": 1024 * 256,   "authenticate": False}),
            ("ro_fs",           {"capacity": 1024 * 2048,  "authenticate": False})
        ]
    }

    flash_device_parameters_4k = {
        "block_size": 4 * 1024,
        "boot_block_size": None,
        "alt_image_offset": 128 * 64 * 1024
    }

    # Tweaked version of flash layout from Earbud dev kit 1.0 - shrunk P1 to fit a stub app only, anyone
    # with a real P1 should be running their own flash layout anyway (ADK). This leaves plenty of room
    # for P0 and grants unallocated space at the end of the flash for running tests.
    default_flash0_4k = {
        "flash_device": flash_device_parameters_4k,
        "encrypt": False,
        "layout": [
            ("curator_fs",      {"capacity": 7 * 4 * 1024, "authenticate": False, "src_file_signed": False}),
            ("apps_p0",         {"capacity": 149 * 4 * 1024, "authenticate": False, "src_file_signed": False}),
            ("apps_p1",         {"capacity": 20 * 4 * 1024, "authenticate": False}),
            # Device config filesystem size limited by size of production test buffer.
            ("device_ro_fs",    {"capacity": 1 * 4 * 1024, "authenticate": False, "inline_auth_hash": True}),
            ("rw_config",       {"capacity": 3 * 4 * 1024}),
            ("rw_fs",           {"capacity": 1 * 4 * 1024}),
            ("ra_partition",    {"capacity": 1 * 4 * 1024}),
            ("ra_partition",    {"capacity": 1 * 4 * 1024, "id": "VMdl"}),
            ("debug_partition", {"capacity": 1 * 4 * 1024}),
            ("ro_cfg_fs",       {"capacity": 64 * 4 * 1024, "authenticate": False}),
            ("ro_fs",           {"capacity": 6 * 4 * 1024, "authenticate": False})
        ]
    }

    def __init__(self, flash_config=None, dma=None, file=None, apps_subsys=None,
                 read_from_sqif=False, select_bank="bank0", signed_xuv_file=None,
                 encrypt=None, section=None, use_4k_layout=False, device_size_kb=None,
                 sector_size_kb=None, force=False):
        """
        :param flash_config: Modified version of default_flash0 provided by the user.
        :param dma: Pylib call to the DMA hardware in Apps for encryption and authentication
        :param file: A file that can be included for use with encryption
        :param apps_subsys: Pylib call to the Apps Subsystem
        :param read_from_sqif: If True, read the SQIF Image Header for information
        :param select_bank: Specify whether the image will be flashed onto bank0 or bank1.
        :param signed_xuv_file: Specify the filepath to an XUV that will require authentication
        :param encrypt: Can be used to update flash_config to encrypt the contents of the image.
        :param section: Can be used to specify a single section to be flashed and authenticated.
        """

        self.boot_image = None
        self.use_4k_layout = use_4k_layout
        self.device_size_kb = device_size_kb
        self.force = force
        oem_authenticate = False

        # If custom flash configuration not provided by user, take a copy of the default for this sector size
        if flash_config is None:
            if self.use_4k_layout:
                self.config = copy.deepcopy(self.default_flash0_4k)
            else:
                self.config = copy.deepcopy(self.default_flash0)
            if sector_size_kb is not None:
                self.config["flash_device"]["block_size"] = sector_size_kb * 1024
        else:
            self.config = flash_config

        # Encryption can be enabled by the user and we need to update the config if so.
        if encrypt is not None:
            self.config["encrypt"] = encrypt

        # If we've been passed an instance of the apps subsystem object then we can check which chip
        # we're programming and conveniently enable OEM authentication for chips we know will need it.
        if apps_subsys is not None:
            oem_authenticate = apps_subsys.chip.name in ['QCC516x7x_QCC306x7x', 'Lora']

        # If you are looking to flash Apps P0 or the Curator filesystem, either may need
        # to be authenticated depending on the board eFuse settings.
        if signed_xuv_file is not None:
            if section == "apps_p0" or section is None:
                for name, params in self.config["layout"]:
                    if name == "apps_p0":
                        params["src_file_signed"] = True
                        params["authenticate"] = True
                        params["src_file"] = signed_xuv_file
                        params["oem_authenticate"] = oem_authenticate
            elif section == "curator_fs":
                for name, params in self.config["layout"]:
                    if name == "curator_fs":
                        params["src_file_signed"] = False
                        params["authenticate"] = True
                        params["src_file"] = signed_xuv_file

        self.dma = dma
        self.image_header = ImageHeader(self, apps_subsys=apps_subsys,
                                        read_from_sqif=read_from_sqif)
        self.select_bank = select_bank
        self.file = file
        if self.file is not None:
            # Create a temporary work folder
            self.dir = tempfile.mkdtemp()

    # See https://docs.python.org/2/library/struct.html#format-characters
    # for meaning of pack format characters.
    # We want to write the elements in the headers as little endian
    # uint32s
    pack_fmt = "<L"

    def build(self, section_name=None, read_nonce=None):
        self.flash_image = []
        if self.boot_image is None:
            # If we're using a default layout, use the correct one. We need to do this because
            # we could have previously been called with a different value of use_4k_layout
            # and self.config might contain the wrong layout.
            if self.config == self.default_flash0_4k or self.config == self.default_flash0:
                if self.use_4k_layout:
                    self.config = copy.deepcopy(self.default_flash0_4k)
                else:
                    self.config = copy.deepcopy(self.default_flash0)
            # We don't want to build another boot image if one exists, as the sector sizes might conflict
            self.boot_image = self._build_boot_image()
        pack_boundary = self.config["flash_device"]["block_size"] if not "pack" in self.config else 4096

        self.image = self._build_image(self.boot_image["size"], section_boundary=pack_boundary,
                                       nonce=read_nonce, section_name=section_name)

    def write_boot_image_xuv(self, filename):        
        '''
        Create an xuv file containing the boot image that contains the 
        offset to the primary image address.
        '''
        xuv = XUVStreamEncoder(open(filename, "w"))
        self._write_boot_image_to_xuv_object(xuv)
        
    def write_image_xuv(self, filename, include_header=True, alt_bank=None):
        '''
        Create an xuv file with or without image header and containing the image offset 
        to the primary image address.
        '''
        xuv = XUVStreamEncoder(open(filename, "w"))
        self._write_image_to_xuv_object(xuv, include_header, alt_bank=alt_bank)
        
    def write_complete_flash_image_xuv(self, filename, alt_bank=None):
        '''
        Creates an xuv file with both the image header and the image data
        for writing onto an empty flash device
        '''
        xuv = XUVStreamEncoder(open(filename, "w"))
        self._write_boot_image_to_xuv_object(xuv)
        self._write_image_to_xuv_object(xuv, alt_bank=alt_bank)
       
    def _write_image_to_xuv_object(self, xuv, include_header=True, alt_bank=None):
        #Get the byte offset for the input bank.
        byte_addr_offset = self._get_byte_address_offset(alt_bank)

        for byte_address,data,name in self.image:
            if name == "image_header" and include_header == False:
                continue
            xuv.write_byte_array((byte_address + byte_addr_offset)//2, data)

    def write_image_section_xuv_files(self, filename_prefix, alt_bank=None):
        '''
        Creates xuv files for all the image sections apart from the RW ones
        for writing onto an empty flash device.
        The xuv files are created as filename_prefix_<<section>>.xuv
        '''

        #Get the byte offset for the input bank.
        byte_addr_offset = self._get_byte_address_offset(alt_bank)

        #Generate XUV files for the image sections specified
        #in the input flash_layout_config file.
        for byte_address, data, name in self.image:
            section_filename = filename_prefix + "_" + name + ".xuv"
            xuv = XUVStreamEncoder(open(section_filename, "w"))
            xuv.write_byte_array((byte_address + byte_addr_offset)//2, data)

    def _write_boot_image_to_xuv_object(self, xuv):
        xuv.write_byte_array(self.boot_image["offset"]//2,
                             self.boot_image["content"])

    def _build_boot_image(self):
        '''
        Build a byte array of the image table that is placed at the start of
        the flash device. This gives the offset to the primary and alternative
        image headers.
        '''
        boot_image_size = self.config["flash_device"]["boot_block_size"] or \
                          self.config["flash_device"]["block_size"]
        primary_image_offset = boot_image_size
        alternate_image_offset = self.config["flash_device"]["alt_image_offset"] + boot_image_size

        boot_image = bytearray(
            # Start Signature
            self.magic_numbers["BOOT_IMAGE_START"] +
            # Bank0 image offset
            struct.pack(self.pack_fmt, primary_image_offset) +
            # Bank1 image offset
            struct.pack(self.pack_fmt, alternate_image_offset)
        )

        dfu_status = struct.pack(self.pack_fmt, 0xffffffff) * 64
        sha_offset = (len(boot_image) + len(struct.pack(self.pack_fmt, 0)) + len(dfu_status))
        boot_image += bytearray(
            # SHA key offset
            struct.pack(self.pack_fmt, sha_offset) +
            # 64 entries of DFU status
            dfu_status +
            # 8 x 3 entries of SHA key0, key1 & key2
            struct.pack(self.pack_fmt, 0xffffffff) * 24 +
            # End Signature
            self.magic_numbers["BOOT_IMAGE_END"]
        )

        return {
            "offset"        : 0,
            "size"          : boot_image_size,
            "content"       : boot_image,
            "primary_image_offset" : primary_image_offset,
            "alternate_image_offset" : alternate_image_offset
            }

    def parse_boot_image(self, boot_image):
        self.boot_image = decode_boot_image(boot_image)

    def _build_image(self, image_offset, section_boundary=4096,
                     nonce=None, section_name=None):
        '''
        Return a list of offsets, data and name corresponding to the image.
        The first entry will be the image header which is a table of
        sections plus the authentication hashes.
        The offsets will be adjusted by the image_offset parameter
        to account for the position of the image within the flash
        memory device (i.e. advanced from 0 to account for the boot
        image). 
        The data will be encrypted if necessary.
        The name of the section is returned in the list so that it
        would be possible to generate a xuv file for each image section.
        The image produced will be identical at this stage regardless 
        of which image bank is to be used. That is only applicable when
        creating an xuv from the image. 
        '''
        sections = self._read_sections()
        self.image_header.build(sections, round_size=section_boundary,
                                nonce=nonce, section_name=section_name)
        self.sections = [self.image_header.section_data] + sections
        if "encrypt" in self.config and self.config["encrypt"] is True:
            for s in self.sections:
                if "content" in s:
                    # Check if the section can be encrypted 
                    if s["name"] in self.non_encryptable_sections:
                        s["encrypted_content"] = s["content"]
                    else:
                        clear_size = 16 if s["name"] == "image_header" else 0
                        # Create a different nonce for the Audio SQIF.
                        if s["name"] == "audio":
                            self.audio_nonce = [random.randrange(256) for dummy in range(16)]
                            iprint("Audio SQIF encryption Nonce %s" % "".join(["%02x" % a for a in self.audio_nonce]))

                        s["encrypted_content"] = self._encrypt_data(s, image_offset, clear_size)

                        # If encryption is enabled, then the nonce for the Audio SQIF
                        # is added at the end of the encrypted audio image.
                        if s["name"] == "audio":
                            s["encrypted_content"] = s["encrypted_content"] + bytearray(self.audio_nonce)

            output_name = "encrypted_content"
        else:
            output_name = "content"

        return [(s["offset"] + image_offset, s[output_name], s["name"])
                for s in self.sections if output_name in s]

    def _read_sections(self):
        '''
        Read source data for each section in the layout file
        '''
        sections = []
        for name, params in self.config["layout"]:
            section = { "name" : name, 
                       "params" : params,
                       "capacity" : params["capacity"]}

            if "src_file" in params:
                image = None
                if params["src_file"].endswith(".xuv"):
                    image = self.read_xuv_file(params["src_file"])
                elif params["src_file"].endswith(".elf"):
                    image = self.read_elf_file(params["src_file"])
                if image:
                    section["content"] = image
                    section["size"] = len(image)

                    # If a src_file is requested with a size that is too large,
                    # we raise a suitable exception
                    if section["size"] > section["capacity"]:
                        err_str = "Image for section {} with size {} exceeds section's capacity of {}".format(
                            section["name"], section["size"], section["capacity"])
                        raise CapacityError(err_str)

                else:
                    section["size"] = params["capacity"]
            elif "capacity" in params:
                # So that new firmware can work with old flash headers the size should be
                # equal to the capacity when an image is not specified.
                section["size"] = section["capacity"]
            else:
                iprint("Section '%s' = '%s' not yet handled" % (name, params))
            sections.append(section)
        return sections

    def read_elf_file(self, filename):
        elfcode = ElfCodeReader(filename, Kalimba32CoreInfo().layout_info)
        return self.read_loadable(elfcode.sections, verbose=True)

    def read_loadable(self, loadable, verbose=False):
        return loadable_to_bytes(loadable, verbose)

    def read_xuv_file(self, filename):
        xuv = XUVStreamDecoder(open(filename, "r"))
        start_byte_addr, output_bytes, padding_list = xuv.contiguous_byte_data()
        if padding_list:
            iprint("Padding %s address 0x%x with %s" % (filename, 
                                                       start_byte_addr, 
                                                       padding_list))
        if start_byte_addr:
            iprint("Removing offset of 0x%x from %s" % (start_byte_addr, 
                                                       filename))
        return output_bytes

    def _get_byte_address_offset(self,alt_bank=None):
        '''
        Return the byte address offset based on the input image bank.
        '''
        if alt_bank == True or (alt_bank == None and
                        self.select_bank == "bank1"):
            byte_addr_offset = self.config["flash_device"]["alt_image_offset"]
        else:
            byte_addr_offset = 0

        return byte_addr_offset
            
    def _encrypt_data_dma(self, section, image_offset, initial_clear_size=0):
        #If the image is audio, then set the offset to 0 for the Audio SQIF
        #and use the nonce created for the audio image.
        if section["name"] == "audio":
            byte_address = 0
            nonce = self.audio_nonce
        else:
            byte_address = image_offset + section["offset"]
            nonce = self.image_header.nonce

        iprint("Encrypting '%s' at offset 0x%x" % (section["name"], byte_address))
        chunk_size = min(16 * 1024, self.dma.max_chunk_size)
        encrypted_data = bytearray(section["content"][0:initial_clear_size])
        for i in range(initial_clear_size, len(section["content"]), chunk_size):
            encrypted_data += bytearray(self.dma.encrypt_for_sqif(
                                    section["content"][i:i+chunk_size], 
                                    None, 
                                    nonce,
                                    (byte_address + i) >> 4))
        
        return encrypted_data

    def _encrypt_data_file(self, section, image_offset, initial_clear_size=0):
        gstrm.iout.flush()

        #Audio image is encrypted from offset 0, and use the nonce created for
        #the Audio SQIF.
        if section["name"] == "audio":
            byte_address = 0
            byte_addr_offset = 0
            nonce = self.audio_nonce
        else:
            nonce = self.image_header.nonce
            byte_address = image_offset + section["offset"]
            byte_addr_offset = self._get_byte_address_offset()

        iprint("Encrypting '%s' at offset 0x%x using %s" % (section["name"], byte_address, self.file))
        unencrypted = bytearray(section["content"][0:initial_clear_size])
        section_infile = os.path.join(self.dir, "in" + "_" + section["name"] + ".xuv")
        section_outfile = os.path.join(self.dir, "out" + "_" + section["name"] + ".xuv")
        if os.path.isfile(section_outfile):
            # The output file has bee left behind so remove before trying to create again
            os.remove(section_outfile)
        nonce_file = os.path.join(self.dir, "nonce.txt")
        with open(section_infile, 'w') as f:
            xuv = XUVStreamEncoder(f)
            xuv.write_byte_array((byte_address + byte_addr_offset)//2,
                section["content"])
        with open(nonce_file, 'w') as f:
            for i in range (len(nonce)):
                f.write("%02X" % (nonce[i]))
        cmd_line = [self.config["host_tools"]["SecurityCmd"], "encrypt",
            section_infile, section_outfile, self.file, nonce_file,
            hex(byte_address)[2:], "-product", "hyd", "-endian", "L"]
        iprint("Invoking '%s'\n" % " ".join(cmd_line))
        gstrm.iout.flush()
        retval = subprocess.call(cmd_line)
        gstrm.iout.flush()
        encrypted_data = None
        if retval == 0:
            if os.path.isfile(section_outfile):
                with open(section_outfile, "r") as f:
                    xuv = XUVStreamDecoder(f)
                    start_byte_addr, encrypted, padding_list = xuv.contiguous_byte_data()
                encrypted_data = unencrypted + encrypted[initial_clear_size:]
        gstrm.iout.flush()
        return encrypted_data

    def _encrypt_data(self, section, image_offset, initial_clear_size=0):
        if self.dma is not None:
            return self._encrypt_data_dma(section, image_offset, initial_clear_size)
        elif self.file is not None:
            return self._encrypt_data_file(section, image_offset, initial_clear_size)

        return None

    def _calculate_hash_dma(self, data, name):
        '''
        Use the chip DMA hardware to calculate the image hash using the
        efuse security key. The data must be a multiple of 128bits (16 bytes)
        in length
        '''
        nonce = [0] * 16
        iv = nonce

        chunk_size = min(32 * 1024, self.dma.max_chunk_size)
        for i in range(0, len(data), chunk_size):
            iv = self.dma.authenticate_data(data[i:i+chunk_size], None, iv)
        iprint("Section %s len %d : hash %s" % (name, len(data), 
                                         "".join(["%02x" % a for a in iv])))
        return bytearray(nonce) + bytearray(iv)

    def _calculate_hash_file(self, data, name):
        '''
        Use the SecurityCmd software to calculate the image hash using the
        encryption ley file. The data must be a multiple of 128bits (16 bytes)
        in length.
        '''
        nonce = [0] * 16
        iv = nonce
        gstrm.iout.flush()
        infile = os.path.join(self.dir, "in_hash_" + name + ".xuv")
        outfile = os.path.join(self.dir, "out_hash_" + name + ".xuv")
        if os.path.isfile(outfile):
            # The output file has bee left behind so remove before trying to create again
            os.remove(outfile)
        with open(infile, 'w') as f:
            xuv = XUVStreamEncoder(f)
            xuv.write_byte_array(0, data)
        cmd_line = [self.config["host_tools"]["SecurityCmd"], "createcbcmac",
            infile, outfile, self.file, "-product", "hyd", "-endian", "L"]
        iprint("Invoking '%s'\n" % " ".join(cmd_line))
        gstrm.iout.flush()
        retval = subprocess.call(cmd_line)
        gstrm.iout.flush()
        if retval == 0:
            if os.path.isfile(outfile):
                with open(outfile, "r") as f:
                    xuv = XUVStreamDecoder(f)
                    start_byte_addr, iv, padding_list = xuv.contiguous_byte_data()
        gstrm.iout.flush()
        iprint("Section %s len %d : hash %s" % (name, len(data), 
                                         "".join(["%02x" % a for a in iv])))
        return bytearray(nonce) + bytearray(iv)

    def _get_hash_signed(self, data, name):
        '''
        Get authentication hash 128 bits (16 bytes) from xuv file. 
        '''
        nonce = [0] * 16
        
        auth_hash = data[-16:]
        iprint("Section %s len %d : hash %s" % (name, len(data), 
                                   "".join(["%02x" % a for a in auth_hash])))
        return bytearray(nonce) + bytearray(auth_hash)


    def _calculate_hash(self, data, name):
        '''
        Use the chip DMA hardware to calculate the image hash using the
        efuse security key, or SecurityCmd software. The data must be a
        multiple of 128bits (16 bytes) in length
        '''
        if self.dma is not None:
            return self._calculate_hash_dma(data, name)
        elif self.file is not None:
            return self._calculate_hash_file(data, name)
        else:
            return bytearray([0xff]*32)

    class BootImageError(RuntimeError):
        pass
    

class ImageHeader(object):
    def __init__(self, parent=None, apps_subsys=None, read_from_sqif=False):
        self.parent = parent
        self.magic_numbers = ImageBuilder.magic_numbers
        self.section_ids = ImageBuilder.section_ids
        self.element_ids = ImageBuilder.element_ids
        self.partition_aliases = ImageBuilder.partition_aliases
        self.pack_fmt = ImageBuilder.pack_fmt
        self.old_header_information = []
        
        self.section_names = create_reverse_lookup(self.section_ids)
        self.element_names = create_reverse_lookup(self.element_ids)
        self.nonce_size = 16
        self.auth_hash_size = 32     # nonce plus hash for authentication
        self.entry_size = 8          # 4 bytes for key, 4 for value

        if read_from_sqif:
            self.sqif_addr, self.sqif_blk_id = apps_subsys.sqif_trb_address_block_id[0]
            sqif_mem_attr = "raw_sqif%d" % self.sqif_blk_id
            self._sqif_mem = getattr(apps_subsys, sqif_mem_attr)
            self.init_from_sqif(apps_subsys.core)
            
    def init_from_sqif(self, apps=None):
        
        if not apps:
            encrypted = False
        else:
            encrypted = True
        if encrypted:
            # Use cleartext area to allow us to read the boot image
            clear_base = apps.fields.READ_DECRYPT_CLEARTEXT_BASE.read()
            clear_size = apps.fields.READ_DECRYPT_CLEARTEXT_SIZE.read()
            apps.fields.READ_DECRYPT_CLEARTEXT_BASE.write(0)
            apps.fields.READ_DECRYPT_CLEARTEXT_SIZE.write(1)
            
        boot_image_data = self._sqif_mem[0:32]
        
        if encrypted:
            # Put the cleartext area back again
            apps.fields.READ_DECRYPT_CLEARTEXT_BASE.write(clear_base)
            apps.fields.READ_DECRYPT_CLEARTEXT_SIZE.write(clear_size)
            
        self.parent.parse_boot_image(boot_image_data)
        self.flash_offset = self.parent.boot_image["primary_image_offset"]
        img_hdr = self._sqif_mem[self.flash_offset:self.flash_offset+1024]
        self._parse_bytes(img_hdr)
        
    def build(self, sections, round_size=4096, nonce=None, section_name=None):
        '''
        Build and return a header section for the given sections.
        All sections will be updated with their offset within the
        image and a pointer to the authentication hash in the hash table.
        '''

        # If nonce is not provided we generate a 16 byte random nonce
        self.nonce = nonce or [random.randrange(256) for dummy in range(16)]
        iprint("Apps SQIF encryption Nonce %s" % "".join(["%02x" % a for a in self.nonce]))
        self.flash_offset = self.parent.boot_image["primary_image_offset"]

        # Count the number of authentication lines we will need based on flash_config
        auth_hash_count = len([s for s in sections if 
                               s["params"].get("authenticate")])
        auth_table_hash_count = len([s for s in sections if
                                     s["params"].get("authenticate") and
                                     not s["params"].get("inline_auth_hash")])

        # self.old_header_information contains the header information currently on the SQIF
        # before we create a new image header. When ImageHeader() is initialised, this variable
        # is populated in the method _parse_bytes().
        self.old_section_details = {}
        for section_details in self.old_header_information:
            # We only want the hash information of sections we aren't going to
            # re-authenticate and flash in this method.
            if section_name != section_details["name"] and 'hash_ptr' in section_details:
                self.old_section_details.update({section_details["name"]: section_details})

        # If we are only authenticating a single section, we will need this
        # hash plus any others that already exist in the image header.
        auth_hash_count += len(self.old_section_details)
        auth_table_hash_count += len(self.old_section_details)
        if section_name in self.old_section_details:
            # If section name is specified, then we are flashing a new image,
            # so we can now ignore previous authentication information for this section.
            auth_hash_count -= 1
            auth_table_hash_count -= 1

        # Quick scan to see if any sections have an 'id' entry.
        id_count = 0
        for section in sections:
            if section["params"].get("id") or "id" in section:
                id_count += 1

        image_header_size_guess = (
                        # Nonce precedes image header but we include it in the size definition.
                        self.nonce_size +
                        # Three entries to give offset, size and capacity for each section
                        len(sections) * self.entry_size * 3 +
                        # Some sections may have an extra 'id' entry
                        id_count * self.entry_size +
                        # Two entries to specify hash location for auth sections
                        auth_hash_count * self.entry_size * 2 +
                        # total size entry + dfu_header_hash_len + end entry
                        self.entry_size * 3
                        )

        # We put authentication hashes immediately after the image header table
        hash_table_size = auth_table_hash_count * self.auth_hash_size

        hash_offset = self.round_up_to_next_boundary(image_header_size_guess, 
                                                     self.auth_hash_size)
        hash_ptr = hash_offset
        hash_table = bytearray()
        image_offset = self.round_up_to_next_boundary(hash_offset + 
                                                      hash_table_size, 
                                                      round_size)
        section_offset = image_offset

        if int(self.flash_offset) > round_size:
            iprint ("WARNING: Boot sector is larger than smallest erasable sector, %d sectors have been wasted. "
                   "Erase flash and try again to recover them." % ((int(self.flash_offset) - round_size) // round_size))

        # Temporary storage to facilitate the potential removal of a section in the following for loop.
        new_sections = []

        for section in sections:
            # If we parsed an existing image header, honour it. Change the offset and capacity of
            # each section to match what's already in the header.

            # Promote the id param to a 'real' element in the case that it came from a config
            # file - if the section came from a flash image header, it already is a real element.
            if section["params"].get("id"):
                section["id"] = section["params"].get("id")

            try:
                if "id" in section:
                    image_header_details = self.get_section_details(section["name"], section["id"])
                else:
                    image_header_details = self.get_section_details(section["name"])
                section_offset = image_header_details["offset"]
                section["capacity"] = image_header_details["capacity"]
                section["params"]["capacity"] = image_header_details["capacity"]
            except AttributeError:
                # The get_section_details() call above will raise an AttributeError exception
                # if there was no image header to parse - that's fine, continue using the
                # layout we were given for this section.
                pass
            except TypeError:
                # If the section we were trying to get details for isn't present in the image header
                # we parsed, we'll end up here. We can't just cram a new section into an existing
                # image so remove this section from the image we're about to program.
                iprint ("Section '%s' has been skipped because no space has been allocated for it in "
                       "the existing on-chip image header." % section["name"])

                # We need to remove this section but removing an item from the list we're iterating
                # over is a bad idea... Instead, just leave it out of the 'new_sections' list. When
                # we reach the end of this for loop, 'sections' will be reassigned to 'new_sections'.

                # Adjust the guess we made earlier to acount for the fact we removed a section
                image_header_size_guess -= self.entry_size * 3
                if "id" in section:
                    image_header_size_guess -= self.entry_size * 1
                # And skip processing this section any further
                continue

            # If encryption is enabled, then the nonce (with length 16 bytes) for the Audio SQIF
            # is added at the end of the encrypted audio image. So, set the audio image size accordingly.
            if "encrypt" in self.parent.config and self.parent.config["encrypt"] is True:
                if "audio" in section["name"]:
                    section["size"] += 16

            # If there is a hash for this section in our authentication table then
            # we will append something, whether it's this one or updated.
            if section["params"].get("authenticate") or section["name"] in self.old_section_details:
                if section["params"].get("inline_auth_hash"):
                    # inline_auth_hash means that we authenticate the
                    # whole capacity of the section apart from the last 
                    # 32 bytes which we set to the expected hash
                    auth_length = section["capacity"] - self.auth_hash_size
                else:
                    if section["params"].get("src_file_signed"):
                        section["size"] -= 16
                    auth_length = self.round_up_to_next_boundary(
                                            section["size"], self.nonce_size)
                length_of_padding = auth_length - section["size"]
                if length_of_padding:
                    # Add deterministic padding
                    state = random.getstate()
                    random.seed(length_of_padding)
                    try:
                        section["content"] += bytearray([random.randint(0, 255)
                                                         for dummy in range(length_of_padding)])
                    except KeyError:
                        # No source data specified
                        pass
                    random.setstate(state)
                section["size"] = auth_length
                section["capacity"] = max(section["capacity"], section["size"])

            section["capacity"] = self.round_up_to_next_boundary(section["capacity"],
                                                                 round_size)
            if "rw_" in section["name"] or "ra_" in section["name"] or "debug_" in section["name"]:
                # Writable sections must be on flash block boundaries
                section_offset = self.round_up_to_next_boundary(
                                            section_offset, 
                                            self.parent.config["flash_device"]
                                                                ["block_size"])

            if section["params"].get("authenticate") or section["name"] in self.old_section_details:
                section["hash_size"] = auth_length
                hash_add = bytearray()

                if section["params"].get("inline_auth_hash"):
                    # inline_auth_hash means that we store the hash
                    # in the last 32 bytes of the section with the
                    # section size being the capacity defined in the
                    # config (which is not the same as the capacity 
                    # entry in the image header table).
                    section["hash_ptr"] = section_offset + auth_length
                    try:
                        section["content"] += self.parent._calculate_hash(
                            section["content"], section["name"])
                    except KeyError:
                        pass # No source data specified
                    section["size"] += self.auth_hash_size
                else:
                    # Store the hash in the hash table
                    section["hash_ptr"] = hash_ptr
                    hash_ptr += self.auth_hash_size
                    if section["params"].get("custom_hash"):
                        # If the user has provided a custom authentication hash then use it.
                        custom_hash = section["params"].get("custom_hash")
                        iprint("Loading the custom hash {} for {}".format(custom_hash,
                                                                          section["name"]))
                        hash_prepend = bytearray([0] * 16)
                        hash_add = hash_prepend + bytearray.fromhex(custom_hash)
                    # if "src_file_signed" parameter is set then auth hash to be extracted from xuv
                    elif section["params"].get("src_file_signed"):
                        try:
                            hash_add = self.parent._get_hash_signed(section["content"], section["name"])
                            # update xuv content to remove last 16 bytes
                            del section["content"][-16:]
                        except KeyError:
                            # No source data specified
                            pass
                    elif section["name"] in self.old_section_details:
                        # Authentication hash in header is defined to be 32 bytes long, as the hash itself
                        # is 16 bytes, we prepend 16 bytes of zeroes.
                        hash_prepend = bytearray([0] * 16)
                        hash_add = hash_prepend + self.old_section_details[section["name"]]["hash_value"]
                    else:
                        hash_add = self.parent._calculate_hash(section["content"], section["name"])

                    hash_table += hash_add

                    if ((section["params"].get("oem_authenticate") and self.parent.config["encrypt"]) or
                        (section["name"] in self.old_section_details and "oem_hash_ptr" in self.old_section_details[section["name"]])):

                        section["oem_hash_ptr"] = hash_ptr
                        hash_ptr += self.auth_hash_size
                        image_header_size_guess += self.entry_size
                        if section["params"].get("custom_oem_hash"):
                            # If the user has provided a custom authentication hash then use it.
                            custom_oem_hash = section["params"].get("custom_oem_hash")
                            iprint("Loading the custom OEM hash {} for {}".format(custom_oem_hash,
                                                                                  section["name"]))
                            hash_prepend = bytearray([0] * 16)
                            hash_table += hash_prepend + bytearray.fromhex(custom_oem_hash)
                        elif (section["name"] in self.old_section_details and
                              "oem_hash_value" in self.old_section_details[section["name"]]):
                            hash_prepend = bytearray([0] * 16)
                            hash_table += hash_prepend + self.old_section_details[section["name"]]["oem_hash_value"]
                        else:
                            hash_table += self.parent._calculate_hash(hash_add[self.nonce_size:], section["name"] + "(OEM auth)")

            section["offset"] = section_offset
            section["unused"] = section["capacity"] - section["size"]
            section_offset += section["capacity"]

            # If, for example, we are flashing a new curator_fs we need to maintain the size/offset/etc
            # information about other existing sections on the SQIF, otherwise we lose that information.
            if section["name"] in self.old_section_details:
                section["size"] = self.old_section_details[section["name"]]["size"]
                section["offset"] = self.old_section_details[section["name"]]["offset"]
                section["hash_size"] = self.old_section_details[section["name"]]["hash_size"]

            # Manage partitions aliases
            if section["name"] in self.partition_aliases:
                section_name_alias = self.partition_aliases[section["name"]]
                section["name"] = section_name_alias
                
            # We've finished processing 'section' in this loop so add it to the 'new_sections' list.
            new_sections.append(section)

        # 'new_sections' will contain only the elements we want to keep. Reassign 'sections' to it
        # so that the rest of the function will work with the new list.
        sections = new_sections

        header = bytearray(self.nonce)
        header += self.magic_numbers["IMAGE_HEADER_START"]
        for section in sections:
            # If we were provided the device size, we can check to make sure that we aren't
            # going to overrun the flash array.
            if self.parent.device_size_kb is not None:
                remaining_flash = (self.parent.device_size_kb * 1024) - \
                                  (section["offset"] + section["capacity"])
                if remaining_flash < 0:
                    if self.parent.force:
                        iprint ("WARNING: '%s' overruns the flash array by %d bytes. This has been permitted because "
                               "force=True was passed.\nConsider using 4K sectors and a 4K flash layout "
                               "(use_4k_layout=True) or providing your own flash layout.\n" %
                               (section["name"], abs(remaining_flash)))
                    else:
                        raise CapacityError("'%s' overruns the flash array by %d bytes. You can use a 4K flash layout "
                                            "by passing use_4k_layout=True or ignore this check by passing "
                                            "force=True." % (section["name"], abs(remaining_flash)))

            header += self.section_entry(section, "offset")
            header += self.section_entry(section, "size")
            header += self.section_entry(section, "hash_ptr")
            header += self.section_entry(section, "hash_size")
            header += self.section_entry(section, "capacity")
            header += self.section_entry(section, "oem_hash_ptr")
            if "id" in section:
                header += struct.pack(self.pack_fmt, self.entry_id(section["name"], "id"))
                if not isinstance(section["id"], (bytes, bytearray)):
                    header += bytearray(section["id"][0:4], 'UTF-8')
                else:
                    header += section["id"][0:4]

        #Calculate the SHA-256 hash length of the image header for DFU,
        #and insert it in the image header
        #SHA-256 hash length = (image header size - AES-CTR nonce length) +
        #                      AES-CBC hash offset padding + AES-CBC hash table

        #First get the total AES-CBC authentication hash length including
        #the offset padding
        if len(hash_table):
            hash_length = (hash_offset - image_header_size_guess) + len(hash_table)
        else:
            hash_length = 0

        #Reduce the length of AES-CTR nonce as it is not included in the
        #DFU image header SHA-256 hashing.
        dfu_header_hash_length = image_header_size_guess + hash_length - len(self.nonce)
        header += self.entry("dfu_header_hash_len", "size", dfu_header_hash_length)

        header += self.entry("total_image", "size", section_offset)
        header += self.magic_numbers["IMAGE_HEADER_END"]

        if len(header) != image_header_size_guess:
            raise RuntimeError("Image header length:%d not matching to guessed length:%d"
                               % (len(header), image_header_size_guess))

        #Verify if the RW image sections and debug section are contiguous when the QSPI is         
        #encrypted, so that the firmware can apply the single decrypt cleartext window over
        #all of them.
        if "encrypt" in self.parent.config and self.parent.config["encrypt"] is True:
            next_expected_offset = 0
            for section in sections:
                if "rw_" in section["name"] or "ra_" in section["name"] or "debug_" in section["name"]:
                    if next_expected_offset and next_expected_offset != section["offset"]:
                        raise RuntimeError("RW section:%s not contiguous in flash layout" % (section["name"]))
                    next_expected_offset = section["offset"] + section["capacity"]

        #Verify if the Apps P0 image section is placed before all other (apart from Curator FS) image sections
        #in the input "flash_layout_config".
        misplaced_sections = []
        apps_p0_found = False
        for section in sections:
            if section["name"] == "apps_p0":
                apps_p0_found = True
            elif section["name"] != "curator_fs":
                if apps_p0_found == False:
                    misplaced_sections.append(section["name"])

        if len(misplaced_sections):
            raise RuntimeError("Image sections:%s have to be placed before the Apps P0 "
                               "section in the input flash_layout_config" %
                               str(misplaced_sections).strip('[]'))

        # Verify that Apps P1 is placed before the read only file system for FileMap support.
        for section in sections:
            if section["name"] == "apps_p1":
                break
            if section["name"] == "ro_fs":
                raise RuntimeError("Image section:ro_fs has to be placed after the Apps P1 section in the input flash_layout_config")

        # Pad up to hash table if authentication hash table is to be inserted
        if len(hash_table):
            header += bytearray([0] * (hash_offset - len(header)))
            # Add the hash table
            header += hash_table

        self.sections = sections
        
        self.section_data =  {
                "name"      : "image_header",
                "offset"    : 0,
                "size"      : len(header),
                "unused"    : image_offset - len(header),
                "content"   : header 
                }
        
    def section_entry(self, section_dict, entry_name):
        if entry_name in section_dict:
            return self.entry(section_dict["name"], 
                              entry_name, 
                              section_dict[entry_name])
        return bytearray()

    def entry(self, section_name, entry_name, value):
        return self._uint32_kv(self.entry_id(section_name, entry_name), value)

    def _names_from_entry_key(self, key):
        section_name = self.section_names[self.section_ids["mask"] & key]
        element_name = self.element_names[self.element_ids["mask"] & key]
        return section_name, element_name
        
    def entry_id(self, section_name, element):
        return self.section_ids[section_name] + self.element_ids[element]

    def round_up_to_next_boundary(self, value, boundary):
        return ((value + (boundary-1))//boundary) * boundary

    def _uint32_kv(self, key, value):
        return (struct.pack(self.pack_fmt, key) + 
                struct.pack(self.pack_fmt, value))
    
    def _kv_from_entry_bytes(self, entry_bytes):
        return (struct.unpack(self.pack_fmt, entry_bytes[0:4])[0],
                struct.unpack(self.pack_fmt, entry_bytes[4:8])[0])

    def _parse_bytes(self, header_bytes):
        '''
        Interpret the header bytes read from the flash into a table that
        we can extract offsets and sizes from.
        '''
        second_ra = False
        header_bytes = bytearray(header_bytes)
        offset = 0
        self.nonce = header_bytes[offset:self.nonce_size]
        offset += self.nonce_size
        magic_size = len(self.magic_numbers["IMAGE_HEADER_START"])
        magic = header_bytes[offset:offset+magic_size]
        offset += magic_size
        if magic != self.magic_numbers["IMAGE_HEADER_START"]:
            raise self.ImageHeaderError("Invalid image header '%s'" % magic)
        sections = dict()
        while True:
            entry_bytes = header_bytes[offset: offset+self.entry_size]
            offset += self.entry_size
            if entry_bytes[0:4] == self.magic_numbers["IMAGE_HEADER_END"]:
                break
            entry_key, entry_value = self._kv_from_entry_bytes(entry_bytes)
            section_name, element_name = self._names_from_entry_key(entry_key)
            if not section_name in sections:
                sections[section_name] = dict()
            # Convert an ID entry_value into a 4 character string
            if element_name == "id":
                entry_value = str(bytearray(flatten_le(entry_value,num_words=4,word_width=8)).decode())
            # Store the element
            if element_name in sections[section_name]:
                if section_name == "ra_partition":
                    second_ra = True
                else:
                    second_ra = False

            if section_name == "ra_partition" and second_ra is True:
                section_name = "vmodel_partition"

            if not section_name in sections:
                sections[section_name] = dict()
            sections[section_name][element_name] = entry_value

        for key,value in sections.items():
            if key == "vmodel_partition":
                key = "ra_partition"
            value.update({"name":key})
        self.sections = sorted(list(sections.values()),
                               key=lambda a: a["offset"] if "offset" in a else 1<<32)

        for s in sections:
            if "capacity" in sections[s]:
                sections[s]["unused"] = sections[s]["capacity"] - \
                                        sections[s]["size"]

        # Store the header information currently on the SQIF. Later when we build a
        # new header we can put some of this information back so that we don't have
        # to re-authenticate Apps, for example.
        self.old_header_information = self.sections
        for section_details in self.old_header_information:
            if 'hash_ptr' in section_details:
                section_details.update(
                    {'hash_value': header_bytes[
                                   section_details['hash_ptr']+16:
                                   section_details['hash_ptr']+32]})
            if 'oem_hash_ptr' in section_details:
                section_details.update(
                    {'oem_hash_value': header_bytes[
                                       section_details['oem_hash_ptr']+16:
                                       section_details['oem_hash_ptr']+32]})
        # Older image headers do not contain 'capacity' so the 'unused'
        # amount needs to be derived from the offsets.
        for s_no in range(len(sections)-2):
            if "capacity" not in self.sections[s_no]:
                self.sections[s_no]["unused"] = \
                    self.sections[s_no+1]["offset"] - (
                    self.sections[s_no]["offset"] +
                    self.sections[s_no]["size"])

    def get_section_details(self, section_name, id_name=None):
        for s in self.sections:
            if s["name"] == section_name and not id_name and "id" not in s:
                return s
            if s["name"] == section_name and id_name and "id" in s and s["id"] == id_name:
                return s
    
    def __repr__(self):
        output = ""
        for section in self.sections:
            if "offset" in section:
                output += "Offset 0x%08x"    % (section["offset"])
                output += " %-18s"           % (section["name"])
                # Older image headers do not contain 'capacity'
                if "capacity" in section:
                    output += "  capacity 0x%08x" % (section["capacity"])
                output += "  size 0x%08x"    % (section["size"])
                if "unused" in section:
                    output += "  unused 0x%08x"  % (section["unused"])
                if "id" in section:
                    output += "  id '%s'" % (section["id"])
            else:
                output += "%-18s"        % (section["name"])
                output += " size 0x%08x" % (section["size"])
            output += "\n"

        for section in self.sections:
            if "hash_ptr" in section:
                output += " %-18s"          % (section["name"])
                output += " Hash ptr %8s"   % (hex(section["hash_ptr"]))
                output += " Hash size %8s"  % (hex(section["hash_size"]))
                output += "\n"
        return output

    class ImageHeaderError(RuntimeError):
        pass

    
