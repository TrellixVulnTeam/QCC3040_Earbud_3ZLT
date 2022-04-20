############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2016-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from multiprocessing.connection import _mmap_counter
"""
@file
Psflash Firmware Component file.

@section Description
Implements class to be used for all Psflash subsystem work.

@section Usage
Currently provides the full set of functionality required to see the state
of the Persistent State storages, run PsStore, PsRetrieve, etc traps.
"""
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.wheels.bitsandbobs import bytes_to_words, words_to_bytes, \
                                   dwords_to_bytes
from csr.wheels.openssl import Aes128Cbc
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.tools.flash_image_builder import ImageBuilder
import time, sys, re, random, os

if sys.version_info > (3,):
    # Python 3
    int_types = (int,)
else:
    # Python 2
    int_types = (int, long)

PSFLASH_HEADER_B = {"id":{"size":4, "elements":1, "offset":0},
                    "checksum":{"size":2, "elements":1, "offset":4},
                    "length_words":{"size":2, "elements":1, "offset":6},
                    "iv":{"size":4, "elements":4, "offset":8}}
PSFLASH_PSKEY_SPECIAL = 0
SIFLASH_ERASED_UINT16 = 0xffff
SIFLASH_ERASED_UINT32 = 0xffffffff
PSFLASH_DEFAULT_CRYPTO_KEY = [0] * 16
PSFLASH_DEFAULT_STORES_OFFSET = 0xd0000
PSFLASH_DEFAULT_STORE_SIZE = 0x10000
PSLFASH_HEADER_B = sum([k["size"] * k["elements"] for k in list(PSFLASH_HEADER_B.values())])
PSLFASH_HEADER_W = PSLFASH_HEADER_B // 2
PSFLASH_AUDIO_OFFSET = 0x80000000
PSFLASH_CRYPTO_AES_BLOCK_SIZE_BYTES = 128 // 8
PSFLASH_CRYPTO_AES_BLOCK_LENGTH_16B_WORDS = PSFLASH_CRYPTO_AES_BLOCK_SIZE_BYTES // 2
PSFLASH_MAX_UINT32 = 0xffffffff
PSFLASH_MAX_UINT16 = 0xffff
PSFLASH_MAX_UINT8 = 0xff

PSFLASH_FEATURE_ENCRYPTED = 0
PSFLASH_FEATURE_ENCRYPTED_MASK = (1 << PSFLASH_FEATURE_ENCRYPTED)

# (key_name, index_from, index_to, ps_key_range_start)
# NOTE: There is block of 10 READ_ONLY keys in between the CUSTOMER keys.
KNOWN_KEYS = [("USR",      0,  49, 650),
              ("DSP",      0,  49, 0x2000 + 600),
              ("CONNLIB",  0,  49, 0x2000 + 1550),
              ("USR",     50,  99, 0x2000 + 1950),
              ("CUSTOMER", 0,  89, 0x2000 + 2000), 
              ("CUSTOMER", 90,299, 0x2000 + 2100), 
              ("UPGRADE",  0,   9, 0x2000 + 2600)]


class RwConfigNotPresent(RuntimeError):
    """
    rw_config section not present in the Image Header
    """


class PsKey(object):
    '''
    Auxiliary class to help with configuration keys parsing
    '''
    def __init__(self, crypto_key, storage_w=None, addr=0, limit=None):
        if not type(crypto_key) in (list, tuple):
            raise TypeError("crypto key needs to be a list or tuple")
        if not len(crypto_key) == 16:
            raise TypeError("crypto key needs to be a list or tuple of 16 elements")
        for elem in crypto_key:
            if not type(elem) in int_types:
                raise TypeError("crypto key needs to be a list or tuple of 16 integers")
            if elem < 0 or elem > PSFLASH_MAX_UINT8:
                raise TypeError("crypto key needs to be a list or tuple of 16 8 bit integers")
        self._crypto_key = crypto_key
        self._storage_w = storage_w
        self._addr_w = addr
        self._limit = limit
        self._aes = Aes128Cbc()

    def _decode(self, field):
        if not self._storage_w is None:
            ret = []
            _header_b = words_to_bytes(
                self._storage_w[self._addr_w: self._addr_w + PSLFASH_HEADER_W])
            for e in range(PSFLASH_HEADER_B[field]["elements"]):
                element = 0
                element_offset = e * PSFLASH_HEADER_B[field]["size"]
                for b in range(PSFLASH_HEADER_B[field]["size"]):
                    index = PSFLASH_HEADER_B[field]["offset"] + element_offset + b
                    element += _header_b[index] << (8 * b)
                ret.append(element)
            return ret[0] if len(ret) == 1 else ret
        else:
            return None

    def _encode(self, field):
        octets = []
        if field == "id":
            value = [self.ps_key]
        elif field == "checksum":
            value = [self.checksum]
        elif field == "length_words":
            value = [self.length_words]
        elif field == "iv":
            value = self.iv
        else:
            raise ValueError("unexpected field value")
        for e in range(PSFLASH_HEADER_B[field]["elements"]):
            for b in range(PSFLASH_HEADER_B[field]["size"]):
                octets.append((value[e] >> 8*b) & 0xff)
        return bytes_to_words(octets)

    def encode(self):
        words = []
        words.extend(self._encode("id"))
        words.extend(self._encode("checksum"))
        words.extend(self._encode("length_words"))
        words.extend(self._encode("iv"))
        words.extend(self.data)
        return words

    def _get_ciphertext_size_w(self, cleartext_size_16b_words):
        block_len = PSFLASH_CRYPTO_AES_BLOCK_LENGTH_16B_WORDS
        return ((cleartext_size_16b_words + block_len - 1) // block_len) * block_len
    
    @property
    def address(self):
        return self._addr_w

    @property
    def ps_key(self):
        try:
            self._ps_key
        except AttributeError:
            self._ps_key = self._decode("id")
        return self._ps_key
    @ps_key.setter
    def ps_key(self, value):
        if not type(value) in int_types:
            if value < 0 or value > PSFLASH_MAX_UINT32:
                raise TypeError("ps_key needs to be a 32 bit integer")
        self._ps_key = value

    @property
    def checksum(self):
        try:
            self._checksum
        except AttributeError:
            self._checksum = self._decode("checksum")
        return self._checksum
    @checksum.setter
    def checksum(self, value):
        if not type(value) in int_types:
            if value < 0 or value > PSFLASH_MAX_UINT16:
                raise TypeError("checksum needs to be a 16 bit integer")
        self._checksum = value

    @property
    def length_words(self):
        try:
            self._length_words
        except AttributeError:
            self._length_words = self._decode("length_words")
        return self._length_words
    @length_words.setter
    def length_words(self, value):
        if not type(value) in int_types:
            if value < 0 or value > PSFLASH_MAX_UINT16:
                raise TypeError("length_words needs to be a 16 bit integer")
        self._length_words = value

    @property
    def length_cleartext_w(self):
        if self.length_words is None:
            return 0
        else:
            return self.length_words
    @length_cleartext_w.setter
    def length_cleartext_w(self, value):
        self.length_words = value

    @property
    def length_cleartext_b(self):
        return self.length_cleartext_w * 2

    @property
    def length_ciphertext_w(self):
        return self._get_ciphertext_size_w(self.length_cleartext_w)

    @property
    def length_ciphertext_b(self):
        return self.length_ciphertext_w * 2

    @property
    def iv(self):
        try:
            self._iv
        except AttributeError:
            iv = self._decode("iv")
            if iv is None:
                self._iv = []
            else:
                self._iv = iv
        return self._iv
    @iv.setter
    def iv(self, value):
        if not type(value) in (list, tuple):
            raise TypeError("IV needs to be a list or tuple")
        if not len(value) == 4:
            raise TypeError("IV needs to be a list or tuple of 4 elements")
        for elem in value:
            if not type(elem) in int_types:
                raise TypeError("IV needs to be a list or tuple of 4 integers")
            if elem < 0 or elem > PSFLASH_MAX_UINT32:
                raise TypeError("IV needs to be a list or tuple of 4 32 bit integers")
        self._iv = value

    @property
    def sanity_check(self):
        if (self.ps_key == SIFLASH_ERASED_UINT32 or
            self.length_words == SIFLASH_ERASED_UINT16 or
            self.checksum == SIFLASH_ERASED_UINT16):
            return False
        if not self._limit is None:
            if self._addr_w + PSLFASH_HEADER_W + self.length_ciphertext_w > self._limit:
                return False
        
        return True

    @property
    def data(self):
        try:
            self._data
        except AttributeError:
            if not self._storage_w is None:
                data_offset_w = self._addr_w + PSLFASH_HEADER_W
                self._data = self._storage_w[data_offset_w:
                                             data_offset_w + self.length_ciphertext_w]
            else:
                self._data = []
        return self._data
    @data.setter
    def data(self, value):
        if not type(value) in (list, tuple):
            raise TypeError("data needs to be a list or tuple")
        for elem in value:
            if not type(elem) in int_types:
                raise TypeError("data needs to be a list or tuple of integers")
            if elem < 0 or elem > PSFLASH_MAX_UINT16:
                raise TypeError("data needs to be a list or tuple of 16 bit integers")
        self._data = value

    @property
    def ciphertext_w(self):
        return self.data
    @ciphertext_w.setter
    def ciphertext_w(self, value):
        self.data = value

    @property
    def cleartext_w(self):
        try:
            self._cleartext_w
        except AttributeError:
            if self.ps_key == PSFLASH_PSKEY_SPECIAL:
                # don't decrypt
                self._cleartext_w = self.ciphertext_w[:self.length_cleartext_w]
            else:
                cleartext_b = self._aes.decrypt(
                    dwords_to_bytes(self.iv),
                    self._crypto_key,
                    words_to_bytes(
                        self.ciphertext_w[:self.length_ciphertext_w]))
                self._cleartext_w = bytes_to_words(
                                        cleartext_b[:self.length_cleartext_b])
        return self._cleartext_w
    @cleartext_w.setter
    def cleartext_w(self, value):
        if not type(value) in (list, tuple):
            raise TypeError("data needs to be a list or tuple")
        for elem in value:
            if not type(elem) in int_types:
                raise TypeError("data needs to be a list or tuple of integers")
            if elem < 0 or elem > PSFLASH_MAX_UINT16:
                raise TypeError("IV needs to be a list or tuple of 16 bit integers")
        self._cleartext_w = value
        self.length_cleartext_w = len(self._cleartext_w)
        cleartext_w_padded = self._cleartext_w
        cleartext_w_padded.extend([PSFLASH_MAX_UINT16] *
                                  (self._get_ciphertext_size_w(self.length_cleartext_w) -
                                   self.length_cleartext_w))
        if self.ps_key == PSFLASH_PSKEY_SPECIAL:
            # don't encrypt
            self.ciphertext_w = cleartext_w_padded
        else:
            ciphertext_b = self._aes.encrypt(
                dwords_to_bytes(self.iv),
                self._crypto_key,
                words_to_bytes(cleartext_w_padded))
            self.ciphertext_w = bytes_to_words(ciphertext_b)

    @property
    def checksum_calc(self):
        if not self.sanity_check:
            return SIFLASH_ERASED_UINT16
        if self.ps_key is None or self.length_words is None or self.iv is []:
            return None
        checksum = 0
        checksum += (self.ps_key) & 0xffff
        checksum += (self.ps_key >> 16) & 0xffff
        checksum += self.length_words
        if not self.iv is None:
            for dword in self.iv:
                checksum += (dword) & 0xffff
                checksum += (dword >> 16) & 0xffff
        
        checksum = checksum & 0xffff
        
        for word in self.ciphertext_w:
            checksum = (checksum + word) & 0xffff
        
        if checksum == SIFLASH_ERASED_UINT16:
            # "42" comes from Bluecore code, but the idea is that checksum
            # should not be equal to the flash erase value.
            checksum = (checksum + 42) & 0xffff;
        
        return checksum

    @property
    def checksum_correct(self):
        if not self.sanity_check:
            return False
        return self.checksum_calc == self.checksum

    def __next__(self):
        if not self.checksum_correct:
            return None
        else:
            return PsKey(self._crypto_key,
                         self._storage_w,
                         self._addr_w + PSLFASH_HEADER_W + self.length_ciphertext_w,
                         self._limit)
    
    # Python 2 compatible iterator method
    def next(self):
        return self.__next__()
    

class PsflashStore:
    '''
    Psflash store representation
    '''
    def __init__(self, crypto_key,
                 stores_offset = PSFLASH_DEFAULT_STORES_OFFSET,
                 store_size = PSFLASH_DEFAULT_STORE_SIZE):
        self._crypto_key = crypto_key
        self._stores_offset = stores_offset
        self._store_size = store_size
        
        self._pskey_list = []
        
    @property
    def stores_offset(self):
        '''
        Byte offset of psflash store inside the flash
        '''
        return self._stores_offset

    @property
    def store_size(self):
        '''
        Size of each (main, backup) store in bytes
        '''
        return self._store_size

    def _store_key(self, ps_key, data, write_checksum = True):
        '''
        Returns psflash store respresentation of the key
        '''
        key = PsKey(self._crypto_key)
        key.ps_key = ps_key
        if ps_key == PSFLASH_PSKEY_SPECIAL:
            key.iv = [PSFLASH_MAX_UINT32] * 4
        else:
            key.iv = [random.randint(0, PSFLASH_MAX_UINT32),
                      random.randint(0, PSFLASH_MAX_UINT32),
                      random.randint(0, PSFLASH_MAX_UINT32),
                      random.randint(0, PSFLASH_MAX_UINT32)]
        key.cleartext_w = data
        if write_checksum:
            key.checksum = key.checksum_calc
        else:
            key.checksum = SIFLASH_ERASED_UINT16
        return key.encode()
    
    def _store_version(self, key):
        '''
        Given a PsKey mapped to a "special" key at the start of the store,
        returns store version.
        '''
        key_version = key.data[0]
        
        if key.ps_key != PSFLASH_PSKEY_SPECIAL:
            return 0
        
        if key_version == SIFLASH_ERASED_UINT16:
            return 0
        
        if key.length_words != 2 or \
                key.checksum != key.checksum_calc:
            return 0
        return key_version
        
    def _parse_store(self, key):
        '''
        Iterates over all keys in the store and builds a list of
        the most recent entries.
        '''                                         
        pskey_dict = {}
        
        while key.ps_key != SIFLASH_ERASED_UINT32:
            if key.ps_key != PSFLASH_PSKEY_SPECIAL:
                pskey_dict[key.ps_key] = key.cleartext_w
            key = next(key)
            
        return list(pskey_dict.items())
    
    def import_list(self, keys_list):
        '''
        Imports keys in the form of the list of tuples (ps_key, byte_data)
        '''
        pskey_list = []
        
        for (ps_key, key_data) in keys_list:
            pskey_list.append( (ps_key, bytes_to_words(key_data)) )
                
        self._pskey_list = sorted(self._pskey_list + pskey_list)

    def import_stores(self, stores):
        '''
        Given psflash stores raw data words
        imports all keys from the main store.
        '''
        store0_key = PsKey(self._crypto_key, stores, 0)
        store1_key = PsKey(self._crypto_key, stores, self.store_size//2)
        
        store_list = []
        for key in (store0_key, store1_key):
            store_list.append( (self._store_version(key), key) )
            
        store_list = sorted(store_list, reverse=True)
        
        (main_version, main_key) = store_list[0]
        
        pskey_list = []

        if main_version != 0:
            pskey_list = self._parse_store(main_key)
            
        self._pskey_list = pskey_list
        
    def export_list(self):
        '''
        Exports keys in the form of the list of tuples (ps_key, byte_data)
        '''
        keys_list = []

        for (ps_key, data) in self._pskey_list:
            keys_list.append( (ps_key, words_to_bytes(data)) )
        return keys_list
    
    def export_stores(self):
        '''
        Export keys in the form of raw psflash data words
        '''
        store_version = 1
        features = PSFLASH_FEATURE_ENCRYPTED_MASK
        store_size_words = self.store_size // 2
        
        # write main store
        main_store = []
        main_store.extend(self._store_key(PSFLASH_PSKEY_SPECIAL,
                                          [store_version, features]))
        for (ps_key, key_data) in self._pskey_list:
            main_store.extend(self._store_key(ps_key, key_data))
         
        # fill with 0xffff up to the end of the main store   
        main_store.extend([SIFLASH_ERASED_UINT16] *
                          (store_size_words - len(main_store)))
        
        # write backup store
        backup_store = []
        backup_store.extend(self._store_key(PSFLASH_PSKEY_SPECIAL,
                                            [SIFLASH_ERASED_UINT16, SIFLASH_ERASED_UINT16],
                                            write_checksum = False) )
        # fill with 0xffff up to the end of the backup store   
        backup_store.extend([SIFLASH_ERASED_UINT16] *
                            (store_size_words - len(backup_store)))
        
        # concatenate and return
        return main_store + backup_store

class PsflashHtfTools:
    '''
    Can read and write .htf files with configuration keys for psflash
    '''
    def __init__(self):
        # maps ps_key ids --> key names
        self.ps_keys = {}
        # maps key names --> ps_key ids
        self.key_names = {}

        for (key_name, idx_from, idx_to, ps_key_range_start) in KNOWN_KEYS:
            counter = 0
            for idx in range(idx_from, idx_to+1):
                key = "%s%d" % (key_name, idx)
                self.ps_keys[key] = ps_key_range_start + counter
                self.key_names[ps_key_range_start + counter] = key
                counter += 1
                
    def decode(self, input_stream):
        '''
        Parses an .htf file and returns list of tuples (ps_key, byte_data),
        where ps_key is a ps key id, e.g. "650" and byte_data
        is a list of bytes with key data, e.g. "[0, 1, 2, 3]"
        '''
        keys_list = []
        filetype = ''

        for line in input_stream:
            # Get rid of comments
            line = re.compile('#.*$').sub('', line)
            # Normalise square braces; they must have adjoining spaces
            line = re.compile(r'(\[|\])').sub(r' \1 ', line)
            # Get rid of leading and trailing whitespace
            line = re.compile(r'(^\s+|\s+$)').sub('', line)
            if line == '':
                continue
            if re.search('=', line):
                # Normalise other spaces, and lose the equals sign
                line = re.compile(r'(\s*=\s*|\s+)').sub(' ', line)
                line = line.split(' ')
                
                if line[0] == "file":
                    filetype = line[1]
                    continue
                
                if filetype == 'app1':
                    # Get key_id if first token is a name
                    try:
                        key_name = line[0]
                        key_id = self.ps_keys[key_name]
                    except KeyError:
                        # If we can't get pskey by name then lets try by ID
                        try:
                            key_id = int(line[0], 0)
                            key_name = self.key_names[key_id]
                        except (ValueError, KeyError):
                            iprint("WARNING: config key %s either unknown or invalid - "
                                  "hex (0x1A), decimal and key name strings supported" % (line[0]))
                            continue

                if filetype == 'audio':
                    key_name = line[0]
                    key_id = int(line[0], 0) + PSFLASH_AUDIO_OFFSET
                
                if line[1] != "[" or line[-1] != "]":
                    raise SyntaxError("config key %s, key data must be enclosed"
                                      " in square brackets" \
                                      " (instead of: '%s' and '%s')" % (\
                                        line[0], line[1], line[-1]))

                line = line[2:-1]
                key_data = list(map(int, line, [16 for dummy in range(len(line))]))
                
                if len(key_data) & 1:
                    raise SyntaxError("config key %s, key data length " \
                                      "is odd: %d" % (key_name,len(key_data)))
                keys_list.append((key_id, key_data))
                
        return keys_list
        
    def encode(self, output_stream, keys_list):
        '''
        Takes tuples (ps_key, bytes_data), where ps_Key is a ps key id,
        e.g. "650" and bytes_data is a list of bytes with key data,
        e.g. "[0, 1, 2, 3]. Writes these into an .htf file.
        '''

        if self.int_key(keys_list[0][0]) & PSFLASH_AUDIO_OFFSET:
            filetype = "audio"
        else:
            filetype = "app1"
        output_stream.write("file = %s" % filetype)
        for (ps_key, key_data) in keys_list:
            output_stream.write("\n\n")
            ps_key = int(self.int_key(ps_key) & ~PSFLASH_AUDIO_OFFSET)
            key = self.key_names.get(ps_key, "0x%06X" % ps_key)
            output_stream.write("%s = [" % key)
            for byte in key_data:
                output_stream.write(" %02x" % byte)
            output_stream.write(" ]")

    @staticmethod
    def int_key(data):
        try:
            return int(data, 16)
        except TypeError:
            return data


class PsflashContent:
    """
    API class for psflash contents
    """
    def __init__(self, rw_cfg_data, rw_cfg_size):
        self.rw_cfg_data = rw_cfg_data
        self.rw_cfg_size = rw_cfg_size
        self.PSKEY_USR0 = 650
        self.PSKEY_DSP0 = 0x2000 + 600
        self.PSKEY_CONNLIB0 = 0x2000 + 1550
        self.PSKEY_USR50 = 0x2000 + 1950
        self.PSKEY_CUSTOMER0 = 0x2000 + 2000
        self.PSKEY_UPGRADE0 = 0x2000 + 2600
        self.key_names = dict()
        self.key_names[0] = "SPECIAL"
        for i in range(0, 50):
            self.key_names[self.PSKEY_USR0 + i] = "USR%d" % i
            self.key_names[self.PSKEY_DSP0 + i] = "DSP%d" % i
            self.key_names[self.PSKEY_CONNLIB0 + i] = "CONNLIB%d" % i
            self.key_names[self.PSKEY_USR50 + i] = "USR%d" % (i + 50)
            self.key_names[self.PSKEY_UPGRADE0 + i] = "UPGRADE%d" % i
        for i in range(0, 300):
            self.key_names[self.PSKEY_CUSTOMER0 + i] = "CUSTOMER%d" % i

    def verbose_key_name(self, ps_key):
        try:
            return self.key_names[ps_key]
        except KeyError:
            if ps_key & PSFLASH_AUDIO_OFFSET:
                return "AUDIO(%d)" % (ps_key & ~PSFLASH_AUDIO_OFFSET)
            else:
                return "0x%x" % (ps_key)

    def keys(self, show_data=False, show_cipher=False, report=False,
             storage_data=None,
             full_log=False, keys=None, crypto_key=None):
        """
        Dump ps_keys defined in the storage.

        If "storage_data" is provided then it is parsed instead of the data
        in the flash.

        If "full_log" is "True" then all keys are printed, otherwise
        just the last instance of each key.
        """
        if keys is not None:
            # Convert a simple parameter to a list
            # Lists, tuples aren't converted.
            try:
                0 in keys
            except TypeError:
                keys = [keys]

        # Dictionary indexed by ps_key, returns tuple (counter, KEY),
        # where "counter" gives number of instances of ps_key,
        # KEY references the last instance (object of class PsKey)
        self._keys = {}

        if crypto_key is None:
            crypto_key = PSFLASH_DEFAULT_CRYPTO_KEY

        if not storage_data:
            storage_data = self.get_main_storage()

        key = PsKey(crypto_key, storage_data, 0, len(storage_data))

        if full_log:
            header = "Configuration keys"
            output = interface.Group(header)

            headers = ["offset", " ps_key", "name", "words", "instance", "check", "data"]
            output_table = interface.Table(headers)

            while key and key.ps_key != SIFLASH_ERASED_UINT32:
                ps_key = key.ps_key

                try:
                    counter = self._keys[key.ps_key]
                except KeyError:
                    counter = 0
                counter += 1
                self._keys[key.ps_key] = counter

                clear = ""
                pos = 0
                for w in key.cleartext_w:
                    tmp = words_to_bytes([w])
                    clear = clear + "%02x %02x " % (tmp[0], tmp[1])
                    pos += 1
                    if (pos % 8) == 0:
                        clear += "\r"

                output_table.add_row([hex(key.address),
                                      hex(ps_key),
                                      self.verbose_key_name(ps_key),
                                      str(key.length_words),
                                      str(counter),
                                      str(key.checksum_correct),
                                      clear])
                key = next(key)
            if key:
                # check for trailing garbage
                offset = key.address
                while offset < len(storage_data):
                    if storage_data[offset] != SIFLASH_ERASED_UINT16:
                        output_table.add_row([hex(offset),
                                              "-",
                                              "garbage",
                                              "-",
                                              "-",
                                              "-",
                                              "%02x %02x" % (storage_data[offset] >> 16,
                                                             storage_data[offset] & 0xff)])
                    offset += 1

            output.append(output_table)
            if report:
                return output
            TextAdaptor(output, gstrm.iout)
            return

        while key and key.ps_key != SIFLASH_ERASED_UINT32:
            try:
                (counter, prev_key) = self._keys[key.ps_key]
            except KeyError:
                counter = 0
            self._keys[key.ps_key] = (counter, key)
            key = next(key)

        ps_keys = sorted(self._keys)

        header = "Configuration keys"
        if show_data or keys is not None:
            output = header
            if show_cipher:
                header = "Crypto key "
                for i in range(len(crypto_key)):
                    header += "%02x" % crypto_key[i]
                output = os.linesep.join([output, header])
            for ps_key in ps_keys:
                (counter, key) = self._keys[ps_key]
                if keys is not None:
                    if ps_key not in keys:
                        continue
                name = "%04d - %s" % (ps_key, self.verbose_key_name(ps_key))
                iv = ""
                if show_cipher:
                    tmp = dwords_to_bytes(key.iv)
                    for i in range(len(tmp)):
                        iv += "%02x" % tmp[i]
                    cipher = ""
                    for w in key.ciphertext_w:
                        tmp = words_to_bytes([w])
                        cipher = cipher + "%02x %02x " % (tmp[0], tmp[1])
                    output = os.linesep.join([output, "%s: (iv %s) %s" % (name, iv, cipher)])
                else:
                    clear = ""
                    for w in key.cleartext_w:
                        tmp = words_to_bytes([w])
                        clear = clear + "%02x %02x " % (tmp[0], tmp[1])
                    output = os.linesep.join([output, "%s: %s" % (name, clear)])
            output = interface.Code(output)
            if report:
                return output
            TextAdaptor(output, gstrm.iout)
        else:
            output = interface.Group(header)

            headers = ["ps_key", "words", "instances", "checksum OK"]
            output_table = interface.Table(headers)

            for ps_key in ps_keys:
                (counter, key) = self._keys[ps_key]
                name = "%04d - %s" % (ps_key, self.verbose_key_name(ps_key))
                output_table.add_row([name,
                                      str(key.length_words),
                                      str(counter),
                                      str(key.checksum_correct)])

            output.append(output_table)
            if report:
                return output
            TextAdaptor(output, gstrm.iout)

    def get_main_storage(self):
        """
        Get the key data from the firmware when P0 symbols are not present
        Storage is divided into two sections. One is main and the
        other is backup. In order to find out the main one, we need
        to find the special key in the section. The special key is the
        first set of bytes in each section.
        The data for special key consists of a header(psflash_key_header)
        and payload(psflash_special_key_data) in
        app_ss/main/fw/src/core/psflash/psflash_private.h.
        We check the version field in each payload. If the version is 0xffff
        in one of the sections, the other storage is main.
        Otherwise, the section with the higher version is main.
        """
        storage_data = bytes_to_words(self.rw_cfg_data[:self.rw_cfg_size])
        # Divide by 4 to get the start address of the second storage in words.
        storage_two_offset = self.rw_cfg_size//4
        # Version comes right after psflash_key_header so the version
        # offset is the length of psflash_key_header.
        version_offset = 192//4  # no. of bits in header/4 (words)
        key_ver_one = storage_data[version_offset]
        key_ver_two = storage_data[storage_two_offset+version_offset]
        if key_ver_one == key_ver_two:
            raise RuntimeError("Storage data either corrupted of not loaded "
                               "properly. Please load firmware and try again.")

        if key_ver_one == 0xffff:
            main_storage = storage_data[storage_two_offset:]
        elif key_ver_two == 0xffff:
            main_storage = storage_data[0:storage_two_offset]
        # Neither versions are 0xffff so compare their values
        elif key_ver_one > key_ver_two:
            main_storage = storage_data[0:storage_two_offset]
        else:
            main_storage = storage_data[storage_two_offset:]

        return main_storage


class Psflash(FirmwareComponent):
    """
    Psflash class implementation.
    """
    
    def __init__(self, fw_env, core):
        FirmwareComponent.__init__(self, fw_env, core)

    # FirmwareComponent interface
    #----------------------------

    def _generate_report_body_elements(self):
        try:
            return [self.info(report=True)]
        except KeyError:
            return [interface.Code("Psflash info failed")]

    def _on_reset(self):
        pass

    @property
    def _crypto_key(self):
        # Get the key straight from memory to support coredumps
        return dwords_to_bytes(
            [k.value for k in self._apps0.fw.gbl.psflash_crypto_key])

    @property
    def _apps0(self):
        return self._core.subsystem.cores[0]

    @property
    def _apps1(self):
        return self._core.subsystem.cores[1]

    def _trb_read_attempt(self, address, bytes_count):
        '''
        Read block of Apps memmory space using raw trb read transactions.
        '''
        trb = self._core.subsystem.chip.device.transport.trb

        # B-232939 is investigating a problem with reading corrupted data
        # from serial flash using trans.trb.read. Until we have the answer,
        # limit max_trans_count to lower the chance of hitting the problem.
        # max_trans_count = trb.get_max_transactions()
        max_trans_count = 100
        
        bytes_per_transaction = 4
        
        data = []
        offset = 0
        
        while bytes_count:
            remaining_trans_count = (bytes_count + bytes_per_transaction - 1) // \
                bytes_per_transaction
            if remaining_trans_count > max_trans_count:
                current_trans_count = max_trans_count
                current_bytes_count = current_trans_count * bytes_per_transaction
            else:
                # the last chunk
                current_trans_count = remaining_trans_count
                current_bytes_count = bytes_count
            
            read_data = trb.read(4, 0, address + offset,
                                 bytes_per_transaction,
                                 current_trans_count * bytes_per_transaction)
            
            if bytes_per_transaction == 4:
                # reverse bytes order in every dword
                for i in range(0, len(read_data), 4):
                    read_data[i], read_data[i+1], read_data[i+2], read_data[i+3] = \
                        read_data[i+3], read_data[i+2], read_data[i+1], read_data[i]
            
            # accumulate read data
            data.extend(read_data[0:current_bytes_count])
            
            offset += current_bytes_count
            bytes_count -= current_bytes_count

        return data

    def _trb_read(self, address, bytes_count):
        '''
        Read block of Apps memmory space using raw trb read transactions.
        '''
        # B-232939 is investigating a problem with reading corrupted data
        # from serial flash using trans.trb.read. Until we have the answer,
        # read it twice and compare results to make sure data is good.
        retry = 5
        while retry > 0:
            data_0 = self._trb_read_attempt(address, bytes_count)
            data_1 = self._trb_read_attempt(address, bytes_count)
        
            if data_0 == data_1:
                return data_0
            
            retry -= 1

        # tried several times and failed to get consistent data
        raise IOError("Can't read reliably, data always changes: "
                      "consider pausing Apps P1 core with apps1.pause() "
                      "or re-trying again later. ")

    def _tctrans_read(self, address, bytes_count):
        '''
        Read block of Apps memmory space using tctrans read transactions.
        '''
        # For trb read data are read twice to avoid bad reading
        # Do the same here to be sure the good data is returned
        retry = 5
        while retry > 0:
            data_0 = self._tctrans_read_attempt(address, bytes_count)
            data_1 = self._tctrans_read_attempt(address, bytes_count)
        
            if data_0 == data_1:
                return data_0
            
            retry -= 1

        # tried several times and failed to get consistent data
        raise IOError("Can't read reliably, data always changes: "
                      "consider pausing Apps P1 core with apps1.pause() "
                      "or re-trying again later. ")

    def _tctrans_read_attempt(self, address, bytes_count):
        '''
        Read block of Apps memory space using tctrans read transactions.
        '''
        tctrans = self._core.subsystem.chip.device.transport.tctrans

        # For _trb_read_attempt function we limit max_trans_count to lower the
        # chance of hitting the problem of reading corrupted data
        # Do the same thing here
        max_trans_count = 100

        bytes_per_transaction = 4
        
        data = []
        offset = 0
        
        while bytes_count:
            remaining_trans_count = (bytes_count + bytes_per_transaction - 1) // \
                bytes_per_transaction
            if remaining_trans_count > max_trans_count:
                current_trans_count = max_trans_count
                current_bytes_count = current_trans_count * bytes_per_transaction
            else:
                # the last chunk
                current_trans_count = remaining_trans_count
                current_bytes_count = bytes_count
            
            read_data = tctrans.read_regbased(4, 0, bytes_per_transaction, True, #debug_txns
                                              address + offset,
                                              current_trans_count * bytes_per_transaction)
            # accumulate read data
            data.extend(read_data[0:current_bytes_count])

            offset += current_bytes_count
            bytes_count -= current_bytes_count

        return data

    def _addr_read_words(self, address, count):
        '''
        Read from Apps memory. For live chip with raw trb transactions
        and for coredump use dm view.
        '''
        mem_data = []
        transport = self.get_transport_type()

        if transport is None:
            # some other transport
            pass
        elif transport == "coredump":
            mem_data =  bytes_to_words(self._apps0.dm[address: address+(count * 2)])
        elif transport == "trb":
            # live chip
            mem_data = bytes_to_words(self._trb_read(address, count * 2))
        elif transport == "tctrans":
            mem_data = bytes_to_words(self._tctrans_read(address, count * 2))

        return mem_data

    def get_transport_type(self):
        # some other transport
        trsp_type = None
        type_list = [ 
            "trb",
            "tctrans" # for USBDBG
            ]
        if self._core.subsystem.chip.device.transport is None:
            # this is coredump
            trsp_type = "coredump"
        else:
            for name in type_list:
                try:
                    cmd_str = "self._core.subsystem.chip.device.transport." + name
                    exec(cmd_str)
                    trsp_type = name
                except AttributeError:
                    continue
        
        return trsp_type
    
    @property
    def cfg(self):
        """
        Psflash configuration structure
        """
        try:
            self._cfg
        except AttributeError:
            self._cfg = self._apps0.fw.gbl.psflash_config

        return self._cfg
            
    def storage_clear(self):
        '''
        Erase serial flash area correspondent to psflash storages.
        '''
        from_addr = self.cfg.storage[0].start.value
        to_addr = self.cfg.storage[1].end.value
        
        success = self._apps0.fw.siflash.erase_region(from_addr, to_addr)
        
        if success:
            return "OK"
        else:
            return "FAILED"

    def storage_dump(self, xuv_output_filename):
        '''
        Dump psflash storage into an .xuv file.
        '''
        psflash_stores_offset = self.cfg.storage[0].start.value
        psflash_store_size = self.cfg.storage[1].end.value - psflash_stores_offset + 1
        
        io_values = self._apps0.core.info._io_map_info.misc_io_values
        sqif_direct_start =  io_values["$P0D_SQIF0_DIRECT_LOWER_ENUM"]
        
        psflash_store_addr = psflash_stores_offset + sqif_direct_start

        transport = self.get_transport_type()

        if transport is None:
            # some other transport
            pass
        elif transport == "coredump":
            store_data =  self._apps0.dm[psflash_store_addr: psflash_store_addr+psflash_store_size]
        elif transport == "trb":
            # live chip
            store_data = self._trb_read(psflash_store_addr, psflash_store_size)
        elif transport == "tctrans":
            store_data = self._tctrans_read(psflash_store_addr, psflash_store_size)

        config_file = open(xuv_output_filename, "w")

        store_pos = 0
        while store_pos < len(store_data):
            data_low = store_data[store_pos]
            data_hi = store_data[store_pos + 1]
            flash_word_address = (psflash_stores_offset + store_pos) // 2
            config_file.write("@%06X   %04X\n" % (flash_word_address,
                                                  (data_hi << 8) | data_low))
            store_pos = store_pos + 2
                
        config_file.close()

    def storage_xuv_detect(self, xuv_input_filename):
        '''
        Parse .xuf with full flash dump to find all PS stores.
        Outputs offset of every store found.
        '''
        # search for storage in the dump
        xuv_file = open(xuv_input_filename, "r")

        # Start of PS store is alligned to erase block size and
        # 4KB is the smallest supported.
        block_size = 4096
    
        seeking = 0
        collecting = False
        storage_data = []
        for line in xuv_file:
            try:
                [offset_str, data_str] = line[1:].split()
            except:
                # ignore any lines that do not match the pattern:
                # "@0C0000   FFFF"
                continue

            offset = int(offset_str, 16)

            if offset < seeking:
                continue

            data = int(data_str, 16)

            #print "0x%x : 0x%x" % (offset, data)

            # xuv offset is in words
            if (offset & (block_size//2-1) == 0) and (data == 0):
                #print "start collecting"
                collecting = True
                storage_data = [data]
            elif collecting:
                #print "keep collecting"
                storage_data.append(data)
                if len(storage_data) == 20:
                    #print "checking at offset "
                    if self._check_storage(storage_data):
                        storage_start = (offset - len(storage_data) + 1) * 2
                        print("Found storage at offset 0x%x" % storage_start)
                    collecting = False
            else:
                # skip the rest of erase block
                seeking = (offset + block_size//2) & ~(block_size//2-1)
                #print "seeking 0x%x" % seeking
                

        xuv_file.close()
        
    def storage_xuv_parse(self, xuv_input_filename,
                     storage_start = None, storage_end = None,
                     full_log = False):
        '''
        Load PS store content from an .xuv file.
        
        If "storage_start" is provided then does not rely on the fw data structures,
        otherwise looks into ps store config to find it.
        
        If "full_log" == TRUE outputs the whole log of PS writes, otherwise
        just the last instance of each key.
        '''
        # If "storage_start" or "storage_end" were not provided, get these
        # from the FW psflash config or use defaults
        if storage_start:
            if not storage_end:
                # default: 128KB PS storage
                storage_end = storage_start + 2*64*1024 - 1
        else: 
            storage_start = self.cfg.storage[0].start.value
            if not storage_end:
                storage_end = self.cfg.storage[1].end.value

        # offset and limit of the storage inside the flash
        storage_offset = storage_start // 2
        storage_offset_limit = storage_end // 2
        
        xuv_file = open(xuv_input_filename, "r")
        
        storage_data = []
        
        for line in xuv_file:
            try:
                [offset_str, data_str] = line[1:].split()
            except:
                # ignore any lines that do not match the pattern:
                # "@0C0000   FFFF"
                continue
            
            offset = int(offset_str, 16)

            if offset > storage_offset_limit:
                # too far, stop reading
                break
            elif offset == storage_offset:
                storage_data.append(int(data_str, 16)) 
                storage_offset += 1
            else:
                # keep searching
                continue

        xuv_file.close()

        if storage_data:
            if self._check_storage(storage_data):
                print("PS STORAGE 0 at 0x%x has keys" % storage_start)
                self.keys(storage_data = storage_data,
                          full_log = full_log)
                return
            
            if len(storage_data) > 32*1024:
                # appear to have both storage 0 and 1, try parsing storage 1
                storage_data = storage_data[len(storage_data) // 2 : ]
                if self._check_storage(storage_data):
                    print("PS STORAGE 1 at 0x%x has keys" % ((storage_start + storage_end + 1) // 2))
                    self.keys(storage_data = storage_data,
                              full_log = full_log)
                    return

        print("Could not find PS keys in the .xuv file")

    def info(self, report=False):
        '''
        Print psflash configuraton
        '''
        output = interface.Group("psflash")

        lines = ""

        for store in [0,1]:
            store_desc = self.cfg.storage[store]
            if self.cfg.main.value == store:
                role = "MAIN"
            else:
                role = "BACKUP"
            flags = ""
            if store_desc.erase_needed.value:
                flags = flags + "ERASE_NEEDED "
            if store_desc.erasing.value:
                flags = flags + "ERASING"

            lines += "Store[%d] %s [0x%x:0x%x] version %d %s\n" % (
                                                store, role,
                                                store_desc.start.value,
                                                store_desc.end.value,
                                                store_desc.version.value,
                                                flags)

        if self.cfg.defrag_running.value:
            defrag = "RUNNING"
        else:
            defrag = "not running"
        lines += "Defrag: %s\n" % defrag

        store_desc = self.cfg.storage[0]
        store_size = store_desc.end.value - store_desc.start.value + 1
        lines += "Main store %0.1f%% free (%d/%d defrag threshold %d)\n" % (
                100 - self.cfg.free_space_offset.value * 100.0 / store_size,
                self.cfg.free_space_offset.value, store_size,
                self.cfg.defrag_threshold.value)

        output_code = interface.Code(lines)
        output.append(output_code)

        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)
        
    def _check_storage(self, storage_data):
        '''
        Check that the data starts with a SPECIAL key that has correct CRC. 
        '''
        key = PsKey(self._crypto_key, storage_data, 0, len(storage_data))

        return (key and 
                key.ps_key == PSFLASH_PSKEY_SPECIAL and
                key.checksum_correct)

    def keys(self, show_data=False, show_cipher=False,
             storage_data=None, report=False,
             full_log=False, keys=None):
        """
        Dump ps_keys defined in the storage.

        If "storage_data" is provided then it is parsed instead of the data
        in the flash.

        If "full_log" is "True" then all keys are printed, otherwise
        just the last instance of each key.
        """
        crypto_key = PSFLASH_DEFAULT_CRYPTO_KEY
        if not storage_data:
            try:
                self.cfg
            except AttributeError:
                # If P0 symbols are not present, we need to calculate
                # the rw_config partition's main_storage offset.
                # Looking for the key index in the firmware directly
                img = ImageBuilder(apps_subsys=self._apps1.subsystem,
                                   read_from_sqif=True)
                # rw_config is where the keys are located
                # We get the raw data from that section
                try:
                    rw_config = [d for d in img.image_header.sections
                                 if d["name"] == "rw_config"][0]
                except IndexError:
                    raise RwConfigNotPresent
                # Offset for rw config partition
                offset = (rw_config["offset"] +
                          img.boot_image["primary_image_offset"])
                # End address of rw_config
                end_offset = offset + rw_config["capacity"]
                storage_data = self._apps1.subsystem.raw_sqif0[offset:end_offset]
                self.ps_content = PsflashContent(storage_data,
                                                 rw_config["size"])
                storage_data = self.ps_content.get_main_storage()
            else:
                storage_index = self.cfg.main.value
                storage = self.cfg.storage[storage_index]

                storage_offset = storage.start.value
                storage_size = storage.end.value - storage.start.value + 1

                io_values = self._apps0.core.info._io_map_info.misc_io_values
                sqif_direct_start = io_values["$P0D_SQIF0_DIRECT_LOWER_ENUM"]

                base = sqif_direct_start + storage_offset

                storage_size_words = storage_size // 2
                storage_data = self._addr_read_words(base, storage_size_words)
                crypto_key = self._crypto_key
                self.ps_content = PsflashContent(storage_data,
                                                 storage_size_words)
        else:
            self.ps_content = PsflashContent(storage_data, len(storage_data))
        self.key_names = self.ps_content.key_names
        output = self.ps_content.keys(show_data=show_data,
                                      show_cipher=show_cipher,
                                      storage_data=storage_data,
                                      report=report,
                                      full_log=full_log, keys=keys,
                                      crypto_key=crypto_key)
        if report:
            return output

    def _get_test_buffer(self, size):
        '''
        Allocates buffer in P1
        '''
        return self._apps1.fw.call.pnew("uint16", size)
    
    def _release_test_buffer(self, buf):
        '''
        Release buffer allocated with _get_test_buffer()
        '''
        self._apps1.fw.call.pfree(buf.address)
    
    def PsStore(self, key, buf):
        '''
        Runs PsStore trap
        '''
        if buf:
            # store data into the test buffer inside P1 firmware
            words = len(buf)
            apps1_buf = self._get_test_buffer(words)
            for i in range(0,words):
                apps1_buf[i].value = buf[i]
            buf_address = apps1_buf.address
        else:
            buf_address = 0
            words = 0
        # run the trap
        
        # the trap can fail because defrag has started
        # retry it 10 times during 10 seconds before giving up
        for retry in range(0,10):
            ret = self._apps1.fw.call.PsStore(key, buf_address, words)
            if ret != 0 or (ret == words and 
                            self.PsRetrieve(key, 0) == 0):
                break
            time.sleep(1)
        
        if buf:
            self._release_test_buffer(apps1_buf)
        return ret 
        
        # mibclear test
        
    def PsRetrieve(self, key, words=None, bytes=False, hex=False):
        '''
        Runs the PsRetrieve trap.
        If words == 0, returns the size of key data in words, if words is not
        supplied then the key data is read and retrieved. If a value is supplied
        for words behaves in the same way as the PsRetrieve API and will return
        the key data if the key is the same size (or smaller) than words.
        The key is normally returned as a list of "words" (uint16 values), which is
        the format used by the PsRetrieve trap. The bytes parameter can change this
        to a list of bytes.
        If the hex parameter is used returns a space separated hex string, of words
        or bytes.
        '''
        if words is None:
            words = self.PsRetrieve(key, 0)
        if words:
            apps1_buf = self._get_test_buffer(words)
            buf_address = apps1_buf.address
        else:
            buf_address = 0
        # run the trap
        res = self._apps1.fw.appcmd.call_function("PsRetrieve",
                                         [key, buf_address, words])
        # read back data from the test buffer inside the P1 firmware,
        # but only if test returns success and we requested data before
        if words:
            buf = []
            for i in range(0, res):
                buf.append(apps1_buf[i].value)
            self._release_test_buffer(apps1_buf)
            if bytes:
                buf2 = []
                for word in buf:
                    buf2.extend([word&0xFF,word>>8])
                buf = buf2
            if hex:
                if bytes:
                    return " ".join("{:02X}".format(x) for x in buf)
                return " ".join("{:04X}".format(x) for x in buf)
            return buf
        return res
    
    def PsFullRetrieve(self, key, words):
        '''
        Runs PsFullRetrieve trap
        '''
        if words:
            apps1_buf = self._get_test_buffer(words)
            buf_address = apps1_buf.address
        else:
            buf_address = 0
        # run the trap
        res = self._apps1.fw.appcmd.call_function("PsFullRetrieve",
                                         [key, buf_address, words])
        # read back data from the test buffer inside the P1 firmware,
        # but only if test returns success and we requested data before
        if words:
            buf = []
            for i in range(0, res):
                buf.append(apps1_buf[i].value)
            self._release_test_buffer(apps1_buf)
            
            return buf
        return res
        
    def PsFreeCount(self, words):
        '''
        Runs PsFreeCount trap
        '''
        return self._apps1.fw.appcmd.call_function("PsFreeCount", [words])
    
    def PsFlood(self, wait=False):
        '''
        Deprecated function that runs the PsDefrag trap
        Calls the current function to run the PsDefrag trap.
        '''
        iprint ("WARNING: Psflash.PsFlood is deprecated and will be removed in future.")
        iprint ("Please use Psflash.PsDefrag instead.")
        return self.PsDefrag(wait=wait)

    def PsDefrag(self, wait=False):
        '''
        Runs PsDefrag trap
        If wait == True, wait for defragment to finish.
        '''
        
        try:
            self._apps1.fw.appcmd.call_function("PsDefrag", [])
        except KeyError:
            # It used to be called PsFlood, so try that
            self._apps1.fw.appcmd.call_function("PsFlood", [])
        
        if wait:
            cfg = self.gbl.psflash_config
            while cfg.defrag_running.value != 0:
                time.sleep(0.1)
    
    def PsSetStore(self, store):
        '''
        Runs PsSetStore trap
        '''
        self._apps1.fw.appcmd.call_function("PsSetStore", [store])
    
    def PsGetStore(self):
        '''
        Runs PsGetStore trap
        '''
        return self._apps1.fw.appcmd.call_function("PsGetStore")
