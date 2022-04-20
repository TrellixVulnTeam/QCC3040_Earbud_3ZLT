############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import PureVirtualError
from .i_layout_info import XapDataInfo, Kalimba24DataInfo, Kalimba32DataInfo, \
ArmCortexMDataInfo, ChipmateDataInfo, X86DataInfo, RISCVDataInfo, XtensaDataInfo

class ICoreInfo (object):
    """\
    Core-centric meta-data interface.
    
    Represent hardware facts, such as register map, common to all Core
    instances of a specific type.
    """
    DATA_INFO_TYPE = None

    @property
    def io_map_info(self):
        """\
        io map interface (IIOMapInfo)
        """
        raise PureVirtualError()
        
    @property
    def layout_info(self):
        """
        Info about endianness, word width, etc
        """
        raise PureVirtualError

    @property
    def custom_digits_path(self):
        
        raise PureVirtualError

    @property
    def layout_info(self):
        try:
            self._layout_info
        except AttributeError:
            self._layout_info = self.DATA_INFO_TYPE()
        return self._layout_info

class XapCoreInfo(ICoreInfo):
    DATA_INFO_TYPE = XapDataInfo

class Kalimba24CoreInfo(ICoreInfo):
    DATA_INFO_TYPE = Kalimba24DataInfo

class Kalimba32CoreInfo(ICoreInfo):
    DATA_INFO_TYPE = Kalimba32DataInfo
        
class ArmCortexMCoreInfo(ICoreInfo):
    DATA_INFO_TYPE = ArmCortexMDataInfo
    
class X86CoreInfo(ICoreInfo):
    DATA_INFO_TYPE = X86DataInfo

class X86_64CoreInfo(ICoreInfo):
    DATA_INFO_TYPE = X86DataInfo

class ChipmateCoreInfo(ICoreInfo):
    DATA_INFO_TYPE = ChipmateDataInfo

class RISCVCoreInfo(ICoreInfo):
    DATA_INFO_TYPE = RISCVDataInfo

class XtensaCoreInfo(ICoreInfo):
    DATA_INFO_TYPE = XtensaDataInfo
