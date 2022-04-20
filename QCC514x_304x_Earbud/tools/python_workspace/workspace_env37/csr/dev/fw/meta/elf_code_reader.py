############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.bitsandbobs import display_hex, NameSpace
from array import array
from csr.dev.hw.core.meta.i_layout_info import ILayoutInfo
import sys
from csr.dwarf.read_elf import Elf_Reader

if sys.version_info > (3,):
    # Python 3
    int_type = int
    from functools import cmp_to_key
else:
    # Python 2
    int_type = (int, long)

class NotInLoadableElf(IndexError):
    """
    An address has been specified which isn't in the loadable ELF (e.g. it's
    a non-constant variable)
    
    This is an IndexError because in effect we end up reading an invalid index
    in the memory "array".
    """
    
class ElfCodeReader(object):
    """
    Simple wrapper for obtaining loadable code sections from an ELF file
    """
    
    class ElfLoadableSection(object):
        """
        Plain structure representing a single loadable section from an ELF file
        The addresses supplied should be the LMA and VMA respectively of the 
        first instruction in the section
        """

        class SHF(object):
            WRITE = 1
            ALLOC = 2
            EXECINSTR = 4
            # There are more, but we probably won't ever care about them

        def __init__(self, name, data, paddr, vaddr, flags, layout_info):
            
            self.paddr = paddr
            self.vaddr = vaddr
            self.name = name
            self.flags = flags
            self._octets_per_addr_unit = layout_info.addr_unit_bits // 8
            # Turn octets into the native addressable unit if that's needed
            if self._octets_per_addr_unit == 2:
                self.data = array("H",data)
                # The array class use the system's byte ordering, so need to
                # byte-swap if the target's ordering is different.
                if ((sys.byteorder == "little")
                     != (layout_info.endianness == ILayoutInfo.LITTLE_ENDIAN)):
                    self.data.byteswap()
            else:
                self.data = array("B", data)
                
        def __eq__(self, other):
            
            # The objects are equal if their attributes are.  We get away with
            # this because there are no attributes except for simple objects 
            return self.__dict__ == other.__dict__ 
                
        @property
        def is_instructions(self):
            return self.flags & self.SHF.EXECINSTR != 0
    
        @property
        def byte_size(self):
            return len(self.data)*self._octets_per_addr_unit
    
        def __repr__(self):
            return ("ElfLoadableSection(name='%s' vaddr=0x%x paddr=0x%x "
                    "size=%d)" % (self.name, self.vaddr, self.paddr, 
                                          self.byte_size))
            
        def contains_addr(self, addr, use_paddr=False):
            base_addr = self.paddr if use_paddr else self.vaddr
            return addr >= base_addr and addr < base_addr + len(self.data)
    
    def __init__(self, elf_filename_or_reader, layout_info):
        """
        Cache the ELF code data
        """
        if isinstance(elf_filename_or_reader, str):
            # The toolchain doesn't matter when we're just going to use 
            # Elf_Reader to read the executable sections.
            elf_reader = Elf_Reader(elf_filename_or_reader, None)
        else:
            elf_reader = elf_filename_or_reader
        
        self._layout_info = layout_info
        # Do this to emulate a core object
        self.info = NameSpace()
        self.info.layout_info = layout_info
        raw_elf_sections = elf_reader.get_program_sections()
        self._elf_sections = [self.ElfLoadableSection(name, data, 
                                                      lma & ~0x80000000, 
                                                      vma & ~0x80000000,
                                                      flags, 
                                                      self._layout_info) for
                              name, data, lma, vma, flags in raw_elf_sections]
        # Sort the sections by increasing paddr
        def cmp(x, y):
            return (x > y) - (x < y)
        if sys.version_info >= (3,):
            self._elf_sections.sort(key=cmp_to_key(lambda x,y : cmp(x.paddr,y.paddr)))
        else:
            self._elf_sections.sort(cmp=lambda x,y : cmp(x.paddr,y.paddr))

    @property
    def sections(self):
        """
        Generator returning a sequence of ElfLoadableSections from the program
        sections dictionary read from the ELF file
        """
        return (sec for sec in self._elf_sections)

    class ElfSpace(object):
        
        def __init__(self, elf_code_reader, use_paddr=False):
            self._reader = elf_code_reader
            self._use_paddr = use_paddr

        @display_hex
        def __getitem__(self, addr_or_slice):
            
            if isinstance(addr_or_slice, int_type):
                for section in self.sections:
                    secaddr = section.paddr if self._use_paddr else section.vaddr
                    addr = addr_or_slice - secaddr 
                    if 0 <= addr and addr < len(section.data):
                        return section.data[addr]
                raise NotInLoadableElf("No loadable ELF data at 0x%x" % addr_or_slice)
            else:
                if addr_or_slice.step is not None:
                    raise NotInLoadableElf("Can't use strided access to loadable ELF")
                for section in self.sections:
                    secaddr = section.paddr if self._use_paddr else section.vaddr
                    addr = (addr_or_slice.start - secaddr if addr_or_slice.start is not None else 0)
                    addr_stop = (addr_or_slice.stop -secaddr if addr_or_slice.stop is not None 
                                 else len(section.data))
                    if 0 <= addr and addr < len(section.data):
                        if 0 <= addr_stop and addr_stop <= len(section.data):
                            return section.data[addr:addr_stop].tolist()
                        else:
                            raise NotInLoadableElf("Invalid range for loadable ELF data [0x%x,0x%x)" % 
                                                   (addr+secaddr,addr_stop+secaddr))

                raise NotInLoadableElf("No loadable ELF data at 0x%x" % 
                                                            addr_or_slice.start)
                
        def __contains__(self, addr):
            """
            Is the given address in the image?
            """
            return any(section.contains_addr(addr, self._use_paddr) for section in self.sections)
            
        @property
        def sections(self):
            return self._reader.sections
                
    @property
    def data_space(self):
        return self.ElfSpace(self, use_paddr=False)

    # Quack like a core (info.layout_info is set up in the constructor)
    data = dm = data_space
    
    @property
    def program_space(self):
        return self.ElfSpace(self, use_paddr=True)

    def elf_var(self, var):
        """
        Construct a _Variable object which is a version of the supplied _Variable
        only with the ELF file's loadable sections being the "data space".  This
        enables a check that the specified ELF file matches the running firmware.
        
        Note: if a variable is supplied which is not loaded from the ELF then
        this function will succeed in creating the variable, but when an attempt
        is made to evaluate it a "NotInLoadableElf" will be raised. 
        """
        from csr.dev.env.env_helpers import _Variable
        
        info = var._info
        return _Variable.factory(info, self.data_space)
        
