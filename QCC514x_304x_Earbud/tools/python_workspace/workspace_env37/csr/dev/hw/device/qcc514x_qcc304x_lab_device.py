############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides class QCC514X_QCC304XLabDevice.
"""
# pylint: disable=invalid-name
import textwrap
from csr.wheels.global_streams import iprint
from csr.dev.hw.address_space import AddressSpace
from csr.dev.framework.connection.trb import MRTSTrbTransConnection
from .qcc514x_qcc304x_device import QCC514X_QCC304XDevice
from .mixins.resettable_device import BootTimeoutResettableDevice

class QCC514X_QCC304XLabDevice(QCC514X_QCC304XDevice, BootTimeoutResettableDevice):
    """\
    QCC514X_QCC304X lab board (Base)
    """
    # BaseDevice compliance
    @property
    def name(self):
        return 'qcc514x_qcc304x-lab'

    @property
    def chips(self):

        return (self.components.chip,)

    @property
    def chip(self):
        '''
        A qcc512x_qcc302x lab board has only one chip
        '''
        return self.components.chip

    @property
    def lpc_sockets(self):

        return None

    # Extensions

    @property
    def trb(self):
        '''
        The chip's Tbridge adapter connects directly to the physical Tbridge
        '''
        return self.components.chip.trb_in


    def reset_dut_old(self, override=None):
        '''
        Reset the chip by one of a variety of methods. Does not reset the
        device model, so that may get out of sync.
        For instance this can change which version of firmware is running
        on the Curator. The case is question is the chip has been restarted
        in ROM, the reset can cause it to restart from SLT with a different
        SLT. This prevent CuCMD's from working.

        There are a variety of ways to reset the device, and which one is
        best depends on your circumstances. By default reset_dut() and its
        minions try to guess which is best for you. If the guess is wrong,
        'override' might let you override it. It's implemented across
        multiple layers of Pylib, so the possible values aren't listed in
        one place.
        '''
        curss = self.chip.curator_subsystem
        if self.trb_raw is not None:
            # When the transaction is remote, connected via XMLRPC,
            # the MRTS TRB driver is used and is incompatible with trb_raw.
            if not self.transport.is_remote and not isinstance(
                    self.transport, MRTSTrbTransConnection):
                # Try magic transport-dependent methods. If the transport
                # doesn't have any, go for the slightly flakier
                # posted-write-to-DBG_RESET method.
                if (self.transport.reset_device(curss, override=override) is
                        NotImplemented):
                    # The chip resets before it can send the response back
                    # so we have to avoid telling the TRB driver to get a
                    # response.
                    DBG_RESET = curss.core.field_refs["DBG_RESET"].start_addr
                    DBG_RESET_BYTES = 2*DBG_RESET
                    self.trb_raw.debug_write(curss.id, DBG_RESET_BYTES, 2, 1,
                                             write_only=True)
            # reset the transport
            self.transport.reset()
        else:
            # We've got the same problem as above with the chip resetting
            # before it can send the response back, but the best we can
            # do here is swallow the WriteFailure exception.
            try:
                curss.core.fields["DBG_RESET"] = 1
            except AddressSpace.WriteFailure:
                pass

    def grab_hci_uart(self, txd_pio=20, rxd_pio=21, rts_pio=22, cts_pio=23):
        '''
        Grab the UART, until FW does it properly.
        Potential extension:: Supports both murphy and other transports: Does it?

        Mux TXD, RXD, CTS, RTS signals of the BT_SYS HCI UART
        out to any PIOs on the chip.
        Default value are for HCI UART on the WBLAM134 Lab/DV board
        being sent to the communicator board.
        '''
        bt = self.chip.bt_subsystem.core
        bt_regs = bt.regs

        iprint("UART Pin assignment: TXD({}) RXD({}) RTS({}) CTS({})".format(
            txd_pio, rxd_pio, rts_pio, cts_pio))

        # find out what register to write for TXD
        txd_reg_num = (txd_pio//4) * 4
        txd_reg_name = "PIO_SELECT_{}_TO_{}".format(txd_reg_num, txd_reg_num+3)
        txd_reg_subfield = "PIO_SELECT_{}".format(txd_pio)

        # find out what register to write for RXD
        rts_reg_num = (rts_pio//4) * 4
        rts_reg_name = "PIO_SELECT_{}_TO_{}".format(rts_reg_num, rts_reg_num+3)
        rts_reg_subfield = "PIO_SELECT_{}".format(rts_pio)

        # set TXD PIO
        iprint("Setting TXD PIO: {}.{} = {}".format(
            txd_reg_name, txd_reg_subfield, bt.iodefs.PIO_SELECT_HCI_UART_TX))
        setattr(getattr(bt_regs, txd_reg_name), txd_reg_subfield,
                bt.iodefs.PIO_SELECT_HCI_UART_TX)

        # set RTS PIO
        iprint("Setting RTS PIO: {}.{} = {}".format(
            rts_reg_name, rts_reg_subfield, bt.iodefs.PIO_SELECT_HCI_UART_RTS))
        setattr(getattr(bt_regs, rts_reg_name), rts_reg_subfield,
                bt.iodefs.PIO_SELECT_HCI_UART_RTS)

        # set UART RXD PIO
        iprint("Setting RTS PIO: PIO_HCI_UART_IN_PIO_SELECT.HCI_UART_RXD_IN = "
              "{}".format(rxd_pio))
        bt_regs.PIO_HCI_UART_IN_PIO_SELECT.HCI_UART_RXD_IN = rxd_pio

        # set UART CTS PIO
        iprint("Setting CTS PIO: PIO_HCI_UART_IN_PIO_SELECT.HCI_UART_CTS_IN = "
              "{}".format(cts_pio))
        bt_regs.PIO_HCI_UART_IN_PIO_SELECT.HCI_UART_CTS_IN = cts_pio

        # enable UART
        bt_regs.PIO_HCI_UART_IN_PIO_SELECT.HCI_UART_EN = 1

        def configure_curator_registers():
            "Configures the curator UART registers"
            txd_reg_num = (txd_pio//2) * 2
            rts_reg_num = (rts_pio//2) * 2
            rxd_reg_num = (rxd_pio//2) * 2
            cts_reg_num = (cts_pio//2) * 2

            txd_reg_name = "PIO_{:02}{:02}_SUBSYS_DEBUG_SELECT".format(
                txd_reg_num, txd_reg_num+1)
            rts_reg_name = "PIO_{:02}{:02}_SUBSYS_DEBUG_SELECT".format(
                rts_reg_num, rts_reg_num+1)
            rxd_reg_name = "PIO_{:02}{:02}_SUBSYS_DEBUG_SELECT".format(
                rxd_reg_num, rxd_reg_num+1)
            cts_reg_name = "PIO_{:02}{:02}_SUBSYS_DEBUG_SELECT".format(
                cts_reg_num, cts_reg_num+1)

            txd_reg_subfield = "PIO{:02}_SUBSYS_DEBUG_SELECT".format(txd_pio)
            rts_reg_subfield = "PIO{:02}_SUBSYS_DEBUG_SELECT".format(rts_pio)
            rxd_reg_subfield = "PIO{:02}_SUBSYS_DEBUG_SELECT".format(rxd_pio)
            cts_reg_subfield = "PIO{:02}_SUBSYS_DEBUG_SELECT".format(cts_pio)

            cur = self.chip.curator_subsystem.core
            btss_id = bt.subsystem.id
            setattr(getattr(cur.regs, txd_reg_name), txd_reg_subfield, btss_id)
            setattr(getattr(cur.regs, rts_reg_name), rts_reg_subfield, btss_id)
            setattr(getattr(cur.regs, rxd_reg_name), rxd_reg_subfield, btss_id)
            setattr(getattr(cur.regs, cts_reg_name), cts_reg_subfield, btss_id)
        configure_curator_registers()

    grab_uart = grab_hci_uart # def synonym c.f. QCC514X_QCC304XHaps7Device.

    def grab_debug_uart(self, txd_pio):
        '''
        Mux TXD signal of the BT_SYS debug UART out to any PIO on the chip.
        '''
        bt = self.chip.bt_subsystem.core
        cur = self.chip.curator_subsystem.core

        iprint("Debug UART Pin assignment: TXD({})".format(txd_pio))

        # find out what register to write for TXD
        txd_reg_num = (txd_pio//4) * 4
        txd_reg_name = "PIO_SELECT_{}_TO_{}".format(
            txd_reg_num, txd_reg_num + 3)
        txd_reg_subfield = "PIO_SELECT_{}".format(txd_pio)
        txd_reg = getattr(bt.regs, txd_reg_name)

        # set TXD PIO
        iprint("Setting TXD PIO: {}.{} = {}".format(
            txd_reg_name, txd_reg_subfield, bt.iodefs.PIO_SELECT_DBG_UART_TX))
        setattr(txd_reg, txd_reg_subfield, bt.iodefs.PIO_SELECT_DBG_UART_TX)

        # configure curator registers
        txd_reg_num = (txd_pio//2) * 2

        txd_reg_name = "PIO_{:02}{:02}_SUBSYS_DEBUG_SELECT".format(
            txd_reg_num, txd_reg_num+1)
        txd_reg_subfield = "PIO{:02}_SUBSYS_DEBUG_SELECT".format(txd_pio)
        txd_reg = getattr(cur.regs, txd_reg_name)

        setattr(txd_reg, txd_reg_subfield, bt.subsystem.id)

    def grab_swd(self, swclk_pad=24, swdio_pad=25):
        '''
        Configure chip pad mux to send out BT ARM SWD lines.
        SWD_DATA_INOUT can only be on pads 3,23,25,61.
        SWD_CLK_IN can only be on pads 2,22,25,60.
        Default values are for SWD header on WBLAM134 Lab & DV board .
        '''
        cur = self.chip.curator_subsystem.core

        iprint("SWD Pin assignment: SWCLK({}) SWDIO({})".format(
            swclk_pad, swdio_pad))

        # valid PAD numbers, source CS-00407502-DD
        # PIO 2,3 are used for TRB,
        SWCLK_VALID_PADS = [2, 22, 24, 60]
        SWDIO_VALID_PADS = [3, 23, 25, 61]

        if (swclk_pad not in SWCLK_VALID_PADS or
                swdio_pad not in SWDIO_VALID_PADS):
            raise ValueError(textwrap.dedent("""\
                Invalid pad used for SWDCLK {} or SWDIO {}:
                Valid pads for SWCLK are {};
                Valid pads for SWDIO are {}.
                """.format(swclk_pad, swdio_pad,
                           SWCLK_VALID_PADS, SWDIO_VALID_PADS)))
        swclk_reg_num = (swclk_pad//4) * 4
        swdio_reg_num = (swdio_pad//4) * 4

        if swclk_pad == 2:
            iprint("Warning: pad {} is normally used for TRB".format(swclk_pad))
            swclk_reg_name = "KA_PIO2_PIO4_MUX_CONTROL"
        else:
            swclk_reg_name = "CHIP_PIO{}_PIO{}_MUX_CONTROL".format(
                swclk_reg_num, swclk_reg_num+3)

        if swdio_pad == 3:
            iprint("Warning: pad {} is normally used for TRB".format(swdio_pad))
            swdio_reg_name = "KA_PIO2_PIO4_MUX_CONTROL"
        else:
            swdio_reg_name = "CHIP_PIO{}_PIO{}_MUX_CONTROL".format(
                swdio_reg_num, swdio_reg_num+3)

        swclk_reg = getattr(cur.regs, swclk_reg_name)
        swdio_reg = getattr(cur.regs, swdio_reg_name)

        swclk_reg_subfield = "PIO{}_MUX_SEL".format(swclk_pad)
        swdio_reg_subfield = "PIO{}_MUX_SEL".format(swdio_pad)

        # set SWCLK pad config
        iprint("Set SWCLK: {}.{} = {}".format(swclk_reg_name, swclk_reg_subfield,
                                             cur.iodefs.IO_FUNC_SEL_BT_MCI_SWD))
        setattr(swclk_reg, swclk_reg_subfield,
                cur.iodefs.IO_FUNC_SEL_BT_MCI_SWD)

        # set SWDIO pad config
        iprint("Set SWDIO: {}.{} = {}".format(swdio_reg_name, swdio_reg_subfield,
                                             cur.iodefs.IO_FUNC_SEL_BT_MCI_SWD))
        setattr(swdio_reg, swdio_reg_subfield,
                cur.iodefs.IO_FUNC_SEL_BT_MCI_SWD)
