############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

"""
  HydraAppsSQIF
     HydraAppsSQIFBootImage
          [valid/invalid]
     HydraAppsSQIFImageBank (primary=True)
        HydraAppsSQIFPartition # curator_fs
           content = HydraROFS
        HydraAppsSQIFPartition # apps_p0
           content = bytes
        ...


     HydraAppsSQIFImageBank (secondary=True)
        HydraAppsSQIFPartition # curator_fs
          ...
"""

import struct

from csr.dev.hw.address_space import AddressMap
from csr.wheels.bitsandbobs import build_le, create_reverse_lookup, \
    flatten_le
from csr.dev.hw.core.meta.i_layout_info import Kalimba32DataInfo
from csr.dev.model import interface
from csr.dev.fw.psflash import PsflashContent, RwConfigNotPresent
from csr.dev.fw.meta.read_only_fs import RawSQIFReadOnlyLEFS, \
    CuratorReadOnlyLEFS
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.wheels import gstrm


magic_numbers = {"BOOT_IMAGE_START": b"IMGE",
                 "BOOT_IMAGE_END": b"END\0",
                 "IMAGE_HEADER_START": b"Imag",
                 "IMAGE_HEADER_END": b"END\0"
                 }
section_ids = {
    "vmodel_partition": 0xfe00,  # Virtual partition. Does not exist on flash
    "p1_header_ptr": 0x0f00,
    "curator_fs": 0x0e00,
    "apps_p0": 0x0d00,
    "apps_p1": 0x0c00,
    "ro_fs": 0x0b00,
    "debug_partition": 0x0a00,
    "device_ro_fs": 0x0900,
    "rw_config": 0x0800,
    "rw_fs": 0x0700,
    "total_image": 0x0600,
    "ro_cfg_fs": 0x0500,
    "dfu_header_hash_len": 0x0400,
    "audio": 0x0300,
    "ra_partition": 0x0200,

    # Used to mask the section id from the key
    "mask": 0xff00
}
element_ids = {
    # Unused
    None: 0x00,
    # An offset from the start of the flash image to this section.
    "offset": 0x01,
    # The amount of supplied data stored in this section in bytes.
    # i.e. The size of the image if "src_image" is specified.
    "size": 0x02,
    # Offset pointing to where this section's hash is stored.
    "hash_ptr": 0x03,
    # The number of bytes in this section that have been hashed.
    "hash_size": 0x04,
    # The entire size of this section, including used and unused bytes.
    "capacity": 0x05,
    # The 'id' of this partition - used for RA partitions only at the moment
    "id": 0x06,
    # Used to mask the element id from the key
    "mask": 0xff
}

# Partitions may have an alias name e.g. named ra_partitions included as a file system project
partition_aliases = {'va_fs': 'ra_partition'}

PACK_FMT = "<L"
NONCE_SIZE = 16  # nonce plus hash for authentication
ENTRY_SIZE = 8  # 4 bytes for key, 4 for value


class BootImageError(RuntimeError):
    """
    Error in validating the Boot Image
    """


class ImageHeaderError(RuntimeError):
    """
    Error in validating the Image Header
    """


class HydraAppsSQIFBootImage(object):
    """
    Boot Image class for easy access to the sqif boot image data and properties
    """

    def __init__(self, sqif):
        self._sqif = sqif
        self.offset = 0
        self.content = self._sqif.data[0:32]
        self.size = self.primary_image_offset

    @property
    def is_valid(self):
        """
        Indicates whether the boot image is valid based on the START and END
        magic bytes
        """
        boot_img_start = self.content[0:4]
        # This comparison works for both Python 2 and 3
        # TODO: Use the list property for comparison once
        # we move completely to Python3
        if (build_le(boot_img_start, word_width=8) !=
                struct.unpack(PACK_FMT, magic_numbers["BOOT_IMAGE_START"])[0]):
            raise BootImageError("Invalid boot image tag {}".
                                 format(boot_img_start))
        return True

    @property
    def primary_image_offset(self):
        """
        The offset of the primary image bank.
        """
        return build_le(self.content[4:8], word_width=8)

    @property
    def secondary_image_offset(self):
        """
        The offset of the secondary image bank
        """
        return build_le(self.content[8:12], word_width=8)


class HydraAppsSQIFPartition(object):
    """
    Represents a generic partition in the layout of a single image bank.
    Its content can be returned as a separate object.
    """

    # pylint: disable=too-many-instance-attributes
    # pylint: disable=too-many-arguments

    def __init__(self,  name, offset, size, capacity, unused,
                 content_type, image_bank):

        self._image_bank = image_bank
        self._offset = offset  # offset into the image bank
        self.capacity = capacity
        self.size = size
        self.unused = unused if unused else NotImplemented
        self._content_type = content_type

        # Public attributes - to be completed (probably passed in)
        self.must_authenticate = NotImplemented
        self.name = name

        # raw data -> offset into image bank data
        self.data_map = AddressMap(self.name, length=self.capacity if
                                   self.capacity != 0 else self.size,
                                   layout_info=self._image_bank.layout_info)
        # Make an "address space" for this partition by offsetting
        self.data_map.add_mapping(0, self.capacity if self.capacity != 0
                                  else self.size,
                                  self._image_bank.data,
                                  self._offset)
        self.data = self.data_map.port

    def info(self, report=False):
        """
        Display partition details
        """
        output = interface.Group("{}".format(self.name))
        table = interface.Table(["Partition Name",
                                 "Offset",
                                 "Capacity",
                                 "Size", "Unused"])
        table.add_row([self.name, self._offset, self.capacity, self.size,
                       self.unused])
        output.append(table)
        if report:
            return output
        TextAdaptor(output, gstrm.iout)

    def get_content(self):
        """
        Returns a properly typed object representing the contents of the partition
        """
        return self._content_type(self.data, self.size)


# Factory functions for making specific types of HydraAppsSQIFPartition

def make_rofs_partition(rofs_data, rofs_size):
    """
    Return the content type class for rofs images
    """
    return RawSQIFReadOnlyLEFS(rofs_data, rofs_size)


def make_rw_cfg_partition(rw_cfg_data, rw_cfg_size):
    """
    Return the content type and main storage data for rw config
    """
    return PsflashContent(rw_cfg_data, rw_cfg_size)

def make_curator_partition(cur_data, cur_size):
    """
    Retuen the content type for the curator filesystem
    """
    return CuratorReadOnlyLEFS(cur_data, cur_size)


class HydraAppsSQIFImageBank(object):
    # pylint: disable=too-many-instance-attributes

    def __init__(self, sqif, primary):

        self._sqif = sqif
        self._primary = primary
        self.apps_ss = self._sqif._apps

        # Get the HydraAppsSQIFBootImage from self._sqif to retrieve offset
        # of the primary image bank
        boot_image = self._sqif.boot_image
        self._offset = (boot_image.primary_image_offset if primary else
                        boot_image.secondary_image_offset)
        img_hdr_content = self._sqif.data[self._offset:self._offset + 1024]
        self.section_names = create_reverse_lookup(section_ids)
        self.element_names = create_reverse_lookup(element_ids)
        self.layout_info = self._sqif.layout_info
        header_offset = 0
        header_offset += NONCE_SIZE
        magic_size = len(magic_numbers["IMAGE_HEADER_START"])
        magic_bytes = img_hdr_content[header_offset:header_offset + magic_size]
        header_offset += magic_size
        if (build_le(magic_bytes, word_width=8) !=
                struct.unpack(PACK_FMT,
                              magic_numbers["IMAGE_HEADER_START"])[0]):
            if self._primary:
                raise ImageHeaderError("Invalid image header {}".format(magic_bytes))
            else:
                # Set secondary bank to None if a valid image header
                # is not present rather than failing unnecessarily
                self._sqif.secondary_bank = None
        self._parse_bytes(header_offset, img_hdr_content)

        bank_length = (boot_image.secondary_image_offset -
                       boot_image.primary_image_offset)
        self.data_map = AddressMap("{}_IMAGE".format("PRIMARY" if primary
                                                     else "SECONDARY"),
                                   # Assuming both primary and secondary
                                   # images are of the same length
                                   length=bank_length,
                                   layout_info=self._sqif.layout_info)
        self.data_map.add_mapping(0, bank_length, self._sqif.data, self._offset)
        self.data = self.data_map.port
        # Create partitions with their specific content types
        rofs_partitions = ["ro_fs", "device_ro_fs", "ro_cfg_fs"]
        for section in self.sections:
            content_type = None
            if section["name"] == "rw_config":
                content_type = make_rw_cfg_partition
            elif section["name"] in rofs_partitions:
                content_type = make_rofs_partition
            elif section["name"] == "curator_fs":
                content_type = make_curator_partition
            self.create_partitions(section, content_type)

    def create_partitions(self, section, content_type):
        """
        Create section partitions by looping through each entry in
        self.sections and creating a HydraAppsSQIFPartition object for it.
        :param - section - The name of the partition we want to create
        :param - content_type - Each partition content is represented by a
        class which needs to be passed in.
        """

        partition_offset = section.get("offset", 0)
        capacity = section.get("capacity", 0)
        size = section.get("size", 0)
        unused = section.get("unused", 0)
        partition = HydraAppsSQIFPartition(section["name"],
                                           partition_offset,
                                           size,
                                           capacity,
                                           unused,
                                           content_type,
                                           self)
        setattr(self, section["name"], partition)

    @property
    def num_partitions(self):
        """
        The total number of partitions in this bank
        """
        return len(self.sections)

    def _names_from_entry_key(self, key):
        section_name = self.section_names[section_ids["mask"] & key]
        element_name = self.element_names[element_ids["mask"] & key]
        return section_name, element_name

    @staticmethod
    def _kv_from_entry_bytes(entry_bytes):
        return (struct.unpack(PACK_FMT, entry_bytes[0:4])[0],
                struct.unpack(PACK_FMT, entry_bytes[4:8])[0])

    def _parse_bytes(self, offset, header_bytes):
        """
        Interpret the header bytes read from the flash into a table that
        we can extract offsets and sizes from.
        """
        second_ra = False
        sections = dict()
        header_bytes = bytearray(header_bytes)
        while True:
            entry_bytes = header_bytes[offset: offset+ENTRY_SIZE]
            offset += ENTRY_SIZE
            if entry_bytes[0:4] == magic_numbers["IMAGE_HEADER_END"]:
                break
            entry_key, entry_value = self._kv_from_entry_bytes(entry_bytes)
            section_name, element_name = self._names_from_entry_key(entry_key)
            if section_name not in sections:
                sections[section_name] = dict()
            # Convert an ID entry_value into a 4 character string
            if element_name == "id":
                entry_value = str(bytearray(flatten_le(entry_value,
                                                       num_words=4,
                                                       word_width=8)).decode())
            # Store the element
            if element_name in sections[section_name]:
                second_ra = bool(section_name == "ra_partition")

            if section_name == "ra_partition" and second_ra is True:
                section_name = "vmodel_partition"
                # Create a vmodel_partition section
                sections[section_name] = dict()

            sections[section_name][element_name] = entry_value

        for key, value in sections.items():
            if key == "vmodel_partition":
                key = "ra_partition"
            value.update({"name": key})
        self.sections = sorted(list(sections.values()),
                               key=lambda a: a["offset"] if "offset" in
                                                            a else 1 << 32)

        for section in sections:
            if "capacity" in sections[section]:
                sections[section]["unused"] = (sections[section]["capacity"]
                                               - sections[section]["size"])

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

        # Older image headers do not contain 'capacity' so the 'unused'
        # amount needs to be derived from the offsets.
        try:
            for s_no in range(len(sections)-2):
                if "capacity" not in self.sections[s_no]:
                    self.sections[s_no]["unused"] = (
                            self.sections[s_no+1]["offset"] - (
                                self.sections[s_no]["offset"] +
                                self.sections[s_no]["size"])
                    )
        except KeyError:
            # The calculation can fail if the next
            # section doesn't have an offset element
            pass


class HydraAppsSQIF(object):
    """
    Class representing the flash memory as presented by the SQIF controller hardware
    """

    def __init__(self, apps_ss):
        self._apps = apps_ss
        self.layout_info = Kalimba32DataInfo()

    @property
    def data(self):
        """
        The raw sqif data in apps
        """
        return self._apps.raw_sqif0

    @property
    def boot_image(self):
        """
        An instance of the boot image class
        """
        return HydraAppsSQIFBootImage(self)

    @property
    def primary_bank(self):
        """
        The primary image bank in the flash
        """
        return HydraAppsSQIFImageBank(self, primary=True)

    @property
    def secondary_bank(self):
        """
        The secondary image bank in the flash
        """
        return HydraAppsSQIFImageBank(self, primary=False)

    bank0 = primary_bank
    bank1 = secondary_bank
