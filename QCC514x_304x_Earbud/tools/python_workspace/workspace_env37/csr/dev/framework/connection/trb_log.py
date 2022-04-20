# Copyright (c) 2016-2018 Qualcomm Technologies International, Ltd.
#   %%version

from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import bytes_to_dwords_be, bytes_to_words_be, \
                                    NameSpace
from csr.transport.trb_raw import TrbRaw, DecodedTransaction, TrbRawTransaction,\
TimedLog
from csr.transport.trbtrans import TrbErrorDriverIOTimeout
from csr.dev.framework.connection.trb import PydbgTrb
from csr.dev.hw.address_space import AddressSpace
import time

class TrbLogger(object):
    '''
    This class is used to capture firmware logs using STREAM transactions
    that are pushed from the firmware (rather than the conventional method
    of polling a firmware RAM buffer). This method has the advantage of 
    working through deep sleep (since no polling is required) and producing
    accurate timestamps.
    
    Parameter block_id_zero can be set to TRUE to use the
    off_chip_subsys_stream interface of the transaction bridge
    dongle so long as the hydra drivers aren't using it. This is useful
    for debugging subsystems starting.
    Otherwise a new conventional debugger stream is created and the
    block ID and tag are set in the firmwares so that the stream 
    transactions are routed to it. This only works if the subsystems
    are running at the time the log is started.    
    '''
    
    # From hydra_trb.h:
    #/**
    # * STREAM transactions can be used by firmware to pass messages or
    # * logging information to a debugger or host viewer application.
    # */
    STREAM_SRC_BLOCK_ID_DEEP_SLEEP          = 0
    STREAM_SRC_BLOCK_ID_PC_TRACE            = 1
    STREAM_SRC_BLOCK_ID_PC_TRACE_CPU_1      = 2
    STREAM_SRC_BLOCK_ID_FIRMWARE_LOG        = 3
    STREAM_SRC_BLOCK_ID_FIRMWARE_LOG_CPU_1  = 4
    STREAM_SRC_BLOCK_ID_PUT_CHAR            = 5
    STREAM_SRC_BLOCK_ID_PUT_CHARS           = 6
    STREAM_SRC_BLOCK_ID_PROTOCOL_LOG        = 7
    
    class LogContext(NameSpace):
        def __init__(self, fw, word_based=False):
            self.fw = fw
            self.pkt_list_len = 0
            self.pkt_list = []
            self.char_list = []
            self.decoder = fw.debug_log.decoder
            self.timestamp = None
            self.prev_timestamp = None
            self.word_based = fw.env.layout_info.addr_unit_bits == 16

    '''
    If this logging is being run from within PipeSubprocessAgent the 
    chip object won't be accessible so pass these in separately. 
    '''        
    def __init__(self, chip=None, name = None, id = None):
        self.stream = PydbgTrb()

        # Flag to indicate the driver reported samples wrapped
        self._lostit = False 
        self.timeout = 0
        self.last_timestamp = None
        self._chip = chip

        if chip is not None:
            self.tr_log = []  # Store for log entries yet to be reported
            cores = sum((list(ss.cores) for ss in chip.subsystems.values()),[])
            self._core_nickname_dict = {core.nicknames[0]: core for core in cores}
            self._log_contexts = {}
            self.dongle_name = self._chip.device.transport.dongle_name
            self.dongle_id = self._chip.device.transport.dongle_id
        else:
            self.dongle_name = name
            self.dongle_id = id

        self.sniffed_transactions = []

        self.tr_trace_p0 = []  #Store for trace entries yet to be reported
        self.tr_trace_p1 = []  #Store for trace entries yet to be reported

    def __del__(self):
        self.stream.close()

    def _get_requested(self, core_nicknames):
        if core_nicknames is None:
            cores = list(self._core_nickname_dict.keys())
        elif isinstance(core_nicknames, str):
            cores = [core_nicknames]
        else:
            cores = list(core_nicknames)
        return cores

    def start(self, core_nicknames=None):
        """
        Set the trb logging enable regiser for each specified subsystem.
        :param subsystems: list of subsystem ids for which to enable trb logging.
        :return a list of booleans reporting for each subsystem if logging was already enabled
        """
        # If chip has not been passed in we can't being running the firmware logs.

        if self._chip is not None and not self._log_contexts:
            self.stream.sample_stream_open(self.dongle_name, dongle_id=self.dongle_id)
            
        requested = self._get_requested(core_nicknames)

        # Populate the log contexts based on requested subsystems
        for nickname in requested:
            core = self._core_nickname_dict[nickname]
            if core.subsystem.is_up:
                if (hasattr(core.fw, "env") and 
                    hasattr(core.fw.env.gbl, "hydra_log_trb_cfg") and 
                    hasattr(core.fw, "debug_log")):
                    self._log_contexts.setdefault(nickname, 
                                                  self.LogContext(core.fw))
        
        requested_log_contexts = [self._log_contexts[core_nickname] for core_nickname in requested]
        already_set = []

        for log in requested_log_contexts:
            if log:
                already_set.append(log.fw.gbl.hydra_log_trb_cfg.enable.value == 1)
                log.fw.gbl.hydra_log_trb_cfg.enable.value = 1

        return already_set

    def attempt_restart(self, core_nicknames=None):
        """
        Attempt to re-trigger TRB logging by poking the enable flag in the firmware.
        This is intended for use at reset, so exceptions relating to failure at
        the transport level are converted into a False return value.  This
        method only returns True if all the trigger settings attempted were
        successful.
        """
        requested = self._get_requested(core_nicknames)
        success = True
        for nickname in requested:
            core = self._core_nickname_dict[nickname]
            try:
                core.fw.gbl.hydra_log_trb_cfg.enable.value = 1
            except AttributeError:
                # This core doesn't support TRB logging, so ignore it
                pass
            except AddressSpace.WriteFailure:
                # We can't write to this core yet.
                success = False
        return success
                    
    def stop(self, core_nicknames=None):

        requested = self._get_requested(core_nicknames)
        requested_log_contexts = [self._log_contexts[core_nickname] for core_nickname in requested]

        for log in requested_log_contexts:
            if log:
                try:
                    log.fw.gbl.hydra_log_trb_cfg.enable.value = 0
                except AttributeError:
                    pass

    def live_log(self):
        while(True):
            self.show()
            self.clear()
                            
    def _get_log(self):
        '''
        Get the sniffed transactions from the sniffing stream. We limit
        the maximum number because of the time it takes to extract them from 
        the driver. The API to the driver is meant to be improving so we can 
        ask it how many transactions it has and get them in bigger lumps.
        '''
        self._max_results = self.stream.get_max_transactions()
        try:
            results, count, _wrapped = self.stream.read_raw_transactions(
                                                   self._max_results, self.timeout)
        except TrbErrorDriverIOTimeout:
            pass
        
        '''
        Cause logging to stop if a wrap is detected. 
        
        Log entries have timestamps, so we could recover form wrapping. But
        for now the most likely cause of wrapping is the Trace module and
        it is going to get confused by gaps whichout significant extra work 
        in the decoder.
        '''
        if _wrapped or self._lostit: 
            iprint("trb_log.py: Wrapping detected in log.")
            self._lostit = True
        else:
            self.sniffed_transactions += results[:count]
        
    def show(self, logger=None, app_decode=True, report=False):
        '''
        Write the sniffed transactions from the sniffing stream to the
        console or the logger provided as an optional parameter.
        The optional parameter AppDecode can be set to False to show
        the logged transactions rather than the application level view of
        them (so firmware logs appear as stream transactions).
        '''
        rpt_or_none = None
        #Get the latest content..
        self.sort(app_decode=app_decode)

        if logger:
            logger.add(self.tr_log)
        else:
            new_log = TimedLog(self.tr_log)
            # TimedLog.show either prints the log or returns it as a lump of
            # text in an interface.Code object.
            rpt_or_none = new_log.show(self.last_timestamp, report=report)
            if new_log.last_timestamp:
                self.last_timestamp = new_log.last_timestamp
            
        # Clear log transactions which have now been displayed.
        self.tr_log = []
        if rpt_or_none is not None:
            return rpt_or_none
            
    def sort(self, app_decode=True):
        '''
        Retrieve and sort transactions from the sniffer log.        

        There are different types of TSample message defined in 
        CS-328658 which can use this special runtime stream. 
        * Program counter Trace data from a Kalimba subsystem
        * Firmware log from any subsystem implements logging over TBus.
        
        These message types have very different formats and can be
        separated by the src block ID they appear to come from. 
        '''

        self._get_log()

        for transaction in self.sniffed_transactions:
            tr = DecodedTransaction(transaction, mrts=(self.dongle_name != "scarlet"))
            
            if TrbRawTransaction.opcode_names[tr.opcode] == "SAMPLE": 
            
                #Sort the stream types here.
                if tr.src_block_id == self.STREAM_SRC_BLOCK_ID_PC_TRACE:
                    payload = bytes_to_dwords_be(tr.payload[1:]) 
                    self.tr_trace_p0.append([tr.timestamp, payload[1]])
                    self.tr_trace_p0.append([tr.timestamp, payload[0]])
                    #iprint("Trace 0:")
                    
                elif tr.src_block_id == self.STREAM_SRC_BLOCK_ID_PC_TRACE_CPU_1:
                    payload = bytes_to_dwords_be(tr.payload[1:]) 
                    self.tr_trace_p1.append([tr.timestamp, payload[1]])
                    self.tr_trace_p1.append([tr.timestamp, payload[0]])
                    #iprint("Trace 1:")
                    tr = None
                    
                else:
                    tr = self.decode_transaction(transaction, app_decode=app_decode)     
                    #Only append to log when a complete entry is returned.    
                    if tr:
                        self.tr_log.append(tr)
            else:
                iprint("Other transactions: %s"%TrbRawTransaction.opcode_names[tr.opcode])

    def report_trace(self, logger=None):
        '''
        Return the trace log entries to the caller.
        Any other entries will be added to there store.
        '''
        
        #Get the latest content..
        self.sort()
        
        trace = [self.tr_trace_p0,self.tr_trace_p1]

        #Clear log transactions which have now been displayed.
        self.tr_trace_p0 = []
        self.tr_trace_p1 = []
        
        return trace       

    def decode_transaction(self, transaction, app_decode=True):
        '''
        Turn a raw transaction into a tuple of timestamp and string 
        representation using the high level interpretation of transactions
        if available.
        '''
        tr = DecodedTransaction(transaction, mrts=(self.dongle_name != "scarlet"))
        if app_decode and TrbRawTransaction.opcode_names[tr.opcode] == "SAMPLE":
            ss = self._chip.subsystems[tr.src_ss_id]
            core_num = 0 if tr.src_block_id == self.STREAM_SRC_BLOCK_ID_FIRMWARE_LOG else 1
            log_context = self._log_contexts.get(ss.cores[core_num].nicknames[0])

            if (tr.src_block_id == self.STREAM_SRC_BLOCK_ID_FIRMWARE_LOG or 
                tr.src_block_id == self.STREAM_SRC_BLOCK_ID_FIRMWARE_LOG_CPU_1):

                if log_context:
                    return self.process_fw_log_transaction(tr, log_context)
                else:
                    return None
            elif (tr.src_block_id == self.STREAM_SRC_BLOCK_ID_PUT_CHAR):
                if log_context:
                    return self.process_put_char(tr, log_context)
                else:
                    return None

        return tr.time_description_tuple()

    def clear(self):
        self.sniffed_transactions = []
        
    def process_fw_log_transaction(self, trans, ctx):
        SOP = 0x80
        EOP = 0x40
        LEN_MASK = 0xF
        flags = trans.payload[0]
        byte_len = flags & LEN_MASK
        payload_bytes = trans.payload[1:1+byte_len]
        if flags & SOP:
            ctx.pkt_list = []
            ctx.pkt_list_len = flags//2
            ctx.prev_timestamp = ctx.timestamp
            ctx.timestamp = trans.timestamp
        if ctx.word_based:
            payload = bytes_to_words_be(payload_bytes) 
        else:
            payload = bytes_to_dwords_be(payload_bytes) 
        ctx.pkt_list += payload
        if flags & EOP:
            event_list = ctx.decoder.decode(ctx.pkt_list, timestamp = False)
            if event_list:
                decoded_string = event_list[0]
            else:
                return
                
            if "putchar string" in decoded_string and ctx.char_list:
                # This needs suppressing because the putchar through 
                # SAMPLE transactions is passing the same information
                return
            
            return ctx.timestamp, decoded_string
            
    def process_put_char(self, trans, ctx):
        character = chr(trans.payload[0])
        if character == "\n":
            decoded_string = "".join(ctx.char_list)
            ctx.char_list = []
            extra = ctx.decoder.parse_debug_string(decoded_string, [])
            if extra:
                decoded_string = decoded_string + " (" + str(extra) + ")"
            decoded_string = ctx.decoder.apply_filters_and_colours(decoded_string)
            if decoded_string is not None: # not filtered out
                return trans.timestamp, decoded_string
        ctx.char_list += character
        
