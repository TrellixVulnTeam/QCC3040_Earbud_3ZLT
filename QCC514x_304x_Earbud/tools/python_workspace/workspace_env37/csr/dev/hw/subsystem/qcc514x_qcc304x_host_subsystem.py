############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .host_subsystem import *
from csr.wheels import PureVirtualError, NameSpace
from csr.dev.hw.address_space import AddressMap


class QCC514X_QCC304XHostSubsystem(HostSubsystem):
    
    @property
    def blocks_present(self):
        return {"HOST_SYS" : 
                {"block_id" : 0xd,
                 "common_start" : 0,
                 "ss_start" : 0x22,
                 "ss_end" : 0x22},
                "UART" :
                  {"block_id" : 0,
                   "common_start" : 0,
                   "ss_start" : 0xc0,
                   "ss_end" : 0xd1},
                "USB2" : 
                 {"block_id" : 1,
                 "common_start" : 0,
                  "ss_start" : 0x80,
                  "ss_end" : 0x100},
                "SDIO" :
                #I'm not too sure how the SDIO registers work - make them all
                #common for now
                  {"block_id" : 2,
                   "common_start" : 0,
                   "ss_start" : 0x100,
                   "ss_end" : 0x100},
                "BITSERIAL0" :
                  {"block_id" : 3,
                   "common_start" : 0,
                   "ss_start" : 0x27,
                   "ss_end" : 0x27},
                "BITSERIAL1" :
                  {"block_id" : 4,
                   "common_start" : 0,
                   "ss_start" : 0x27,
                   "ss_end" : 0x27},
                }
                

class QCC514X_QCC304XHostSubsystemView(object):

    def __init__(self, ssid, host_ss):
        
        self._ssid = ssid
        self._host_ss = host_ss

    @property
    def usb2(self):
        return self._host_ss.get_subsystem_view("USB2",self._ssid)
            
        
    @property
    def uart(self):
        return self._host_ss.get_subsystem_view("UART",self._ssid)
    
        
    @property
    def sdio(self):
        return self._host_ss.get_subsystem_view("SDIO",self._ssid)
 
    @property
    def host_sys(self):
        return self._host_ss.get_subsystem_view("HOST_SYS", self._ssid)
    
    @property
    def bitserial0(self):
        return self._host_ss.get_subsystem_view("BITSERIAL0", self._ssid)
    
    @property
    def bitserial1(self):
        return self._host_ss.get_subsystem_view("BITSERIAL1", self._ssid)