############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.bitsandbobs import PureVirtualError
import os

class IFirmwareInfo (object):
    """\
    Firmware Source-level Debugging Information Interface.
    
    Logical interface to firmware debugging data independent of source
    representation (e.g. ELF, DWARF, legacy).

    Known users:
    
    - StandaloneFirmwareEnvironment
    """

    @property
    def globalvars(self):
        """\
        All global vars (IVariableInfos) indexed by globally unique source
        symbol.
        """
        raise PureVirtualError(self)
    
    @property
    def cus(self):
        """\
        All compilation units (ICompilationUnitInfos) indexed by source
        file basename.
        
        Note: If we encounter non-unique basenames might have to provide an
        alternative index using full path - for use in those special cases.
        (curator fw has none at time of writing)
        """
        raise PureVirtualError(self)

    @property
    def elf(self):
        """\
        Direct Access to ELF object.        
        
        This is only exposed to support features, such as hydra debug log, that
        need direct access to non-standard meta-data, such as the debug log
        strings, held in ELF file.
        
        (Consider abstracting the detail behind this interface)
        """
        raise PureVirtualError(self) 

    @property
    def host_interface_info(self):
        """\
        Interface to host interface meta data.
        """
        raise PureVirtualError(self) 
    
        try:
            self._host_interface_info
        except AttributeError:
            hip_dir = os.path.join(self._root_dir, 'common', 'interface')
            self._host_interface_info = HydraHostInterfaceInfo(hip_dir)
            
        return self._host_interface_info

class ICompilationUnitInfo (object):
    """\
    Firmware Compilation Unit (CU) Metadata Interface
    """    
    @property
    def localvars(self):
        """\
        All local variables (IVariableInfos) defined in this CU indexed by
        locally unique source symbol.
        """
        raise PureVirtualError(self)

    @property
    def localfuncs(self):
        """\
        All local functions defined in this CU indexed by
        locally unique source symbol.
        """
        raise PureVirtualError(self)

class IVariableInfo (object):
    """\
    Firmware Variable (extern or static) MetaData Interface
    """

    @property
    def symbol(self):
        """\
        Source symbol (str)
        """
        raise PureVirtualError(self)

    @property
    def datatype(self):
        """\
        Type information (ITypeInfo)
        """
        raise PureVirtualError(self)

    @property
    def is_external(self):
        """\
        Is this variable public/private to the defining CU?
        """
        raise PureVirtualError(self)

    @property
    def start_address(self):
        """\
        Start address in data-space (inclusive).
        """
        raise PureVirtualError(self)

    @property
    def stop_address(self):
        """\
        Stop address in data-space (exclusive).
        """
        raise PureVirtualError(self)
        
    @property
    def size(self):
        """\
        Number of data-space words occupied.
        """
        raise PureVirtualError(self)
    
    @property
    def linked_list(self):
        """
        In general a variable is not a linked list
        """
        return None


class IDwarfVariableInfo(IVariableInfo):
    """
    Extension to IVariableInfo Interface for DWARF variable-like entities, i.e. 
    variables and struct members
    """
    @property
    def struct(self):
        """
        Structure dictionary access
        """
        raise PureVirtualError
    
    @property
    def type_tag(self):
        """
        Return the DWARF tag of the variable's resolved type to enable the
        correct IVariable subclass to be instantiated for the variable
        """
        raise PureVirtualError

    @property
    def parent_info(self):
        """
        Every DWARF variable has an ELF symbol of which it is a member, which 
        may be itself
        """
        raise PureVirtualError


class ITypeInfo (object):
    """\
    Firmware Type Interface (Abstract Interface)
    """
    
    @property
    def symbol(self):
        """
        Source code symbol (or None if anonymous)
        """
        raise PureVirtualError(self)

    @property
    def size(self):
        """\
        Number of data-space words occupied.
        """
        raise PureVirtualError(self)
        

class ITypeDefInfo (ITypeInfo):
    """\
    Firmware TypeDef Interface
    """
    @property
    def element_type(self):
        """\
        The underlying/aliased type (ITypeInfo subclass).
        """
        raise PureVirtualError(self)


class IEnumeratorInfo (object):
    """\
    Firmware Enumerator Interface
    """

    @property
    def symbol(self):
        raise PureVirtualError(self)

    @property
    def value(self):
        raise PureVirtualError(self)


class IEnumerationInfo (ITypeInfo):
    """\
    Firmware Enumeration Type Interface
    """

    @property
    def enumerators(self):
        """\
        Set of IEnumeratorInfos.
        """
        raise PureVirtualError(self)


class IArrayTypeInfo (ITypeInfo):
    """\
    Firmware Array Type Interface
    """

    @property
    def element_type(self):
        """\
        The type (ITypeInfo subclass) of the elements.
        """
        raise PureVirtualError(self)

    @property
    def dim(self):
        """\
        Dimmension (1D)
        """
        raise PureVirtualError(self)
