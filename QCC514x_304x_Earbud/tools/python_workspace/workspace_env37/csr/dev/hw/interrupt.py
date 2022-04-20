############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import sys
from csr.wheels import gstrm
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor

class Interrupt(object):
    '''
    Class to decode the hardware state of the interrupt controller.
    '''
    def __init__(self, core):
        self._core = core
           
    def state(self, report=False):          
        interrupt_regs = ["INT_GBL_ENABLE",
               "INT_UNBLOCK",
               "INT_ADDR",
               "INT_CLK_SWITCH_EN",
               "INT_CLOCK_DIVIDE_RATE",
               "INT_ACK",
               "INT_SW0_EVENT",
               "INT_SW1_EVENT",
               "INT_SELECT",
               "INT_PRIORITY",
               "INT_LOAD_INFO",
               "INT_SAVE_INFO",
               "INT_SOURCE",
               "INT_STATUS",
               "INT_SOURCES_EN"]
        

        output = self._core._print_list_regs("Interrupt controller registers", 
                          [(a,a,"x") for a in interrupt_regs], True)

        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)
            
    @property
    def sources(self):
        return ["INT_SOURCE_TIMER1",
                "INT_SOURCE_TIMER2",
                "INT_SOURCE_SW_ERROR",
                "INT_SOURCE_SW0",
                "INT_SOURCE_SW1",
                "INT_SOURCE_TBUS_INT_ADPTR_EVENT",
                "INT_SOURCE_TBUS_MSG_ADPTR_EVENT",
                "INT_SOURCE_OUTBOUND_ACCESS_ERROR_EVENT",
                "INT_SOURCE_TIME_UPDATE_EVENT",
                "INT_SOURCE_NFC_EVENT",
                "INT_SOURCE_USB_EVENT",
                "INT_SOURCE_VML_EVENT",
                "INT_SOURCE_DMAC_QUEUE0_EVENT",
                "INT_SOURCE_DMAC_QUEUE1_EVENT",
                "INT_SOURCE_DMAC_QUEUE2_EVENT",
                "INT_SOURCE_SQIF_ARBITER_EVENT",
                "INT_SOURCE_SQIF_ARBITER1_EVENT",
                "INT_SOURCE_AUX_DATA_CONV_EVENT",
                "INT_SOURCE_CPU1_ACCESS_FAULT_EVENT",
                "INT_SOURCE_SDIO_HOST_INTERRUPT_EVENT",
                "INT_SOURCE_SDIO_HOST_WAKEUP_EVENT",
                "INT_SOURCE_INTERPROC_EVENT_1",
                "INT_SOURCE_INTERPROC_EVENT_2",
                "INT_SOURCE_PIO_INT_EVENT_1",
                "INT_SOURCE_PIO_INT_EVENT_2",
                "INT_SOURCE_PIO_TIMER_EVENT_3",
                "INT_SOURCE_CPU1_EXCEPTION"]