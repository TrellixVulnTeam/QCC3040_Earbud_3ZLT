############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################



class ChipVersion (object):
    """\
    CSR Chip Version (Immutable)
    
    Decodes CSR chip version fields.
    """
    def __init__(self, raw_version):
        """\
        Construct ChipVersion from raw version word (as read from hardware).
        """
        self._raw = raw_version
    
    def __str__(self):
        # Potential extension:: Consider import/define sub-field meta data & reuse "bitz"
        # formatting?
        return "0x%04x (Major: 0x%02x, Minor: 0x%x, Variant: 0x%x)" % \
            (self.raw, self.major, self.minor, self.variant) 
    
    @property
    def raw(self):
        return self._raw
    
    @property
    def major(self):
        # 8 Bits 0-7
        return self._raw & 0xff
    
    @property
    def minor(self):
        # 4 Bits 12-15
        return (self._raw >> 12) & 0xf

    @property
    def variant(self):
        # 4 Bits 8-11
        return (self._raw >> 8) & 0xf
    
    @property
    def is_fpga(self):
        # variant 0xf reserved from fpga
        return self.variant == 0xf

    def __eq__(self, other):
        return self.raw == other.raw

    """Common Variant Number Assignments"""
    FPGA = 0xf
    FPGA_PARTIAL = 0xe
        
class JTAGVersion:
    """
    Version information from the SWD TARGETID register (if available) 
    """
    
    QUALCOMM_JEDEC_ID = 0x70
    
    def __init__(self, designer, partno, revision, instance=0):
        if designer != self.QUALCOMM_JEDEC_ID:
           raise ValueError("Unknown chip designer 0x{:x}".format(designer))
        self._partno = partno
        self._revision = revision
        self._instance = instance

    @property
    def partno(self):
        return self._partno

    @property
    def revision(self):
        return self._revision
        