############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################


from csr.wheels.global_streams import iprint, wprint
from csr.dev.fw.bd_addr import BdAddr

class RfPowerReader(object):
    """
    Library supporting reading TX and RX(RSSI) of active links using Trap APIs
    """

    def __init__(self, trap_utils):
        """
        init RfPowerReader
        """
        self._utils = trap_utils
        self._apps1 = trap_utils.apps1
        self._apps0 = trap_utils.apps0

    def get_bt_tx_power_sink(self, sink, stype="Unknown", print_log=False):
        """
        Returns the TX power level and remote bd address of a given sink id
        """
        bd_addr = None
        tx_dBm = None
        try:
            tp = self._apps1.fw.call.pnew("tp_bdaddr")
            task = self._apps1.fw.trap_utils.create_task()
            rsp = None

            if self._apps1.fw.call.SinkGetBdAddr(sink, tp):
                self._apps1.fw.call.ConnectionReadTxPower(task, tp)

                while rsp is None:
                    rsp = self._apps1.fw.trap_utils.try_get_core_msg()

                if rsp['t'] == task:
                    if (rsp['id'] == self._apps1.fw.enum.ConnectionMessageId[
                            'CL_DM_READ_TX_POWER_CFM']):
                        pwr_cfm = self._apps1.fw.env.cast(
                            rsp['m'], "CL_DM_READ_TX_POWER_CFM_T")

                        if self._apps1.fw.enum.hci_status[
                                pwr_cfm.status.value] == "hci_success":
                            bd_addr = BdAddr.from_struct(pwr_cfm.tpaddr.taddr)
                            tx_dBm = pwr_cfm.tx_power.value
                            if print_log:
                                iprint("Sink %04x Type:%-15s BD_ADDR %s %s "
                                       "RTPL: %d dBm " % (
                                           sink, stype, bd_addr.short_str,
                                           pwr_cfm.tpaddr.transport, tx_dBm))

                            self._apps1.fw.call.pfree(rsp['m'])

                else:
                    wprint("Received unexpected message %s" %
                           self._apps1.fw.enum.ConnectionMessageId[rsp['id']])
        finally:
            self._apps1.fw.call.pfree(tp)
            self._apps1.fw.trap_utils.delete_task(task)
        return (bd_addr, tx_dBm)

    def get_bt_rssi_sink(self, sink, stype="Unknown", print_log=False):
        """
        Returns the RSSI value of given sink id
        """
        ret = None
        try:
            rssi = self._apps1.fw.call.pnew("int16")

            if self._apps1.fw.call.SinkGetRssi(sink, rssi):
                ret = rssi.value
                if print_log:
                    iprint("Sink %04x Type:%-15s RSSI: %d dBm" % (
                        sink, stype, ret))
        finally:
            self._apps1.fw.call.pfree(rssi)
        # return the RSSI
        return ret

    def _bt_streams(self):
        """
        Returns a list of streams associated with Bluetooth activity.
        """
        return [
            stream for stream in self._apps0.fw.streams.streams()
            if stream.kind in ('l2cap', 'sco', 'rfcomm', 'csb_tx', 'csb_rx')]

    def bt_tx_power_streams(self, print_log=False):
        """
        Get the transmit power levels for active links.

        Returns a list of tuples of:
            (stream, remote bd address, tx power).

        stream is a Python object encapsulating the stream which can answer
        various questions about the stream.

        remote bd address is the Bluetooth address of the remote side of the
        link as a string.

        tx_power is the transmit power in dBm.
        """
        results = [
            (stream,) + self.get_bt_tx_power_sink(
                stream.sink_id, stream.description, print_log=print_log)
            for stream in self._bt_streams()]
        return [(stream, addr, pwr) for stream, addr, pwr in results
                if pwr is not None]

    def bt_tx_power_sink(self, print_log=False):
        """
        get the TX power level of active TX links
        returns a list of tuples with:
          (sink_id, stream_type, cid, remote bd address, tx_power)

        sink_id is the identifier of this sink as a string (a hexadecimal
        conversion of the ID).

        stream_type is a string giving the overall type of the stream (l2cap,
        rfcomm and so on).

        cid is some sort of numeric (not string) identifier of the connection
        if we can have more than one connection of that type (for example,
        L2CAP CID or RFCOMM Mux ID) or None if the stream does not have any
        sub-identification.

        remote bd address is the Bluetooth address of the remote side of the
        link as a string.

        tx_power is the transmit power in dBm.

        You may prefer bt_tx_power_streams which returns objects which
        can provide more information than is in this tuple.
        """
        return [('%04x' % stream.sink_id, stream.kind, stream.key,
                 addr.short_str, pwr)
                for stream, addr, pwr in self.bt_tx_power_streams(
                    print_log=print_log)]

    def bt_rssi_streams(self, print_log=False):
        """
        Get the RSSI values of active RX links

        Returns a list of tuples of (stream, rssi)

        stream is a Python object encapsulating the stream which can answer
        various questions about the stream.

        rssi is the received signal strength in dBm.
        """
        results = [
            (stream, self.get_bt_rssi_sink(
                stream.sink_id, stream.description, print_log=print_log))
            for stream in self._bt_streams()]
        return [(stream, rssi) for stream, rssi in results if rssi is not None]

    def bt_rssi_sink(self, print_log=False):
        """
        get the RSSI values of active RX links
        returns a list of tuples with (sink_id, stream_type, cid, rssi)

        sink_id is the identifier of this sink as a string (a hexadecimal
        conversion of the ID).

        stream_type is a string giving the overall type of the stream (l2cap,
        rfcomm and so on).

        cid is some sort of numeric (not string) identifier of the connection
        if we can have more than one connection of that type (for example,
        L2CAP CID or RFCOMM Mux ID) or None if the stream does not have any
        sub-identification.

        rssi is the received signal strength in dBm.

        You may prefer bt_rssi_streams which returns objects which
        can provide more information than is in this tuple.
        """
        return [('%04x' % stream.sink_id, stream.kind, stream.key, rssi)
                for stream, rssi in self.bt_rssi_streams(print_log=print_log)]
