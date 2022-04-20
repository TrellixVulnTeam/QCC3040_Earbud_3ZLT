############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels import flatten_le

def memory_write_helper(memport, start_addr, byte_array):
    """
    Helper to support fiddling around with non-word-aligned writes on a word
    orientated memory port, either using the port's built-in sub-word access 
    support, or doing RMW.  Assumes the target memory port is little-endian.

    Relies on the following methods in "memport":
     - supports_subword_access
       Predicate indicating whether the hardware supports switching to a
       subword access mode
     - set_access_size
       Hardware control of the access size: this is passed log(access_size),
       i.e. 0 for byte access, 1 for halfword access, 2 for word access
     - write_out_simple
        Writes out the given bytes at the given address via a given number
        of writes.  _write_out_simple is responsible for any grouping of
        the bytes into larger units - e.g. if two bytes are supplied to be 
        written in one half-word-size access, they may need to be assembled
        into a single half-word value first.  The implementation can assume
        that the unit required is determined by the alignment of the given
        address
     - write_out_aligned_block
        Writes out the word-aligned bytes at the given address
     - read_word
        Returns the word at the given address (used to implement RMW when 
        subword access is not supported.  If subword access is always 
        supported this method need to be supplied.)
    """
    end_addr = start_addr+len(byte_array)
    write_start = 4*((start_addr+3)//4)
    write_end = 4*(end_addr//4)
    if write_start > start_addr or write_end < end_addr:
        # Write the unaligned leading and/or trailing sections, either using
        # hardware subword access support or by RMW.  In the former case we
        # strip off the corresponding entries in the byte_array, whereas in 
        # the latter case we add bytes from the partially-touched words; 
        # the result in both cases is a word-aligned byte array.
        if memport.supports_subword_access:
            # Track whether we've changed access size settings in the hardware,
            # so we can set them back if and only if necessary.
            access_size = 2
            # Write any leading/trailing subword bytes using subword accesses
            if write_start > write_end:
                # Corner case: the bytes to write don't touch any word-aligned address.
                # Must be limited to the two bytes in the middle of a word.
                assert len(byte_array) <= 2
                access_size_expnt = 0
                memport.set_access_size(access_size_expnt)
                access_size = access_size_expnt
                memport.write_out_simple(start_addr, byte_array, len(byte_array))
                byte_array = []

            else:
                # The byte array runs from, up to or across a word-aligned address
                if write_start > start_addr:
                    num_ldg_bytes = write_start - start_addr
                    # Do a single half-word access if possible or else byte accesses
                    access_size_expnt = 1 if num_ldg_bytes == 2 else 0
                    memport.set_access_size(access_size_expnt)
                    access_size = access_size_expnt
                    memport.write_out_simple(start_addr, byte_array[:num_ldg_bytes], 
                                            num_ldg_bytes // (1<<access_size_expnt))
                    byte_array = byte_array[num_ldg_bytes:]
                if write_end < end_addr:
                    num_trlg_bytes = end_addr - write_end
                    access_size_expnt = 1 if num_trlg_bytes == 2 else 0
                    if access_size_expnt != access_size:
                        memport.set_access_size(access_size_expnt)
                        access_size = access_size_expnt
                    memport.write_out_simple(write_end, byte_array[-num_trlg_bytes:],
                                            num_trlg_bytes // (1<<access_size_expnt))
                    byte_array = byte_array[:-num_trlg_bytes]
            if access_size != 2:
                memport.set_access_size(2)
        else:
            # RMW the leading/trailing word to simulate subword access
            if write_start < start_addr:
                # need to RMW the first word
                byte_array = flatten_le(memport.read_word(write_start), 
                        word_bits=8, num_words=4)[:start_addr-write_start] + byte_array
            if write_end > end_addr:
                # need to RMW the last word
                # We deliberately don't use += here to ensure that the original
                # "byte_array" object passed in isn't modified
                byte_array = byte_array + flatten_le(memport.read_word(write_end-4), 
                                word_bits=8, num_words=4)[4-(write_end-end_addr):]
    
    assert(len(byte_array)%4 == 0)

    if byte_array:
        # If there's anything left to write...
        memport.write_out_aligned_block(write_start, byte_array)

class TransportAccessError(RuntimeError):
    """
    Base class for transport-specific access problems
    """