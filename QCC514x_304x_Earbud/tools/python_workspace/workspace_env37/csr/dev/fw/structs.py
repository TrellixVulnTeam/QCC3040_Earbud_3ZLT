############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import gstrm
from csr.wheels.bitsandbobs import PureVirtualError
from csr.dev.env.env_helpers import _Structure, var_typename, var_address
from csr.dev.hw.mmu import MMUHandleRecord
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
import sys

class StructWrapper(object):
    """
    Base class for Python objects that wrap C structures. This can be used
    for lots of purposes not just within the structs module. The primary job
    of this class is to give the large design guideline on the c_struct()
    method.
    """
    def __init__(self, struct):
        self._st = struct

    def c_struct(self):
        """
        The underlying C structure for this link.

        Avoid using this if at all possible. Not all objects for a given
        logical entity are built on top of the same C structure (historically
        there has been more than one C structure for a given logical entity
        even in an individual firmware version). This method should be used
        only by presentation routines that want to display information to the
        user without interpreting it or at the PyDbg command line when the
        user has exhausted the automated analysis.

        If you need stuff out of here programmatically then add a new API
        to this object.
        """
        return self._st

class IStructHandler(object):
    """
    Interface for types which extend _Structure to provide custom handling.
    This class and its subclasses are used to organise the hierarchy of
    HandledStructures into levels of genericity corresponding to the Hydra
    architecture: a given HandledStructure subclass also inherits from a 
    specific IStructHandler subclass such as IHydraStructHandler or 
    ICuratorStructHandler depending on whether the HandledStructure is a
    generic Hydra type or a Curator-specific type. This allows clients to
    control where they start in the hierarchy when searching for a handler for
    a given type name.  E.g. CuratorFirmware will look up struct handling types 
    using ICuratorStructHandler.handler_factory(), which means it will see
    everything that inherits directly from ICuratorStructHandler, 
    IHydraStructHandler and IStructHandler, but won't see IAppsStructHandler's
    subclasses. 
    """
    @classmethod
    def handler_factory(cls, type_name):
        """
        Generic factory method for finding a class that handles the given type
        as close to the given class "cls" as possible.
        
        E.g. if there is a subclass of IHydraStructHandler that handles the type 
        "BUFFER" but no subclass of AppsStructHandler that does so,  
        """
        
        # Does any subclass of this class directly handle the requested type?
        for subclass in cls.__subclasses__():
            try:
                if type_name in subclass.handles():
                    return subclass
            except PureVirtualError:
                # There may be abstract classes amongst cls's subclasses
                pass
        # If not, does any parent class have an immediate subclass which handles 
        # it?
        for superclass in cls.__bases__:
            if issubclass(superclass, IStructHandler):
                return superclass.handler_factory(type_name)


class HandledStructure(StructWrapper):
    """
    Abstract base extending the _Structure type to provide handling information
    """
    def __init__(self, core, struct):
        super(HandledStructure, self).__init__(struct)
        if var_typename(struct) not in self.handles():
            raise TypeError("HandledStructure for '%s' passed a '%s'!" %
                            (self.handles(), var_typename(struct)))
        self._core = core
        self._mmu = self._core.subsystem.mmu


    @staticmethod
    def handles():
        """
        Indicates which type the IStructHandler handles
        """
        raise PureVirtualError
    
    @staticmethod
    def requires():
        """
        Indicates which type(s) the IStructHandler requires to have their 
        handlers installed
        """
        return []
    
 
class IHydraStructHandler(HandledStructure, IStructHandler):
    """
    Base type for StructHandlers that correspond to generic Hydra
    types
    """
    pass 

    
    
class HydraBuffer(IHydraStructHandler, HandledStructure):
    """
    Handler designed for _Structures of type BUFFER
    """

    # IStructHandler compliance
    
    @staticmethod
    def handles():
        return ("BUFFER",)
        
    @property
    def index(self):
        """
        The BUFFER's index
        """
        return self._st.index.value

    # Extensions

    @property
    def outdex(self):
        """
        The BUFFER's outdex
        """
        return self._st.outdex.value

    @property
    def tail(self):
        """
        The BUFFER's tail
        """
        return self._st.tail.value

    @property
    def handle_index(self):
        """
        The local part of the BUFFER's underlying MMU handle
        """
        if isinstance(self._st.handle, _Structure):
            return self._st.handle.index.value
        return self._st.handle.value & 0xff

    @property
    def size_mask(self):
        """
        The BUFFER's size_mask field
        """
        return self._st.size_mask.value

    def add(self, index, offset):
        """
        Add an offset to the given buffer index, wrapping if necessary
        """
        return (index + offset) & self.size_mask

    def sub(self, index, offset):
        """
        Subtract an offset from the given buffer index, wrapping if necessary
        """
        while offset > index:
            index += self.size_mask + 1
        return (index - offset) & self.size_mask

    def older(self, index1, index2):
        """
        Is index1 logically older than index2 (note - this doesn't take account
        of page mapping)
        """
        offset = self.sub(self.size_mask, self.index)
        return self.add(index1, offset) < self.add(index2, offset)

    @property
    def mapped_range(self):
        """
        The range of indices that are backed by pages (returns a start, end
        tuple where start is guaranteed to be older than end)
        """
        return self.pagedBuffer.mapped_range
        
    @property
    def pagedBuffer(self):
        """
        Access an MMUPagedBuffer representing the data in this buffer
        """
        try:
            self._pagedBuffer
        except AttributeError:
            self._pagedBuffer = (self._mmu.handleTable[self.handle_index].
                                                        pageTable.pagedBuffer)
        return self._pagedBuffer

    @property
    def unread(self):
        return self.pagedBuffer[self.outdex:self.index]

    @property
    def read(self):
        return self.pagedBuffer[self.tail:self.outdex]

    @property
    def cleared_and_mapped(self):
        """
        Compute the region that is older than tail and is still mapped in.
        """
        start_mapped, end_mapped = self.mapped_range
        if start_mapped is None:
            # There were no pages mapped in
            return []

        # If the buffer is fully mapped we can't rely on start being the oldest
        # mapped byte, but in this case the index will point to it.
        if start_mapped == 0 and end_mapped == self.get_size:
            return self.pagedBuffer[self.index:self.get_size] + \
                   self.pagedBuffer[0:self.index]

        # If the buffer is not fully mapped then start points to the oldest
        # mapped byte, there must be a gap between end and start and in this
        # case start is guaranteed to be older than end.
        return self.pagedBuffer[start_mapped:self.tail]

    @property
    def get_size(self):
        '''
        Return buffer size
        '''
        return self.size_mask + 1 
    
    @property
    def get_used(self):
        '''
        Return number of octets that has been written into the buffer without
        being cleared
        '''
        return (self.index - self.tail) & self.size_mask
    
    @property
    def get_unfreed(self):
        '''
        Return octets that have been passed for further processing
        but not freed.
        '''
        return (self.outdex - self.tail) & self.size_mask
        
    @property
    def get_freespace(self):
        '''
        Return octets of free space available in a buffer.
        '''
        if self.get_size == self._mmu.page_size:
            tail = self.tail
        else:
            tail = self.tail & ~(self._mmu.page_size - 1)
            
        return self.get_size - ((self.index - tail) & self.size_mask) - 1
    
    @property
    def get_available(self):
        '''
        Return octets available to be read from a buffer.
        '''
        return (self.index - self.outdex) & self.size_mask
 
    def set_decoder(self, decoder):
        self._decode = decoder
 
    def display(self, report=False):
        """
        Display a Buffer
        """
        try:
            ostr = self._st.display(""," |", [], [])
            ostr += ["---------------------------------------------------------"]
            ostr += [" Cleared data still mapped: {} bytes".format(len(self.cleared_and_mapped))]
            ostr += ["---------------------------------------------------------"]
            ostr += self._decode(self.cleared_and_mapped)
            ostr += ["---------------------------------------------------------"]
            ostr += [" Uncleared data (oldest first): {} bytes".format(len(self.read))]
            ostr += ["---------------------------------------------------------"]
            ostr += self._decode(self.read)
            ostr += ["---------------------------------------------------------"]
            ostr += [" Unread data (oldest first): {} bytes".format(len(self.unread))]
            ostr += ["---------------------------------------------------------"]
            ostr += self._decode(self.unread)
        except MMUHandleRecord.NotConfiguredError:
            pass

        code = interface.Code("\n".join(ostr))
        if report:
            return code
        return TextAdaptor(code, sys.stdout)

class HydraBufferMsg(IHydraStructHandler, HandledStructure):
    """
    Handler designed for _Structures of type BUFFER_MSG .
    
    Relies on the underlying BUFFER being handled by a Buffer class (see above)
    rather than just a plain _Structure.
    """
    
    _decoders = {}
    
    # IStructHandler compliance
    
    @staticmethod
    def handles():
        return ("BUFFER_MSG",)
    
    @staticmethod
    def requires():
        return ["BUFFER"]

    
    # Extensions

    def msg_xrange(self, start_msg, end_msg):
        """
        Generator of message indices in the given range (like normal range, but
        takes message wrapping into account)
        """
        if start_msg <= end_msg:
            for imsg in range(start_msg, end_msg):
                yield imsg
        else:
            for imsg in range(start_msg, self._st.msg_lengths.num_elements):
                yield imsg
            for imsg in range(0, end_msg):
                yield imsg
    
    @property
    def buf(self):
        """
        Access to the underlying BUFFER (assumed to be represented as a Buffer) 
        """
        try:
            self._buf
        except AttributeError:
            self._buf = IAppsStructHandler.handler_factory("BUFFER")(self._core,
                                                                     self._st.buf)
        return self._buf
    
    @property
    def front(self):
        """
        The BUFFER_MSG's front message index
        """
        return self._st.front.value
    
    @property
    def back(self):
        """
        The BUFFER_MSG's back message index
        """
        return self._st.back.value
    
    @property
    def behind(self):
        """
        The BUFFER_MSG's behind message index
        """
        return self._st.behind.value
    
    @property
    def meta(self):
        """
        The BUFFER_MSG's meta data pointer
        """
        return self._st.meta.value

    @property
    def msg_lengths(self):
        """
        The BUFFER_MSG's msg_lengths array.  Note that the elements of this array
        at _Integer objects, so must be accessed using their "value" member, e.g.
        bufmsg.msg_lengths[bufmsg.back]
        """
        return self._st.msg_lengths
        
    @property
    def msg_capacity(self):
        """
        Total number of messages this BUFFER_MSG supports
        """
        return self.msg_lengths.num_elements
        
    def add(self, msg, n):
        """
        Increment the given message index by the given amount, taking wrapping
        into account
        """
        return (msg + n) % self.msg_capacity
    
    def sub(self, msg, n):
        """
        Decrement the given message index by the given amount, taking wrapping
        into account
        """
        while msg < n:
            msg += self.msg_capacity
        return (msg - n) % self.msg_capacity
        
    def older(self, msg1, msg2):
        """
        Is msg1 older or newer than msg2
        """
        # The newest message is front.  Everything is in age order if 
        # front=msg_capacity-1, so adjust and compare
        offset = self.sub(self.msg_capacity-1, self.front)
        return self.add(msg1, offset) < self.add(msg2, offset)
        
    def _get_msgs(self, start_index, start_msg, end_msg):
        """
        Return a list of raw messages (lists of bytes) corresponding to the range
        [start_msg, end_msg) of message indices, where start_msg starts at
        start_index in the underlying BUFFER.  The messages don't need to be
        within the range [behind, front), but they do (of course) need to be
        completely backed by MMU pages.
        """
        msgs = []
        for imsg in self.msg_xrange(start_msg, end_msg):
            length = self.msg_lengths[imsg].value
            msgs.append(self.buf.pagedBuffer[start_index:start_index+length])
            start_index = self.buf.add(start_index, length)
        return msgs
        
    def _display_msgs(self, start_entry, start_index, msgs):
        """
        Invoke the decoder on the supplied set of messages.
        
        Note: the messages must be contiguous and the first message must start
        at start_index (for multiple non-contiguous messages this function should
        be called multiple times)
        """
        ostr = []
        for msg in msgs:
            try:
                addr = var_address(self._st)
                decoder = self._decoders[addr] 
            except KeyError:
                # Fall through to the default decoder
                decoder = self._decode_msg
            ostr += ["---------------------------------------------------------"]
            ostr += decoder(self._st, start_entry, start_index, msg)
            ostr += ["---------------------------------------------------------"]
            start_index = self.buf.add(start_index, len(msg))
            start_entry = (start_entry + 1) % len(self._st.msg_lengths)
        return ostr


    def _get_cleared_but_still_mapped(self):
        """
        Compute the region that is older than tail and is still mapped in.  Note
        that in the case of statically mapped or nearly full buffers it is 
        possible that this region is corrupt because the firmware or hardware is
        in the middle of writing a message but the index has not yet been
        updated.  It's not obvious how to avoid this, so the user should have 
        his wits about him. (In the case of dynamically mapped buffers which 
        happen at the time of access to have at least one unmapped page this 
        can't happen because the unmapped gap provides a clear demarcation 
        between old and new values.)  
        """
        start_mapped, end_mapped = self.buf.mapped_range
        if start_mapped is None:
            # There were no pages mapped in
            return self.buf.tail, self.behind

        fully_mapped = start_mapped == 0 and end_mapped == self.buf.get_size
        
        # Walk backwards through the still-mapped region until either we "go off
        # the beginning" or we loop right back round to front.
        
        # Start with the message one behind behind
        mbehind = self.sub(self.behind, 1)
        mtail = self.buf.sub(self.buf.tail, 
                                     self.msg_lengths[mbehind].value)
        # If we're still within the mapped region and...
        while ((fully_mapped or not self.buf.older(mtail, start_mapped)) and
               # ... we haven't walked back into overwritten data and...
               self.buf.older(mtail, self.buf.tail) and
                # ... we haven't walked all the way back to front and...
                mbehind != self.front and
                # ... we haven't walked into untouched territory, then...
                self.msg_lengths[mbehind].value != 0):
            # ...step back to the previous message
            mbehind = self.sub(mbehind, 1)
            mtail = self.buf.sub(mtail, self.msg_lengths[mbehind].value)

        # The loop above exits when we step back to a message that has been 
        # at least partly lost.  So step forward by one.
        mtail = self.buf.add(mtail, self.msg_lengths[mbehind].value)
        mbehind = self.add(mbehind, 1)
        
        return mtail, mbehind

    @property
    def unread_msgs(self):
        """
        Return all the messages that haven't yet been marked as consumed
        """
        return self.back, self.buf.outdex, self._get_msgs(self.buf.outdex, 
                                                          self.back, self.front)

    @property
    def read_msgs(self):
        """
        Return all the messages that have been consumed but haven't been marked
        as freed
        """
        return self.behind, self.buf.tail, self._get_msgs(self.buf.tail, 
                                                      self.behind, self.back)
        
    @property
    def still_mapped_msgs(self):
        """
        Return all the messages that have been marked as freed but are still 
        backed by a live MMU page
        """
        mtail, mbehind = self._get_cleared_but_still_mapped()
        return mbehind, mtail, self._get_msgs(mtail, mbehind, self.behind)

    def set_msg_decoder(self, decoder):
        """
        Register a callable to be invoked for message decode taking 
        a BufferMsg, message ring entry number, index and 
        raw message byte array 
        """
        # Register this decoder for all BufferMsg objects that get created to
        # represent the BUFFER_MSG at the same address
        addr = var_address(self._st)
        self._decoders[addr] = decoder

    def display(self, report=False):
        """
        Display a BufferMsg
        """
        try:
            ostr = self._st.display(""," |", [], [])
            
            unread_start_entry, unread_start, unread = self.unread_msgs
            read_start_entry, read_start, read = self.read_msgs
            still_mapped_start_entry, still_mapped_start, still_mapped = self.still_mapped_msgs
    
            ostr += [" Cleared messages still mapped: %d" % len(still_mapped)]     
            ostr += self._display_msgs(still_mapped_start_entry, still_mapped_start, still_mapped)
            ostr += [" Uncleared messages (oldest first): %d" % len(read)]
            ostr += self._display_msgs(read_start_entry, read_start, read)
            ostr += [" Unread messages (oldest first): %d" % len(unread)]
            ostr += self._display_msgs(unread_start_entry, unread_start, unread)
                
        except MMUHandleRecord.NotConfiguredError:
            pass
        
        code = interface.Code("\n".join(ostr))
        if report:
            return code
        return TextAdaptor(code, gstrm.iout)
    
    def _decode_msg(self, start_index, msg):
        """
        Default decoder: just formats the raw bytes so that they are aligned to
        16-byte boundaries
        """
        start_bdy = (start_index / 16) * 16
        imsg = 0
        ostr = []
        while imsg < len(msg):
            line = "  %04x : " % start_bdy
            for iprint in range(start_bdy, start_bdy + 16):
                if iprint < start_index:
                    line += "   "
                else:
                    line += " %02x" % msg[imsg]
                    imsg += 1
                    if imsg >= len(msg):
                        break
            start_bdy += 16
            ostr.append(line)
        return ostr
    
class IAppsStructHandler(IHydraStructHandler):
    """
    Base type for StructHandlers that are specific to the Apps subsystem
    """
    pass


class RfcMuxInfoT(IAppsStructHandler, HandledStructure):
    """
    Structure handler for RFC_MUX_INFO_T
    """
    @staticmethod
    def handles():
        return ("RFC_MUX_INFO_T",)

    def _display(self, name, prefix=""):
        
        ostr = []
        for name, member in self._st.member_list:
            if name not in ("p_dlcs",):
                ostr += member.display(name, prefix+"   |", [],[])
            elif name == "p_dlcs":
                # This is a pointer to a linked list of RFC_CHAN_T
                handler_type = IAppsStructHandler.handler_factory(
                                                    var_typename(member.deref))
                ostr += handler_type(self._core,
                                    member.deref)._display(name=name,
                                                           is_mux=False,
                                                           prefix=prefix+"   |")
                    
        return ostr
        
    def display(self, name, prefix="", report=False):
        ostr = self._display(name, prefix=prefix)
        code = interface.Code("\n".join(ostr))
        if report:
            return code
        return TextAdaptor(code, gstrm.iout)


class RfcChanT(IAppsStructHandler, HandledStructure):
    """
    Structure handler for RFC_CHAN_T
    """
    @staticmethod
    def handles():
        return ("RFC_CHAN_T", "struct RFC_CHAN_T_tag")
    
    def _display(self, name, is_mux=True, prefix=""):
        
        ostr = []
        next_struct = self._st
        list_index = 0
        while next_struct:
            ostr += ["%s-%s %s[%d]" % (prefix, var_typename(next_struct),
                                      name, list_index)]
            for mname, member in next_struct.member_list:
                if mname not in ("p_next", "timers", "info"):
                    ostr += member.display(mname, prefix+"   |", [],[])
                elif mname == "p_next":
                    next_struct = None if member.value == 0 else member.deref
                    list_index += 1
                elif mname == "info":
                    if is_mux:
                        member = member.mux
                        mname = "%s.mux" % mname
                        ostr += IAppsStructHandler.handler_factory(
                                   var_typename(member))(self._core,
                                            member)._display(name=mname,
                                                             prefix=prefix+"   |")
                    else:
                        member = member.dlc
                        mname = "%s.dlc" % mname
                        # Use the native display method this time
                        ostr += member.display(mname, prefix+"   |", [],[])
        return ostr
        
    def display(self, name, prefix="", report=False):
        ostr = self._display(name, prefix=prefix)
        code = interface.Code("\n".join(ostr))
        if report:
            return code
        return TextAdaptor(code, gstrm.iout)
    
class RfcServChanT(IAppsStructHandler, HandledStructure):
    
    @staticmethod
    def handles():
        return ("RFC_SERV_CHAN_T", "struct RFC_SERV_CHAN_T_tag")
    
    def _display(self, name, prefix=""):
        ostr = []
        next_struct = self._st
        list_index = 0
        while next_struct:
            ostr += ["%s-%s %s[%d]" % (prefix, var_typename(next_struct),
                                      name, list_index)]
            for mname, member in next_struct.member_list:
                if mname not in ("p_next",):
                    ostr += member.display(mname, prefix+"   |", [],[])
                elif mname == "p_next":
                    next_struct = None if member.value == 0 else member.deref
                    list_index += 1
        return ostr
        
    def display(self, name, prefix="", report=False):
        ostr = self._display(name, prefix=prefix)
        code = interface.Code("\n".join(ostr))
        if report:
            return code
        return TextAdaptor(code, gstrm.iout)


class RfcCtrlT(IAppsStructHandler, HandledStructure):
    """
    RFCOMM's RFC_CTRL_T is a bit of a nightmare for the generic decoder with
    an implicitly discriminated union...
    """
    
    @staticmethod
    def handles():
        return ("RFC_CTRL_T", "struct RFC_CTRL_T_tag")
    
    def _display(self):
        ostr = []
        if self._st.member_list:
            for name, member in self._st.member_list:
                if name not in ("mux_list", "serv_chan_list"):
                    ostr += member.display(name, "   |", [],[])
                else:
                    if member._py_valid:
                        ostr += IAppsStructHandler.handler_factory(
                               var_typename(member.deref))(self._core,
                                                member.deref)._display(name=name,
                                                                       prefix="   |")
        return ostr
    
    def display(self, report=False):
        ostr = self._display()
        code = interface.Code("\n".join(ostr))
        if report:
            return code
        return TextAdaptor(code, gstrm.iout)


class ICuratorStructHandler(IHydraStructHandler):
    """
    Base type for StructHandlers that are specific to the Curator subsystem
    """
    pass
