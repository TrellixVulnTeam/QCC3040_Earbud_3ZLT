############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from .structs import IAdkStructHandler

class A2dpConnection(FirmwareComponent):
    '''
        Decoder for the A2DP library remote_device structure.
        This decoder not not support the SINGLE_MEM_SLOT library variant.
    '''

    def __init__(self, env, core, connection, data_block, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        self._connection = connection
        self._data_block = data_block

    def _data_block_decoder(self, index, data_type, elements_from_size=False):
        ''' Returns an array of typed objects stored in the indexed data block '''
        try:
            data_block_obj = self._data_block.deref
        except InvalidDereference:
            return None
        with data_block_obj.footprint_prefetched():
            block = data_block_obj.block[index]
            size = block.element_size.value
            if size == 0:
                return None
            elements = block.block_size.value // size
            if elements_from_size:
                assert (elements == 1)
                elements = size
            offset = block.offset.value
            return self.env.cast(data_block_obj.address + offset, data_type, array_len=elements)

    @property
    def connection(self):
        ''' Return the A2DP library remote_device structure state. '''
        return self._connection

    @property
    def data_block(self):
        ''' Return the A2DP library data_block_header structure state for this connection. '''
        return self._data_block

    @property
    def address(self):
        ''' Return the Bluetooth address of the remote device. '''
        return IAdkStructHandler.handler_factory("bdaddr")(self._core, self._connection.bd_addr)

    @property
    def local_sep(self):
        ''' Return the A2DP library local sep_info structure for this connection. '''
        return self._connection.local_sep

    @property
    def remote_sep(self):
        ''' Return the A2DP library remote sep_info structure for this connection. '''
        return self._connection.remote_sep

    @property
    def signalling_state(self):
        ''' Return the connection's signalling channel state. '''
        return self._connection.signal_conn.status.connection_state

    @property
    def streaming_state(self):
        ''' Return the connection's streaming state. '''
        return self._connection.signal_conn.status.stream_state

    @property
    def sep_list(self):
        '''
            Return the list of local stream endpoints. The sep list is stored
            at an offset in the connection's data block.
        '''
        block_index = self.env.enum.data_block_id.data_block_sep_list
        return self._data_block_decoder(block_index, "sep_data_type")

    @property
    def preferred_local_seids_list(self):
        '''
            Return the list of preferred local seids. The list is stored
            at an offset in the connection's data block.
        '''
        block_index = self.env.enum.data_block_id.data_block_list_preferred_local_seids
        return self._data_block_decoder(block_index, "sep_info")

    @property
    def discovered_remote_seids_list(self):
        '''
            Return the list of discovered remote seids. The list is stored
            at an offset in the connection's data block.
        '''
        block_index = self.env.enum.data_block_id.data_block_list_discovered_remote_seids
        return self._data_block_decoder(block_index, "sep_info")

    @property
    def configured_service_caps(self):
        '''
            Return the configured service capabilities. The data is stored
            at an offset in the connection's data block.
        '''
        block_index = self.env.enum.data_block_id.data_block_configured_service_caps
        return self._data_block_decoder(block_index, "uint8", elements_from_size=True)

    def _generate_report_body_elements(self):
        ''' Report the connection state. '''
        grp = interface.Group("Connection")
        tbl = interface.Table(['Name', 'Value'])
        with self._data_block.deref.footprint_prefetched():
            tbl.add_row(['Address', self.address])
            tbl.add_row(['Local Sep', self.local_sep])
            tbl.add_row(['Remote Sep', self.remote_sep])
            tbl.add_row(['Signalling State', self.signalling_state])
            tbl.add_row(['Streaming State', self.streaming_state])
            tbl.add_row(['Sep List', self.sep_list])
            tbl.add_row(['Preferred Local Seids List', self.preferred_local_seids_list])
            tbl.add_row(['Discovered Remote Seids List', self.discovered_remote_seids_list])
            tbl.add_row(['Configured Service Caps', self.configured_service_caps])
        grp.append(tbl)
        return [grp]

class A2dp(FirmwareComponent):
    '''
        This class reports the A2DP library profile state.
    '''

    def _connections_generator(self):
        ''' Iterates the connections array yielding connections '''
        if self.a2dp != 0:
            a2dp_inst = self.a2dp.deref
            for conn, data_block in zip(a2dp_inst.remote_conn, a2dp_inst.data_blocks):
                yield A2dpConnection(self.env, self._core, conn, data_block, self)

    def _generate_report_body_elements(self):
        ''' Report the list of connections'''
        grp = interface.Group("Connections")
        tbl = interface.Table(["BdAddr", "Remote Sep", "Local Sep", "Sig State", "Media State"])
        with self.a2dp.deref.footprint_prefetched():
            for conn in self._connections_generator():
                tbl.add_row([conn.address, conn.remote_sep, conn.local_sep, conn.signalling_state, conn.streaming_state])
        grp.append(tbl)
        return [grp]

    @property
    def connections(self):
        ''' Returns a list of A2DP connections '''
        return list(self._connections_generator())

    @property
    def a2dp(self):
        ''' Returns the A2DP library instance '''
        return self.env.cu.a2dp_init.local.a2dp

