############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2016 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""IPC Analysis.

Module for analysing inter processor communication
"""
from . import Analysis
from ..Core import Arch
from ACAT.Core.exceptions import BundleMissingError
from ACAT.Core.CoreTypes import ChipVarHelper as ch

try:
    # pylint: disable=redefined-builtin
    from future_builtins import hex
except ImportError:
    pass

VARIABLE_DEPENDENCIES = {'strict': ('$_g_ipc_data',)}
ENUM_DEPENDENCIES = {'strict': ('HWSEMIDX', '_IPC_SETUP_STATE', 'KIP_MSG_ID')}
TYPE_DEPENDENCIES = {
    'IPC_PROCDATA': ('msg_chan', 'sig_block', 'data_chan', 'aux_states'),
    'IPC_AUX_STATES': ('aux',),
    'IPC_MESSAGE_BLOCK': (
        'number_of_channels', 'max_msg_sz', 'max_msgs', 'ipc_message_channel'
    ),
    'IPC_MESSAGE_CHANNEL': (
        'p0_2_px_message_queue', 'px_2_p0_message_queue', 'channel_id'
    ),
    'IPC_SIGNAL_BLOCK': (
        'sigsem_idx',
        'total_active_channels',
        'callback',
        'channels',
    ),
    'IPC_SIGNAL_CHANNEL': (
        'signal_id', 'priority', 'signal_owner', 'dedisig_handler',
        'signal_data'
    ),
    'IPC_DATA_BLOCK': (
        'chansem_idx',
        'first_data_channel',
    ),
    'IPC_DATA_CHANNEL': ('remote_proc_id', 'cbuffer'),
    'IPC_SIGNAL_FIFO': ('max', 'entry', 'numelts', 'entry')
}

# IPC messages headers that shows internal or external messages indicators.
VALID_MSG_HEADER_INT = 0xC440
VALID_MSG_HEADER_EXT = 0xC448


class IPC(Analysis.Analysis):
    """Encapsulates an analysis for Kymera inter-processor communication.

    Args:
        **kwarg: Arbitrary keyword arguments.
    """

    def __init__(self, **kwarg):
        Analysis.Analysis.__init__(self, **kwarg)

        # Check if the IPC module is available
        kymera = self.debuginfo.get_kymera_debuginfo()
        self.available = '$_g_ipc_data' in kymera.var_by_name
        self._do_debuginfo_lookup = True
        self.ipc_data = None
        self.ipc_msg_block_p = None
        self.signal_block_p_array = None
        self.data_block_p_array = None
        self.aux_states_p = None

    def run_all(self):
        """Perform analysis and spew the output to the formatter.

        Displays information about inter-processor communication.
        """
        if not self.available:
            self.formatter.section_start('Inter-process communication')
            self.formatter.output('Not present in the build')
            self.formatter.section_end()
            return

        # Look up the debug information, unless we already have.
        if self._do_debuginfo_lookup:
            self._lookup_debuginfo()
            self._do_debuginfo_lookup = False

        self.formatter.section_start('Inter-process communication')

        self.analyse_aux_states()
        self.analyse_messages()
        self.analyse_signals()
        self.analyse_data()
        self.read_kip_requests()

        self.formatter.section_end()

        if self.chipdata.is_volatile():
            self._do_debuginfo_lookup = True

    def print_message_queue(self, msg_queue_var,
                            display_buffer, display_messages):
        """Displays the message queue.

        Args:
            msg_queue_var: Pointer to a message queue variable.
            display_buffer (bool): Flag to indicate if the underlying
                buffer should be further analysed.
            display_messages (bool): Flag to indicate if message
                information is needed.
        """
        # buffers are used to store messages, this import buffer analysis for
        # analysing it
        if display_buffer:
            buffer_analysis = self.interpreter.get_analysis(
                "buffers", self.chipdata.processor
            )

        self.formatter.output(
            'Queue ID: {0}'.format(msg_queue_var['queue_id'].value)
        )
        self.formatter.output(
            'Read counter: {0}'.
            format(msg_queue_var['message_read_counter'].value)
        )
        self.formatter.output(
            'Next message counter: {0}'.
            format(msg_queue_var['next_message_counter'].value)
        )
        try:
            module_name = self.debuginfo.get_source_info(
                msg_queue_var['callback'].value
            ).module_name
        except BundleMissingError:
            module_name = ("No source information. Bundle is missing.")
        self.formatter.output('Callback: {0}'.format(module_name))

        buffer_pointer = msg_queue_var['cbuffer'].value
        buffer_var = self.chipdata.cast(buffer_pointer, 'tCbuffer')
        self.formatter.output(
            'Messages are stored in buffer at: {0}\n'.
            format(hex(buffer_var['base_addr'].value))
        )

        if display_messages:
            messages = self._get_buffer_messages(buffer_var)
            msg_str = 'Messages: \n'
            for (header, msg_id) in messages:
                if header == VALID_MSG_HEADER_EXT:
                    msg_str += (
                        '  External message with ID: {0}\n'.format(hex(msg_id))
                    )
                elif header == VALID_MSG_HEADER_INT:
                    msg_str += (
                        '  Internal message with ID: {0}\n'.format(hex(msg_id))
                    )
            self.formatter.output(msg_str + '\n')

        # if asked, print out the buffer associated with the queue
        if display_buffer:
            if msg_queue_var['cbuffer'].value != 0:
                buffer_analysis.analyse_cbuffer(buffer_pointer)
            else:
                self.formatter.output('Message queue buffer is null \n')

    def analyse_messages(self, display_buffer=True, display_messages=False):
        """Analyses information about messages of ipc.

        If True parameter is passed to the function, buffer containing
        messages is analysed as well.

        Args:
            display_buffer (bool, optional): Flag to indicate if the
                underlying buffer should be further analysed.
            display_messages (bool, optional): Flag to indicate if message
                information is needed.
        """
        # Look up the debug information, unless we already have.
        if self._do_debuginfo_lookup:
            self._lookup_debuginfo()
            self._do_debuginfo_lookup = False

        self.formatter.section_start('IPC Messages Information')
        if self.ipc_msg_block_p == 0:
            self.formatter.output('No message information')
            self.formatter.section_end()
            if self.chipdata.is_volatile():
                self._do_debuginfo_lookup = True
            return

        msg_block = self.chipdata.cast(
            self.ipc_msg_block_p, 'IPC_MESSAGE_BLOCK'
        )
        self.formatter.output(
            'Number of message channels used: {0}'.format(
                msg_block['number_of_channels'].value
            )
        )
        self.formatter.output(
            'Maximum message size: {0}'.format(
                msg_block['max_msg_sz'].value
            )
        )
        self.formatter.output(
            'Maximum number of messages: {0}'.format(
                msg_block['max_msgs'].value
            )
        )

        # there will be array of msg channels, so read messages from them
        msg_cannel_p_array = msg_block['ipc_message_channel']
        self.formatter.output('IPC message channels:')
        # there might be more than one channel if more processors are used
        for msg_channel_p in msg_cannel_p_array:
            msg_channel = self.chipdata.cast(
                msg_channel_p.value, 'IPC_MESSAGE_CHANNEL'
            )
            self.formatter.output(
                '  Channel ID: {0}\n'.
                format(hex(msg_channel['channel_id'].value))
            )

            p0_to_px_msq_q = msg_channel['p0_2_px_message_queue']
            self.formatter.section_start('P0 to Px message queue:')
            self.print_message_queue(
                p0_to_px_msq_q, display_buffer, display_messages
            )
            self.formatter.section_end()

            px_to_p0_msg_q = msg_channel['px_2_p0_message_queue']
            self.formatter.section_start('Px to P0 message queue:')
            self.print_message_queue(
                px_to_p0_msg_q, display_buffer, display_messages
            )
            self.formatter.section_end()

        self.formatter.section_end()

        if self.chipdata.is_volatile():
            self._do_debuginfo_lookup = True

    def _display_aux_state(self, aux_state, processor):
        """Analyses and displays Aux state in IPC.

        Args:
            aux_state: Processor specific aux_state.
            processor: Processor number.
        """
        all_states = self.debuginfo.get_enum('_IPC_SETUP_STATE', None)
        setup_state = aux_state['setup_state'].value
        wakeup_time = aux_state['wakeup_time'].value
        # Status state represents several possible flags. Find these flags
        # now
        if setup_state == 0x0000:
            active_states = ['SETUP_NONE']
        else:
            active_states = []
            for state, flag in all_states.items():
                if state == 'IPC_SETUP_NONE':
                    # IPC_SETUP_NONE is the default no/zero state. Skip it.
                    continue
                mask = 0x000F
                for _ in range(4):
                    current_flag = setup_state & mask
                    if current_flag == flag:
                        active_states.append(state.replace("IPC_", ""))
                    mask = mask << 4
        # active_states lists all the flags raised and the actual state.
        # Converted it into a human readable comma separated list
        state_str = ', '.join(sorted(active_states))

        self.formatter.output(
            'Aux state for processor p{0}:'.format(processor)
        )
        self.formatter.output(
            '    Setup state: {0} - {1}'.
            format(hex(setup_state), state_str)
        )
        self.formatter.output('    Wake-up time: {0}'.format(wakeup_time))

    def _display_signal_block(self, signal_p, block_index, analyse_fifo):
        """Analyses and displays signals used in IPC.

        Args:
            signal_p: IPC signal block pointer.
            block_index: Index of signal_p within signal_block_p_array.
            analyse_fifo (bool): If true, fifo will be analysed.
        """
        free_channel_bit = 0x7F
        self.formatter.output(
            'Signal block {0} information:'.format(block_index)
        )
        if signal_p == 0:
            self.formatter.output('    Not initialised')
            return

        signal_block = self.chipdata.cast(signal_p, 'IPC_SIGNAL_BLOCK')
        enum_member = signal_block['sigsem_idx'].value
        sem_id = self.debuginfo.get_enum('HWSEMIDX', enum_member)
        sem_id = sem_id[0].replace('HWSEMIDX_', '')
        self.formatter.output('    Semaphore ID: {0}'.format(sem_id))
        self.formatter.output(
            '    Total active channels: {0}'.
            format(signal_block['total_active_channels'].value)
        )
        try:
            signal_handler = signal_block['callback'].value
            try:
                module_name = self.debuginfo.get_source_info(
                    signal_handler
                ).module_name
            except BundleMissingError:
                module_name = "No source information. Bundle is missing."
            self.formatter.output('    Signal handler: {0}'.format(module_name))
        except AttributeError:
            # IPC improvements removed the call back. nothing to do
            pass
        self.formatter.output(' ' * 6 + 'Signal channels information:\n')
        free_channels_count = 0
        chanels_str = ''
        active_chans = signal_block['total_active_channels'].value
        channel_type_size = self.debuginfo.get_type_info(
            'IPC_SIGNAL_CHANNEL'
        )[5]
        for channel_nr in range(active_chans):
            channels_p = (
                signal_block['channels'].value +
                channel_type_size * channel_nr
            )
            channel = self.chipdata.cast(channels_p, 'IPC_SIGNAL_CHANNEL')
            # Owner of the signal channel can be one of the processors or it
            # can be free, indicated by magic value
            if channel['signal_owner'].value == free_channel_bit:
                free_channels_count += 1
            else:
                chanels_str += ' ' * 8 + 'Signal channel ID: {0}\n'.format(
                    hex(channel['signal_id'].value)
                )
                try:
                    chanels_str += ' ' * 8 + 'Priority: {0}\n'.format(
                        hex(channel['priority'].value)
                    )
                except AttributeError:
                    # IPC improvements removed the Priority. nothing to do
                    pass
                chanels_str += ' ' * 8 + 'Signal Owner: {0}\n'.format(
                    hex(channel['signal_owner'].value)
                )
                try:
                    module_name = self.debuginfo.get_source_info(
                        channel['dedisig_handler'].value
                    ).module_name
                except BundleMissingError:
                    module_name = "No source information. Bundle is missing."
                chanels_str += ' ' * 8 + 'Handler: {0}\n'.format(module_name)
                try:
                    signal_data = channel['signal_data']
                    chanels_str += ' ' * 8 + 'Signal data: '
                    if signal_data['signal_size'].value == 0:
                        chanels_str += 'None\n\n'
                    else:
                        chanels_str += 'Signal of size {0} at {1}\n\n'.format(
                            hex(signal_data['signal_size'].value),
                            hex(signal_data['signal_ptr'].value)
                        )
                except AttributeError:
                    # IPC improvements removed the signal_data. nothing to do
                    pass

        chanels_str = chanels_str[:-2]  # for beauty reasons
        self.formatter.output(
            ' ' * 8 +
            'There are {0} free signal channels.'.format(free_channels_count)
        )
        self.formatter.output(
            ' ' * 8 + 'Occupied channels:\n\n' + chanels_str + '\n'
        )
        if analyse_fifo:
            self._analyse_fifo(signal_block)

    def analyse_signals(self, analyse_fifo=False):
        """Analyses all the IPC signals with the help of _display_signal_block.

        Args:
            analyse_fifo (bool, optional): If true, fifo will be analysed.
        """
        # Look up the debug information, unless we already have.
        if self._do_debuginfo_lookup:
            self._lookup_debuginfo()
            self._do_debuginfo_lookup = False

        self.formatter.section_start('IPC Signals information')
        self.formatter.output(
            'There are {0} IPC signal blocks\n'.format(
                len(self.signal_block_p_array)
            )
        )
        for block_index, signal_p in enumerate(self.signal_block_p_array):
            self._display_signal_block(signal_p, block_index, analyse_fifo)

        self.formatter.section_end()

        if self.chipdata.is_volatile():
            self._do_debuginfo_lookup = True

    def analyse_data(self, display_buffer=True):
        """Analyses data channels present in ipc.

        If True parameter is passed, buffer associated with data channel
        is displayed.

        Args:
            display_buffer (bool, optional)
        """
        # buffers are used to store messages, this import buffer analysis for
        # analysing it
        if display_buffer:
            buffer_analysis = self.interpreter.get_analysis(
                "buffers", self.chipdata.processor
            )
        free_data_channel_magic = 0xF5EE

        # Look up the debug information, unless we already have.
        if self._do_debuginfo_lookup:
            self._lookup_debuginfo()
            self._do_debuginfo_lookup = False

        self.formatter.section_start('IPC Data')
        # filter out null pointers
        self.data_block_p_array = [
            i for i in self.data_block_p_array if i != 0
        ]
        self.formatter.output(
            'There are {0} IPC data blocks\n'.format(
                len(self.data_block_p_array)))
        for data_block_p in self.data_block_p_array:
            data_block = self.chipdata.cast(data_block_p, 'IPC_DATA_BLOCK')
            block_index = self.data_block_p_array.index(data_block_p)

            self.formatter.output(
                'Data block {0} information:'.format(block_index)
            )
            sem_id = self.debuginfo.get_enum(
                'HWSEMIDX', data_block['chansem_idx'].value
            )[0].replace('HWSEMIDX_', '')
            self.formatter.output('    Semaphore id: {0}'.format(sem_id))
            if data_block['first_data_channel'].value == 0:
                self.formatter.output('    No data channels present')
                # if first data channel points to NULL, we probably do not need
                # to analyse it
                continue

            first_data_channel = self.chipdata.cast(
                data_block['first_data_channel'].value,
                'IPC_DATA_CHANNEL'
            )
            data_cannels = [
                c for c in ch.parse_linked_list(first_data_channel, 'next')
            ]
            self.formatter.output('    Following data channels present:\n')
            for data_cannel in data_cannels:
                self.formatter.output(
                    ' ' * 6 + 'Channel ID: {0}'.format(
                        hex(data_cannel['data_channel_id'].value)
                    )
                )
                remote_proc_id = data_cannel['remote_proc_id'].value
                if remote_proc_id == free_data_channel_magic:
                    self.formatter.output(
                        ' ' * 6 + 'This data channel is free'
                    )
                    continue

                self.formatter.output(
                    ' ' * 6 + 'Remote processor id: {0}'.format(
                        hex(remote_proc_id)
                    )
                )
                if data_cannel['cbuffer'].value != 0:
                    buffer_var = self.chipdata.cast(
                        data_cannel['cbuffer'].value, 'tCbuffer'
                    )
                    self.formatter.output(
                        ' ' *
                        6 +
                        'Channel has created a buffer at {0}\n'.format(
                            hex(
                                buffer_var['base_addr'].value)))
                    if display_buffer:
                        if data_cannel['cbuffer'].value != 0:
                            buffer_analysis.analyse_cbuffer(
                                data_cannel['cbuffer'].value
                            )
                        else:
                            self.formatter.output(
                                'data_cannel buffer is null \n'
                            )
                else:
                    self.formatter.output(
                        ' ' * 6 + 'No buffer created by the channel\n'
                    )

        self.formatter.section_end()

        if self.chipdata.is_volatile():
            self._do_debuginfo_lookup = True

    def analyse_aux_states(self):
        """Analyses the stated at which processors are at the time."""
        # Look up the debug information, unless we already have.
        if self._do_debuginfo_lookup:
            self._lookup_debuginfo()
            self._do_debuginfo_lookup = False

        self.formatter.section_start('Aux states')

        if self.aux_states_p == 0:
            self.formatter.output('No aux states information')
            self.formatter.section_end()
            if self.chipdata.is_volatile():
                self._do_debuginfo_lookup = True
            return

        aux_states_array = self.chipdata.cast(
            self.aux_states_p, 'IPC_AUX_STATES'
        )['aux']

        if len(aux_states_array) == 0:
            # IPC improvements got rid of states for every processor
            self._display_aux_state(aux_states_array, 1)
        else:
            # This is states for every processor used, and it will be an array
            for processor in range(len(aux_states_array)):
                aux_state = aux_states_array.__getitem__(processor)
                self._display_aux_state(aux_state, processor)

        self.formatter.section_end()

        if self.chipdata.is_volatile():
            self._do_debuginfo_lookup = True

    def read_kip_requests(self, display_all=False):
        """Shows kip request for ipc.

        If True is passed as an argument, all requests are displayed. If
        not, then only reasonable amount of them will be displayed.
        """
        # Look up the debug information, unless we already have.
        if self._do_debuginfo_lookup:
            self._lookup_debuginfo()
            self._do_debuginfo_lookup = False

        self.formatter.section_start('KIP requests')
        # kip requests are held in linked list, thus read these entries
        kip_first_req = self.chipdata.get_var_strict('L_kip_first_req')
        kip_requests = [
            req for req in ch.parse_linked_list(kip_first_req, 'next')
        ]
        if not kip_requests:
            self.formatter.output('No KIP request present')
        else:
            # there might be lots of kip requests, thus display reasonable
            # number of them, unless ser asked to display all of them
            self.formatter.output('Pending KIP requests:\n')
            kip_str = ''
            max_to_display = 10
            displayed = 0
            apology = ''
            for req in kip_requests:
                kip_str += 'Connection ID: {0}\n'.format(
                    hex(req['con_id'].value)
                )
                msg = self.debuginfo.get_enum(
                    'KIP_MSG_ID', req['msg_id'].value
                )[0].replace('KIP_MSG_ID_', "")
                kip_str += 'Message ID: {0}\n'.format(msg)
                kip_str += 'Sequence number: {0}\n'.format(
                    hex(req['seq_nr'].value)
                )
                req_context = req['context'].value
                if req_context != 0:
                    try:
                        module_name = self.debuginfo.get_source_info(
                            req_context
                        ).module_name
                    except BundleMissingError:
                        module_name = (
                            "No source information. " + "Bundle is missing."
                        )
                    kip_str += 'Context: {0}\n'.format(module_name)
                kip_str += '\n'
                displayed += 1
                if displayed == max_to_display and not display_all:
                    apology = (
                        '\n...\n{0} more requests. '
                        'Pass True to the method to display '
                        'all requests'.format(len(kip_requests) - displayed)
                    )

            self.formatter.output(kip_str + apology)

        self.formatter.section_end()

        if self.chipdata.is_volatile():
            self._do_debuginfo_lookup = True

    def _analyse_fifo(self, signal_block):
        """Shows content of fifo, if any pending actions are stored.

        Args:
            signal_block: Signal block that this fifo belongs to.
        """

        self.formatter.section_start('FIFO information')
        fifo_p_array = signal_block['fifo'].value
        # different fifo's are for different priority messages, but not all
        # of them might be  used, thus only display those which had messages
        # in them at some point, which is indicated by the counter of maximum
        # slots taken
        for fifo_p in fifo_p_array:
            priority = fifo_p_array.index(fifo_p)
            fifo = self.chipdata.cast(fifo_p, 'IPC_SIGNAL_FIFO')
            if fifo['max'].value == 0:
                self.formatter.output(
                    'FIFO of priority {0} have not been used'.format(priority)
                )
                # Nothing to do here anymore
                continue
            else:
                max_entries = len(fifo['entry'])
                self.formatter.output(
                    'FIFO of priority {0} information:\n'.format(priority)
                )
                numelts = fifo['numelts'].value
                self.formatter.output(
                    ' ' * 2 +
                    'Number of elements currently stored: {0}'.format(numelts)
                )
                self.formatter.output(
                    ' ' * 2 + 'Maximum number stored: {0}'.
                    format(fifo['max'].value)
                )
                if numelts > 0:
                    # display pending elements
                    read_idx_initial = fifo['rdidx'].value
                    for element_nr in range(numelts):
                        read_idx = (
                            read_idx_initial + element_nr
                        ) % max_entries
                        element = fifo['entry'].__getitem__(read_idx)
                        self.formatter.output(
                            ' ' * 4 +
                            'Fifo entry at position: {0}'.format(read_idx)
                        )
                        self.formatter.output(
                            ' ' * 6 + 'Signal channel: {0}'.format(
                                element['sigchan'].value
                            )
                        )
                        self.formatter.output(
                            ' ' * 6 + 'Sender: {0}'.format(
                                element['sender'].value
                            )
                        )
                        self.formatter.output(
                            ' ' * 6 + 'Signal ID: {0}'.format(
                                element['sigid'].value
                            )
                        )
                        signal_ptr = element['sigreq']['signal_ptr'].value
                        self.formatter.output(
                            ' ' * 6 + 'Signal pointer: {0}'.format(
                                hex(signal_ptr)
                            )
                        )

        self.formatter.section_end()

    def _get_buffer_messages(self, buffer_var):
        """Get buffer messages.

        Args:
            buffer_var
        """
        size = buffer_var['size'].value
        read_ptr = buffer_var['read_ptr'].value
        base_addr = buffer_var['base_addr'].value
        current_ptr = read_ptr
        message_list = []
        # Messages can be different length so search for valid id to treat it
        # as message.
        # messages can be either internal or external and they might have
        # different size as well. So try both options. Finding either of the
        # headers in the buffer, and according to the header. get information
        # about the message
        while True:
            # now read the messages that were in this buffer before they were
            #  read. They will be ones before read pointer do it until
            # current pointer reaches write pointer, when reading backwards.
            # Do not forget about buffer wrapping
            current_data = self.chipdata.get_data(current_ptr)
            possible_header = current_data & 0x0000FFFF
            if ((possible_header == VALID_MSG_HEADER_INT) or
                    (possible_header == VALID_MSG_HEADER_EXT)):
                if current_ptr == base_addr + size:
                    # this case may happen if our message header is in last
                    # word of the buffer. Then message info will be
                    # in first word of the buffer
                    message_info = self.chipdata.get_data(base_addr)
                else:
                    message_info = self.chipdata.get_data(
                        current_ptr + Arch.addr_per_word
                    )
                message_id = message_info >> 16
                message_list.append((possible_header, message_id))

            if current_ptr == base_addr:
                current_ptr += size
            else:
                current_ptr -= Arch.addr_per_word

            # if we reached this point and we see that we started here, we have
            # already read the whole buffer
            if current_ptr == read_ptr:
                break

        return message_list

    def _lookup_debuginfo(self):
        """Reads the information required for analyses.

        It may need to be performed more than once if working with live
        chip.
        """

        self.ipc_data = self.chipdata.get_var_strict('$_g_ipc_data')
        self.ipc_msg_block_p = self.ipc_data['msg_chan'].value
        self.signal_block_p_array = self.ipc_data['sig_block'].value
        self.data_block_p_array = self.ipc_data['data_chan'].value
        self.aux_states_p = self.ipc_data['aux_states'].value
