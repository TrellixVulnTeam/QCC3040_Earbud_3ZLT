############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.bitsandbobs import PureVirtualError

"""
Basic support for accessing streams through the trap API
"""

from .system_message import SystemMessage as SysMsg

class Stream(object):
    """
    Generic stream.  Most functionality is added by the Source and Sink
    subclasses.
    """
        
    def __init__(self, id, utils, handler=None, drop_msgs=False):    
        """
        Create a Stream with the given ID and one of the following message
        handling options:
         - drop_msgs=True => messages will be dropped in the firmware
         - handler=None => messages will be received but not passed to a handler
         - handler=<callable> => messages will be received and passed to the
         supplied handler
        """
        self._utils = utils
        self._id = id
        if not drop_msgs:
            # Register to receive and handle messages via the message loop using
            # the supplied handler, if any
            self.receive_msgs()

    @property
    def id(self):
        """
        Return the underlying ID.
        """
        return self._id

    def drop_msgs(self):
        """
        Remove the handler for messages, if any
        """
        if hasattr(self, "_task"):
            if isinstance(self, Sink):
                self._utils.remove_sink_msg_task(self.id)
            else:
                self._utils.remove_source_msg_task(self.id)  

    def receive_msgs(self, handler=None):
        """
        Register a handler for messages, dropping the old one if any
        """
        self.drop_msgs()
        if isinstance(self, Sink):
            self._task = self._utils.handle_sink_msgs(self.id, handler=handler)
        else:
            self._task = self._utils.handle_source_msgs(self.id, handler=handler)

    def get_msg(self, msg_id, immediate=True):
        """
        Look for a message of the given ID posted to this Stream's handler. If
        immediate=True, don't wait for a message, just get what's sitting there,
        if anything
        """
        if immediate:
            raw = self._utils.get_core_msg(msg_id, task=self._task, timeout=None)
        else:
            raw = self._utils.get_core_msg(msg_id, task=self._task)
        if raw is False:
            return raw
        # If the message has a body, construct and return a variable of that type
        if msg_id in SysMsg.id_to_type:
            type_dict = self._utils.apps1.fw.env.types[SysMsg.id_to_type[msg_id]]
            return self._utils.build_var(type_dict, raw)
        # Otherwise just return True to indicate that the message was seen
        return True

class Sink(Stream):
    """
    Generic Sink object.  Allows simplified access to the basic trap API 
    functionality - SinkSlack, SinkClaim, SinkMap, etc.  Supports all-in-one
    writes.
    """
    CLAIM_FAILED = 0xffff
            
    def slack(self):
        """
        Call SinkSlack for this sink
        """
        return self._utils.call1.SinkSlack(self._id)
    
    def claim(self, extra):
        """
        Call SinkClaim for this sink.  Returns the amount previously claimed if
        successful, else self.CLAIM_FAILED (=0xffff)
        """
        return self._utils.call1.SinkClaim(self._id, extra)
    
    def map(self):
        """
        Map the sink into memory for writing.  The stream can be written to via
        the Apps P1 core's dm property.
        """
        return self._utils.call1.SinkMap(self._id)
    
    def flush(self, nbytes):
        """
        Call SinkFlush for this sink.
        """
        return self._utils.call1.SinkFlush(self._id, nbytes)
    
    def flush_header(self, nbytes, header_ptr, nheader_bytes):
        """
        Call SinkFlushHeader for this sink.
        """
        return self._utils.call1.SinkFlushHeader(self._id, nbytes, header_ptr, nheader_bytes)
    
    def write(self, data, header=None):
        """
        All-in-one write: check slack, claim, map, write, flush.  Returns False
        if the write fails. 
        """
        if self.slack() < len(data):
            return False
        
        if self.claim(len(data)) == 0xffff:
            return False
        
        ptr = self.map()
        if isinstance(data, str):
            data = [ord(c) for c in data]
        p1_ptr = self._utils.apps1.fw.call.xpmalloc_trace(len(data),0)
        if p1_ptr is None:
            raise RuntimeError("P1 memory not available for data in "
                               "sink write %s" % self._id)
        if header is not None:
            if isinstance(header, str):
                header = [ord(c) for c in header]            
            header_ptr = self._utils.call1.xpmalloc_trace(len(header), 0)
            if header_ptr is None:
                raise RuntimeError("P1 memory not available for header in sink "
                               "write %s" % self._id)
        
        self._utils.apps1.dm[p1_ptr:p1_ptr+len(data)] = data
        self._utils.apps1.fw.call.memmove(ptr, p1_ptr, len(data))
        
        if header is not None:
            self._utils.apps1.dm[header_ptr:header_ptr+len(header)] = header    
            result = self.flush_header(len(data), header_ptr, len(header))
            self._utils.call1.pfree(header_ptr)
        else: 
            result = self.flush(len(data))
            
        self._utils.apps1.fw.call.pfree(p1_ptr)
        return result

    def check_more_space(self, immediate=True):
        """
        Is there a MESSAGE_MORE_SPACE waiting for this Sink?
        """
        msg = self.get_msg(SysMsg.MORE_SPACE, immediate=immediate)
        if msg == False:
            # No message received
            return False
        # Received a MORE_SPACE message which really ought to be addressed to us
        if msg.sink.value != self._id:
            raise RuntimeError("Received message 0x%0x which is for sink 0x%x "
                               "but our id is 0x%x (our task is 0x%x)" % 
                               (msg.address, msg.sink.value, self._id, self._task))
        self._utils.free_var_mem(msg)
        return True

    def check_more_data(self, source_id=None, immediate=True):
        """
        Is there a MESSAGE_MORE_DATA waiting for the corresponding source? 
        """
        msg = self.get_msg(SysMsg.MORE_DATA, immediate=immediate)
        if msg == False:
            # No message received
            return msg
        # Received a MORE_DATA message
        assert(source_id is None or msg.source.value == source_id)
        self._utils.free_var_mem(msg)
        return True

    def configure(self, key, value):
        """
        Configure sink stream key with given value
        """
        return self._utils.call1.SinkConfigure(self._id, key, value)
    
class Source(Stream):
    """
    Generic Source object.  Allows simplified access to the basic trap API 
    functionality - SourceSize, SourceMap, etc.  Supports all-in-one
    reads.
    """
    
    def size(self, header=None):
        """
        Call SourceSize for this source
        """
        if header is None:
            return self._utils.call1.SourceSize(self._id)
        else:
            return self._utils.call1.SourceSizeHeader(self._id)

    def map(self, header=None):
        """
        Call SourceMap for this source
        """
        if header is None:
            return self._utils.call1.SourceMap(self._id)
        else:
            return self._utils.call1.SourceMapHeader(self._id)
    
    def drop(self, nbytes):
        """
        Call SourceDrop for this source
        """
        return self._utils.call1.SourceDrop(self._id, nbytes)
    
    def boundary(self):
        """
        Call SourceBoundary for this source
        """
        return self._utils.call1.SourceBoundary(self._id)
    
    def read(self, msg=True, as_string=False):
        """
        All-in-one read from the source.  Reads either the next message or as
        much data as is available, depending on whether msg is True or False
        """
        source_size = self.boundary() if msg else self.size()
        if source_size == 0:
            return []
        source_ptr = self.map()
        read = self._utils.apps1.dm[source_ptr:source_ptr+source_size]
        self.drop(len(read))
        if as_string:
            return "".join([chr(v) for v in read])
        return read

    def read_header(self, msg=True, as_string=False):
        """
        All-in-one read from the source.  Reads either the next message or as
        much data as is available, depending on whether msg is True or False
        """
        header_size = self.size(header=True)
        if header_size ==0:
            return []
        
        source_size = self.boundary() if msg else self.size()
        if source_size == 0:
            raise RuntimeError("Sourceboundary for %s returned 0 after SourceSizeHeader"
                                " returned valid size %s" % 
                               (self._id, header_size))
        
        
        header_ptr = self.map(header=True)
        header = self._utils.apps1.dm[header_ptr:header_ptr+header_size]
        if as_string:
            header = "".join([chr(v) for v in header])
        return header    

    def check_more_data(self, immediate=True):
        """
        Is there a MESSAGE_MORE_DATA waiting for this source 
        """
        msg = self.get_msg(SysMsg.MORE_DATA, immediate=immediate)
        if msg == False:
            # No message received
            return msg
        # Received a MORE_DATA message        
        if msg.source.value != self._id:
            raise RuntimeError("Received message 0x%0x which is for source 0x%x "
                               "but our id is 0x%x (our task is 0x%x)" % 
                               (msg.address, msg.source.value, self._id, self._task))
        self._utils.free_var_mem(msg)        
        return True
