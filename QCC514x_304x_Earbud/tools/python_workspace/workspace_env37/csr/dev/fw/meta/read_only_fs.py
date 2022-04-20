############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides means of flashing and dumping a read-only filesystem.
"""

# python3 compatibility, 'reduce' is a built-in in python3
import os
import shutil
from os.path import isdir, exists, join
from zipfile import ZipFile

from functools import reduce  # pylint: disable=redefined-builtin
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import words_to_bytes_be, words_to_dwords_be, \
                                   words_to_bytes, \
                                   dwords_to_bytes, bytes_to_dwords, \
                                   PureVirtualError, \
                                   bytes_to_words, bytes_to_words_be, \
                                   pack_unpack_data_le
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.wheels import gstrm
from csr.dev.model import interface
from .xuv_stream_decoder import XUVStreamDecoder


def build_be_byte_swapped_dword_le(words):
    """
    Converts an input list of one or two 16-bit words,
    where each is a BE number into a single LE dword value.
    This is useful when reading 16 or 32 bit values in the AppsFs.
    """
    if len(words) > 2:
        raise ValueError("Limited to up to 2 input words")
    return bytes_to_dwords(words_to_bytes_be(words))[0]

# Seem to be a lot of following accesses via class rather than self:
# pylint: disable=protected-access


class FSImageError(ValueError):
    """
    For reporting errors in a supplied filesystem image
    """


class IReadOnlyFS(object):
    """
    Abstract interface to fixed filesystem images
    """

    class _IEntry(object):
        """
        Generic filesystem entry, specialised into files and directories
        """
        DIR_FLAG = 0x80
        WFILE_FLAG = 0x40
        FILE_FLAG = 0

        @property
        def name(self):
            """
            Basename of the entry
            """
            raise PureVirtualError

        def list(self, indent):
            """
            Print a representation of the entry
            """
            iprint("%s%s [%d]" % (" "*indent, self.name, self.size))

        @property
        def size(self):
            """
            Size of the entry: number of bytes for a file, number of entries
            for a directory
            """
            raise PureVirtualError

    class _IDir(_IEntry):

        @property
        def entries(self):
            """
            Return a list of the directory's _IEntry objects
            """
            raise PureVirtualError

        def list(self, indent=0):
            """
            List the directory's name and contents
            """
            IReadOnlyFS._IEntry.list(self, indent)
            for entry in self.entries:
                entry.list(indent + 2)


    class _IFile(_IEntry):

        @property
        def data(self):
            """
            List of bytes constituting the file
            """
            raise PureVirtualError

    @property
    def num_entries(self):
        """
        Total number of entries in the filesystem (including the root dir)
        """
        raise PureVirtualError

    @property
    def root(self):
        """
        The root entry
        """
        raise PureVirtualError

    @property
    def loadable_le32(self):
        """
        Return a loadable version of the filesystem in K32 format
        """
        return LoadableReadOnlyFSLE32(self).loadable_sections


class PackfileReadOnlyFS(IReadOnlyFS):
    """
    Provides access to a traditional packfile-generated read-only filesystem
    image
    """

    class EntryTypes(object):
        # pylint: disable=no-init,too-few-public-methods
        # Nothing here uses this class so perhaps it can be removed.
        'The types of filesystem entries: directory, file and wide-file'
        DIR = 0
        FILE = 1
        WFILE = 2

    class _Entry(IReadOnlyFS._IEntry):
        """
        A generic filesystem entry - directory, file or wide file
        """
        @classmethod
        def flag(cls, index, fs, big_endian=True):
            """
            Return entry flag
            Create an appropriate type of _Entry based on the flag
            """
            meta_offset = (fs.WORD_SIZES["header_size"] +
                           index * fs.WORD_SIZES["entry_size"])
            if big_endian:
                return fs.slice(meta_offset, meta_offset+1)[0] >> 8
            return fs.slice(meta_offset+1, meta_offset+2)[0]

        @classmethod
        def _create(cls, entry_flag, index, fs):
            if entry_flag == cls.DIR_FLAG:
                return fs._Dir(index, fs)
            return fs._File(
                index, fs, (entry_flag == cls.WFILE_FLAG))

        @classmethod
        def create(cls, index, fs, big_endian=True):
            """
            Create an appropriate type of _Entry based on the flag
            """
            return cls._create(
                cls.flag(index, fs, big_endian=big_endian), index, fs)

        def _init_size_offsets(self):
            '''
            Initialises self attributes _name_offset, data_offset, _size
            '''
            # For _File this the offset to start of name in words,
            # for _Dir it is an entry number
            self._name_offset = words_to_dwords_be([
                self._meta[
                    PackfileReadOnlyFS.WORD_OFFSETS['name_offset']] & 0xff,
                self._meta[
                    PackfileReadOnlyFS.WORD_OFFSETS['name_offset']+1]])[0]

            # For a _File this is a data offset in words,
            # but for a _Dir this is an index
            self._data_offset = words_to_dwords_be(
                self._meta[
                    PackfileReadOnlyFS.WORD_OFFSETS['data_offset']:
                    PackfileReadOnlyFS.WORD_OFFSETS['data_offset']+2])[0]

            # For a _File this is a size in octets,
            # but for a _Dir it is number of entries in directory.
            self._size = words_to_dwords_be(
                self._meta[
                    PackfileReadOnlyFS.WORD_OFFSETS['size_offset']:
                    PackfileReadOnlyFS.WORD_OFFSETS['size_offset']+2])[0]

        def is_dir(self):
            """
            Returns whether this entry is for a directory
            """
            return self.flag(
                self._index, self._fs, big_endian=True) == self.DIR_FLAG

        def __init__(self, index, fs):

            self._fs = fs
            self._index = index
            meta_offset = (fs.WORD_SIZES["header_size"] +
                           index * fs.WORD_SIZES["entry_size"])
            self._meta = fs.slice(meta_offset, meta_offset+6)
            self._init_size_offsets()

        @property
        def name(self):
            """
            Decodes the name data for this entry
            """
            if self._name_offset == 0:
                return "<ROOT>"

            name_len = self._fs.slice(self._name_offset,
                                      self._name_offset+1)[0]
            name_words = self._fs.slice(self._name_offset + 1,
                                        self._name_offset + 1 + name_len)
            return "".join(chr(w) for w in name_words)

        @property
        def size(self):
            return self._size

    class _Dir(IReadOnlyFS._IDir, _Entry):
        """
        Specialisation of _Entry for directories
        """
        @property
        def entries(self):
            """
            The first entry (index) is given by the data offset;
            the number of entries by the size
            """
            return [self._fs._Entry.create(idx, self._fs) \
                    for idx in range(self._data_offset - 1,
                                     self._data_offset - 1 + self._size)]

        def get_fs_index(self, entry_name):
            """
            Get the read-only filesystem index of the
            entry pointed to by entry_name
            """
            entry_names = [entry.name for entry in self.entries]
            try:
                dir_entry_index = entry_names.index(entry_name)
            except ValueError:
                # entry_name not present
                raise FileNotFoundError(entry_name)
            else:
                return dir_entry_index + self._data_offset - 1

        def get_index_from_path(self, path):
            """
            Get read-only filesystem index from path.
            The method does a recursive search with the first fragment
            of path as root until it reaches the basename.
            """
            try:
                subdir, remaining_path = path.split("/", 1)  # get first fragment
            except ValueError:
                # No more /'s in the path - we've reached the basename
                basename = path
                return self.get_fs_index(basename)
            else:
                # recursively call on the subdirectory
                subdir_entry = self._fs.entry(self.get_fs_index(subdir))
                try:
                    return subdir_entry.get_index_from_path(remaining_path)
                except FileNotFoundError as exc:
                    # Recursively add leading path fragments as exception
                    # propagation unwinds the recursive stack
                    raise FileNotFoundError(subdir + "/" + str(exc))

    class _File(IReadOnlyFS._IFile, _Entry):

        def __init__(self, idx, fs, is_wide=False):

            PackfileReadOnlyFS._Entry.__init__(self, idx, fs)
            self._is_wide = is_wide

        @property
        def data(self):
            """
            Get the data
            """
            data = self._fs.slice(self._data_offset,
                                  self._data_offset + (self._size+1)//2)
            if self._is_wide:
                return data
            return words_to_bytes_be(data)[0:self._size]

    WORD_OFFSETS = {
        # offsets from start of FS
        "header_magic" : 0, # word-pair starting at this offset form 'File'
        "header_size"  : 2, # word-pair starting at this offset form FS size
        "header_num_entries" : 4,
        # offsets in an _Entry
        "name_offset" : 0, # word-pair starting at this offset form name_offset
        "data_offset" : 2, # word-pair starting at this offset form data_offset
        "size_offset" : 4, # word-pair starting at this offset form size_offset
        # offsets from end of FS
        "footer_magic"   : -2, # word-pair starting at this offset form 'eliF'
        "footer_size"    : -4, # word-pair starting at this offset form FS size
        "footer_version" : -5, # word at this offset is the FS format version
        "footer_checksum": -6} # word at this offset is the FS checksum

    WORD_SIZES = {
        "header_size" : 5,
        "entry_size" : 6}

    @staticmethod
    def read(image_file):
        """
        Reads image_file and returns tuples of form (address, value)
        """
        with open(image_file) as fs_stream:
            return PackfileReadOnlyFS(
                list(XUVStreamDecoder(fs_stream).address_value_pairs))

    def __init__(self, packfile_image):
        """
        Supply a list of (address, value) pairs in XAP format, i.e.
         - the addresses are word addresses,
         - the values are words, and
         - where the words represent packed bytes, they are packed big-endianly
        """
        self._pimage = packfile_image

        self._validate()


    def _validate(self):
        """
        Check that the magic words appear at top and bottom and that the
        size field is correct
        """
        def check_magic(offset_name):
            'Checks the magic constant occurs at offset_name'
            header_magic_bytes = words_to_bytes_be(
                [self._pimage[self.WORD_OFFSETS[offset_name]][1],
                 self._pimage[self.WORD_OFFSETS[offset_name]+1][1]])
            if "".join([chr(b) for b in header_magic_bytes]) != "File":
                raise  FSImageError("Bad %s" % offset_name)

        check_magic("header_magic")
        check_magic("footer_magic")

        # Calculate the checksum
        xor = reduce(lambda x, y: x^y,
                     bytes_to_words([v for _, v in self._pimage]))
        if xor != 0:
            # Fallback to calculating an 8-bit checksum.
            # An old implementation of this script used an 8-bit checksum,
            # so some valid .xuv files may exist with such a checksum.
            xor = reduce(lambda x, y: x^y, [v for _, v in self._pimage])
            if xor != 0:
                raise FSImageError("Bad checksum")

        if not isinstance(self.root, self._Dir):
            raise FSImageError("First entry is not a directory!")

        if self.root.name != "<ROOT>":
            raise FSImageError("Root dir unexpectedly had an explicit name "
                               "'%s'" % self.root.name)

    def slice(self, start, stop):
        """
        Get a slice of the filesystem data without reference to addresses
        """
        return [self._pimage[s][1] for s in range(start, stop)]

    @property
    def size(self):
        'Accessor to the size of the filesystem in words'
        return (words_to_dwords_be(
            [self._pimage[self.WORD_OFFSETS["header_size"]][1],
             self._pimage[self.WORD_OFFSETS["header_size"]+1][1]])[0])

    @property
    def num_entries(self):
        return self._pimage[self.WORD_OFFSETS["header_num_entries"]][1]

    def entry(self, idx):
        'Returns an object to represent the indexth entry in the filesystem'
        return self._Entry.create(idx, self)

    @property
    def root(self):
        'Accessor to the root folder of this filesystem'
        return self.entry(0)

    @property
    def entries(self):
        'Generator yielding each entry in the filesystem'
        for i in range(self.num_entries):
            yield self.entry(i)

    def list_all(self):
        'Lists the contents of the whole filesystem'
        for idx in range(self.num_entries):
            self.ls(idx)

    def ls(self, fs_index): #pylint: disable=invalid-name
        """
        List the indicated entry, be it directory or file
        """
        self.entry(fs_index).list(0)


    def dump(self, filename):
        """
        Just dump the file exactly as it was read in
        """
        with open(filename, "w") as out:
            for line in self._pimage:

                out.write("@0x%06x 0x%04x" % line)

    def get_index_from_path(self, path):
        """
        Get filesystem index from the given path
        In this method, just call on the ROOT directory entry.
        """
        return self.entry(0).get_index_from_path(path)

    def metadata(self, title_str=None, report=False):
        """
        Print names of files and folders under this FileSystem.
        """
        if title_str is None:
            title_str = "Read Only File System"
        output = interface.Group(title_str)
        count = 0
        table = interface.Table(["Index", "Name", "Is Dir", "Size"])
        for entry in self.entries:
            if entry.name == "<ROOT>":
                count += 1
                continue
            table.add_row([count, entry.name, entry.is_dir(), entry._size])
            count += 1
        output.append(table)
        if report:
            return output
        TextAdaptor(output, gstrm.iout)

    def read_file(self, filename):
        """
        Return file data of filename in bytearray.
        """
        file_data = bytearray()
        file_entry = self.entry(self.get_index_from_path(filename))
        for data in file_entry.data:
            file_data.append(data)
        return file_data

    def dump_fs(self, filename=None, dirname=None, dest_dir=None):
        """
        Dump a read only filesystem on the host machine.
        You can provided filename or dirname but not both.
        param:filename If provided, dump the file on the host
        param:dirname If provided, dump everything under the directory
            on the host
        If neither have been provided, dump everything under root
        param:dest_dir The directory on the host where the filesystem
            will be dumped. Should already exist on the host.
        """
        if not exists(dest_dir) and not isdir(dest_dir):
            raise ValueError("The directory provided either does not exist"
                             "or is not a directory!"
                             "Please create the host directory first "
                             "and then call this method")
        if dirname and filename:
            raise ValueError("Cannot provide a directory and a filename at "
                             "the same time. Please provided one at a time.")
        if dirname:
            dir_entry = self.entry(self.get_index_from_path(dirname))
            for entry in dir_entry.entries:
                if not entry.is_dir():
                    self.dump_fs(filename=entry.name, dest_dir=dest_dir)
                else:
                    self.dump_fs(dirname=entry.name, dest_dir=dest_dir)
        elif filename:
            with open(join(dest_dir, filename), "ab") as file:
                file.write(self.read_file(filename))
        else:
            # Neither a file nor a directory name has been provided.
            # Dump everything under the root directory
            for entry in self.entries:
                if entry.is_dir() and entry.name != "<ROOT>":
                    self.dump_fs(dirname=entry.name,
                                 dest_dir=join(dest_dir, entry.name))
                elif not entry.is_dir():
                    self.dump_fs(filename=entry.name, dest_dir=dest_dir)

    def zip_fs(self, dest_dir=None):
        """
        Zip the entire filesystem
        WARNING: This will delete any other files or directories
        lying around in the dest_dir. Please check the destination dir
        """
        if not isdir(dest_dir):
            raise ValueError("The directory provided either does not exist"
                             "or is not a directory!"
                             "Please create the host directory first "
                             "and then call this method")
        self.dump_fs(dest_dir=dest_dir)
        # For some reason, you have to create the zipfile in-situ
        # and then move it to the destination directory.
        # Directly creating it in the destination makes it hang.
        with ZipFile("ro_filesystem.zip", mode="w") as zip_obj:
            for root, _, files in os.walk(dest_dir):
                for file in files:
                    filepath = join(root, file)
                    zip_obj.write(filepath, os.path.basename(filepath))
                    os.remove(filepath)
        shutil.move("ro_filesystem.zip", dest_dir)


class FlashedReadOnlyLEFS(PackfileReadOnlyFS):
    '''
    Accesses an in-SQIF apps filesystem arranged in LE format.
    Note the differences from a curator FS are more than just LE.
    E.g. many offsets are in octets rather than words;
    there is extra padding to ensure 32 bit alignment in places.
    '''

    class _Entry(PackfileReadOnlyFS._Entry):
        """
        A generic filesystem entry - directory, file or wide file
        specialised for AppsFs entries.
        """
        @classmethod
        def create(cls, index, fs, big_endian=False):
            """
            Create an appropriate type of _Entry based on the flag
            """
            return cls._create(
                cls.flag(index, fs, big_endian=big_endian), index, fs)

        def _init_size_offsets(self):
            '''
            Initialises self attributes _name_offset, data_offset, _size
            '''
            # For _File this is the offset to start of name in octets
            # (could be an odd number so keep in octets),
            # for _Dir it is an entry number.
            # This is a DWORD at 'name_offset'.
            self._name_offset = build_be_byte_swapped_dword_le(
                [self._meta[PackfileReadOnlyFS.WORD_OFFSETS['name_offset']],
                 self._meta[PackfileReadOnlyFS.WORD_OFFSETS['name_offset']+1]
                 & 0xff00])

            # For a _File this is a data offset in octets (not words),
            # but for a _Dir this is an index.
            # This is a DWORD at DATA_OFFSET.
            self._data_offset = build_be_byte_swapped_dword_le(
                self._meta[PackfileReadOnlyFS.WORD_OFFSETS['data_offset']:
                           PackfileReadOnlyFS.WORD_OFFSETS['data_offset']+2])

            # For a _File this is a size in octets,
            # but for a _Dir it is number of entries in directory.
            self._size = build_be_byte_swapped_dword_le(
                self._meta[PackfileReadOnlyFS.WORD_OFFSETS['size_offset']:
                           PackfileReadOnlyFS.WORD_OFFSETS['size_offset']+2])

        def is_dir(self):
            'returns whether this entry is for a directory'
            return self.flag(
                self._index, self._fs, big_endian=False) == self.DIR_FLAG

        @property
        def name(self):
            """
            Decodes the name data for this entry
            """
            if self._name_offset == 0:
                return "<ROOT>"

            #name-len (octets) is twice as long as it should be for word counts
            word_offset = self._name_offset//2
            # which byte offset depends on oddness/evenness of name_offset
            name_len = self._fs.slice(word_offset, word_offset+1)[0]
            #if even then take upper octet, else lower
            if self._name_offset % 2:
                name_len = name_len & 0x00FF
                # next word has other octet in upper byte
                name_len |= (self._fs.slice(
                    word_offset+1, word_offset+2)[0] & 0xFF00)
            else:
                name_len = build_be_byte_swapped_dword_le([name_len])
            # name_words is one short when name_len is even
            # so use name_len+2 not +1
            name_words = self._fs.slice(word_offset + 1,
                                        word_offset + 1 + (name_len+2)//2)
            # Characters are packed two per word with 0 padding at end
            # beware any extra one beyond name_len
            return "".join(chr(w) for w in words_to_bytes_be(name_words)[
                self._name_offset%2:name_len+self._name_offset%2])

    class _Dir(_Entry, PackfileReadOnlyFS._Dir):
        """
        Specialisation of _Entry for AppsFs directories
        """
        def __init__(self, index, fs):
            #pylint: disable=useless-super-delegation
            #If this method is removed, it breaks.
            super(FlashedReadOnlyLEFS._Dir, self).__init__(index, fs)

    class _File(_Entry, PackfileReadOnlyFS._File):
        """
        Specialisation of _Entry for AppsFs files
        """
        def __init__(self, idx, fs, is_wide=False):
            #Deliberately don't call other __init__ because that's big-endian.
            #pylint: disable=super-init-not-called
            super(FlashedReadOnlyLEFS._File, self).__init__(idx, fs)
            self._is_wide = is_wide

        @property
        def data(self):
            """
            Get the data from octet offset _data_offset which needs
            converting to a word offset (assume starts word aligned)
            and is a multiple of 4 octets with 0 trailing values
            """
            data = self._fs.slice(self._data_offset//2,
                                  self._data_offset//2 + ((self._size+3)//4)*2)
            if self._is_wide:
                return data # TBC
            return words_to_bytes_be(data)[0:self._size]

    def entry(self, idx):
        if self._pimage:
            return self._Entry.create(idx, self, big_endian=False)
        return None

    def __init__(self, apps0, fs_sqif_offset_words):
        # Size of header of what ended up being flashed is different
        # from what was provided in PackfileReadOnlyFS.
        # use new instance variable so as not to modify the base class one.
        FlashedReadOnlyLEFS.WORD_SIZES = PackfileReadOnlyFS.WORD_SIZES.copy()
        FlashedReadOnlyLEFS.WORD_SIZES["header_size"] = 6
        # get an array of words from SQIF
        import csr
        self.apps0 = apps0
        self.sqif_base_addr = apps0.subsystem.sqif_trb_address_block_id[0][0]
        self.fs_sqif_offset_words = fs_sqif_offset_words

        trans = csr.dev.transport # pylint: disable=no-member
        size = trans.mem_read32(
            self.apps0.subsystem.id,
            self.sqif_base_addr+(self.fs_sqif_offset_words*2) +4, #address
            block_id=0)
        iprint("Size = 0x%x octets" % size)
        remaining = size//2 #octets to words
        if size == 0xffffffff: # empty test
            self._pimage = None
            return
        convertor = bytes_to_words
        #dword_convertor = dwords_to_words_be
        # There's a maximum number quantum of words that can be read at once.
        quantum = 5000
        word_offset = 0
        fs_words = []
        while remaining > 0:
            quantum = quantum if remaining > quantum else remaining
            fs_words.extend(
                # c.f. trans.block_read32
                convertor(trans.trb.read(
                    self.apps0.subsystem.id, 0,
                    (self.sqif_base_addr+self.fs_sqif_offset_words*2+
                     word_offset*2),
                    2, 2 * quantum)))
            remaining -= quantum
            word_offset += quantum
        # now we have the memory as words, in address order,
        # supply to base class
        super(FlashedReadOnlyLEFS, self).__init__(list(enumerate(fs_words)))

    @property
    def num_entries(self):
        # This is a DWORD, whereas base class assumed a single WORD;
        # the header is one word longer to compensate.
        # In practice the upper word is probably always zero.
        return build_be_byte_swapped_dword_le(
            self._pimage[self.WORD_OFFSETS["header_num_entries"]:
                         self.WORD_OFFSETS["header_num_entries"]+1])

    @property
    def size(self):
        'Accessor to the size of LE filesystem which is in octets (not words)'
        # This is a DWORD.
        return build_be_byte_swapped_dword_le(
            self._pimage[self.WORD_OFFSETS["header_size"]:
                         self.WORD_OFFSETS["header_size"]+1])


class RawSQIFReadOnlyLEFS(FlashedReadOnlyLEFS):
    """
    Accesses an in-SQIF apps filesystem arranged in LE format.
    Note the differences from a curator FS are more than just LE.
    E.g. many offsets are in octets rather than words;
    there is extra padding to ensure 32 bit alignment in places.
    This class takes raw sqif data directly along with the size of that
    data to calculate the attributes and metadata associated with that FS.
    """

    def __init__(self, fs_sqif_data, fs_sqif_size):
        """
        Class constructor that takes raw sqif data for the
        partition and it's size as param.
        Uses new instance variable for word sizes so as not
        to modify the base class one.
        """

        RawSQIFReadOnlyLEFS.WORD_SIZES = PackfileReadOnlyFS.WORD_SIZES.copy()
        RawSQIFReadOnlyLEFS.WORD_SIZES["header_size"] = 6
        # Get the read only fs data in words

        self.fs_sqif_data = bytes_to_words_be(fs_sqif_data[:fs_sqif_size])
        if fs_sqif_size == 0xffffffff:  # empty test
            self._pimage = None
            return
        PackfileReadOnlyFS.__init__(self, list(enumerate(self.fs_sqif_data)))

    @property
    def num_entries(self):
        """
        This is a DWORD, whereas base class assumed a single WORD;
        the header is one word longer to compensate.
        In practice the upper word is probably always zero.
        """
        return build_be_byte_swapped_dword_le([
            self._pimage[self.WORD_OFFSETS["header_num_entries"]][1],
            self._pimage[self.WORD_OFFSETS["header_num_entries"] + 1][1]])

    @property
    def size(self):
        """
        Accessor to the size of LE filesystem which is in octets (not words)
        """
        return build_be_byte_swapped_dword_le([
            self._pimage[self.WORD_OFFSETS["header_size"]][1],
            self._pimage[self.WORD_OFFSETS["header_size"] + 1][1]])


class CuratorReadOnlyLEFS(PackfileReadOnlyFS):
    """
    Accesses an in-SQIF curator filesystem arranged in BE format.
    This class takes raw sqif data directly along with the size of that
    data to calculate the attributes and metadata associated with that FS.
    """
    class _Entry(PackfileReadOnlyFS._Entry):
        """
        A curator filesystem entry - directory, file or wide file
        specialised for AppsFs entries.
        """

    class _Dir(_Entry, PackfileReadOnlyFS._Dir):
        def __init__(self, index, fs):
            # pylint: disable=useless-super-delegation
            # If this method is removed, it breaks.
            PackfileReadOnlyFS._Dir.__init__(self, index, fs)

    class _File(_Entry, PackfileReadOnlyFS._File):
        def __init__(self, idx, fs, is_wide=False):
            PackfileReadOnlyFS._File.__init__(self, idx, fs, is_wide=is_wide)


    def __init__(self, fs_sqif_data, fs_sqif_size):
        """
        Class constructor that takes raw sqif data for the
        partition and it's size as param.
        """
        # Get the read only fs data in words
        self.fs_sqif_data = bytes_to_words(fs_sqif_data[:fs_sqif_size])
        if fs_sqif_size == 0xffffffff:  # empty test
            self._pimage = None
            return
        PackfileReadOnlyFS.__init__(self, list(enumerate(self.fs_sqif_data)))

    @property
    def size(self):
        """
        Accessor to the size of Curator filesystem which is in octets (not words)
        """
        return bytes_to_words(pack_unpack_data_le([
            self._pimage[self.WORD_OFFSETS["header_size"] + 1][1],
            self._pimage[self.WORD_OFFSETS["header_size"] + 2][1]], 16, 8))[0] * 2

    def metadata(self, title_str=None, report=False):
        """
        Print names of files and folders under this FileSystem.
        """
        if title_str is None:
            title_str = "Curator Read-Only File System"
        PackfileReadOnlyFS.metadata(self, title_str, report)


class CustomReadOnlyFS(IReadOnlyFS):
    """
    Read only fs implementation that can be programmatically constructed
    """

    class CustomEntry(IReadOnlyFS._IEntry):
        'An entry in a custom filesystem'
        def __init__(self, name):
            self._name = name
            self._data = []
            self._is_wide = False

        # _IEntry compliance
        # ------------------
        @property
        def name(self):
            return self._name

        @property
        def size(self):
            """
            Returns the length of the data in bytes
            """
            return len(self._data)*2 if self._is_wide else len(self._data)

    class CustomDir(IReadOnlyFS._IDir, CustomEntry):
        'A directory entry in a custom filesystem'
        def __init__(self, name, fs):

            CustomReadOnlyFS.CustomEntry.__init__(self, name)
            self._fs = fs

        # _IDir compliance
        # ------------------
        @property
        def entries(self):
            """
            Look up the directory's entries in the filesystem's central list
            """
            return [self._fs.entries[i] for i in self._data]

        # Construction interface
        # ----------------------
        def add_file(self, name, contents, is_wide=False):
            """
            Adds a file of given name and contents into the filesystem
            """
            entry = CustomReadOnlyFS.CustomFile(name, contents, is_wide=is_wide)
            idx = self._fs.add_entry(entry)
            self._data.append(idx)

        def add_dir(self, name):
            """
            Create a directory inside this one with the given name, returning
            it so that the caller can add things to it
            """
            entry = CustomReadOnlyFS.CustomDir(name, self._fs)
            idx = self._fs.add_entry(entry)
            self._data.append(idx)
            return entry

    class CustomFile(IReadOnlyFS._IFile, CustomEntry):
        'A file entry in a custom filesystem'
        def __init__(self, name, data, is_wide=False):

            CustomReadOnlyFS.CustomEntry.__init__(self, name)
            self._data = data
            self._is_wide = is_wide

        # _IFile compliance
        # -----------------
        @property
        def data(self):
            return self._data

        @property
        def is_wide(self):
            'returns whether is a wide file'
            return self._is_wide

    def __init__(self):
        """
        Create a root directory automatically.  Further construction is done
        via directories.
        """
        self._entries = [self.CustomDir("<ROOT>", self)]

    # IReadOnlyFS compliance
    #------------------------

    @property
    def num_entries(self):
        return len(self._entries)

    @property
    def root(self):
        return self._entries[0]

    # Construction interface
    #-----------------------

    def add_entry(self, entry):
        'Adds another entry into the filesystem'
        index = self.num_entries
        self._entries.append(entry)
        return index

    @property
    def entries(self):
        'Returns the entries in the custom filesystem'
        return self._entries

class LoadableReadOnlyFS(object):
    """
    Create an iterable of ELF-esque "loadable sections" out of the given
    IReadOnlyFS
    """
    #pylint: disable=too-few-public-methods
    class GenericLoadable(object):
        #pylint: disable=too-few-public-methods
        'A generic filesystem image section that is loadable'
        def __init__(self, name=None):
            self.paddr = None
            self.data = []
            self.name = name

    def __init__(self, read_only_fs):
        self._rofs = read_only_fs
        self._header = self.GenericLoadable("Header")
        self._footer = self.GenericLoadable("Footer")
        self._entries = self.GenericLoadable("Entry metadata")
        self._names = self.GenericLoadable("Names")
        self._data = self.GenericLoadable("File data")

    def _populate(self): #pylint: disable=no-self-use

        raise PureVirtualError

    @property
    def loadable_sections(self):
        'Generator that yields each of the loadable sections'
        self._populate()

        yield self._header
        yield self._entries
        yield self._names
        yield self._data
        yield self._footer

class LoadableReadOnlyFSLE32(LoadableReadOnlyFS):
    """
    Specialisation that populates the different sections using the specific
    format idiosyncracies of Kalimba32 (e.g. 32-bit alignment etc)
    """
    @staticmethod
    def _get_next_32bit_aligned_addr(addr):
        addressable_unit_size = 8
        addresses_in_32bits = 32 // addressable_unit_size
        return (((addr + (addresses_in_32bits - 1)) // addresses_in_32bits) *
                addresses_in_32bits)

    def _pad_data_32bit_boundary(self, data):
        padding = [0] * (self._get_next_32bit_aligned_addr(
            len(data)) - len(data))
        data.extend(padding)

    def add_dir_entries(self, dir_entries, offset, data, start_entry_index):
        """
        Add the given set of entries in a contiguous block starting at the entry
        index indicated, recursively adding subdirectories' contents in later
        contiguous blocks.

        Potential extension: This function mixes up the generic task of ensuring entries are
        listed contiguously for a given directory with the format-specific
        task of laying out the entry data itself.  These ought to be separated
        and the generic part moved up into the base class for use by other
        formatters.
        """

        # Reserve a contiguous block of entry indices for this directory's
        # contents
        free_entry_index = start_entry_index + len(dir_entries)
        for i, entry in enumerate(dir_entries):

            index = 12*(start_entry_index + i)
            # Name offset
            data["e"][index:index+3] = dwords_to_bytes(
                [offset["n"] + len(data["n"])])[0:3]
            # Append name to name data while we're at it
            data["n"] += words_to_bytes(
                [len(entry.name)]) # The length field is 16 bits wide
            data["n"] += [ord(c) for c in entry.name]

            # File type flag
            if isinstance(entry, IReadOnlyFS._IDir):
                data["e"][index + 3] = 0x80

                # "data offset" for a directory is the entry index of the
                # folder's contents, using a 1-based indexing scheme (for some
                # reason)
                data_offset = free_entry_index + 1
                data["e"][index+4:index+8] = dwords_to_bytes([data_offset])

                # Now recursively add the directory's contents, updating
                # the next unused entry index
                free_entry_index = self.add_dir_entries(
                    entry.entries, offset, data, free_entry_index)

            else:
                if entry._is_wide:
                    data["e"][index + 3] = 0x40
                else:
                    data["e"][index + 3] = 0


                # Data offset.  As noted above, this is incorrect for now by the
                # as-yet-unknown length of the names section.  We'll loop over
                # and correct it later. We still store it as unpacked bytes, for
                # consistency
                data["e"][index+4:index+8] = dwords_to_bytes([len(data["d"])])
                # Append data to data data while we're at it
                if entry._is_wide:
                    # Data is returned in 16-bit words
                    data["d"] += words_to_bytes(entry.data)
                else:
                    data["d"] += entry.data
                # Pad data to align next file data
                self._pad_data_32bit_boundary(data["d"])

            # Data size
            data["e"][index+8:index+12] = dwords_to_bytes([entry.size])

        return free_entry_index


    def _populate(self):

        # All addresses are relative to the start of the file system image
        cur_addr = 0

        # Convenience dictionaries for passing around the various data arrays
        # and section offsets
        offset = {}
        data = {}

        offset["h"] = 0

        # First, populate the header.  This is pretty straightforward.
        self._header.paddr = cur_addr
        self._header.data = [0]*12
        data["h"] = self._header.data

        # Header magic
        data["h"][0:4] = [ord(c) for c in "File"]
        # Total size in bytes is not known until after we've written everything
        # Number of entries
        data["h"][8:12] = dwords_to_bytes([self._rofs.num_entries])
        # Pad data to align next section
        self._pad_data_32bit_boundary(data["h"])

        cur_addr += len(data["h"])
        offset["e"] = offset["h"] + len(data["h"])

        # Now, populate the entries

        self._entries.paddr = cur_addr
        self._entries.data = [0]*(12*self._rofs.num_entries)
        data["e"] = self._entries.data
        # Pad data to align next section
        self._pad_data_32bit_boundary(data["e"])

        cur_addr += len(data["e"])

        self._names.paddr = cur_addr
        data["n"] = self._names.data
        offset["n"] = offset["e"] + len(data["e"])

        # We can't set the start address of the data block until we've written
        # all the names.  Similarly, we'll have to loop back through entries'
        # data offsets correcting them by offset["d"]
        data["d"] = self._data.data

        # Add root directory entry

        # Name offset is 0
        data["e"][0:3] = [0, 0, 0]
        # File type flag
        data["e"][3] = 0x80
        # Entry index of contents (1-based)
        data["e"][4:8] = [2, 0, 0, 0]
        # Number of entries
        data["e"][8:12] = dwords_to_bytes([len(self._rofs.root.entries)])

        # Now recursively add all the entries, names and data
        self.add_dir_entries(self._rofs.root.entries, offset, data, 1)
        # Pad data to align next section
        self._pad_data_32bit_boundary(data["n"])

        cur_addr += len(data["n"])
        offset["d"] = offset["n"] + len(data["n"])
        self._data.paddr = cur_addr

        # Loop over all the non-directory entries incrementing their data offset
        for i in range(self._rofs.num_entries):
            index = 12*i
            if not data["e"][index+3] == IReadOnlyFS._IEntry.DIR_FLAG:
                data["e"][index+4:index+8] = dwords_to_bytes(
                    [bytes_to_dwords(data["e"][index+4:index+8])[0]
                     + offset["d"]])

        cur_addr += len(data["d"])

        # The footer should already be 32 bit aligned.
        # It must be aligned to at least 16-bits.
        # 32-bit alignment allows efficient loads.
        self._footer.paddr = cur_addr

        self._footer.data = 24*[0]
        data["f"] = self._footer.data
        # Magic number
        data["f"][-4:] = data["h"][0:4]

        # Total size
        total_size = sum(len(d) for d in data.values())
        data["h"][4:8] = dwords_to_bytes([total_size])
        # Total size
        data["f"][-8:-4] = data["h"][4:8]
        # minor format version (= 1 to indicate K32 format variant)
        data["f"][-10] = 1
        # major format version
        data["f"][-9] = 1

        # Finally, calculate the checksum
        def checksum(data):
            'calculates the filesystem checksum'
            # Iterate in the same order as the loadable_sections generator
            # h:header e:entries n:names d:data f:footer
            return reduce(lambda x, y: x^y,
                          bytes_to_words([v for d in "hendf" for v in data[d]]))

        data["f"][-12:-10] = words_to_bytes([checksum(data)])

        # Double-check
        if checksum(data) != 0:
            raise FSImageError("LoadableReadOnlyFSLE32 checksum failed!")
