from __future__ import division

import argparse
import array
import binascii
import codecs
import logging
import math
import os
import re
import struct
import sys

from collections import namedtuple

logging.getLogger().setLevel(logging.INFO)

def parse_args(args):
    """
    Parse the args 
    """
    parser = argparse.ArgumentParser(description='Assemble file system parts into a single image')
    parser.add_argument('-o', '--output_file',
                        help='Path to the output file')
    parser.add_argument('-x', '--xuv_file',
                        nargs='?',
                        const=True,
                        required=False,
                        default=False,
                        help='Output file as xuv (default')
    parser.add_argument('-b', '--bin_file',
                        nargs='?',
                        const=True,
                        required=False,
                        default=False,
                        help='Output file as binary')
    parser.add_argument('fsparts', nargs='*', help='Sequence of file system parts in the order to be assembled')
    parser.add_argument('-s', '--partition_size',
                        type=int,
                        required=True,
                        help='Size of va_fs partition (bytes)')
    return parser.parse_args(args)

def create_bin_file(file_name, vafs_buffer):
    # Write file system buffer as bin file
    file_name += '.bin'
    print("Creating %s" % file_name)
    with open(file_name, 'wb') as fh:
        fh.write(vafs_buffer)

def create_xuv_file(file_name, vafs_buffer=bytes()):
    # Convert file system buffer to xuv format and write as xuv file
    file_name += '.xuv'
    print("Creating %s" % file_name)
    xuv_buffer=''
    b_vafs_buffer = bytearray(vafs_buffer)
    for i in range(0,len(b_vafs_buffer), 2):
        high_byte = b_vafs_buffer[i+1]
        low_byte = b_vafs_buffer[i]
        xuv_buffer += '@%06x    %02x%02x\n' % (i//2, high_byte, low_byte)
    with open(file_name, 'w') as f:
        f.write(xuv_buffer)

def run_script(arglist):
    pargs = parse_args(arglist)

    if len(pargs.fsparts) > (FileSystem.MAX_FILE_ENTRIES):
        sys.exit('ERROR: Number of files exceeds max FAT entries {}'.format(FileSystem.MAX_FILE_ENTRIES))
    if(pargs.xuv_file == False and pargs.bin_file == False):
        pargs.xuv_file = True

    # Build a list of files with their attributes
    file_entries = []
    for fpath in pargs.fsparts:
        fn = os.path.basename(fpath)
        if len(fn) > FileSystem.MAX_FILE_NAME_LENGTH:
            sys.exit('ERROR: Filename exceeds max length {}:{}').format(FileSystem.MAX_FILE_NAME_LENGTH, fn)

        if not os.path.exists(fpath):
            sys.exit('ERROR: File does not exist: {}'.format(fpath))

        fsize = os.path.getsize(fpath)

        with open(fpath, 'rb') as fh:
            fbuffer = fh.read()
        entry = FileSystem.FILE_ATTRIBS(filename=fn, length=fsize, buffer=fbuffer)
        file_entries.append(entry)

    # Create an empty file system
    fs = FileSystem()

    # Construct the primary and backup FATS 
    fs_buff = fs.build_fats(file_entries)

    # Pad files to sector boundaries and append to file system buffer
    fs_buff += fs.add_files(file_entries)

    # Pad any remaining space in file system
    ptn_padding = b'\xff' * (pargs.partition_size - len(fs_buff))
    fs_buff += ptn_padding

    if pargs.xuv_file:
        create_xuv_file(pargs.output_file, fs_buff)

    if pargs.bin_file:
        create_bin_file(pargs.output_file, fs_buff)

    return pargs.output_file

class FileSystem(object):
  SECTOR_LEN = 4096
  FAT_ENTRY_SIZE = 36
  MAX_FAT_ENTRIES = SECTOR_LEN // FAT_ENTRY_SIZE
  MAX_FILE_ENTRIES = MAX_FAT_ENTRIES - 1 # First entry used by FAT index
  MAX_FILE_NAME_LENGTH = 0x10 - 1 # leave space for \0 terminator
  FAT_ENTRY_FMT = struct.Struct('<16s L H H H H H H H H')
  FILE_ATTRIBS = namedtuple('FileAttribs', [
        'filename',
        'length',
        'buffer'
    ])
  FAT_ENTRY = namedtuple('FatEntry', [
        'filename',
        'length',
        'sequence_count',
        'access_bitmap',
        'extent01_start',
        'extent01_length',
        'extent02_start',
        'extent02_length',
        'extent03_start',
        'extent03_length'
    ])

  def __init__(self):
    self.entries = []

  def read(self, buffer, sector):
    """
    Read a file system from a buffer.
    Extract and write out the individual files listed in the FAT.
    """
    self.buffer = buffer
    self.sector = sector
    # Iterate reading entries until end of file or entry has filename '' and length 0
    entryIdx = 0
    entry_size = self.FAT_ENTRY_FMT.size
    while True:
      entry_offset = (self.sector * self.SECTOR_LEN) + (entryIdx * entry_size) 
      entry = self.FAT_ENTRY._make(self.FAT_ENTRY_FMT.unpack(self.buffer[entry_offset: entry_offset + entry_size]))
      
      if entry.filename == b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff':
        # Zero length entry is end of FAT entries
        break

      fname = entry.filename.decode("utf-8").rstrip("\0") 
      logging.debug('T FN: {}'.format(fname))
      logging.debug('T ENTRY: {}'.format(entry))

      self.entries.append(entry)
      entryIdx += 1

  def add_files(self, file_attrib_list):
    """
    Iterate over the files.
    Calculate the number of sectors occupied by a file 
    Pad with erased bytes to the next sector boundary.
    Append the resulting sector-aligned buffer to the output buffer
    """

    file_buff = b''
    for attribs in file_attrib_list:
      sectors_spanned =  int(math.ceil( len(attribs.buffer) / self.SECTOR_LEN ))
      bytes_spanned = self.SECTOR_LEN * sectors_spanned
      file_buff += attribs.buffer
      sector_padding = b'\xff' * (bytes_spanned - len(attribs.buffer))
      logging.debug('\n================================================')
      logging.debug('File: {}'.format(attribs.filename))
      logging.debug('Length: {}'.format(attribs.length))
      logging.debug('SectorsSpanned: {}'.format(sectors_spanned))
      logging.debug('BytesSpanned: {}'.format(bytes_spanned))
      logging.debug('Padlen: {}'.format(len(sector_padding)))
      file_buff += sector_padding
    return file_buff

  def build_fats(self, file_attrib_list):
    """
    Build both primary and backup FATs
    Pad the FAT to the end of the sector
    Append to the output buffer and return
    """
    fats_buf = self.build_fat_sector(file_attrib_list, 0)
    fats_buf += self.build_fat_sector(file_attrib_list, 1)
    return fats_buf


  def build_fat_sector(self, file_attrib_list, sector_number):
    """ 
    Build a FAT Sector 
    Create the initial FAT index entry in the output buffer
    Iterate over the files adding a FAT entry for each one to the output buffer
    """
    fat_buff = b''

    # FAT index entry
    fat_index = self.FAT_ENTRY(filename="~QTIL-RAFS-V001".encode('utf-8'),
                          length=self.SECTOR_LEN,
                          sequence_count=0,
                          access_bitmap=0,
                          extent01_start=sector_number,
                          extent01_length=1,
                          extent02_start=0xFFFF,
                          extent02_length=0xFFFF,
                          extent03_start=0xFFFF,
                          extent03_length=0xFFFF,
                          )

    fat_buff += self.FAT_ENTRY_FMT.pack(*fat_index)
    
    # FAT file entries
    for idx, attribs in enumerate(file_attrib_list):
      file_entry = self.FAT_ENTRY(filename=attribs.filename.encode('utf-8'), 
                            length=attribs.length,
                            sequence_count=idx+1,
                            access_bitmap=0xFFFF,
                            extent01_start=idx+2,
                            extent01_length=int(math.ceil(attribs.length/self.SECTOR_LEN)),
                            extent02_start=0xFFFF,
                            extent02_length=0xFFFF,
                            extent03_start=0xFFFF,
                            extent03_length=0xFFFF,
                            )
      logging.debug('FILEENTRY: {}'.format(file_entry))
      fat_buff += self.FAT_ENTRY_FMT.pack(*file_entry)

    # Pad any remaining space in the sector with erased bytes (0xFF)
    sector_padding = b'\xff' * (self.SECTOR_LEN - len(fat_buff))
    fat_buff += sector_padding

    return fat_buff

  def __str__(self):
    """Display attribs"""
    val = '\n==== FAT ==== SEC: {}\n'.format(self.sector)
    val += '# Entries         : {}\n'.format(len(self.entries))
    for entry in self.entries:
      val += '{}\n'.format(entry)
    val += 'sector            : {}\n'.format(self.sector)
    val += '  ------------------------------------------------------------------------------------\n'
    return val

  def get_files(self):
    """
    Extract the file names and buffers from the FAT and file system
    """
    file_info = []
    for entry in  self.entries[1:]:
      #decode the file name byte sequence and strip any trailing nulls
      fname = entry.filename.decode("utf-8").rstrip("\0") 
      logging.debug('       FN: {}'.format(fname))
      data01_start = entry.extent01_start * self.SECTOR_LEN
      data01_end =  data01_start + entry.length
      data = self.buffer[data01_start: data01_end]
      file_info.append((fname, data))
      return file_info

