############################################################################
# CONFIDENTIAL
#
# %%fullcopyright(2016)
#
############################################################################
'''
Converts between .htf files with configuration keys and
.xuv files with psflash stores BLOB.

Usage:
> python psflash_converter.py -f <input_file> -t <output_file> \
      [-o <stores_offset>] [-s <store_size>]
where
<input_file> and <output_file> can be either .htf or .xuv files.
<stores_offset> is byte offset of the psflash store in the flash
<store_size> size in bytes of each store
'''

if __name__ == "__main__":
    import sys
    import os
    sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 '..', '..', '..'))

from optparse import OptionParser
import re
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import dwords_to_bytes_be
from csr.dev.fw.meta.xuv_stream_decoder import XUVStreamDecoder
from csr.dev.fw.meta.xuv_stream_encoder import XUVStreamEncoder
from csr.dev.fw.psflash import PsflashStore, PsflashHtfTools, \
                               SIFLASH_ERASED_UINT16, \
                               PSFLASH_DEFAULT_CRYPTO_KEY, \
                               PSFLASH_DEFAULT_STORES_OFFSET, \
                               PSFLASH_DEFAULT_STORE_SIZE, \
                               KNOWN_KEYS



class PsflashStorage:
    def __init__(self, input_file = None, output_file = None):
        self.input_file = input_file
        self.output_file = output_file
        
        self.ps_keys = {}
        self.key_names = {}
        
        # KNOWN_KEYS (key_name, index_from, index_to, ps_key_range_start)
        for (key_name, idx_start, idx_stop, ps_key_range_start) in KNOWN_KEYS:
            counter = 0
            for idx in range(idx_start, idx_stop+1):
                key = "%s%d" % (key_name, idx)
                self.ps_keys[key] = ps_key_range_start + counter
                self.key_names[ps_key_range_start + counter] = key
                counter += 1 
    
        self.psflash_stores_offset = 0xD0000
        self.psflash_store_size = 0x10000
        self.PSFLASH_PSKEY_SPECIAL = 0
        self.SIFLASH_ERASED_UINT8 = 0xff
        self.SIFLASH_ERASED_UINT16 = 0xffff
        
    def convert(self):
        '''
        Loads configuration keys from input file and stores them
        into output file
        '''
        if re.search(r".*\.[hH][tT][fF]$", self.input_file):
            self.load_htf(self.input_file)
        elif re.search(r".*\.[xX][uU][vV]$", self.input_file):
            self.load_xuv(self.input_file)
        else:
            raise ValueError("Input file is neither .htf nor .xuv")
        
        if re.search(r".*\.[hH][tT][fF]$", self.output_file):
            self.write_htf(self.output_file)
        elif re.search(r".*\.[xX][uU][vV]$", self.output_file):
            self.write_xuv(self.output_file)
        else:
            raise ValueError("Output file is neither .htf nor .xuv")
    
    def load_htf(self, input_file):
        """
        Load configuration keys from an .htf file.
        """
        self.pskey_list = []
    
        # Get rid of comments
        p1 = re.compile('#.*$')
        # Normalise square braces; they must have adjoining spaces
        p2 = re.compile('(\[|\])')
        # Get rid of leading and trailing whitespace
        p3 = re.compile('(^\s+|\s+$)')
        # Normalise other spaces, and lose the equals sign
        p4 = re.compile('(\s*=\s*|\s+)')
    
        # Read through the *text* config file and dig out the ones we need.
        config_file = open(input_file, "r")
        for line in config_file:
            line = p1.sub('', line)
            line = p2.sub(' \\1 ', line)
            line = p3.sub('', line)
            if line == '':
                continue
            if re.search('=', line):
                line = p4.sub(' ', line)
                line = line.split(' ')
                
                if line[0] not in self.ps_keys:
                    continue
                
                if line[1] != "[" or line[-1] != "]":
                    raise SyntaxError("config key %s, key data must be enclosed"
                                      " in square brackets" \
                                      " (instead of: '%s' and '%s')" % (\
                                        line[0], line[1], line[-1]))

                key_name = line[0]
                line = line[2:-1]
                
                ps_key = (self.ps_keys[key_name],
                          map(int, line, [ 16 for dummy in range(len(line))]))
                
                if len(ps_key[1]) & 1:
                    raise SyntaxError("config key %s, key data length " \
                                      "is odd: %d" % (key_name,len(ps_key[1])))
                self.pskey_list.append(ps_key)
                
        self.pskey_list = sorted(self.pskey_list)
        
        iprint(self.pskey_list)
    
    def write_htf(self, output_file):
        '''
        Write confiugration keys into an .htf file
        '''
        config_file = open(output_file, "w")
        
        config_file.write("file=app1\n")
        
        for (ps_key, data) in self.pskey_list:
            config_file.write("%s = [" % self.key_names[ps_key])
                        
            for d in data:
                config_file.write(" %02x" % d)
                
            config_file.write(" ]\n")
            
        config_file.close()
    
    
    def _load_xuv(self, xuv_file):
        '''
        Load configuration keys from an .xuv file
        '''
        pass
    
    def _put_key(self, data, pos, ps_key, key_data):
        '''
        Write configuration key into the store, returns new free space
        offset.
        '''
        # ps_key
        data[pos] = ps_key >> 8
        data[pos+1] = ps_key & 0xff
        
        # checksum
        length = len(key_data)
        if length & 1:
            raise SyntaxError("ps_key %d, key data length " \
                              "is odd: %d" % (ps_key, length))
        length_words = length // 2
        
        checksum = (ps_key + length_words) & 0xffff;
        data_pos = 0
        while data_pos < length:
            d = (key_data[data_pos] << 8) + key_data[data_pos+1]
            checksum = (checksum + d) & 0xffff
            data_pos = data_pos + 2
            
        if checksum == self.SIFLASH_ERASED_UINT16:
            checksum = (checksum + 42) & 0xffff
            
        data[pos+2] = checksum >> 8
        data[pos+3] = checksum & 0xff
        
        # length
        data[pos+4] = length_words >> 8
        data[pos+5] = length_words & 0xff
        
        data[pos+6 : pos+6+length] = key_data
        return pos + 6 + length
    
    def _swap_bytes(self, data):
        pos = 0
        data_swapped = []
        while pos < len(data):
            data_swapped.append(data[pos+1])
            data_swapped.append(data[pos])
            pos = pos + 2
        return data_swapped
    
    def write_xuv(self, output_file):
        '''
        Write configuration keys into an .htf file
        '''
        
        store_data = [self.SIFLASH_ERASED_UINT8] * self.psflash_store_size
        
        store_pos = 0
        store_version = 10
        store_pos = self._put_key(store_data, store_pos,
                                  self.PSFLASH_PSKEY_SPECIAL,
                                  [store_version >> 8, store_version & 0xff])
        
        for (ps_key, key_data) in self.pskey_list:
            key_data = self._swap_bytes(key_data)
            store_pos = self._put_key(store_data, store_pos, ps_key, key_data)
        
        config_file = open(output_file, "w")
        
        store_max_pos = store_pos
        store_pos = 0
        while store_pos < store_max_pos:
            data_hi, data_low = store_data[store_pos], store_data[store_pos+1]
            flash_word_address = (self.psflash_stores_offset + store_pos) // 2
            config_file.write("@%06X   %04X\n" % (flash_word_address,
                                                  (data_hi << 8) | data_low))
            store_pos = store_pos + 2
        
        flash_word_address = (self.psflash_stores_offset + self.psflash_store_size) // 2
        config_file.write("@%06X   %04X\n" % (flash_word_address,
                                              self.SIFLASH_ERASED_UINT16))
        
        config_file.close()


class PsflashConverter:
    '''
    Converts between .htf and .xuv files with psflash keys
    '''
    def __init__(self,
                 crypto_key = PSFLASH_DEFAULT_CRYPTO_KEY,
                 stores_offset = PSFLASH_DEFAULT_STORES_OFFSET,
                 store_size = PSFLASH_DEFAULT_STORE_SIZE):
        self.psstore = PsflashStore(crypto_key, stores_offset, store_size)
        self.htf = PsflashHtfTools()
        
    def convert(self, input_files, output_file):
        '''
        Loads configuration keys from input file and stores them
        into output file
        '''
        for input_file in input_files:
        
            if re.search(r".*\.[hH][tT][fF]$", input_file):
                self.load_from_htf(input_file)
            elif re.search(r".*\.[xX][uU][vV]$", input_file):
                self.load_from_xuv(input_file)
            else:
                raise ValueError("Input file is neither .htf nor .xuv")
        
        if re.search(r".*\.[hH][tT][fF]$", output_file):
            self.store_to_htf(output_file)
        elif re.search(r".*\.[xX][uU][vV]$", output_file):
            self.store_to_xuv(output_file)
        else:
            raise ValueError("Output file is neither .htf nor .xuv")
        
    def load_from_htf(self, input_file):
        """
        Load configuration keys from an .htf file.
        """        
        input_stream = open(input_file, "r")
        
        keys_list = self.htf.decode(input_stream)
        self.psstore.import_list(keys_list)
        
        input_stream.close() 
        
    def store_to_htf(self, output_file):
        '''
        Write confiugration keys into an .htf file
        '''
        output_stream = open(output_file, "w")
        
        keys_list = self.psstore.export_list()
        self.htf.encode(output_stream, keys_list)
        
        output_stream.close

    def load_from_xuv(self, input_file):
        '''
        Load configuration keys from an .xuv file
        '''                    
        stores = [SIFLASH_ERASED_UINT16] * (self.psstore.store_size // 2 * 2)
    
        # Read through the *text* config file and dig out the ones we need.
        input_stream = open(input_file, "r")
        xuv_data = XUVStreamDecoder(input_stream).address_value_pairs
        
        for (word_addr, word_data) in xuv_data:
                offset = word_addr - self.psstore.stores_offset // 2
                stores[offset] = word_data
        
        self.psstore.import_stores(stores)
        
        input_stream.close()

    def store_to_xuv(self, output_file):
        '''
        Write configuration keys into an .htf file
        '''
        stores = self.psstore.export_stores()
        
        output_stream = open(output_file, "w")
        xuv_encoder = XUVStreamEncoder(output_stream)
        
        word_addr = self.psstore.stores_offset // 2
        for word_data in stores:
            xuv_encoder.address_value_pair(word_addr, word_data)
            word_addr += 1
        
        output_stream.close()
        


if __name__ == "__main__":
    parser = OptionParser()
    parser.add_option("-f", "--from",
                      help="Input file name (either .htf or .xuv)",
                      default=None, type="string", dest="input_files")
    parser.add_option("-t", "--to",
                      help="Output file name (either .htf or .xuv)",
                      default=None, type="string", dest="output_file")
    parser.add_option("-o", "--stores_offset",
                      help="Psflash stores offset in the flash",
                      default=PSFLASH_DEFAULT_STORES_OFFSET, type="int",
                      dest="stores_offset")
    parser.add_option("-s", "--store_size",
                      help="Size of each store in bytes",
                      default=PSFLASH_DEFAULT_STORE_SIZE, type="int",
                      dest="store_size")
    parser.add_option("-k", "--crypto_key",
                      help="128 bit cryptographic key",
                      default=None, type="string", dest="crypto_key")
    (options, args) = parser.parse_args()

    try:
        options.input_files = options.input_files.split(",")
    except AttributeError as error:
        raise error("Input file has not been specified with -f <filename>")
        
    try:
        options.output_file
    except AttributeError as error:
        raise error("Output file has not been specified with -t <filename>")

    if options.crypto_key == None:
        options.crypto_key = PSFLASH_DEFAULT_CRYPTO_KEY
    else:
        options.crypto_key = int(options.crypto_key, 16)
        if options.crypto_key > ((1 << 128) - 1):
            raise ValueError("crypto key size must be 128 bits "
                             "(e.g. 17d3abc6ff0349421bc63380fac3b298)")
        options.crypto_key = dwords_to_bytes_be(
                                 [(options.crypto_key >> 96) & 0xffffffff,
                                  (options.crypto_key >> 64) & 0xffffffff,
                                  (options.crypto_key >> 32) & 0xffffffff,
                                  (options.crypto_key >> 0) & 0xffffffff])
    psfs = PsflashConverter(options.crypto_key,
                            options.stores_offset,
                            options.store_size)
    psfs.convert(options.input_files, options.output_file)
    
