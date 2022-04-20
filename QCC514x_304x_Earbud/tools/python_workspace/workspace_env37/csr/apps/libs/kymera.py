from collections import OrderedDict
import contextlib
import time

import logging

log = logging.getLogger("csr.apps.libs.kymera")
hdlr = logging.StreamHandler()
hdlr.setFormatter(logging.Formatter(fmt="%(msg)s"))
log.addHandler(hdlr)

"""
Library of Kymera-related functions and classes mainly focused on the 
construction and execution of operator graphs.
"""

# These are the capabilities in the CSRA68100 build 
# "kymera_1606171257_csra68100_sqif_d00_rel", hardcoded to avoid long load times
# of the Audio ELF.  The triple of integers is (capid, max_sources, max_sinks) 
DEFAULT_CAP_TABLE = {
 'CAP_ID_AEC_REFERENCE_1MIC': (0x00000040, 0x0000000d, 0x0000000c),
 'CAP_ID_AEC_REFERENCE_2MIC': (0x00000041, 0x0000000d, 0x0000000c),
 'CAP_ID_AEC_REFERENCE_3MIC': (0x00000042, 0x0000000d, 0x0000000c),
 'CAP_ID_AEC_REFERENCE_4MIC': (0x00000043, 0x0000000d, 0x0000000c),
 'CAP_ID_APTX_DECODER': (0x00000019, 0x00000002, 0x00000001),
 'CAP_ID_BASIC_PASS': (0x00000001, 0x00000008, 0x00000008),
 'CAP_ID_COMPANDER': (0x00000092, 0x00000008, 0x00000008),
 'CAP_ID_CVCHF1MIC_SEND_NB': (0x0000001c, 0x00000001, 0x00000002),
 'CAP_ID_CVCHF1MIC_SEND_WB': (0x0000001e, 0x00000001, 0x00000002),
 'CAP_ID_CVCHF2MIC_SEND_NB': (0x00000020, 0x00000001, 0x00000003),
 'CAP_ID_CVCHF2MIC_SEND_WB': (0x00000021, 0x00000001, 0x00000003),
 'CAP_ID_CVCHS1MIC_SEND_NB': (0x00000023, 0x00000001, 0x00000002),
 'CAP_ID_CVCHS1MIC_SEND_WB': (0x00000024, 0x00000001, 0x00000002),
 'CAP_ID_CVCHS2MIC_BINAURAL_SEND_NB': (0x00000027, 0x00000001, 0x00000003),
 'CAP_ID_CVCHS2MIC_BINAURAL_SEND_WB': (0x00000028, 0x00000001, 0x00000003),
 'CAP_ID_CVCHS2MIC_MONO_SEND_NB': (0x00000025, 0x00000001, 0x00000003),
 'CAP_ID_CVCHS2MIC_MONO_SEND_WB': (0x00000026, 0x00000001, 0x00000003),
 'CAP_ID_CVCSPKR1MIC_SEND_NB': (0x00000029, 0x00000001, 0x00000002),
 'CAP_ID_CVCSPKR1MIC_SEND_WB': (0x0000002a, 0x00000001, 0x00000002),
 'CAP_ID_CVCSPKR2MIC_SEND_NB': (0x0000002d, 0x00000001, 0x00000003),
 'CAP_ID_CVCSPKR2MIC_SEND_WB': (0x0000002e, 0x00000001, 0x00000003),
 'CAP_ID_CVCSPKR3MIC_SEND_NB': (0x00000044, 0x00000001, 0x00000004),
 'CAP_ID_CVCSPKR3MIC_SEND_WB': (0x00000045, 0x00000001, 0x00000004),
 'CAP_ID_CVCSPKR4MIC_SEND_NB': (0x00000046, 0x00000001, 0x00000005),
 'CAP_ID_CVCSPKR4MIC_SEND_WB': (0x00000047, 0x00000001, 0x00000005),
 'CAP_ID_CVC_RECEIVE_FE': (0x0000001b, 0x00000001, 0x00000001),
 'CAP_ID_CVC_RECEIVE_NB': (0x0000001d, 0x00000001, 0x00000001),
 'CAP_ID_CVC_RECEIVE_WB': (0x0000001f, 0x00000001, 0x00000001),
 'CAP_ID_DBE': (0x0000002f, 0x00000002, 0x00000002),
 'CAP_ID_DBE_FULLBAND': (0x00000090, 0x00000002, 0x00000002),
 'CAP_ID_DBE_FULLBAND_BASSOUT': (0x00000091, 0x00000002, 0x00000002),
 'CAP_ID_IIR_RESAMPLER': (0x00000094, 0x00000008, 0x00000008),
 'CAP_ID_MIXER': (0x0000000a, 0x00000006, 0x0000000c),
 'CAP_ID_PEQ': (0x00000049, 0x00000008, 0x00000008),
 'CAP_ID_SBC_DECODER': (0x00000016, 0x00000002, 0x00000001),
 'CAP_ID_SCO_RCV': (0x00000004, 0x00000001, 0x00000001),
 'CAP_ID_SCO_SEND': (0x00000003, 0x00000001, 0x00000001),
 'CAP_ID_SPDIF_DECODE': (0x00000036, 0x00000009, 0x0000000a),
 'CAP_ID_SPLITTER': (0x00000013, 0x00000010, 0x00000008),
 'CAP_ID_VOL_CTRL_VOL': (0x00000048, 0x00000008, 0x00000010),
 'CAP_ID_VSE': (0x0000004a, 0x00000002, 0x00000002),
 'CAP_ID_WBS_DEC': (0x00000006, 0x00000001, 0x00000001),
 'CAP_ID_WBS_ENC': (0x00000005, 0x00000001, 0x00000001),
 'CAP_ID_XOVER': (0x00000033, 0x00000010, 0x00000008)}

# Note: AAC and CELT decoders aren't in the firmware I got the above table
# from
DECODERS = (DEFAULT_CAP_TABLE["CAP_ID_SBC_DECODER"][0],
            DEFAULT_CAP_TABLE["CAP_ID_APTX_DECODER"][0],
            0x18, # AAC decoder
            0x9D) # CELT decoder

FILE_NONE = 0
FILE_ROOT = 1

def get_table_for_cap_id(tables, cap_id):
    """
    Search the array of capability table pointers "tables" for one matching
    the capability ID "cap_id".
    Note: this requires OperatorFrameworkEnable to have been called to power up
    the Audio SS
    """
    for table in tables:
        if table.deref.id.value == cap_id:
            return table.deref
    return None
    

class OperatorChannelError(ValueError):
    """
    Bad channel request
    """
    
class Operator(object):
    """
    Encapsulates a Kymera operator, providing access to:
     - the operator's ID
     - its input and output channels (as sinks/sources)
     - its messaging interface
     - the ability to start it
    """
    
    def __init__(self, cap_details, call):
        self._cap_id, self._max_outputs, self._max_inputs = cap_details
        self._op_id = call.OperatorCreate(self._cap_id, 0, 0)
        self._call = call

        # Set of currently-active channels
        self._active_inputs = []
        self._active_outputs = []
        self._inactive_outputs = []

    @property
    def cap_id(self):
        """
        The ID of the operator's capability
        """
        return self._cap_id
    
    @property
    def op_id(self):
        """
        The operator ID
        """
        return self._op_id

    @property
    def max_outputs(self):
        """
        The total possible number of outputs for this operator's capability
        """
        return self._max_outputs

    @property
    def max_inputs(self):
        """
        The total possible number of inputs for this operator's capability
        """
        return self._max_inputs

    @property
    def active_inputs(self):
        """
        Return the list of inputs that have data flowing into them
        """
        return self._active_inputs
    
    @property
    def active_outputs(self):
        """
        Return the list of outputs that have data flowing into and out of them
        """
        return self._active_outputs

    @property
    def unconnected_outputs(self):
        """
        Return the list of outputs that have data flowing into them but aren't
        yet connected to anything
        """
        return self._inactive_outputs
    
    def select_unconnected_inputs(self, num_to_select=2):
        """
        Find the first num_to_select unused input channels in the Operator
        """
        unconn_chans = [ichan for ichan in range(self.max_inputs) 
                                            if ichan not in self.active_inputs]
        if len(unconn_chans) < num_to_select:
            raise OperatorChannelError("%d unconnected inputs requested but "
                                       "only %d available" % (num_to_select, 
                                                          len(unconn_chans)))
        return unconn_chans[:num_to_select]

        
    def get_output_channel(self, channel_id=0):
        """
        Get the Source corresponding to the given operator output channel, 
        raising an exception if the channel ID is invalid
        """
        if channel_id >= self.max_outputs:
            raise OperatorChannelError("Illegal request for output channel %d. "
                                       "Operator 0x%x (capability 0x%x) can have "
                                       "up to %d output channels" % 
                                       (channel_id, self.op_id, self.cap_id,
                                        self.max_outputs))
        return self._call.StreamSourceFromOperatorTerminal(self.op_id, 
                                                           channel_id)
    
    def get_input_channel(self, channel_id=0):
        """
        Get the Sink corresponding to the given operator input channel, 
        raising an exception if the channel ID is invalid
        """
        if channel_id >= self.max_inputs:
            raise OperatorChannelError("Illegal request for input channel %d. "
                                       "Operator 0x%x (capability 0x%x) can have "
                                       "up to %d input channels" % 
                                       (channel_id, self.op_id, self.cap_id,
                                        self.max_inputs))
        return self._call.StreamSinkFromOperatorTerminal(self.op_id, 
                                                         channel_id)

    def connect_inputs(self, chans):
        """
        Record that a connection has occurred on the given input channels.  
        Performs various sanity checks that this is a reasonable thing to have
        done.
        Connecting inputs implies that certain output channels are now active.
        In the simplest case these are the same channels as the inputs, but for
        some operators it's more complicated.
        """
        if max(chans) > self.max_inputs:
            raise OperatorChannelError("Trying to connect channels that don't "
                                       "exist for this operator")
        
        if set(self._active_inputs).intersection(chans):
            raise RuntimeError("Trying to connect channels that are "
                               "already connected!")
            
        # Handle the cases where the output channels are not identical to the
        # input ones
        if self.cap_id == DEFAULT_CAP_TABLE["CAP_ID_MIXER"][0]:
            if not self._active_inputs:
                # The first stream to be added is the primary one.  We assume
                # that these are also the output channels. (I *think* it's
                # necessary that these are contiguous starting from 0, but 
                # we're not enforcing that here)
                self._inactive_outputs = chans
            self._active_inputs += chans
        elif self.cap_id == DEFAULT_CAP_TABLE["CAP_ID_SPLITTER"][0]:
            self._active_outputs = chans
            # Just expose the even channels until a connection is made
            self._inactive_outputs = [2*n for n in chans]
        elif self.cap_id in DECODERS:
            if chans != [0]:
                if len(chans) > 1:
                    raise OperatorChannelError("Trying to connect multiple "
                                               "channels of a stream decoder")
                else:
                    raise OperatorChannelError("Trying to connect channel %d of a "
                                           "stream decoder - only 0 expected")
            self._active_inputs = [0]
            self._inactive_outputs = [0,1]
        else:
            self._active_inputs = chans
            self._inactive_outputs = chans
            
    def connect_outputs(self, chans):
        """
        Record that a connection has occurred on the given output channels.
        Performs various sanity checks that this is a reasonable thing to have
        done.
        Connecting outputs affects the set of output channels: the ones that
        have now been connected (usually all of them) are no longer available
        for connection.  In the case of a splitter, the first connection grabs
        the even channels and the second grabs the odd channels
        """
        if not chans:
            return
        
        if max(chans) > self.max_outputs:
            raise OperatorChannelError("Trying to activate channels that don't "
                                       "exist for this operator")
        
        if not set(chans).issubset(set(self._inactive_outputs)):
            raise RuntimeError("Trying to connect output channels that aren't "
                               "listed as available!")
        
        self._active_outputs += chans
        if self.cap_id == 0x13 and chans[0] % 2 == 0: # Splitter
            # If we've just connected the even outputs, switch over to the odd
            # outputs
            self._inactive_outputs = [c+1 for c in self._inactive_outputs]
        else:
            self._inactive_outputs = [c for c in self._inactive_outputs 
                                      if not c in chans]
            # We don't normally expect any channels left over here
            if self._inactive_outputs:
                log.warn("Unconnected outputs after an operator connection!")

    def message(self, msg_id, msg_body=None, rsp_len=None):
        """
        Send the given message (an ID and an optional list of uint16s) to the 
        Operator, optionally
        retrieving a response of the given length.  If a response is required,
        this will be returned as a list of uint16s, otherwise the return value 
        is None
        """
        msg = [msg_id]
        if msg_body is not None:
            msg += msg_body
        with self._call.create_local("uint16", len(msg)) as msg_buf:
            for val, buf_entry in zip(msg, msg_buf):
                if val < 0:
                    # Negative values need encoding with 16-bit two's complement
                    val += 0x10000
                buf_entry.value = val
                
            if rsp_len is None:
                if not self._call.OperatorMessage(self.op_id, 
                                                  msg_buf, len(msg_buf), 
                                                  0, 0):
                    raise RuntimeError("Sending operator message failed")
            else:
                with self._call.create_local("uint16", rsp_len) as rsp_buf:
                    if not self._call.OperatorMessage(self.op_id, 
                                                      msg_buf, len(msg_buf),
                                                      rsp_buf, len(rsp_buf)):
                        raise RuntimeError("Sending operator message failed")
                    return [entry.value for entry in rsp_buf]
    
    # Compulsory message
                
    def get_capability_version(self):
        """
        Get the capability major and minor versions
        """
        return self.message(0x1000, rsp_len=2)
    
    # Standard messages (may or may not be supported by a given Operator - it's
    # up the user to know what they're doing)
    
    def enable_fade_out(self):
        self.message(0x2000)
    
    def disable_fade_out(self):
        self.message(0x2001)
    
    def set_control(self, control_id, value):
        self.message(0x2002, msg_body=[control_id, value])
    
    def get_params(self, offset, range):
        return self.message(0x2003, msg_body=[offset, range], rsp_len=range)
    
    def get_defaults(self, offset, range):
        return self.message(0x2004, msg_body=[offset, range], rsp_len=range)
    
    def set_params(self, offset, range, values):
        return self.message(0x2005, msg_body=[offset, range] + values)
    
    def get_status(self, num_status_vars):
        return self.message(0x2006, rsp_len=num_status_vars)
    
    def set_ucid(self, ucid):
        return self.message(0x2007, msg_body=[ucid])
    
    def get_logical_ps_id(self):
        return self.message(0x2008, rsp_len=1)[0]
    
    def set_buffer_size(self, size):
        return self.message(0x200C, msg_body=[size])
    
    def set_terminal_buffer_size(self, size, sinks_bitmap=0, sources_bitmap=0):
        self.message(0x200D, msg_body=[size, sinks_bitmap, sources_bitmap])
    
    def set_sample_rate(self, sample_rate):
        self.message(0x200E, msg_body=[sample_rate//25])
    
    def set_data_stream_based(set_stream_based=True):
        self.message(0x200F, msg_body=[1 if set_stream_based else 0])
                
    def start(self):
        """
        Start the operator running
        NB This isn't normally used: Operators are started by a bulk call to
        start all operators in a graph
        """
        with self._call.create_local("Operator",1) as op_array:
            op_array[0].value = self.op_id
            self._call.OperatorStartMultiple(1, op_array, 0)

    def stop(self):
        """
        Start the operator running
        """
        with self._call.create_local("Operator",1) as op_array:
            op_array[0].value = self.op_id
            self._call.OperatorStopMultiple(1, op_array, 0)


def connect_operators(source_op, sink_op, call, source_chans=None, 
                                                sink_chans=None):
    """
    Connect the output endpoint(s) of the source operator to the input 
    endpoint(s) of the sink operator, and call StreamConnect on them
    """
    # By default, try to connect all the channels that are currently active in
    # the source
    if source_chans is None:
        source_chans = source_op.unconnected_outputs
    # By default, try to connect the first n unconnected channels in the sink, 
    # where n is the number of source channels
    if sink_chans is None:
        sink_chans = sink_op.select_unconnected_inputs(len(source_chans))
    
    # The source channels requested should be a subset of the source channels
    # that are available but not yet connected
    source_available = source_op.unconnected_outputs
    requested_inactive = [n for n in source_chans if n not in source_available]
    if requested_inactive:
        raise OperatorConnectionError("Requested source channels (%s) not a "
                                      "subset of available source channels (%s)!" %
                                      (source_chans, source_available))
    
    # The number of sink channels requested should be no greater than the number
    # the capability supports
    if len(sink_chans) > sink_op.max_inputs:
        raise OperatorConnectionError("More downstream channels requested (%d) "
                                      "than the sink operator supports!")
    
    log.debug("Connecting channels %s of source to channels %s of sink" % 
              (source_chans, sink_chans))
    
    # Check that the numbers of inputs and outputs match
    n_source_outputs = len(source_chans)
    n_sink_inputs = len(sink_chans)
    if n_source_outputs != n_sink_inputs:
        raise OperatorConnectionError("Attempting to connect %d source "
                                      "outputs to %d sink inputs!" % 
                                      (n_source_outputs, n_sink_inputs))
        
    for src_chan, snk_chan in zip(source_chans, sink_chans):
        src = source_op.get_output_channel(src_chan)
        snk = sink_op.get_input_channel(snk_chan)
        if not call.StreamConnect(src, snk):
            raise OperatorConnectionError("Failed to connect source and sink"
                                          " for src channel %d, snk channel %d: "
                                          "src = 0x%x, snk = 0x%x" % 
                                          (src_chan, snk_chan, src, snk))
    # Record in the operators what connections have been made
    source_op.connect_outputs(source_chans)
    sink_op.connect_inputs(sink_chans)

def get_cap_table(audio0=None):
    if audio0 is not None and hasattr(audio0.fw, "env"):
        cap_table = {
             audio0.fw.env.enums["CAP_ID"][c.deref.id.value] : 
                    (c.deref.id, c.deref.max_sources, c.deref.max_sinks) 
                            for c in audio0.fw.gbl.capability_data_table}
    else:
        cap_table = DEFAULT_CAP_TABLE
        
    return cap_table



class OperatorConnectionError(RuntimeError):
    """
    The user is attempting to construct the OperatorChain wrongly
    """

class OperatorChainNameClash(ValueError):
    """
    The user has tried to add an operator to the chain with a name that is 
    already in use 
    """

class OperatorChain(object):
    """
    Linear chain of operators.  Supports step-by-step creation and start/stop.
    
    Chains aren't complete by themselves: they must be inserted into an 
    AudioGraph to have physical inputs and outputs attached.
    """
    def __init__(self, apps1, audio0=None):
        
        self._apps1 = apps1
        self._call = self._apps1.fw.call
        
        self._cap_table = get_cap_table(audio0)
        

        self._operator_list = OrderedDict()
        self._upstream_op = None
        
    
    def _connect_operators(self, source_op, sink_op, source_chans=None,
                           sink_chans=None):
        """
        Connect the given operators from left (upstream) to right (downstream)
        """
        connect_operators(source_op, sink_op, self._call,
                          source_chans=source_chans, sink_chans=sink_chans)
        

    def create_operator(self, cap_id):
        """
        Create an instance of the given capability.  If cap_id is a string it
        is looked up in the CAP_ID enum (with "CAP_ID_" prepended if required).
        """
        cap_details = None
        if isinstance(cap_id, str):
            # Map a named capability to the underlying ID using the firmware
            # enum
            if not cap_id.startswith("CAP_ID"):
                cap_id = "CAP_ID_%s" % cap_id
            cap_details = self._cap_table[cap_id]
        else:
            for id, max_src, max_snk in self._cap_table.values():
                if id == cap_id:
                    cap_details = (id, max_src, max_snk)
        if cap_details is None:
            raise ValueError("Capability ID 0x%x unknown" % cap_id)
        
        return Operator(cap_details, self._call)

    def add_operator(self, name, cap_id, source_chans=None,
                                         sink_chans=None):
        """
        Create an operator to connect to the downstream end of the current chain
        (if the chain is empty, just insert the operator unconnected 
        """
        if name in self._operator_list:
            raise OperatorChainNameClash("There is already an Operator named "
                                         "'%s'" % name)
        sink_op = self.create_operator(cap_id)
        self._call.log.log_variable(sink_op.op_id, name)

        if not self._operator_list:
            self._upstream_op = sink_op
            
        # Add the new operator to the list
        self._operator_list[name] = sink_op

        return sink_op

    def connect_internally(self):
        """
        Walk through the chain making the connections implied by the connectivity
        of the upstream end.
        """
        if not self._upstream_op.active_inputs:
            raise OperatorConnectionError("Trying to connect a chain internally "
                                          "before the upstream end has been "
                                          "connected")
        
        op_names = self._operator_list.keys()
        for i, op_name in enumerate(op_names):
            try:
                next_op_name = op_names[i+1]
            except IndexError:
                # Got to the end
                return
            op = self._operator_list[op_name]
            next_op = self._operator_list[next_op_name]
            log.debug("Connecting '%s' to '%s' via channels %s" % 
                            (op_name, next_op_name, op.unconnected_outputs))
            self._connect_operators(op, next_op, 
                                    source_chans=op.unconnected_outputs,
                                    sink_chans=op.unconnected_outputs)
            
    
    @property
    def op_list(self):
        """
        The set of Opertor objects in the chain, in downstream order
        """
        return self._operator_list.values()

    @property
    def us_op(self):
        """
        The operator at the upstream end of the chain
        """
        return self._upstream_op

    @property
    def ds_op(self):
        """
        The operator at the downstream end of the chain
        """
        ds_name, ds_op = self._operator_list.popitem()
        self._operator_list[ds_name] = ds_op
        return ds_op



class AudioGraphNameClash(ValueError):
    """
    The user has tried to add a graph component with a name that is already in
    use 
    """
    
class AudioGraphNameUnknown(ValueError):
    """
    The user has specified a non-existent component
    """

class AudioGraphIncomplete(RuntimeError):
    """
    We've encounted a node in the graph that isn't connected to anything
    """
    
class AudioGraph(object):
    """
    An AudioGraph is a directed graph of operator chains connected internally
    by mixers and splitters and externally to sources and sinks (e.g. audio 
    files, microphones, ADCs, DACs).
    
    An AudioGraph is constructed in strictly downstream order, with each
    new component specifying the upstream entity (source, operator chain,
    mixer or splitter) that forms its input (in the case of adding a mixer,
    up to 3 upstream entities may be specified).  OperatorChains that are to
    be added must be constructed separately, and they shouldn't be modified
    after they have been added to the graph.  Other components are constructed
    as they are added.
    
    The components of an AudioGraph are represented by a small dictionary of
    attributes which is referenced by a client-specified name, which must be
    unique (uniqueness is checked at creation time).  The standard attributes
    are:
    
         - "type" : string describing the type of attribute, either:
          -- "adc", "dac", "file", "mic" etc - these are collectively sources 
          or sinks
          -- "chain" - an OperatorChain (i.e. a linear sequence of Operators)
          -- "mixer" - a Operator instantiating the Mixer capability, potentially
          indicating a topological merging of chains
          -- "splitter" - an Operator instantiating the Splitter capability,
          potentially indicating a topological splitting of chains
         - "handle" : a type-specific reference to the object iself.  For
          -- sources/sinks this is a Source/Sink ID
          -- chains, mixers and splitters this is a Python handle to the object 
          (OperatorChain or Operator)
         - "us" : name of the graph component that is connected upstream of this
          component (in the case of a Mixer, this is a list of names).  For
          sources, this attribute is False; for all other components, it must
          contain a valid component name
         - "ds" : name of the graph component (if any) this is connected
         downstream of this component (in the case of a Splitter, this is a list
         of names).  For sinks, this attribute is False; for all other components
         it is None until a downstream connection is made, after which it contains
         the downstream component's name.  
    """
    
    SOURCES = ("adc", "file", "mic")
    SINKS = ("dac",)
    
    def __init__(self, apps1, audio0=None):
        
        self._apps1 = apps1
        self._call = self._apps1.fw.call

        # Fire up Kymera
        self._call.OperatorFrameworkEnable(1)

        self._audio_hardware = self._apps1.fw.env.enums["audio_hardware"]
        self.AUDIO_HARDWARE_CODEC = self._audio_hardware["AUDIO_HARDWARE_CODEC"]
        self._audio_instance  = self._apps1.fw.env.enums["audio_instance"]
        self.AUDIO_INSTANCE_0 = self._audio_instance["AUDIO_INSTANCE_0"]
        self.AUDIO_INSTANCE_1 = self._audio_instance["AUDIO_INSTANCE_1"]
        self._audio_channel = self._apps1.fw.env.enums["audio_channel"]
        self.AUDIO_CHANNEL_A = self._audio_channel["AUDIO_CHANNEL_A"]
        self.AUDIO_CHANNEL_B = self._audio_channel["AUDIO_CHANNEL_B"]
        self._stream_config_key = self._apps1.fw.env.enums["stream_config_key"]
        self.STREAM_CODEC_INPUT_RATE = self._stream_config_key["STREAM_CODEC_INPUT_RATE"]
        self.STREAM_CODEC_INPUT_GAIN = self._stream_config_key["STREAM_CODEC_INPUT_GAIN"]
        self.STREAM_CODEC_OUTPUT_RATE = self._stream_config_key["STREAM_CODEC_OUTPUT_RATE"]
        self.STREAM_CODEC_OUTPUT_GAIN = self._stream_config_key["STREAM_CODEC_OUTPUT_GAIN"]

        self._cap_table = get_cap_table(audio0)

        self._cmpts = {}
    
    def _add_cmpt(self, name, details):
        """
        Add a component with the given name, checking that there's not already a
        component with that name.
        """
        if name in self._cmpts:
            raise AudioGraphNameClash("Error adding '%s': component of that "
                                         "name already exists!" % name)
        self._cmpts[name] = details

    def _get_ds_facing_op(self, cmpt):
        """ 
        Get the downstream-facing operator for the component - either the 
        operator itself, or the downstream-end operator of a chain
        """
        if cmpt["type"] in self.SOURCES or cmpt["type"] in self.SINKS:
            raise ValueError("Wrong type of graph component for getting the "
                             "downstream-facing operator")
        if cmpt["type"] == "chain":
            return cmpt["handle"].ds_op
        return cmpt["handle"]

    
    def _set_downstream(self, input_dict, name):
        """
        Helper function to register the downstream connection of a component
        """
        if input_dict["type"] == "splitter":
            if input_dict["ds"] is not None and len(input_dict["ds"]) > 1:
                raise OperatorConnectionError("Attempt to duplicate downstream "
                                              "connection")
            if input_dict["ds"] is None:
                input_dict["ds"] = []
            input_dict["ds"].append(name)
        else:
            if input_dict["ds"] is not None:
                raise OperatorConnectionError("Attempt to duplicate downstream "
                                              "connection")
            input_dict["ds"] = name
        
    
    def _connect_operators(self, source, sink, source_chans=None, 
                           sink_chans=None):
        """
        Connect the given source and sink operators over the indicated channels
        (by default these are the source's unconnected_outputs)
        """
        return connect_operators(source, sink, self._call,
                                 source_chans=source_chans, 
                                 sink_chans=sink_chans)

    def _connect_source_to_operator(self, input_dict, ds, sink_chans=None):
        """
        Connect a source component to an operator by calling StreamConnect on
        the source's handles (which are Source IDs)
        """
        if input_dict["type"] == "adc":
            # Connect ADC(s) to the end of the chain
            adcs = input_dict["handle"]
            if sink_chans is None:
                sink_chans = range(len(adcs))
            elif len(sink_chans) != len(adcs):
                raise OperatorConnectionError("Trying to connect wrong "
                                              "number of operator inputs "
                                              "(%d) to %d-channel ADC" %
                                              (len(sink_chans), len(adc)))
            if ds.max_inputs < len(adcs):
                raise OperatorConnectionError("Can't connect operator with "
                                              "%d inputs to ADC with %d "
                                              "outputs!" % (ds.max_inputs,
                                                            len(adcs)))
            for i,adc in enumerate(adcs):
                if not self._call.StreamConnect(adc, 
                                        ds.get_input_channel(sink_chans[i])):
                    raise RuntimeError("Couldn't connect start of chain "
                                       "to ADC")
            ds.connect_inputs(sink_chans)
        elif input_dict["type"] == "file":
            if sink_chans is None:
                sink_chans = [0]
            elif len(sink_chans) != 1:
                raise OperatorConnectionError("Trying to connect wrong "
                                              "number of operator inputs "
                                              "(%d) to file source" % 
                                              len(sink_chans))
            if not self._call.StreamConnect(input_dict["handle"], 
                                            ds.get_input_channel(sink_chans[0])):
                raise RuntimeError("Couldn't connected start of chain "
                                   " to file '%s'" % input_dict["fname"])
            ds.connect_inputs(sink_chans)
        else:
            raise NotImplementedError("Don't know how to connect source type "
                                      "'%s'" % input_dict["type"])

    def _attach_operator(self, name, op, input_name):
        """
        Helper function to attach an operator representing the named downstream 
        component (e.g. the upstream end of an OperatorChain or a Mixer or 
        Splitter) to the named input.
        Note: this function doesn't add the downstream component, it just 
        performs generic work associated with connecting it to the graph.
        """
        input_dict = self.get_component_by_name(input_name)
        
        # The specified input should have an empty downstream connection, so 
        # check it does - the only exception is a splitter that has one connection
        if input_dict["ds"] is not None:
            if input_dict["ds"] is False:
                raise OperatorConnectionError("Attempting to add component '%s'"
                                              "downstream of an output ('%s')!" %
                                              (name, input_name))
            elif not (input_dict["type"] == "splitter" and 
                                                    len(input_dict["ds"]) < 2):
                raise OperatorConnectionError("Attempting to add commponent '%s'"
                                              " downstream of component '%s' which"
                                              " is already connected to '%s'" %
                                              (name, input_name, input_dict["ds"]))
        
        if input_dict["type"] in self.SOURCES:
            # Connect the source to the chain's upstream operator
            self._connect_source_to_operator(input_dict, op)
            
        else: # Input is an operator or operator chain
            # Get the input component's downstream-facing operator and connect 
            # to the supplied operator
            self._connect_operators(self._get_ds_facing_op(input_dict), op)

        self._set_downstream(input_dict, name)


    def get_component_by_name(self, name):
        try:
            return self._cmpts[name]
        except KeyError:
            raise AudioGraphNameUnknown("No graph component called '%s'" 
                                           % name)
    
    # -----------------------------------------------------------------
    # Source (input endpoint) creation
    #------------------------------------------------------------------
        
    def add_file_source(self, name, fname):
        """
        Create a source for the given file
        """
        log.debug("Adding file source '%s' for %s" % (name, fname))

        # Create a File Source for reading the RTP-encoded SBC.
        file_id = self._call.FileFind(FILE_ROOT, fname, len(fname))
        if file_id == FILE_NONE:
            raise ValueError("Couldn't find '%s' in the SQIF filesystem" % fname)
        
        file_source = self._call.StreamFileSource(file_id)
        self._call.log.log_variable(file_source,name)
        
        if file_source == 0:
            raise ValueError("File '%s' not found in filesystem" % fname)

        self._add_cmpt(name, {"type" : "file",
                             "handle" : file_source,
                             "us" : False,
                             "ds" : None,
                             "fname" : fname})
    
    def add_adc_source(self, name, audio_hw=None, 
                       audio_instance=None,
                       channels=None,
                       codec_rate=48000,
                       input_gain=10):
        """
        Set up source(s) for the chosen ADC, and configure them with a codec 
        rate and a gain.  By default this will create an AUDIO_HARDWARE_CODEC on
        AUDIO_INSTANCE_0 with two channels, AUDIO_CHANNEL_A and AUDIO_CHANNEL_B.
        """
        log.debug("Adding ADC '%s' " % name)

        if audio_hw is None:
            audio_hw = self.AUDIO_HARDWARE_CODEC
        if audio_instance is None:
            audio_instance = self.AUDIO_INSTANCE_0
        if channels is None:
            channels = [self.AUDIO_CHANNEL_A, self.AUDIO_CHANNEL_B]

        # Create and configure the ADC stream(s)
        adcs = []        
        for chan in channels:
            # Configure ADC channel
            adc = self._call.StreamAudioSource(audio_hw, audio_instance, chan)
            self._call.log.log_variable(adc, "%s#%d"%(name,chan))
            
            if not self._call.SourceConfigure(adc, self.STREAM_CODEC_INPUT_RATE, 
                                      codec_rate):
                raise RuntimeError("adc rate configuration failed")
            if not self._call.SourceConfigure(adc, self.STREAM_CODEC_INPUT_GAIN,
                                              input_gain):
                raise RuntimeError("adc gain configuration failed")
            adcs.append(adc)

        if len(adcs) == 2:
            # Synchronise the two channels 
            if not self._call.SourceSynchronise(adcs[0], adcs[1]):
                raise RuntimeError("SinkSynchronise failed")
    
        self._add_cmpt(name, {"type" : "adc",
                             "handle" : adcs,
                             "us" : False, 
                             "ds" : None})
    
    def add_mic_source(self, name, mic_source):
        """
        Create and configure a microphone source
        """
        raise NotImplementedError
    
        
    
    def add_chain(self, name, chain, input_name):
        """
        Add a pre-built OperatorChain to the graph at the given input
        """
        log.debug("Attaching chain '%s' downstream of '%s'" % (name, input_name))
        self._attach_operator(name, chain.us_op, input_name)
        chain.connect_internally()
        self._add_cmpt(name, {"type"  : "chain",
                              "handle" : chain,
                              "us" : input_name,
                              "ds" : None})


    def add_mixer(self, name, input_names, input_gains):
        """
        Add a mixer to the graph.
        
        @param input_names Names of the input components
        @param input_gains Gains on the input streams
        
        We loop over the input components connecting their n output channels to
        the next n input channels of the mixer.
        """
        log.debug("Attaching mixer '%s' downstream of %s" % (name, 
                                                       ", ".join(input_names)))

        mixer_op = Operator(self._cap_table["CAP_ID_MIXER"], self._call)
        self._call.log.log_variable(mixer_op, name)
        # Configure the mixer.  We must allocate channels before we connect
        # anything to it.
        msg_payload = [0]*3
        msg_payload[:len(input_gains)] = input_gains
        mixer_op.message(0x0001, msg_payload)
             
        # Count how many channels will be connected by each stream
        input_cmpts = [self.get_component_by_name(n) for n in input_names]
        input_num_chans = []
        for cmpt in input_cmpts:
            if cmpt["type"] in self.SOURCES:
                if cmpt["type"] == "adc":
                    input_num_chans.append(2)
                elif cmpt["type"] == "file":
                    input_num_chans.append(1)
            else:
                input_num_chans.append(
                       len(self._get_ds_facing_op(cmpt).unconnected_outputs))
        # Tell the mixer
        msg_payload = [0]*3
        msg_payload[:len(input_num_chans)] = input_num_chans
        mixer_op.message(0x0002, msg_payload)

        start_mixer_chan = 0
        for i,cmpt in enumerate(input_cmpts):
            
            log.debug("Adding input '%s' to mixer '%s'" % (input_names[i],
                                                           name))
            if cmpt["type"] in self.SOURCES:
                self._connect_source_to_operator(cmpt, mixer_op)

            else:
                # Join upstream operator to mixer on the appropriate mixer
                # channels
                self._connect_operators(self._get_ds_facing_op(cmpt), mixer_op)
                
            # Record the upstream operator's connection to the mixer
            self._set_downstream(cmpt, name)

        # Register the mixer in the graph
        self._add_cmpt(name, {"type":"mixer",
                              "handle":mixer_op,
                              "us":input_names,
                              "ds":None})

    # Create and add a splitter
    def add_splitter(self, name, input_name):
        """
        Create a Splitter, attach it to the given upstream component, and list
        it in the graph.
        """
        splitter_op = Operator(self._cap_table["CAP_ID_SPLITTER"], self._call)
        self._call.log.log_variable(splitter_op, name)
        log.debug("Attaching splitter '%s' downstream of '%s'" % (name, 
                                                                  input_name))
        self._attach_operator(name, splitter_op, input_name)
        self._add_cmpt(name, {"type" : "splitter",
                              "handle" : splitter_op,
                              "us" : input_name,
                              "ds" : None})
        
    
    # Create and add a sink
    def add_dac_sink(self, name, input_name, audio_hw=None, 
                     audio_instance=None,
                     channels=None,
                     codec_rate=48000,
                     output_gain=15):
        """
        Set up sink(s) for the chosen DAC, configure them with a codec rate and
        a gain, and wire them up the downstream end of the chain
        """
        
        log.debug("Attaching DAC '%s' downstream of '%s'" % (name, input_name))

        # Set up defaults
        if audio_hw is None:
            audio_hw = self.AUDIO_HARDWARE_CODEC
        if audio_instance is None:
            audio_instance = self.AUDIO_INSTANCE_0

        # Get the upstream operator
        input_dict = self.get_component_by_name(input_name)
        if input_dict["type"] in self.SOURCES:
            # Wrong: we can't connect sinks directly to sources
            raise OperatorConnectionError("Can't attach a source ('%s') "
                                          "directly to a DAC ('%s')" % 
                                          (input_name, name))
        us_op = self._get_ds_facing_op(input_dict)
        source_chans = us_op.unconnected_outputs
        self._set_downstream(input_dict, name)

        # If the DAC channels weren't explicitly specified, just take one or
        # both of AUDIO_CHANNEL_A and AUDIO_CHANNEL_B, as required by the number
        # of input channels
        if channels is None:
            channels = [self.AUDIO_CHANNEL_A, self.AUDIO_CHANNEL_B][:len(source_chans)]
        elif len(channels) != len(source_chans):
            raise OperatorConnectionError("%d DAC channels requested but %d "
                                          "upstream channels active!")

        
        log.info("Attaching DAC channels %s at sample rate %d" % (channels, 
                                                                   codec_rate))
        
        # Create and configure the DAC stream(s)
        dacs = []        
        for chan in channels:
            # Configure DAC channel 
            dac = self._call.StreamAudioSink(audio_hw, audio_instance, chan)
            self._call.log.log_variable(dac, "%s#%d"%(name,chan))
            if not self._call.SinkConfigure(dac, self.STREAM_CODEC_OUTPUT_RATE, 
                                            codec_rate):
                raise RuntimeError("dac rate configuration failed")
            if not self._call.SinkConfigure(dac, self.STREAM_CODEC_OUTPUT_GAIN, 
                                            output_gain):
                raise RuntimeError("dac gain configuration failed")
            dacs.append(dac)

        if len(dacs) == 2:
            # Synchronise the two channels 
            if not self._call.SinkSynchronise(dacs[0], dacs[1]):
                raise RuntimeError("SinkSynchronise failed")
    
        # Connect DACs to the operator at the bottom of the graph
        for i,dac in enumerate(dacs):
            if not self._call.StreamConnect(us_op.get_output_channel(
                                                             source_chans[i]), 
                                            dac):
                raise RuntimeError("Couldn't connected end of chain to DAC "
                                   "(channel %d)" % i)
        # Tell the upstream operator that its output channels are now 
        # connected to something
        us_op.connect_outputs(source_chans)

    @contextlib.contextmanager
    def op_id_array(self):
        """
        Create a self-freeing array of operator IDs in RAM from the values 
        stored here (used for invoking "OperatorXYZMultiple" traps)
        """
        
        operator_list = [op["handle"] for op in self._cmpts.values() 
                                        if op["type"] in ("mixer", "splitter")]
        for ch in [op["handle"] for op in self._cmpts.values()
                                  if op["type"] == "chain"]:
            operator_list += ch.op_list
                                      
        
        array = self._call.pnew("Operator", len(operator_list))
        try:
            for i, op in enumerate(operator_list):
                array[i].value = op.op_id
            yield array
        finally:
            self._call.pfree(array)

        
    def start(self):
        """
        Run the start method on all the operators in the graph
        """
        log.info("Starting graph")
        with self.op_id_array() as op_ids:
            if not self._call.OperatorStartMultiple(len(op_ids), op_ids, 0):
                return False
        return True

    def stop(self):
        """
        Run the stop method on all the operators in the graph
        """
        log.info("Stopping graph")
        with self.op_id_array() as op_ids:
            if not self._call.OperatorStopMultiple(len(op_ids), op_ids, 0):
                return False
        return True

    def reset(self):
        """
        Run the reset method on all the operators in the chain
        """
        log.info("Resetting graph")
        with self.op_id_array() as op_ids:
            if not self._call.OperatorResetMultiple(len(op_ids), op_ids, 0):
                return False
        return True

