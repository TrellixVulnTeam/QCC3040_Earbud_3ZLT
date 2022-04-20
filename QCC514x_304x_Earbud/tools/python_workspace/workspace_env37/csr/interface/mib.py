############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
'''
CSR MIB model & io.
'''

from csr.wheels import PureVirtualError, TypeCheck
from csr.wheels.int import U8Writer, LEU16Writer

from .vldata import VLIntWriter, VLStringWriter

# ----------------------------------------------------------------------------
# Model
# ----------------------------------------------------------------------------

class MIBType:
    '''
    MIB Type

    \Potential extension: parse metadata from xml.
    \Potential extension: Use to validate MIB values.
    '''                 
    def __init__(self, psid, name):
        self._id = psid
        self._name = name

    def __str__(self):
        return "%s (%d)" % (self.name, self.psid)
            
    @property
    def psid(self):
        return self._id
    
    @property
    def name(self):
        return self._name
    

class MIB:
    '''
    MIB Value (abstract base)
    '''
    def __init__(self, type):
        self._type = type

    @property
    def type(self):
        return self._type
        
    def accept(self, visitor):
        '''
        Accept MIB.Visitor (pure virtual).
        
        Subclasses must override to call respective visitor interface.
        '''
        raise PureVirtualError()

    class Visitor:
        '''
        MIB visitor interface (pure virtual).
        Pattern: http://en.wikipedia.org/wiki/Visitor_pattern
        '''
        def visit_int_mib(self, int_mib):
            raise PureVirtualError()
    
        def visit_string_mib(self, string_mib):
            raise PureVirtualError()


class IntMIB(MIB):
    '''Integer MIB Value'''
    
    def __init__(self, type, i):
        '''Construct Integer MIB Value.'''
        MIB.__init__(self, type)
        self._i = i

    def __str__(self):
        return "%s, data=0x%x" % (self.type, self.value)
    
    @property
    def value(self):
        return self._i
    
    def accept(self, visitor):
        visitor.visit_int_mib(self)


class StringMIB(MIB):
    '''String MIB Value'''
    
    def __init__(self, type, string):
        '''Construct String MIB Value.'''
        MIB.__init__(self, type)
        self._string = string

    def __str__(self):        
        return "%s, data=%s" % (self.type, str(self._string))
    
    @property
    def value(self):
        return self._string
    
    def accept(self, visitor):
        visitor.visit_string_mib(self)

# ----------------------------------------------------------------------------
# IO
# ----------------------------------------------------------------------------


class XmlMibTypeReader: pass

class XmlMibTypeWriter: pass

class TextMibWriter(MIB.Visitor):
    """
    Writes MIB values in hydra text format.
    
    Ref:-
    - CS-213274-SP
    
    Example output:-
        unifiCoexPTACDLConfig=[00 11 22 33 44 55]
        dot11EDCATableTXOPLimit.0=0xF694
    """
    def __init__(self, ostream):        
        self._ostream = ostream
    
    def write(self, *mibs):
        '''
        Write one or more MIBs in text format.
        '''
        for mib in mibs:
            mib.accept(self)
    
    def visit_int_mib(self, int_mib):
        """        
        Write integer MIB value as "<mib_name>=0x<hex_value>\n"
        """    
        text = "%s=0x%x\n" % (int_mib.type.name, int_mib.value)
        self._ostream.write(text)

    def visit_string_mib(self, string_mib):
        """
        Write octet string MIB value as "<mib_name>=[aa bb cc dd 00 11 22 33]\n"
        """
        val_text = ""
        for octet in string_mib.value:
            val_text += '%02x ' % octet
        val_text = val_text.rstrip()
        text = "%s=[%s]\n" % (string_mib.type.name, val_text)
        self._ostream.write(text)
        

class BinaryMibWriter(object):
    """
    Writes MIB values in binary format.
    
    Params:-
    -- octet_alignment - used to force octet padding at end - e.g. for hydra.
    
    Format:-
    
    typedef struct
    {
        uint16     psid;
        uint16     length;  # off vldata_array (excl. any pad byte)
        MIB_VLData vldata_array[ANY_SIZE];
        (uint8      pad_if_needed;)
    } MIB_KEYVAL;
    
    N.B. Integers are streamed LittleEndian unless documented otherwise 
    """
    def __init__(self, ostream, octet_alignment = 1):
        
        self._ostream = ostream
        self._alignment = octet_alignment
    
    def write(self, *mibs):
        '''
        Write one or more MIBs in binary format.
        '''
        for mib in mibs:
            self._write_one(mib)
                
    def _write_one(self, mib):
        '''
        Write MIB in binary format.
        '''        
        from io import BytesIO
        
        TypeCheck(mib, MIB)
        
        # Shorthands
        ostream = self._ostream
        alignment = self._alignment
        
        # Format the MIB's data into a tmp vldata buffer so we can measure 
        # its length and work out if a pad byte is needed.
        #
        # The VLData formatting is delegated to a DataWriter helper
        #
        tmp_stream = BytesIO()
        self.DataWriter(tmp_stream).write_vldata(mib)
        vldata = tmp_stream.getvalue()
        tmp_stream.close()
        
        # Is padding needed?
        #
        vldata_len = len(vldata)
        pad_octets = (alignment - ((vldata_len % alignment))) % alignment
        assert((vldata_len + pad_octets) % alignment == 0)
        
        # Write the MIB header & VLData
        #
        LEU16Writer(ostream).write(mib.type.psid, vldata_len)
        ostream.write(vldata)
            
        # Append pad octets as necessary
        #
        while pad_octets:
            U8Writer(ostream).write(0x00)
            pad_octets -= 1
                 
        return ostream
        

    class DataWriter(MIB.Visitor):
        '''
        Writes MIB's data in VLData format.
        
        Uses VLString or VLInt format according to MIB sub-type.     
        ''' 
        def __init__(self, ostream):            
            self._ostream = ostream
        
        def write_vldata(self, mib):
            mib.accept(self) 
        
        def visit_int_mib(self, int_mib):
            VLIntWriter(self._ostream).write(int_mib.value)
    
        def visit_string_mib(self, string_mib):
            VLStringWriter(self._ostream).write(string_mib.value)
