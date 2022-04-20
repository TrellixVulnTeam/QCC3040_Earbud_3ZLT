############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Device memory access network model

Overview
-------- 

For each Device instance an explicit, directed, hierarchical, memory access
network is defined that models the physical memory access network by connecting
significant memory components together via memory access ports.

The network starts at the edge of the whole device (e.g. SPI & TRB ports on the
PCB) and works inwards, via published ports, into the main Chip, possibly via
LPCs - and in several cases back out again (e.g. via LPC links).

This model is used to virtually map all access requests "inwards" to their
canonical address/state space, and then, if the data can not be served from a
cache, the request is routed "outwards" along a debug access path (see below)
to be served by a physical debug connection (SPI/TRB).

Example model
-------------

Example access model showing various access paths to the subsystem register 
map starting at the edge of a device/pcb:-

                           spi_connection                    trb connection
                                 |                                 |
                                 v                                 |
                           lpc bypass switch <==> lpc slot 0       |
                                 |                                 |
                                 v                                 v
         tool cmd           spi chip mux                      trb chip map
             |                   |                                 |*   
             v                   v                                 v   
         cpu data map        ss spi map                        trb ss map
             |*                  |*                                |*
             v                   v                                 v 
          [                         ss reg map                        ]

Schema
------

Each significant component in the memory access network is modelled by a
python object (e.g. GenericWindow) and publishes one or more AddressMasterPorts
and/or AddressSlavePorts.

Each AddressMaster-Slave interconnection is modelled by an AddressConnection
object.

More than one AddressMasterPort may be connected to the same AddressSlavePort 
modelling multi-ported memory.

Older notes that need revising
------------------------------

# Common components include:- - maps - which offset and route accesses based on
# address range. - muxes - which route accesses based on some control state
# (register or other condition).
#
# Known uses:-
#
# - Automation of debug access routing:- 
#
# When a memory access request is made (e.g. to write a subsystem register) an
# access path is selected (based on best deadline) and the request is logically
# mapped _backwards_ from the target space along the chosen access path,
# reserving and configuring routing resources, until it reaches a component
# (e.g SPI, TRB or even toolcmd connection) that can physically initiate the
# request.
#
# Setting up the route typically triggers auxilliary access requests, (e.g.
# reprogramming the hydra glb spi mux register). These requests are processed
# in exactly the same way.
#
# Each intermediate component is responsible for reverse mapping the request to
# its AddressSlave ports. In a few cases the mapping is non trivial as word
# sizes may differ, wide requests may need to be squeezed through narrow
# windows etc.
#
# In the important case of programable muxes, they shall inject any additional
# access requests to set their physical state appropriately according to which
# of its (Addresser) ports received the logical request and must physically
# service it.

Routing Resource Contention
---------------------------

This must all interact with fine-grained resource locking to prevent conflicts
over resources in the path 'tween SPI, TRB - and especially the CPUs.

Optimisation
------------ 

As an optimisation to avoid evaluating all possible access paths on each access
request an "accessibility" graph is propagated through, and overlayed upon the
static memory model whenever a debug connection (e.g SPI or TRB) becomes
available. This means that when access to a specific AddressSpace is requested
the available access paths to that space are already cached.

The AddressSpace "layer" is not flat 
------------------------------------

Some AddressSpaces may not be accessible directly via the available debug
connections.

In order to route access requests for on-chip state intelligently we also model
many of the mappings between these AddressSpaces.

This modelling could be implicit in the connection drivers but by making it
explicit we can unify and automate the access routing logic. (As well as reuse
the models elsewhere)

For example GenericWindows are vital to routing some accesses via SPI and
resolving CPU-relative accesses to GenericWindow regions for both SPI and TRB.
       
Access Path Selection
---------------------

Where more than one access path exists to the implied state the path with
fastest completion-time is selected .

Active connections decorate the address model with their access graph,
registering a node with each AddressSpace they can reach.

When an AddressSpace is accessible in more than one way there will be multiple
AccessPathNodes registered with it. These nodes provide rough estimates of an
access's completion time (e.g. allowing for latency + data size / data rate) -
the addressSpace just has to select the best one to service an access request.

Path Indirection 
----------------

In some cases there is no direct debug access to a nominal Address (consider
GenericWindow in CPU space). In this case the request is mapped, possibly
recursively, through any muxes/switches, until an AddressSpace is found with at
least one available access path to it (e.g. via the SPI GenericWindow).

Strategy for CoreDumps
---------------------- 

Existing XDC coredumps are very SPI-centric. They are approximately programs
for reloading a devices' state via SPI address-space.

By executing the spi program wrt the a model's spi data space the model will
automatically route all write requests to their canonical address spaces (just
as they would on a real device).

A design goal is to avoid special provisions in the logical model for different
underlying device implementations - however a pragmatic exception is made for
core dumps - access requests support a "force-write" option to enable the state
of read only registers to be re-loaded.

WIP: cache model to support this.

WIP
--------------
Step 1...

- Kill visitor system :) - DONE

- Remove "connection" from AddressSpace 'structors' - DONE

- Have SPI connection extend routes as far as possible  - DONE

- Make use of them to route accesses / SPI - DONE

- Move responsibility for AccessPath extension (back) to the SlavePorts and add
address range property to AccessPath. This is necessary to support, for
example, forking of AccessPaths to reach disjointly mapped regions. (Consider
the gap left in SPI access path by 0xfe82 for example). The SlavePort-
AccessPath selection interface will need to be sensitive to request address
range (in general each AccessPath will reach only a sub-region of the target
space).

Step 2...

- Add GW muxing - HALF BAKED

Step 3...

- Add Banking for coredump

- Work out how/which AddressSpaces coredump/dummy connection is going to hook -
the most resolved? the most resolved without multiple-paths?

Beyond:-

- Handle Scatter Gather when requests span multiple mappings.

"""

from csr.wheels.global_streams import iprint
from csr.wheels import TypeCheck, PureVirtualError, IndexOrSlice, display_hex,\
 pack_unpack_data_be, pack_unpack_data_le, BitArray, CloneFactory, SliceableDict
from time import time
from csr.dev.model import interface
import pdb
import sys
from array import array
from .port_connection import MasterPort, SlavePort, PortConnection, \
AccessPath as PortAccessPath, NetPort, AccessType, \
AccessFailure as _AccessFailure,\
NoAccess as _NoAccess,\
ReadFailure as _ReadFailure,\
WriteFailure as _WriteFailure, \
ReadFailureDeviceChanged as _ReadFailureDeviceChanged,\
WriteFailureDeviceChanged as _WriteFailureDeviceChanged
from weakref import WeakSet
from contextlib import contextmanager
from functools import reduce, cmp_to_key
import math
import itertools

if sys.version_info > (3,):
    # Python 3
    int_type = int

    def cmp(x, y):
        """
        Reimplement cmp function for python 3
        """
        if x < y:
            return -1
        elif x > y:
           return 1
        else:
            return 0

else:
    # Python 2
    int_type = (int, long)


class AddressRange (object):
    """\
    A contiguous range of word Addresses.
    
    AddressSpaces support slices at their interface however these are a pain to
    manipulate so they are normalised to absolute AddressRanges internally.
    """
    def __str__(self):
        return "[0x%x:0x%x]" % (self.start, self.stop)
    
    def __eq__(self, other):
        return self._start == other._start and self._stop == other._stop
    
    def __hash__(self):
        return hash((self._start, self._stop))
    
    def __repr__(self):
        return "{:s}(start=0x{:x},stop=0x{:x})".format(self.__class__.__name__, 
                                                     self.start, self.stop)
    
    @staticmethod
    def from_index_or_slice(index_or_slice):
        """\
        Construct AddressRange from index or slice.
        """
        helper = IndexOrSlice(index_or_slice)
        if helper.step != 1:
            raise ValueError()
        return AddressRange(helper.start, helper.stop)
    
    def __init__(self, start, stop):
        
        # Check that we haven't accidentally got floats here. Easily caused by py3 float division.
        if isinstance(start, float) or isinstance(stop, float):
            raise ValueError("AddressRange requires integer values. If on Py3 check "
                             "that you aren't accidentally using float division now")
        self._start = start
        self._stop = stop
    
    def __len__(self):
        return self.length

    def __add__(self, offset):
        return AddressRange(self.start + offset, self.stop + offset)
    
    @property
    def slice(self):
        """Represent as a slice"""
        return slice(self._start, self._stop)
    
    def __iter__(self):
        return iter(range(self._start, self._stop))
    
    def __contains__(self, other_range):
        
        if isinstance(other_range, AddressRange):
            start = other_range.start
            stop = other_range.stop
        elif isinstance(other_range, slice):
            start = other_range.start if other_range.start is not None else self.start
            stop = other_range.stop if other_range.stop is not None else self.length
        elif isinstance(other_range, int_type):
            start = other_range
            stop = other_range+1
        return self.start <= start and stop <= self.stop
    
    @property
    def start(self):        
        return self._start

    @property
    def stop(self):        
        return self._stop

    @property
    def length(self):        
        return self._stop - self._start

    def intersect(self, other):
        """
        Return intersection of two ranges (or None)
        """
        TypeCheck(other, AddressRange)
        start = max(self.start, other.start)

        stop = min(self.stop, other.stop)

        if stop > start:
            return AddressRange(start, stop)
        else:
            return None
        
    def does_span(self, other):
        """\
        Does this AddressRange fully span an other AddressRange? 
        """
        return self.start <= other.start and other.stop <= self.stop

    def shift(self, offset):
        """\
        Return new AddressRange shifted wrt. this one by offset words.
        """
        return AddressRange(self.start + offset, self.stop + offset)
    
    def transform(self, mult=1, div=1, offset=0):
        '''
        Return version of address space that is first scaled, e.g. to byte
        addressing from word addressing, and then offset
        
        E.g. for word addressing to byte addressing transformation:
         - mult = 16 ( = "from space" word_bits)
         - div = 8 ( = "to space" word_bits)
         - offset = offset in "to space" units)
        '''
        return AddressRange(mult * self.start // div + offset,
                            mult * self.stop // div + offset)

    @property
    def max_access_width(self):
        # What is the widest access compatible with the alignment of the
        # start and end addresses?
        if self.start & 0x3 == 0 and self.length & 0x3 == 0:
            return 4
        if self.start & 0x1 == 0 and self.length & 0x1 == 0:
            return  2
        return 1

    def minimal_aligned(self, alignment):
        
        if self.max_access_width >= alignment:
            return self

        align_mask = alignment - 1
        start = self.start - (self.start & align_mask)
        stop = self.stop + (alignment - self.stop & align_mask)
        return AddressRange(start, stop)

class AddressMultiRange(object):
    """
    Simple interface to a set of AddressRanges allowing combined containment
    tests
    """
    def __init__(self, ranges):
        
        self._range_list = list(ranges)
        
    def __contains__(self, other_range):
        """
        Test whether a simple range lies within this multi-range
        """
        return any((other_range in r) for r in self._range_list)

    def __str__(self):
        return ", ".join(str(r) for r in self._range_list)

class AccessView(object):
    """
    Enumeration of the possible views of an address that could (logically)
    be made across the debug transport to a multi-processor subsystem. Not
    all transports support these.
    """
    RAW = 0x7
    PROC_0_DATA = 0x8
    PROC_0_PROG = 0x9
    PROC_1_DATA = 0xa
    PROC_1_PROG = 0xb
    
    PROG_DATA_MASK = 0x1
    PROC_MASK      = 0x6
    NON_RAW_MASK       = 0x8
    
    class ViewClash(RuntimeError):
        """
        An access request is resolved through multiple address spaces which
        want to apply different views to data requested through them.  This is
        a configuration error.
        """
    
    @staticmethod
    def is_raw(view):
        return view & AccessView.NON_RAW_MASK == 0
    
    @staticmethod
    def is_prog(view):
        return (view & AccessView.NON_RAW_MASK) and (view & AccessView.PROG_DATA_MASK)

    @staticmethod
    def map(region, view):
        """
        Map the given region for the given view, returning the block ID and 
        the mapped region for the corresponding TRB transaction
        """
        raise PureVirtualError
    
    @staticmethod
    def resolve_view_requests(rq_view, map_view):
        """
        Given a view spec in a request and a view spec in a map involved in 
        resolving the request, return a suitable view spec for the resolved
        request
        """
        
        if map_view is not None:
            if rq_view is not None:
                if rq_view == map_view:
                    # Trivial case: the view specs are the same, so nothing to do
                    return map_view
                
                # If one is a raw request, they both have to be
                if AccessView.is_raw(rq_view) and AccessView.is_raw(map_view):
                    return map_view
                
                if rq_view & AccessView.PROC_MASK == map_view & AccessView.PROC_MASK:
                    # The views are for the same processor 
                    return map_view

                if AccessView.is_prog(rq_view) != AccessView.is_prog(map_view):
                    # The views are for different categories of space so it's
                    # fine to go ahead
                    return map_view
            else:
                # The request doesn't have a view specified, so pass on the 
                # view specified in the map
                return map_view
        else:
            # This mapping is not view-aware: so pass the request on
            return rq_view

        raise AccessView.ViewClash("Request for view 0x%x trying to be "
                                     "resolved by mapping for view 0x%x" %
                                     (rq_view, map_view))

    
class DefaultAccessView(AccessView):
    """
    AccessView variant for subsystems where there is no concept of different
    views
    """
    @staticmethod
    def map(region, view):
        """
        When there is no concept of a view, just return the default block ID and
        leave the region unchanged
        """
        return 0, region
    
class AddressMungingAccessView(AccessView):
    """
    AccessView variant for subsystems where the view is specified by changing
    the top nibble of the address
    """
    @staticmethod
    def map(region, view):

        def reset_top_nibble(value, top_nibble, width=32):
            """
            Replace the top nibble with the supplied nibble, returning the
            original top nibble
            """
            nibble_shift = width - 4
            nibble_mask = 0xf << nibble_shift
            original_nibble = (value & nibble_mask) >> nibble_shift
            if top_nibble & 0x8 == 0:
                # If the top bit is clear, use as a mask rather than a replacement
                top_nibble = original_nibble & top_nibble
            value &= ~(nibble_mask)
            value |= (top_nibble << nibble_shift)
            return value

        top_nibble = view
        munged_start = reset_top_nibble(region.start,top_nibble)
        munged_stop = reset_top_nibble(region.stop-1,top_nibble)+1
        munged_region = AddressRange(munged_start, munged_stop)
        return 0, munged_region

    
class BlockIDAccessView(AccessView):
    """
    AccessView variant for subsystems where the view is specified in the block 
    ID
    """
    @staticmethod
    def map(region, view):
        """
        Uses the view code as the block ID and leaves the address range untouched
        """
        return view, region

class AddressSpace (object):
    """
    Model address-able state spaces (well known and otherwise) within a Device
    and provide an interface to address that state.
    
    AddressSpaces are the _only_ means by which higher level abstractions and
    applications should access the attached device's state.

    Most abstractions (e.g. Registers, MMU, Stack, Log, Buffer) are written in
    terms of an associated Core's DataAddressSpace. But there are other
    significant AddressSpaces; E.g. The LPC abstraction includes LPCControl and
    LPCSRAM spaces.
    """    
    AccessFailure = _AccessFailure
    NoAccess = _NoAccess
    ReadFailure = _ReadFailure
    WriteFailure = _WriteFailure
    ReadFailureDeviceChanged = _ReadFailureDeviceChanged
    WriteFailureDeviceChanged = _WriteFailureDeviceChanged

    Debug=False

    def __init__(self,
                 name,
                 length = None,
                 length_kB = None,
                 layout_info = None, 
                 word_bits = 16,
                 min_access_width = 1,
                 max_access_width = None,
                 view=None):
        """\
        Construct AddressSpace.
        Clients supply either an ILayoutInfo or specify word_bits, 
        and le explicitly.  If the former, the latter are
        ignored.
        """     
        if length is None:
            if length_kB is not None:
                length = length_kB * 0x400
            else:
                length = 1 << 16 # historical default.  Ideally we'd get rid of this
        
        self._num_words = length
        self._name = name
        if layout_info is not None:
            self._word_bits = layout_info.addr_unit_bits
        else:
            self._word_bits = word_bits
            
        self._min_access_width = min_access_width
        self._max_access_width = max_access_width
            
        self._display_resolution_comments = True
        
        # Set a specific logical view for requests to this space.  Usually views
        # are specified for mappings between spaces rather than spaces themselves
        # because the view is an indication of how a given space should be 
        # accessed, not something intrinsic to spaces themselves.  However there
        # are situations, typically involving the manual implementation of 
        # logical views, in which a space is intrinsically tied to a particular
        # view in Pydbg's address space model. 
        self._view = view
        
        self._preread_store = {}
        
    
    def __len__(self):
        """\
        Size of this AddressSpace in addressable words.
        """
        return self._num_words

    def length(self):
        return self._num_words
    
    @property
    def word_bits(self):
        return self._word_bits
    
    @property
    def min_access_width(self):
        return self._min_access_width

    @property
    def max_access_width(self):
        return self._max_access_width
    
    def start_capturing(self):
        """
        Record all values read or written through this memory interface until 
        stop_capturing is called.  If capturing was already ongoing, it is 
        restarted, i.e. previously captured values are discarded
        """
        self._capture = SliceableDict()
    
    def stop_capturing(self):
        """
        Stop recording values read or written through this memory interface 
        and return the values captured, or None if capturing wasn't ongoing.
        """
        try:
            capture = self._capture
        except AttributeError:
            return None
        del self._capture
        return capture
    
    @display_hex
    def __getitem__(self, index_or_slice):
        """\
        Synchronous Read from region of this AddressSpace.        
        """
        # Map slices to regions at this interface to simplify internals
        #
        region = AddressRange.from_index_or_slice(index_or_slice)
        
        preread_data = self._retrieve_from_preread(region)
        if preread_data is not None:
            the_data = preread_data
            
        else:
        
            rq = ReadRequest(region, min_access_width = self._min_access_width,
                             max_access_width = self._max_access_width,
                             view=self._view)
            if self.Debug:
                rq.set_debug()
            try:
                self.timed_execute(rq)
            except self.ReadFailure as exc:
                if isinstance(index_or_slice, int_type):
                    msg = "0x%x: %s" % (index_or_slice, exc)
                elif isinstance(index_or_slice, slice):
                    msg = "[0x%x:0x%x]: %s" % (index_or_slice.start,
                                               index_or_slice.stop,
                                               exc)
                raise exc.__class__(msg)
            
            # Return data packaged according to integer/slice parameter
            #
            if self._display_resolution_comments and "comments" in rq.metadata:
                for comment in rq.metadata["comments"]:
                    iprint(comment.text)
    
            the_data = rq.data
    
        try:
            int(index_or_slice)
        except TypeError:
            # slice - caller expects a list
            pass
        else:
            # int-like - caller expects a single value
            the_data = the_data[0]

        try:
            self._capture[index_or_slice] = the_data
        except AttributeError:
            pass

        return the_data


    def __setitem__(self, index_or_slice, data):
        """\
        Synchronous write to region of this AddressSpace.
        """
        # Map slices to regions at this interface to simplify internals
        #
        region = AddressRange.from_index_or_slice(index_or_slice)
        
        # Check that we are not writing to a region that is currently prefetched
        self._check_for_preread_overlaps(region, write=True)
        
        # Re-Package data according to integer/slice parameter
        #
        try:
            int(index_or_slice)
        except TypeError:
            # slice - data already in list
            data_array = data
        else:
            # int-like - put into a list
            data_array = [data]


        if isinstance(index_or_slice, int_type):
            # Caller passed single value - put it into an array
            data_array = [data]
        elif isinstance(index_or_slice, slice):
            # Caller passed array so use it as is
            data_array = data
        else:
            raise TypeError()

        rq = WriteRequest(region, data_array, 
                          min_access_width = self._min_access_width,
                          max_access_width = self._max_access_width,
                          view=self._view)
        if self.Debug:
            rq.set_debug()
        try:
            self.timed_execute(rq)
        except self.WriteFailure as exc:
            if isinstance(index_or_slice, int_type):
                msg = "0x%x: %s" % (index_or_slice, exc)
            elif isinstance(index_or_slice, slice):
                msg = "[0x%x:0x%x]: %s" % (index_or_slice.start,
                                           index_or_slice.stop,
                                           exc)
            raise exc.__class__(msg)
        
        try:
            self._capture[index_or_slice] = data
        except AttributeError:
            pass
        
        if self._display_resolution_comments and "comments" in rq.metadata:
            for comment in rq.metadata["comments"]:
                iprint(comment.text)
            
    class WriteIntersectsPrefetchedBlock(RuntimeError):
        """
        A write request covers some of the same address range as a prefetched
        block.  This is not allowed - the prefetched range needs to be discarded
        first.
        """
            
    def _check_for_preread_overlaps(self, new_range, write=False):
        """
        Check whether there are any existing pre-read regions that intersect
        the given region.  What happens if there are depends on whether we 
        are checking a read request or a write request.
         - With read requests that are fully contained within an existing prefetched
           block we simply reuse the prefetched block
         - With read requests that full contain an existing prefetched block, we
           completely replace the existing smaller block with a new block representing
           the larger read
         - For simplicity, read requests that partially overlap are treated the
           same way as read requests that don't overlap at all
         - Write requests that overlap an existing prefetched block at all are
           illegal
        """
        ret_val = None
        ranges_to_replace = set()
        for range in self._preread_store:
            if write and range.intersect(new_range):
                raise self.WriteIntersectsPrefetchedBlock(
                    "Trying to write to a range {} intersecting "
                               "a prefetched range {}".format(new_range, range))
            elif not write and new_range in range:
                # We've already read a range containing the one we want,
                # so return that
                ret_val = range
            elif not write and range in new_range:
                # We're asking for a range that completely replaces one
                # we've already read, so discard the smaller one
                ranges_to_replace.add((range, new_range))
                ret_val = new_range
                
        for old_range, new_range in ranges_to_replace:
            rq = ReadRequest(new_range,
                             min_access_width = self._min_access_width,
                             max_access_width = self._max_access_width,
                             view=self._view)
            self.timed_execute(rq)
            # Insert the new data with the old data's reference count
            self._preread_store[new_range] = rq.data, self._preread_store[old_range][1]
            del self._preread_store[old_range]
        return ret_val
        
        
    def prefetch_address_range(self, range_start, range_stop):
        """
        Explicitly cache a particular memory range so that any requests for that
        range are provided by the cache, not requests to main memory.
        WARNING: there is no way for cache contents to be automatically flagged 
        as stale, so user code must ensure caches are deleted immediately after 
        use under all circumstances.  Hence wherever possible instead of calling
        this method and discard_prefetched explicitly, use the 
        address_range_prefetched context guard.
        """
        range = AddressRange(range_start, range_stop)
        
        already_preread = self._check_for_preread_overlaps(range)
        
        if not already_preread:
            rq = ReadRequest(range,
                             min_access_width = self._min_access_width,
                             max_access_width = self._max_access_width,
                             view=self._view)
            self.timed_execute(rq)
            self._preread_store[range] = (rq.data, 1)
        else:
            containing_data, count = self._preread_store[already_preread]
            self._preread_store[already_preread] = containing_data, count+1
        
    def discard_prefetched(self, range_start, range_stop):
        """
        Discard prefetched data corresponding to the given address range.  A
        particular prefetch buffer is only actually discarded when it has been 
        logically discarded as many times as it was logically prefetched.
        """

        range = AddressRange(range_start, range_stop)
        
        preread_range = self._lookup_preread_store(range)
        if preread_range is None:
            raise KeyError("Range {} not present in preread_store".format(range))
        data, req_count = self._preread_store[preread_range]
        if req_count == 1:
            del self._preread_store[preread_range]
        else:
            self._preread_store[preread_range] = (data, req_count - 1)
        
    @contextmanager
    def address_range_prefetched(self, *ranges):
        """
        Context guard that automatically prefetches and discards data in the given
        range on entry and exit respectively.  Users of the prefetch mechanism
        should always use the context guard if possible to avoid accidentally
        leaving data in the cache beyond the execution of the relevant block of
        code.
        """
        # We need to make range_pairs a list - zip is an iterator so the second
        # call in the finally block will not loop over the expected set otherwise.
        range_pairs = list(zip(ranges[:-1:2],ranges[1::2]))
        
        for (range_start, range_stop) in range_pairs:
            self.prefetch_address_range(range_start, range_stop)
        try:
            yield
        finally:
            for (range_start, range_stop) in range_pairs:
                self.discard_prefetched(range_start, range_stop)
        
    def _lookup_preread_store(self, range):
        containing = [range_key for range_key in self._preread_store if range in range_key]
        if len(containing) >= 1:
            return containing[0]
                
        
    def _retrieve_from_preread(self, range):
        """
        Return data from the preread cache if it's there
        """
        preread_range = self._lookup_preread_store(range)
        if preread_range is not None:
            preread_data, _ = self._preread_store[preread_range]
            requested_data_offset = range.start - preread_range.start
            return preread_data[requested_data_offset:requested_data_offset+range.length]
        
        
    @property
    def name(self):
        """\
        Friendly name for diagnostic purposes.
        """
        if self._name:
            return self._name
        else:
            return "(anon)"
    
    @property
    def word_bits(self):
        """\
        Nominal number of bits per addressable word.
         
        At some locations/registers the number of
        significant/valid bits may be less than this.

        This reflects the access hardware of this space.
        In some cases addresses may map to locations with 
        other constraints.
        """
        return self._word_bits
    
    def timed_execute(self, access_request):
        
        rq = access_request
        verbose = False
        if verbose:
            start = time()            
            self.execute(rq)            
            elapsed = time() - start        
            iprint("%20s [%04x]%s[%04x] x %d (%.1fmS)" % \
                    (self.name, rq.region.start, 
                    "->" if isinstance(rq, ReadRequest) else "<-", 
                    rq.data[0], len(rq.data),
                    elapsed * 1000))                        
        else:
            self.execute(rq)
        
    def execute(self, access_request):
        """\
        Execute an access request w.r.t. this AddressSpace (synchronously).

        Raises:-
        - NoAccess: No access path currently available.
        """ 
        raise PureVirtualError(self)
    
    @property
    def cached_register_names(self):
        """\
        Store most recently accessed register names
        
        Bitfield uses it to store most recently used register name
        while address request is processing, e.g.
        self.mem[self.start_addr : self.stop_addr]
        This allows modules like host_subsystem to identify origin
        of the address request and do some clever things.
        Make it a list to support nested calls in case
        address request triggers another register access.
        """
        try:
            self._cached_register_names
        except AttributeError:
            self._cached_register_names = []
        return self._cached_register_names


class AddressRegion (object):
    """\
    Defines an AddressRange in a specific AddressSpace.
    """
    def __init__(self, address_space, address_range=None):
        """\
        Construct AddressRegion
        
        Params:-
        
        - address_space: Where the region lies.
        
        - address_range: The numerical range (optional: default is entire
        space).
        """
        TypeCheck(address_space, AddressSpace)
        TypeCheck(address_range, (type(None), AddressRange))

        self._space = address_space
        if address_range is None: # default is entire address space
            address_range = AddressRange(0, len(address_space))
        self._range = address_range

    @property
    def space(self):
        return self._space
    
    @property
    def range(self):
        return self._range

    # Convenience
    
    @property
    def length(self):
        """\
        Length of this AddressRegion in words.
        """
        return self.range.length
    
    def does_span(self, address_range):
        """\
        Does this AddressRegion span the specified AddressRange?
        """
        return self.range.does_span(address_range)


class AddressMasterPort (MasterPort):
    """\
    Model of a memory master port.
    
    Used to define a Device's memory network model for debug access routing
    purposes.
    
    Currently doesn't add anything to a plain MasterPort
    """



class AccessCache (object):
    """\
    AccessRequest cache (abstract base)
     
    Implements AccessRequest caching policy for an AddressSlavePort
    
    Requests that can not be resolved any further than the port are passed to
    cache.execute(). The cache should decide if the request can be served from
    cache, If not then it should delegate the request back to the slave to pass
    it on outwards.
    
    The cache logic is abstracted to allow different policies to be easily
    substituted.
    
    Known derivatives:-
    - NullAccessCache: Does no caching at all.
    """
    def __init__(self, slave_port):
        
        self.slave_port = slave_port
        
    def execute(self, access_request):
        
        raise PureVirtualError(self)


class NullAccessCache (AccessCache):
    """\
    An AccessCache that does no caching at all!
    
    All requests are passed on via physical access path.
    
    For test and development purposes.
    """
    def __init__(self, slave_port):
        AccessCache.__init__(self, slave_port)

    def execute(self, access_request):
        
        # no cache - send it straight outward via the AddressSlavePort
        self.slave_port.execute_outwards(access_request)
                

class ExtremeAccessCache (AccessCache):
    """\
    An AccessCache that caches all requests, including writes, and passes none
    on via physical access path.
    
    Used to implement a "soft" device where there is no physical access path!
    Such as to support coredump analysis.
    
    Raises:-
    - NoAccess: On read requests from memory that has not been written to. 
    """
    def __init__(self, slave_port):
        
        AccessCache.__init__(self, slave_port)
        
        # Remember this so we can construct a suitable cache on demand
        self._slave_port = slave_port
        # Store either unsigned char or unsigned short
        self._typecode = "B" if slave_port.word_bits == 8 else "H"
        
        self._logical_len = slave_port.length()
        self._physical_len = min(0x100, self._logical_len) # Arbitrary maximum
         
        
    def execute(self, access_request):
        
        rq = access_request
        
        if rq.debug:
            iprint("%s cache: executing request for [0x%x,0x%x]" %
                    (self._slave_port.name, rq.region.start, rq.region.stop))

        if isinstance(rq, ReadRequest) and not hasattr(self, "_cache"):
            # Cache hasn't been instantiated yet so there's certainly nothing
            # there to be read!
            raise AddressSpace.NoAccess("Access to uninitialised state %s in "
                                        "address space '%s'" 
                                        % (rq.region, self.slave_port.name))
            

        # Raise NoAccess if attempt to access outside range
        if rq.region.stop > self._logical_len:
            raise AddressSpace.NoAccess("Attempt to access outside range "
                                        "of AddressSpace '%s'" \
                                                        % self.slave_port.name)

        if isinstance(rq, ReadRequest):
            if rq.region.stop > self._physical_len or 0 in self._is_set[rq.region.slice]:
                # Trying to access an unset value
                raise AddressSpace.NoAccess("Access to uninitialised state %s in "
                                        "address space '%s'" 
                                        % (rq.region, self.slave_port.name))
            rq.data = self.cache[rq.region.slice].tolist()
            
            verbose = False
            if verbose:
                iprint("XCache %s: Read %s = %s" % (self.slave_port.name, 
                                                   str(rq.region.slice),
                                                   str(rq.data)))
            
        elif isinstance(rq, WriteRequest):
            
            if rq.region.stop < len(self.cache):
                self.cache[rq.region.slice] = array(self._typecode, rq.data)
            else:
                # Off the end, so grow first
                self._expand_cache(rq.region.stop)
                self._cache[rq.region.slice] = array(self._typecode, rq.data)
                
            self._is_set.set(rq.region.start, rq.region.stop)
            
        else:
            raise TypeError()
        
        access_request.merge_metadata(rq.metadata)              
        
    @property
    def cache(self):
        try:
            self._cache
        except AttributeError:
            # ? Wasteful - several spaces are sparsely cached
            # Use dict instead ?
            try:
                self._cache = array(self._typecode, [0xff]*self._physical_len)
            except MemoryError:
                raise MemoryError("Trying to allocate list of 0x{:x} bytes for "
                                  "cache of {}".format(self._physical_len, 
                                                       self._slave_port.name))
            self._is_set = BitArray(self._physical_len,0)
        return self._cache
        
    def _expand_cache(self, new_min_len):
        """
        Make the cache at least big enough for the given length by doubling it
        enough times
        """
        if new_min_len <= self._physical_len:
            return
        if new_min_len > self._logical_len:
            raise ValueError("Trying to expand cache beyond its logical size")
        
        # How many times to double the length to get above the requested length
        # In Py2 math.ceil returns a float, annoyingly
        shift = int(math.ceil(math.log(float(new_min_len)/self._physical_len)/math.log(2)))
        new_physical_len = min(self._physical_len << shift, self._logical_len)
        new_cache = array(self._typecode, itertools.chain(self._cache, 
                                                          [0xff]*(new_physical_len-
                                                                  self._physical_len)))
        self._physical_len = new_physical_len
        self._cache = new_cache
        self._is_set.expand(self._physical_len)


    def load(self, f, size):
        """
        Load the cache directly from the given file.  Throws away what might have
        been in the cache already.
        """
        scale = 1 if self._typecode == "B" else 2
        self._cache = array(self._typecode)
        self._is_set = BitArray(len(self._slave_port),0)
        self._cache.fromfile(f, size)
        self._physical_len = len(self._cache)
        self._is_set.set(0,size)


class PassiveAccessCache (AccessCache):
    """
    Cache that supports simulation by remembering everything that is written to
    it and returning 0 for any access that it doesn't have an entry for.
    """
    def __init__(self, slave_port):
        
        AccessCache.__init__(self, slave_port)
        self._cache = {}
        
    def execute(self, rq):
        """
        Serve read requests from the cache dictionary, returning 0 for any bytes
        that aren't available.  Update the dictionary with any write requests.
        """
        if isinstance(rq, ReadRequest):
            rq.data = [(self._cache[addr] if addr in self._cache else 0) 
                                                        for addr in rq.region]
        elif isinstance(rq, WriteRequest):
            for addr, value in zip(rq.region, rq.data):
                self._cache[addr] = value
        else:
            raise TypeError
                
    @property
    def cache(self):
        return self._cache


class AddressSlavePort (NetPort, AddressSpace):
    """\
    A memory slave port.
    
    Implements AddressSpace
    
    Used to wire up device's memory network model for access routing purposes.
    """
    def __init__(self, 
                 name,
                 cache_type = NullAccessCache,
                 layout_info = None,
                 length = None,
                 length_kB=None,
                 word_bits = 16,
                 min_access_width = 1,
                 max_access_width = None,
                 view = None):

        TypeCheck(name, str)
                
        NetPort.__init__(self)
        self._access_paths = WeakSet()
        AddressSpace.__init__(self, name, length=length,length_kB=length_kB, 
                              layout_info=layout_info, 
                              word_bits=word_bits, min_access_width=min_access_width, 
                              max_access_width=max_access_width,view=view)

        # Store this because BankedAddressSpace needs to be able to construct
        # caches on the fly
        self._cache_type = cache_type
        self._cache = cache_type(self)
        TypeCheck(self._cache, AccessCache)
        
        self._identity_checker = None

    @property
    def cache_type(self):
        """
        The type of the cache.  Exposed publicly to allow for creation of
        auto-filling sibling spaces with the same cache type.
        """
        return self._cache_type

    @property
    def on_access_path(self):
        return len(self._access_paths) > 0

    def simple_subspace(self, name, length, cache_type=None):
        """
        Return a subspace of the same type and general characteristics
        """
        if cache_type is None:
            cache_type = self._cache_type
        return AddressSlavePort(name, cache_type=cache_type,
                                word_bits = self._word_bits,
                                length=length,
                                min_access_width=self._min_access_width,
                                max_access_width=self._max_access_width)
        
    # AddressSpace compliance
    
    def execute(self, access_request):
        """\
        Execute an access request w.r.t. this AddressSpace as fast as possible.
        
        The strategy is to recursively resolve requests inwards through any
        maps/muxes until they reach a "leaf" AddessSlavePort.

        At that point, if possible, the request will be satisfied from local
        cache, if not then the request is recursively mapped outwards via the
        fastest available AccessPath till it reaches a debug port (SPI|TRB|...)
        that can physically serve it.
        
        In general this process may involve multiple request transformations 
        (scatter-gather, word size changes etc.).
        
        Raises:-
        - NoAccess: If no available AccessPath.
        """
        try:
            # Resolve request recursively.
            self.resolve_access_request(access_request)
        except self.NoMapping:
            # Request has hit a leaf space.
            self.execute_here(access_request)
        
    # Extensions
    
    class NoMapping (RuntimeError):
        """\
        Exception raised if there is no downstream mapping for an
        AccessRequest.
        """ 
    
    def register_access_path(self, access_path):
        """\
        Registers an active debug AccessPath with this port/space.
        
        If the associated component is a map or mux then the path will be
        extended, recursively, all AddressSpaces reachable via the component.        
        """
        self._access_paths.add(access_path)
        self._extend_access_path(access_path)

    def resolve_access_request(self, access_request):
        """\
        Map the AccessRequest inwards if possible else raise NoMapping.
        
        Ports on address maps and muxes should override this to map the request
        inwards.
        """ 
        raise self.NoMapping()

    def execute_here(self, access_request):
        """\
        Request can go no further inwards. 
        
        Delegate to cache manager to decide if can serve from cache or need to
        pass the request outwards along an AccessPath in search of daylight/a
        debug connection.

        The cache manager is objectified to make it simple to change cache
        policy. E.g. to implement a soft device for coredump analysis.
        
        Raises:- 
        
        - NoAccess      If can't serve from local cache and no AccessPaths
                        registered.
        """                 
        self._cache.execute(access_request)
        
    def execute_outwards(self, access_request):
        """\
        Select fastest AccessPath and pass the request to it.
        """
        rq = access_request
        path = self.get_best_access_path(rq)
        if path:
            path.execute_outwards(rq)
        else:
            if access_request.view:
                raise self.NoAccess("AddressSpace '%s' [%04x:%04x] (view 0x%x) "
                                    "is inaccessible. Available paths are: %s" % (self.name,  
                                         rq.region.start, rq.region.stop,
                                         access_request.view, list(self._access_paths)))
                
            raise self.NoAccess("AddressSpace '%s' [%04x:%04x] is inaccessible. Available paths are: %s" % (
                                    self.name,  rq.region.start, rq.region.stop,
                                    list(self._access_paths)))
      
    def load_cache(self, f, size):
        try:
            self._cache.load(f, size)
        except AttributeError:
            raise TypeError("Can't load cache: this space is not cached!")
      
    def get_best_access_path(self, access_request):
        """\
        Return fastest available direct AccessPath for this request, or None.
        """        
        rq = access_request
        
        for path in self._access_paths:
            if path.range.does_span(rq.region):
                if not hasattr(rq, "view") or path.master.handles_view(rq.view):
                    return path

    def check_identity(self):
        """\
        Does this node recognise itself?!
        
        In rare cases access route setup is unreliable and the only way a
        router can check if it has successfully configured a route is to ask
        the intended target node to perform a self identity check.
                
        Only a select few AddressSlavePort will need to override this. So far
        its just those that are connected directly to an FPGAControl mux.
        
        Slaves should examine the content of their own state-spaces and return
        True if they recognise themselves (e.g. correct CHIP signature,
        plausible build date, correct lpc board/slot id etc).

        This interface avoids embedding any slave-specific details in the dodgy
        routers.
        """
        # This should not be called on Slaves that don't implement/override it
        # so we raise an error rather than return a false positive (or
        # negative).
        #
        if self._identity_checker:
            return self._identity_checker()
        else:
            raise NotImplementedError(self)    
    
    def set_identity_checker(self, checker):
        
        self._identity_checker = checker
    
    # Protected / Overridable 
    
    def _extend_access_path(self, access_path):
        """
        Extends the specified AccessPath to all AddressSpaces reachable via the
        component exposing this AddressSlavePort.
        
        AddressSlavePorts on AddressMap and AddressMUX components should
        override this to extend the path via all the associated
        AddressMasterPorts.
        
        In some cases this may involve specific transformations (e.g. forking
        the path by address sub-ranges). For the common case where no
        transformation is needed the implementation can just call
        access_path._extend_simply() for each output port.
        
        The default implementation assumes this is a leaf AddressSpace and
        therefore does not extend the path.
        """
        # temp: catch any derived ports that dont override!
        if type(self) is not AddressSlavePort:
            raise PureVirtualError(self)
        return
    
    
class AddressConnection (PortConnection):
    """\
    Models a directed connection between an AddressMasterPort and an
    AddressSlavePort.
    """
    def __init__(self, addresser_port, addressee_port):

        # Reuse generic NetConnection but restrict the port types        
        TypeCheck(addresser_port, AddressMasterPort)
        TypeCheck(addressee_port, AddressSlavePort)
        PortConnection.__init__(self, addresser_port, addressee_port)

class AccessPath (PortAccessPath):
    """\
    Represents direct or indirect accessibility of a Device's AddressSpace via
    an active debug connection.
        
    A directed graph of AccessPath nodes is propagated via all AddressSpaces
    that can be reached directly or indirectly via each active debug connection
    (SPI, TRB...).
    
    This pre-built graph makes it simple/quick to determine what paths are
    available to access a specified AddressSpace, to choose between any
    alternatives based on speed and then to use the chosen path to physically
    access the corresponding state.
    """
    trace = False
    
    def __init__(
            self, 
            path_name,      # Diagnostic path name e.g "SPI".
            rank,           # How far down along the path is this node?
            master_port,    # The MasterAddressPort implementing the access.
            address_range,  # Range of addresses accessible via this path.
        ):
        """\
        Construct AccessPath node.

        The path should be explicitly extended as soon as it has been
        constructed.
        """
        PortAccessPath.__init__(self, rank, master_port, type=AccessType.MEMORY)
        
        TypeCheck(address_range, AddressRange)
        
        self.path_name = path_name
        self.range = address_range
            
    def __repr__(self):
        return "AccessPath: name=%s rank=%d, range=%s" % (self.path_name, 
                                                          self.rank, self.range)
            
    def debug_trace(self, slave):
        iprint("  "*self.rank + "%s -> %s[%x:%x]" % (self.path_name, slave.name,
                                                    self.range.start, self.range.stop))    
    
    def clone(self, master_port, new_rank):
        return AccessPath(self.path_name, new_rank,
                              master_port, self.range)

    

class AddressMap (object):
    """
    Model the hardware mapping of two or more distinct AddressSlaves into
    distinct regions of one AddressSpace. Aka a Memory Map.
    
    Individual mappings are represented by AddressMapping objects.
    
    AddressMaps can support multi-view semantics, where AccessRequests arrive
    in both directions with optional View information.  
    """
    
    def __init__(self, *args, **kwargs):
        """\
        Construct AddressSpace
        """        
        self.mappings = [] # ordered set of mappings
        if "view_type" in kwargs:
            self._view_type = kwargs["view_type"]
            del kwargs["view_type"]
        else:
            self._view_type = DefaultAccessView
            
        self._port = AddressMap.InPort(self, *args, **kwargs)
        
        # Mappings can optionally be put into specific named groups when they're
        # added.  The point is to allow later retrieval of the ranges using
        # generic names like "local RAM", "registers" etc.
        self._groups = {}

    @property
    def port(self):
        return self._port
    
    def add_mappings(self, *mapping_tuples, **kw_args): 
        """\
        Adds address mappings en-mass from  a list of tuples.
        
        Tuples like:- (from_start, from_stop, to_space[, to_start])
        """
        view = kw_args["view"] if "view" in kw_args else None
        le = kw_args["le"] if "le" in kw_args else None
        group = kw_args["group"] if "group" in kw_args else None 
        for mapping in mapping_tuples:
            self.add_mapping(*mapping, view=view, le=le, group=group)
             
    def add_mapping(self, from_start, from_stop, to_space, to_start=0,
                    view=None, autofill=False, le=None, group=None):
        """\
        Add an address mapping from specified range to specified address space.
        
        Order is significant. The first mapping that spans a range will be
        used. This allows default mappings to be defined and small bites to be
        taken out of it (by defining small areas first).
        
        Params:- 
        
        - to_start: Offset in to space (default 0)
        - view: specifies a particular view of hardware (relevant to Apps K32
        address space)
        - autofill: autogenerate address spaces for the gaps in the mapping
        - le : manually specify whether the address mapping is little-endian
        wrt the address space being mapped from (this is moot if bytewidths match)
        - group : Name of a group to assign this mapping to.  Names are arbitrary
        but conventional names like "local RAM" etc are encouraged.  Case is
        unimportant.
        """
        if le is None:
            # Little-endian by default because in the canonical case of processor
            # bus accesses being transformed onto a narrower debug transport we 
            # most likely want to be able to treat the transport interface as a 
            # byte stream, i.e. we want the list of transport bytes in address
            # order
            le = True
        
        if isinstance(view, (list, tuple)):
            if not self.view_enabled:
                iprint("Should only add multiple views of a "
                        "mapping to a view-enabled Map (%s)" % self.port.name)
            for v in view:
                self.add_mapping(from_start, from_stop, to_space, to_start, v,
                                 le=le, group=group)
        else:
            from_range = AddressRange(from_start, from_stop)
            block_id, view_range = self._view_type.map(from_range, view)
            mapping = self.Mapping(self, view_range, to_space, to_start,
                                   view=view, 
                                   block_id=block_id,
                                   le=le)
            self.mappings.append(mapping)
            if group is not None:
                self._groups.setdefault(group.lower(), set()).add(mapping)
            
        if autofill:
            self.add_default_mappings(le=le, cache_type=to_space.cache_type)
            
    def add_default_mappings(self, le=None, cache_type=None, group=None):
        """
        If a space is partially mapped, it can be convenient to auto-create
        leaf spaces to map the remainder of the address range because mixed
        mapped/leaf spaces aren't supportable if we want to do 
        """
        
        if le is None:
            le = False
        
        # What range(s) aren't mapped?
        mapped_regions = self.get_partial_mappings_for_region(AddressRange(0, len(self.port)))
        mapped_regions.sort(key=cmp_to_key(lambda x, y: cmp(x.region.start, y.region.start)))

        start_mapped = 0
        end_mapped = start_mapped
        unmapped_ranges = []
        for mapped_region, overlap in mapped_regions:
            if overlap.start <= end_mapped:
                # The region overlaps the current mapped range
                if overlap.stop > end_mapped:
                    # The region extends the current mapped range
                    end_mapped = overlap.stop
            else:
                # The region doesn't overlap the current mapped range, so there's
                # an unmapped gap
                unmapped_ranges.append(AddressRange(end_mapped, 
                                                    overlap.start))
                start_mapped = overlap.start
                end_mapped = overlap.stop
        if end_mapped < len(self.port):
            unmapped_ranges.append(AddressRange(end_mapped, len(self.port)))
            
        for range in unmapped_ranges:
            # Make a simple leaf space of length range.stop - range.start
            subspace_name = self.port.name + "_[0x%x:0x%x]" % (range.start,range.stop)
            self.add_mapping(range.start, range.stop, 
                             self.port.simple_subspace(subspace_name, 
                                                       range.stop-range.start,
                                                       cache_type=cache_type),
                             autofill=False, le=le, group=group)
        
        
    def get_mapping_for_region(self, region):
        """\
        Return mapping that spans the specified AccessRequest - or None.
        
        Future:-
        - Support requests that span more than one mapping.
        """
        for mapping in self.mappings:
            if mapping.does_span(region):
                return  mapping
        return None

    def get_partial_mappings_for_region(self, region):
        """
        Return a list of all the mappings that intersect this request, pairing
        them with an AddressRange describing the overlap for convenience
        """
        mappings = []
        for mapping in self.mappings:
            overlap = mapping.intersect(region)
            if overlap:
                mappings.append((mapping, overlap))
        return mappings

    @property
    def groups(self):
        return self._groups

    def select_subranges(self, *slave_or_group_name_list, **kwargs):
        """
        Return a list of AddressRange objects corresponding to the mappings to
        the named slaves.
        If must_exist is True, passing a non-existent group/slave name in 
        slave_or_group_name_list will cause a ValueError exception.  If False, 
        non-existent slaves/groups are just silently dropped.
        """
        must_exist = kwargs.get("must_exist",True)
        
        name_list = slave_or_group_name_list
        if must_exist:
            try:
                mapping_list = reduce(lambda x,y : x | y, 
                                      (self._groups[name.lower()] for name in name_list), set())
            except KeyError:
                if len([n for n in name_list if n in self._groups]) == 0:
                    mapping_list = set()
                else:
                    raise ValueError("Address map groups %s in supplied name list don't "
                                     "exist. Valid group names are %s" % 
                                     (", ".join((n for n in name_list if n not in self._groups)),
                                      ", ".join(list(self._groups.keys()))))
        else:
            mapping_list = reduce(lambda x,y : x | y,
                                  (self._groups.get(name.lower(),set()) for name in name_list),
                                  set())

        if len(mapping_list) == 0:
            mapping_list = {m for m in self.mappings if m.slave.name in name_list}
            if must_exist and len(mapping_list) != len(name_list):
                # Didn't get as many mapping as expected: find which are missing
                mappings_found = {name:False for name in name_list}
                for m in mapping_list:
                    mappings_found[m.slave.name] = True
                mappings_not_found = [name for (name, found) in list(mappings_found.items()) 
                                        if not found]
                # list len difference could have been due to duplicates
                # but all may be found
                if mappings_not_found:
                    raise ValueError(
                        "Slave(s) '%s' in supplied name list don't "
                        "exist. Valid slave names are %s" % (
                            ", ".join(mappings_not_found),
                            ", ".join([m.slave.name for m in self.mappings]))) 
        return [m.range for m in mapping_list]
            
    
    @property
    def view_enabled(self):
        return self._view_type != DefaultAccessView

    def __repr__(self):
        if not self.mappings:
            return "AddressMap <no mappings>"
        return "AddressMap:\n" + "\n  ".join([repr(m) for m in self.mappings])
    

    class Mapping (AddressMasterPort):
        """\
        Models a hardware mapping from an AddressRegion to an AddressSlavePort 
        that serves addresses in that region.
        
        Pragmatically Mappings automatically create a connection to the target
        port.
        """
        def __init__(self, which_map, address_range, to_slave_port, to_start,
                     view=None, block_id=None, le=False):
            
            TypeCheck(address_range, AddressRange)
            
            AddressMasterPort.__init__(self)

            self.map = which_map            
            self.range = address_range # input side
            
            # Figure out if we have to convert between byte and word addressing
            self.to_width = to_slave_port.word_bits
            self.from_width = self.map.port.word_bits
            
            if isinstance(to_start, int_type):
                # Fixed window 
                
                # We have to be careful with the offset between the spaces in case 
                # they have different address widths.  We define two offsets for
                # convenience:
                #  - to_offset is the distance *already scaled* "from" addresses must be
                # shifted to map correctly to "to" space
                #  - from_offset is the distance *already scaled* "to" addresses must be
                # shifted to map correctly to "from" space
                
                # The to offset is in terms of the *to* space's addressing
                self._to_offset = to_start - \
                                self.from_width * self.range.start // self.to_width
                # The from offset is in terms of the *from* space's addressing
                self._from_offset = self.range.start - \
                                        self.to_width * to_start // self.from_width
                
            else:
                # Dynamic window: assume to_start is a callable.  The above
                # to_offset/from_offset calculations are done on demand below
                self._to_start = to_start

            # Endianness of the mapping.  Obviously this is meaningless if the
            # mapping is between spaces of the same width
            self._le = le
            self._auto_connection = AddressConnection(self, to_slave_port)
            
            self._view = view
            self._block_id = block_id

        def __repr__(self):
            return "Mapping: %s: [0x%x,0x%x) -> %s" % (self.map.port.name, 
                                                       self.range.start, 
                                                       self.range.stop,
                                                       self.slave.name)

        @property
        def to_offset(self):
            try:
                return self._to_offset
            except AttributeError:
                # Start address determined dynamically
                return (self._to_start() - 
                            self.from_width * self.range.start // self.to_width)
                
        @property
        def from_offset(self):
            try:
                return self._from_offset
            except AttributeError:
                return (self.range.start - 
                            self.to_width * self._to_start() // self.from_width)
    
        # AddressMasterPort compliance
        
        def execute_outwards(self, access_request):
            
            rq = access_request
            
            # Transform requested address to the "in" port's addressing, i.e.
            # map backwards - "from" to "to"
            # And pass "out" via "in" port.
            in_port = self.map.port
            new_block_id = (self._block_id if self._block_id is not None 
                                                else rq.block_id)
            mapped_rq = rq.mapped_request(self.to_width,
                                          self.from_width, 
                                          self.from_offset,
                                          self._le,
                                          max_to_width = 
                                                      in_port.max_access_width,
                                          min_to_width = 
                                                      in_port.min_access_width,
                                          block_id=new_block_id,
                                          view=rq.view)
            in_port.execute_outwards(mapped_rq)
            # If it's a read request, we need to populate the original request
            # with suitably mapped data
            if isinstance(rq, ReadRequest):            
                rq.data = mapped_rq.mapped_data(self.from_width, 
                                                self.to_width,
                                                self._le)
            elif not isinstance(rq, WriteRequest):
                raise TypeError()

            rq.merge_metadata(mapped_rq.metadata)                
                        
        # Extensions
        
        @property
        def output_range(self):
            """\
            The output AddressRange reachable via this Mapping.
            """
            return self.range.transform(mult=self.to_width,
                                        div=self.from_width, 
                                        offset=self.to_offset)
        
        def does_span(self, address_range):
            """\
            Does this AddressMapping's from_region completely span the
            specified input AddressRange? (Convenience)
            
            Future:- Will need does_overlap() to support scatter gather.
            """
            return self.range.does_span(address_range)
        
        def intersect(self, address_range):
            
            return self.range.intersect(address_range)
        
        def extend_access_path(self, path):
            """\
            Extend an AccessPath via this mapping.
            
            Ignores paths that do not intersect this Mapping.
            """
            intersection = path.range.intersect(self.range)
            if intersection is not None:
                output_range = intersection.transform(mult=self.from_width,
                                                      div=self.to_width,
                                                      offset=self.to_offset)
                new_fork = AccessPath(path.path_name, path.rank + 1,
                                      self, output_range)
                path.add_fork(new_fork)
                        
        def resolve_access_request(self, access_request):
            """\
            Map access request from mapped-from space into the 
            mapped-to space.  
            """
            
            
            rq = access_request
            
            # Transform the requested into the connected slave's units/offset.
            # Set this Mapping's view in the request: the incoming request
            # either has no view specified, or already has this view specified,
            # or this view is not specified. In any case we can set this 
            # Mapping's view to the "most specified", raising an error if there
            # is an inconsistency, i.e. both have a specified view and they
            # are not the same.
            # And pass to connected slave.
            new_view = AccessView.resolve_view_requests(access_request.view,
                                                        self._view)
            
            mapped_rq = rq.mapped_request(self.from_width,
                                          self.to_width, 
                                          self.to_offset,
                                          max_to_width = 
                                                 self.slave.max_access_width,
                                          min_to_width = 
                                                 self.slave.min_access_width,
                                          view=new_view,
                                          le=self._le)
            if rq.debug:
                mapped_rq.set_debug()
                iprint("Mapping (%s to %s): Original rq [0x%x:0x%x]; mapped rq [0x%x:0x%x]" %
                        (self.map.port.name, self.slave.name, 
                         rq.region.start, rq.region.stop, 
                         mapped_rq.region.start, mapped_rq.region.stop))
                try:
                    rq_print_len = min(4,len(rq.data))
                    mapped_rq_print_len = min(4,len(mapped_rq.data))
                    fmt = ("Original data ["+ ",".join(["0x%x"]*rq_print_len) + 
                           ",...] (len %d); mapped data [" + 
                           ",".join(["0x%x"]*mapped_rq_print_len) +
                           "(len %d)")
                    iprint(fmt % tuple(rq.data[:rq_print_len] + [len(rq.data)] +
                                  mapped_rq.data[:mapped_rq_print_len] + 
                                  [len(mapped_rq.data)]))
                except AttributeError:
                    # No data attribute - read request
                    pass
            self.slave.execute(mapped_rq)
            # If it's a read request, we need to populate the original request
            # with suitably "mapped-back" data
            if isinstance(rq, ReadRequest):
                rq.data = mapped_rq.mapped_data(self.to_width, 
                                                self.from_width,
                                                self._le)
                if rq.debug:
                    rq_print_len = min(4,len(rq.data))
                    mapped_rq_print_len = min(4,len(mapped_rq.data))
                    fmt = ("Read data ["+ ",".join(["0x%x"]*mapped_rq_print_len) + 
                           ",...] (len %d); mapped-back data [" + 
                           ",".join(["0x%x"]*rq_print_len) +
                           "(len %d)")
                    iprint(fmt % tuple(mapped_rq.data[:rq_print_len] + [len(mapped_rq.data)] +
                                  rq.data[:rq_print_len] + 
                                  [len(rq.data)]))
            elif not isinstance(rq, WriteRequest):
                raise TypeError()

            rq.merge_metadata(mapped_rq.metadata)                

        def handles_view(self, view):
            """
            A Mapping handles an access request's view if it's not view-enabled,
            or if the views match
            """
            return not self.map.view_enabled or not view or view == self._view
            

    class InPort (AddressSlavePort):
        
        def __init__(self, my_map, *args, **kwargs):
            AddressSlavePort.__init__(self, *args, **kwargs)
            self.map = my_map
            
        # AddressSlavePort compliance
        
        def _extend_access_path(self, access_path):            
            # This is a non-trivial case: The path must fork _and narrow_ via
            # each mapped region. The nitty-gritty is delegated to the mapping
            # objects.
            for mapping in self.map.mappings:
                mapping.extend_access_path(access_path)

        def resolve_access_request(self, rq):
            # Delegate to corresponding Mapping object (if any)
            #
            # Future: handle requests that span mappings.
            # 
            mapping = self.map.get_mapping_for_region(rq.region)
            if mapping:
                if rq.debug:
                    iprint("%s: Directly resolving access request for "
                           "[0x%x,0x%x]" % (self.name, rq.region.start,
                                            rq.region.stop))
                mapping.resolve_access_request(rq)
            else:
                mappings_and_overlaps = \
                         self.map.get_partial_mappings_for_region(rq.region)
                if mappings_and_overlaps:
                    if rq.debug:
                        iprint("%s: Resolving access request via scatter-gather"
                               "for [0x%x,0x%x]" % (self.name, rq.region.start,
                                                    rq.region.stop))
                    # Turn a list of pairs into a pair of lists
                    mappings, overlaps = list(zip(*mappings_and_overlaps))
                    if rq.debug:
                        iprint(" Split [0x%x,0x%x] access into %d partial "
                               "accesses" % (rq.region.start,
                                             rq.region.stop, len(mappings)))
                        for overlap in overlaps:
                            iprint("  [0x%x,0x%x]" % (overlap.start, overlap.stop))
                    # Build a list of partial requests from the main request.  These
                    # cover as much of the original request as possible, including
                    # possible duplications
                    partial_requests = rq.scatter(overlaps)
                    if rq.debug:
                        for i in range(len(partial_requests)):
                            iprint(" Request for [0x%x,0x%x]" %
                                                            (overlaps[i].start,
                                                             overlaps[i].stop))
                    # Execute the partial requests
                    for mapping, partial_rq in zip(mappings, partial_requests):
                        if rq.debug:
                            partial_rq.set_debug()
                        mapping.resolve_access_request(partial_rq)
                    # Gather the results back
                    rq.gather(partial_requests)
                else:
                    if rq.debug:
                        iprint("%s: No further mapping for [0x%x:0x%x]" %
                               (self.name, rq.region.start, rq.region.stop))
                    raise self.NoMapping()
            

class AccessAlignmentError(ValueError):
    '''
    Used to indicate bad alignment of a request
    '''
    pass


class AccessRequest (object):
    """\
    AddressSpace-relative access request (abstract base)
    """
    type = AccessType.MEMORY
    
    def __init__(self, address_range, 
                 min_access_width=None,
                 max_access_width=None,
                 indirection_limit=None,
                 view=None,
                 block_id=0):
                
        TypeCheck(address_range, AddressRange)
        
        self._region = address_range
        self._indirection_limit=indirection_limit
        self._min_access_width = min_access_width
        self._max_access_width = max_access_width
        self._metadata = {}
        self._view = view
        self._block_id = block_id
        self._debug = False
    
    @property
    def region(self):        
        return self._region

    @property
    def length(self):        
        return self._region.length

    @property
    def view(self):
        return self._view
    
    @property
    def block_id(self):
        return self._block_id
    
    def set_debug(self, on_not_off=True):
        self._debug = on_not_off
        
    @property
    def debug(self):
        return self._debug

    def _mapped_request_helper(self, current_width, to_width, to_offset,
                               min_to_width = None, max_to_width = None):
        '''
        Calculate the mapped range and access width limits for the "to" space,
        for use in constructing the mapped Read/WriteAccessRequest
        '''
        mapped_range = self.region.transform(mult=current_width,
                                             div=to_width,
                                             offset=to_offset)
        min_from_width_mapped = (current_width * self._min_access_width // to_width
                               if self._min_access_width is not None else None)
        max_from_width_mapped = (current_width * self._max_access_width // to_width
                               if self._max_access_width is not None else None)
        
        if min_from_width_mapped is None:
            min_width = min_to_width
        elif min_to_width is None:
            min_width = min_from_width_mapped
        else:
            min_width = max(min_from_width_mapped, min_to_width)
            
        if max_from_width_mapped is None:
            max_width = max_to_width
        elif max_to_width is None:
            max_width = max_from_width_mapped
        else:
            max_width = min(max_from_width_mapped, max_to_width)
        
        return mapped_range, min_width, max_width

    def mapped_request(self, current_width, to_width, to_offset, le_space,
                       min_to_width = None, max_to_width = None,
                       view=None, block_id=0):
        '''
        Return a new AccessRequest that is the exact equivalent in the "to"
        space to the current request in the current space 
        
        Must be overridden by ReadRequest and WriteRequest
        '''
        raise PureVirtualError

    @property
    def access_width(self):
        
        # Get the best width allowed by the actual data alignment and the
        # supplied width restriction
        
        if self._max_access_width is None:
            width = self._region.max_access_width
        else:
            width = min(self._region.max_access_width, self._max_access_width)

        if self._min_access_width is not None and width < self._min_access_width:
            raise AccessAlignmentError("Access range [0x%x,0x%x) inconsistent "
                                       "with supplied minimum access alignment "
                                       "%d " % (self._region.start, 
                                                self._region.stop, 
                                                self._min_access_width) )
            
        return width

    @property
    def trimmed_access_width(self):
        """
        Trim the supplied access width if bounds have been set for this 
        AccessRequest
        """
        if self._min_access_width is not None:
            access_width = max(self._min_access_width, self.region.max_access_width)
        if self._max_access_width is not None:
            access_width = min(self._max_access_width, self.region.max_access_width)
        return access_width

    @property
    def new_aligned_region(self):
        """
        Return either a new region which satisfies the various
        constraints caused by the interaction of the alignment and
        the maximum and minimum access width constraints, or None, if the
        constraints are already satisfied. 
        """
        if self._max_access_width is None:
            max_width = self._region.max_access_width
        else:
            max_width = min(self._region.max_access_width, self._max_access_width)
        
        if max_width < self._min_access_width:
            # Set up a modified range that will satisfy the minimum width
            # requirements
            return self._region.minimal_aligned(self._min_access_width)
        
        return None

    @property
    def aligned_read(self):
        """
        Return the *read* access that This is used for
        WriteRequests where we need to do a RMW to satisfy the access constraints
        on the write. 
        """
        new_aligned_region = self.new_aligned_region
        
        if new_aligned_region:
            return ReadRequest(new_aligned_region, self._min_access_width, 
                                   self._max_access_width,
                                   block_id=self._block_id)
        return self


    @property
    def metadata(self):
        return self._metadata

    def merge_metadata(self, other_metadata):
        for key, other_value in other_metadata.items():
            try:
                value = self._metadata[key]
                if isinstance(value, list):
                    value += other_value
                elif isinstance(value, dict):
                    #Note: this means other_value wins in any conflicts
                    value.update(other_value)
                else:
                    raise TypeError("Merge-able AccessRequest metadata dict entries must be lists or dicts")
                
            except KeyError:
                self._metadata[key] = other_value
                
                
    
    def add_comment(self, comment):
        """
        Simple facility to allow resolvers or executors to comment on the access
        request as it is processed, so the client can be told useful things,
        such as what particular mux setting was used if it's not explicit in the
        object model (such as the Xap's host subsystem view mux)
        """
        try:
            self._metadata["comments"]
        except KeyError:
            self._metadata["comments"] = []
        self._metadata["comments"].append(interface.Code(comment))
        

class ReadRequest (AccessRequest):
    """\
    AddressSpace-relative read request.
    """
    _DATA_ATTRIBS = ["_region",
                     "_indirection_limit",
                     "_min_access_width",
                     "_max_access_width",
                     "_view",
                     "_block_id"]
    
    def __init__(self, address_range, min_access_width=1, 
                 max_access_width=1, view = None,
                 block_id = 0):
        
        AccessRequest.__init__(self, address_range, min_access_width,
                               max_access_width, 
                               view=view, block_id=block_id)        

    def __repr__(self):
        return "ReadRequest(%s)" % ",".join("%s=%s" % (name.lstrip("_"), getattr(self, name))
                                                        for name in self._DATA_ATTRIBS)

    def __eq__(self, other):
        return reduce(lambda a,b : a and b, 
                      (getattr(self,a)==getattr(other,a) for a in self._DATA_ATTRIBS),
                      True)
    # Extensions

    def __get_data(self):
        return self._data
                
    def __set_data(self, data):
        self._data = data
        
    data = property(__get_data, __set_data)

    def mapped_request(self, current_width, to_width, to_offset, le,
                       min_to_width = None, max_to_width = None,
                       view=None,block_id=0):
        range, min_width, max_width = \
                self._mapped_request_helper(current_width, to_width, to_offset,
                                            min_to_width, max_to_width)
        return ReadRequest(range, min_width, max_width, view=view,
                           block_id=block_id)
        
    def mapped_data(self, current_width, to_width, le):
        if le:
            return pack_unpack_data_le(self.data, current_width, to_width)
        else:
            return pack_unpack_data_be(self.data, current_width, to_width)

    def dealign_read_data(self, aligned_read):
        """
        Given an aligned read request, extract the subset of data it contains
        that we care about
        """
        aregion = aligned_read.region
        if aregion.start > self.region.start or aregion.stop < self.region.stop:
            raise ValueError("Aligned read [0x%x,0x%x) is not superset of "
                             "unaligned read [0x%x,0x%x)" % 
                             (aregion.start,aregion.stop, 
                              self.region.start, self.region.stop))
        stop_offset = -1*(aregion.stop-self.region.stop)
        if stop_offset == 0:
            stop_offset = None
        self.data = aligned_read.data[(self.region.start -
                                                    aregion.start): stop_offset]

    def scatter(self, subranges):
        """
        Return a partial copy of this request for the given subrange
        """
        subrequests = []
        for subrange in subranges:
            subrequests.append(ReadRequest(subrange,
                                           min_access_width = self._min_access_width,
                                           max_access_width = self._max_access_width,
                                           view = self._view, 
                                           block_id = self._block_id))
        return subrequests
        
    def gather(self, partial_requests):
        """
        Merge the supplied partial_requests into this master request
        """
        self._data = [None]*len(self.region)
        for partial_rq in partial_requests:
            self._metadata.update(partial_rq._metadata)
            start = partial_rq.region.start - self.region.start
            stop = partial_rq.region.stop - self.region.start
            self._data[start:stop] = partial_rq.data
            

class WriteRequest (AccessRequest):
    """\
    AddressSpace-relative read request.
    """
    _DATA_ATTRIBS = ["_region",
                     "_indirection_limit",
                     "_min_access_width",
                     "_max_access_width",
                     "_view",
                     "_block_id",
                     "_data"]
    
    def __init__(self, address_range, data, min_access_width=1, 
                 max_access_width=1, view = None, block_id = 0):
        
        AccessRequest.__init__(self, address_range, min_access_width,
                               max_access_width,
                               view=view, block_id=block_id)
        if len(data) != address_range.length:
            raise ValueError("Data array length %d; address_range length %d"
                              % (len(data),address_range.length))
        self._data = data

    def __repr__(self):
        return "WriteRequest(%s)" % ",".join("%s=%s" % (name.lstrip("_"), 
                                                        getattr(self, name))
                                                        for name in self._DATA_ATTRIBS)

    def __eq__(self, other):
        return reduce(lambda a,b : a and b, 
                      (getattr(self,a)==getattr(other,a) for a in self._DATA_ATTRIBS),
                      True)
    
    # Extensions
        
    @property
    def data(self):
        return self._data

    def mapped_request(self, current_width, to_width, to_offset, le,
                       min_to_width = None, max_to_width = None,
                       view=None,block_id=0):
        range, min_width, max_width = \
                self._mapped_request_helper(current_width, to_width, to_offset,
                                            min_to_width, max_to_width)
        
        if le:
            data = pack_unpack_data_le(self.data, current_width, to_width)
        else:
            data = pack_unpack_data_be(self.data, current_width, to_width)
        
        return WriteRequest(range, data, min_width, max_width, 
                            view=view, block_id=block_id)

    def realign_write_data(self, aligned_read_data):
        """
        Create a WriteRequest that replaces the write data in this request with
        an extended copy that satisfies the access width constraints.  If the
        original request already satisfies the constraints, return it instead.
        """
        if len(aligned_read_data) > self.length:
            
            new_aligned_region = self.new_aligned_region
            real_data_offset = self.region.start-new_aligned_region.start 
            aligned_read_data[real_data_offset: real_data_offset + self.length] = self.data
            return WriteRequest(new_aligned_region, aligned_read_data,
                                self._min_access_width, self._max_access_width,
                                block_id = self._block_id)
        return self
        
    def scatter(self, subranges):
        """
        Return a list of partial copies of this request corresponding to the 
        given subranges.
        Note: copies the subrange of data - could provide a view, although it
        may be assumed subsequently that the data really are in a list.
        """
        subrequests = []
        for subrange in subranges:
            data_start = subrange.start - self.region.start
            data_stop = subrange.stop - self.region.start
            subrequests.append(WriteRequest(subrange, self._data[data_start:data_stop],
                                            min_access_width = self._min_access_width,
                                            max_access_width = self._max_access_width,
                                            view = self._view, 
                                            block_id = self._block_id))
        return subrequests

    def gather(self, partial_requests):
        """
        Merge the supplied partial_requests into this master request.  Not
        much to do for a write request
        """
        for partial_rq in partial_requests:
            self._metadata.update(partial_rq._metadata)

class BankedAddressSpace(AddressSlavePort):
    """
    Simple specialisation of AddressSlavePort that allows coredump loads to
    store banked data.  Every time an access is made, the bank select register
    is read and the corresponding cache is accessed, being created on the fly if
    it doesn't yet exist.  In the non-coredump case, this just means we'll have
    a dictionary full of NullAccessCaches, which is mildly wasteful but no big
    deal - each one will simply forward the request back out to the same place. 
    
    In the coredump case, data access will go to the current active bank. This
    works for loading as well as for subsequent accesses, because the coredump
    loader does a write of the select register before it loads the corresponding
    bank of values.
    """
    
    def __init__(self, select_reg_name, core_or_subsystem, *args, **kwargs):
        
        AddressSlavePort.__init__(self, *args, **kwargs);
        
        # It would be easier just to pass an accessor for selector register,
        # but we can't do that. CSRA68100AppsSubsystem.__init__ adds
        # banked registers to the common registers mapping and at that stage
        # we can't reference a "core" object because it leads to initialization
        # loop.
        # Hence this rather complex approach - instead of the accessor we
        # pass a selector register name and an object which can be either
        # a core or a submsystem (with the core attribute).
        # We use these to reference core.fields["selector_name"] the first
        # time it is needed.
        self._selector_name = select_reg_name
        self._core_or_subsystem = core_or_subsystem
        
        self._caches = {}
        
        # We don't override resolve_access_request because we're never going to
        # map inwards from a banked space
        
    @property
    def selector(self):
        try:
            self._selector
        except AttributeError:
            try:
                core = self._core_or_subsystem.core
            except AttributeError:
                core = self._core_or_subsystem

            self._selector = core.field_refs[self._selector_name]
        return self._selector
        
    def execute_here(self, access_request):
        """
        Forward the access_request to whatever bank the select register currently
        points to.  If this bank hasn't been accessed before, create an empty
        cache and pass the request in.  In the case of a coredump load the first
        access should be a write which fully populates the cache 
        """
        try:
            selector = self.selector.read()
        except AddressSpace.NoAccess:
            # The select register hasn't been written yet: assume by default it's
            # zero
            selector = 0
        if access_request.debug:
            iprint(" %s: accessing with selector set to 0x%x" % (self.name,selector))
        self._caches.setdefault(selector, self._cache_type(self)).execute(access_request)

    def _extend_access_path(self, access_path):
        """
        No extension to be done here: banked blocks never map to lower-level
        address buses that we care about
        """
        pass


class BankedAddressMap(object):
    """
    AddressMap-like class that creates a new (real) AddressMap instance for
    each value of a bank select register, on demand.  Access request resolution
    is forwarded to the appropriate bank's AddressMap instance; outward execution
    from a bank's AddressMap works as normal because any access path 
    passed for extension by the BankedAddressMap's parent is saved and extended
    into each bank AddressMap at creation time, so each bank AddressMap has its
    own copy of the route back to the parent AddressMasterPort.  Note that in
    practice, it would be possible to optimise the class for the non-coredump
    case by short-circuiting the bank spaces completely: we can simply assume
    that inward resolution is impossible and jump to executing outwards  
    """
    class BankedInPort(AddressSlavePort):
        """
        SlavePort interface to a set of AddressMaps created on demand.   This
        is the class whose instances connect to AddressMasterPorts of higher-
        level mappings.  However, when access paths are extended through this
        class it just passes them to the BankedAddressMap instance which creates
        the necessary outward mapping route in each AddressMaps when it 
        constructs them.
        """    
        def __init__(self, map, *args, **kwargs):
            
            AddressSlavePort.__init__(self, *args, **kwargs)
            self._map = map

        def resolve_access_request(self, rq):
            """
            Forward an access request on to the appropriate banked map
            
            In the non-coredump case we *should* just go straight back out by
            calling execute_outwards on ourselves because we know that's 
            effectively what will happen anyway.  However we don't have that 
            information in general at this point.
            """
            bank_map = self._map.get_bank_map()
            bank_map.port.resolve_access_request(rq)
            
        def _extend_access_path(self, access_path):
            """
            Pass the AccessPath to the underlying map to worry about.
            """
            self._map.extend_access_path(access_path)

    
    def __init__(self, select_reg_name, core_or_subsystem, 
                 *args, **kwargs):
        
        self._in_port = self.BankedInPort(self, *args, **kwargs)
        self._in_port_args, self._in_port_kwargs = args, kwargs
        self._mapping_cons = []
        self._bank_maps = {}
        self._parent_access_paths = []
        
        self._selector_name = select_reg_name
        self._core_or_subsystem = core_or_subsystem
        
    @property
    def port(self):
        return self._in_port

    def add_mapping(self, start, stop, to_space_type, *args, **kwargs):
        """
        Register a mapping to be created with a distinct instance for every
        value of the bank select.  Note that the object to be constructed can
        be either an AddressMap-like object or an AddressSlavePort. If the
        object has an attribute "port" it's assumed to be the former; otherwise 
        we act as though it's the latter.
        """
        try:
            autofill = kwargs["autofill"]
            del kwargs["autofill"]
        except KeyError:
            autofill = False
        self._mapping_cons.append((start, stop, 
                                   CloneFactory(to_space_type, *args, **kwargs),
                                   autofill))

    def extend_access_path(self, access_path):
        """
        Save the access_path object in order to register it with bank maps as
        and when they are created.
        """
        self._parent_access_paths.append(access_path)

    @property
    def selector(self):
        try:
            self._selector
        except AttributeError:
            try:
                core = self._core_or_subsystem.core
            except AttributeError:
                core = self._core_or_subsystem

            self._selector = core.field_refs[self._selector_name]
        return self._selector

    def get_bank_map(self):
        """
        Return an AddressMap for the current value of the selector register,
        creating it it if necessary.
        """
        try:
            bank_selector = self.selector.read()
        except AddressSpace.NoAccess:
            # The select register hasn't been written yet: assume by default it's
            # zero
            bank_selector = 0
            
        try:
            return self._bank_maps[bank_selector]
        except KeyError:
            # Create the mapping
            bank_map = self._create_bank_map()
            # Remember this one has been built
            self._bank_maps[bank_selector] = bank_map
            return bank_map

    def _create_bank_map(self):
        """
        Create a new underlying AddressMap with all its Mappings for this value 
        of the selector, and extend the parent access path through it.  
        """
        map = AddressMap(*self._in_port_args, **self._in_port_kwargs)
        for start, stop, mapping_factory, autofill in self._mapping_cons:
            mapping = mapping_factory()
            try:
                # Assume we've created an AddressMap-like object, so we want to
                # connect to its port.
                map.add_mapping(start, stop, mapping.port, autofill=autofill)
            except AttributeError:
                # However we may have created a simple leaf space which is 
                # itself the port.
                map.add_mapping(start, stop, mapping, autofill=autofill)
        # Connect it to the parent spaces via the saved access path
        for pth in self._parent_access_paths:
            map.port.register_access_path(pth)
        
        return map

