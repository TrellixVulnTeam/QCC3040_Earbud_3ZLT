############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides a DebugController which supports DebugReadRequest and DebugWriteRequest
objects for operations fielded by a GeneralRegisterPort or RunCtrlPort object
as appropriate to the type of request.
"""

import contextlib
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.model import interface
from csr.wheels.bitsandbobs import dwords_to_bytes
from csr.dev.adaptor.text_adaptor import TextAdaptor
from ..address_space import AddressMasterPort, AddressConnection, AddressRange,\
AccessPath, ReadRequest, WriteRequest
from ..port_connection import SlavePort, AccessType, NoAccess
from ...model.base_component import BaseComponent
from .mixin.is_in_hydra import IsInHydra

def create_access_request(access_type, read_not_write, meta, data=None):
    "Create a DebugReadRequest else DebugWriteRequest if data supplied"
    if read_not_write:
        return DebugReadRequest(access_type, meta)
    return DebugWriteRequest(access_type, meta, data)

class DebugAccessRequest(object):
    """
    Common base class for holding common details of a debug access request
    these being the type and meta data.
    """
    def __init__(self, access_type, meta):
        self._type = access_type
        self._meta = meta

    @property
    def type(self):
        "Accessor to the type of the DebugAccessRequest instance"
        return self._type

    @property
    def meta(self):
        "Accessor to the meta data of the DebugAccessRequest instance"
        return self._meta

class DebugReadRequest(DebugAccessRequest):
    "A debug mode read request of supplied type and meta data"

class DebugWriteRequest(DebugAccessRequest):
    "A debug mode write request of supplied type and meta data along with data"
    def __init__(self, access_type, meta, data):

        DebugAccessRequest.__init__(self, access_type, meta)
        self.data = data


class GeneralRegisterPort(BaseComponent):
    """
    Provides read and write methods for operations on a general register
    in debug mode.
    """
    def __init__(self, debug_ctrl, core_register_names,
                 run_ctrl):

        self._debug_ctrl = debug_ctrl
        self._register_names = core_register_names
        self._run_ctrl = run_ctrl

    def read(self, regids):
        "Reads one or more general registers specified in regids"
        rq = create_access_request(AccessType.REGISTERS, True, regids)
        self._debug_ctrl.execute_request(rq)
        return rq.data[:]

    def write(self, regids, values):
        "Writes one or more general registers specified in regids"
        rq = create_access_request(AccessType.REGISTERS, False, regids, values)
        self._debug_ctrl.execute_request(rq)




class RunCtrlPort(BaseComponent):
    """
    Drives an arbitrary underlying transport that supports
     - pause, step, run
     - breakpoint setting and clearing by address

    Enhances breakpoint logic with concept of breakpoint IDs, enabling and
    disabling.  This all relies on Pydbg knowing what is currently set in the
    chip, because it has no way of retrieving state from the chip.  There's not
    even a way to clear all breakpoints from the chip, so support is a bit
    limited.

    TODO Add thread info to the pause/step/run commands
    """
    SET_BRK = 0
    DEL_BRK = 3
    SET_WATCH = 4

    PAUSE = 0x80
    STEP = 0x81
    RUN = 0x82
    RESET = 0x83

    def __init__(self, debug_ctrl):
        """
        Construct a RunCtrlPort with a DebugController instance
        """
        self._debug_ctrl = debug_ctrl

        # Internal record of the breakpoints the user has set
        self._brks = {}
        self._next_brk_id = 0

    def _on_reset(self):
        """
        Disable all the current breakpoints if the chip gets reset
        """
        if self._brks:
            iprint("Disabling breakpoints")
        for brk_id, (address, _) in self._brks:
            self._brks[brk_id] = (address, False)

    def _get_brk(self, brk_id):
        "Looks up details of a breakpoint specified by brk_id"
        if brk_id not in self._brks:
            raise ValueError("No such breakpoint %d" % brk_id)
        return self._brks[brk_id]

    def brk_set(self, addr):
        """
        Set a breakpoint at the given program address, returning a handle for
        use in subsequent manipulation of the breakpoint.
        """

        rq = create_access_request(AccessType.RUN_CTRL, False, self.SET_BRK,
                                   addr)
        self._debug_ctrl.execute_request(rq)

        brk_id = self._next_brk_id
        self._next_brk_id += 1
        self._brks[brk_id] = (addr, True)
        return brk_id

    def brk_delete(self, brk_id):
        """
        Delete the given breakpoint, identified by its handle
        """
        address, _ = self._get_brk(brk_id)
        self.brk_delete_address(address)

    @contextlib.contextmanager
    def managed_brk(self, addr):
        """
        Context manager that sets and clears a breakpoint around a block of code
        """
        brk_id = self.brk_set(addr)
        yield
        self.brk_delete(brk_id)

    def brk_delete_address(self, address):
        """
        Delete any breakpoint at the given address, without reference to what
        has been set.  If this did in fact correspond to a breakpoint known to
        the RunCtrlPort, marks it as deleted and returns the (now stale) break
        ID. Otherwise returns None.
        """
        rq = create_access_request(AccessType.RUN_CTRL, False, self.DEL_BRK,
                                   address)
        self._debug_ctrl.execute_request(rq)
        for brk_id, (brk_address, _) in self._brks.items():
            if brk_address == address:
                del self._brks[brk_id]
                return brk_id
        return None

    def brk_enable(self, brk_id):
        """
        Enable the given breakpoint, if necessary
        """
        address, enabled = self._get_brk(brk_id)
        if not enabled:
            rq = create_access_request(AccessType.RUN_CTRL, False, self.SET_BRK,
                                       address)
            self._debug_ctrl.execute_request(rq)
            self._brks[brk_id] = address, True

    def brk_disable(self, brk_id):
        """
        Disable the given breakpoint, if necessary
        """
        address, enabled = self._get_brk(brk_id)
        if enabled:
            rq = create_access_request(AccessType.RUN_CTRL, False, self.DEL_BRK,
                                       address)
            self._debug_ctrl.execute_request(rq)
        self._brks[brk_id] = address, False

    def brk_address(self, brk_id):
        """
        Look up the program address associated with the given breakpoint
        """
        address, _ = self._get_brk(brk_id)
        return address

    def brk_is_enabled(self, brk_id):
        """
        Is the given breakpoint currently enabled?
        """
        _, enabled = self._get_brk(brk_id)
        return enabled

    def brk_display(self, report=False):
        """
        Return a Table describing the current set of breakpoints
        """
        output = interface.Group()
        breakpts = interface.Table(headings=["ID", "Address", "Enabled?"])
        for brk_id, (address, enabled) in self._brks.items():
            breakpts.add_row(
                [brk_id, "0x%08x"%address, "Y" if enabled else "N"])
        output.append(breakpts)
        if report:
            return output
        TextAdaptor(output, gstrm.iout)
        return None

    def run(self):
        """
        Run the processor
        """
        rq = create_access_request(AccessType.RUN_CTRL, False, self.RUN)
        self._debug_ctrl.execute_request(rq)

    def pause(self):
        """
        Pause the processor
        """
        rq = create_access_request(AccessType.RUN_CTRL, False, self.PAUSE)
        self._debug_ctrl.execute_request(rq)

    def step(self):
        """
        Step the processor
        """
        rq = create_access_request(AccessType.RUN_CTRL, False, self.STEP)
        self._debug_ctrl.execute_request(rq)

    def reset(self, level):
        """
        Resets the processor
        """
        rq = create_access_request(AccessType.RUN_CTRL, False, self.RESET,
                                   level)
        self._debug_ctrl.execute_request(rq)

class CortexMRunCtrlPort(RunCtrlPort):
    """
    Drives an arbitrary underlying transport that supports
     - pause, step, run
     - breakpoint setting and clearing by address
    Although this derived Class is tagged as CortexM, its functionality is
    more related to ensuring the number of enabled breakpoints never exceeds
    that supported by hardware. It is envisaged that this could be renamed
    to a more generic name in the future should additional CPU architectures
    be used.
    """

    def __init__(self, debug_ctrl):
        super(CortexMRunCtrlPort, self).__init__(debug_ctrl=debug_ctrl)

    def _get_function_state(self, function):
        return self._debug_ctrl.core.DWT_FUNCTIONS[
            function & self._debug_ctrl.core.DWT_FUNCTION_MASK]

    def _get_match_state(self, function):
        return ("Y" if function & self._debug_ctrl.core.DWT_MATCHED_MASK
                else "N")

    def watch_display(self, report=False):
        """
        Return a report describing the current set of watchpoints
        """
        output = interface.Group()
        debug_regs = self._debug_ctrl.core_debug_regs
        dataw = self._debug_ctrl.core.dataw
        watchpts = interface.Table(headings=[
            "ID", "Comparator", "Mask", "Function", "Matched"])
        for wp_id in range(dataw[debug_regs("DWT_CTRL")] >>
                           self._debug_ctrl.core.DWT_NUMCOMP_BIT_POS):
            function = dataw[debug_regs("DWT_FUNCTION%d" % wp_id)]
            watchpts.add_row(
                [wp_id, "0x%08x" % dataw[debug_regs("DWT_COMP%d" % wp_id)],
                 "0x%08x" % dataw[debug_regs("DWT_MASK%d" % wp_id)],
                 "%s" % self._get_function_state(function),
                 "%s" % self._get_match_state(function)])
        output.append(watchpts)
        if report:
            return output
        TextAdaptor(output, gstrm.iout)
        return None

    def watch_set(self, wp_id, address, mask, function):
        'Sets a debug watchpoint'
        info = [wp_id, address, mask, function]
        rq = create_access_request(AccessType.RUN_CTRL, False, self.SET_WATCH,
                                   info)
        self._debug_ctrl.execute_request(rq)

    def watch_delete(self, wp_id):
        'Deletes/disables a debug watchpoint'
        address = 0
        mask = 0
        function = 0 # Disable watchpoint
        info = [wp_id, address, mask, function]
        rq = create_access_request(AccessType.RUN_CTRL, False, self.SET_WATCH,
                                   info)
        self._debug_ctrl.execute_request(rq)

    @property
    def _brk_enabled_count(self):
        """
        Returns the number of currently enabled breakpoints
        """
        return sum([x[1] for x in self._brks.values()])

    def get_brks(self):
        """
        Return the breakpoint database
        """
        return self._brks

    def get_next_brk_id(self):
        """
        Return next break id
        """
        return self._next_brk_id

    def brk_enable(self, brk_id):
        """
        Enable the given breakpoint, if necessary
        """
        address, enabled = self._get_brk(brk_id)
        if not enabled:
            # Issue a warning, via a display report, if the number of enabled
            # breakpoints is at the limit supported by h/w
            if self._debug_ctrl.hw_breakpoint_limit is not None:
                if (self._brk_enabled_count >=
                        self._debug_ctrl.hw_breakpoint_limit):
                    self.brk_display(report=False)
                    return
            rq = create_access_request(AccessType.RUN_CTRL, False, self.SET_BRK,
                                       address)
            self._debug_ctrl.execute_request(rq)
            self._brks[brk_id] = address, True

    def brk_set(self, addr):
        """
        Set a breakpoint at the given program address, returning a handle for
        use in subsequent manipulation of the breakpoint.
        """
        # Issue a warning, via a display report, if the number of enabled
        # breakpoints is already at the limit supported by h/w
        if self._debug_ctrl.hw_breakpoint_limit is not None:
            if self._brk_enabled_count >= self._debug_ctrl.hw_breakpoint_limit:
                self.brk_display(report=False)
                return None
        rq = create_access_request(AccessType.RUN_CTRL, False, self.SET_BRK,
                                   addr)
        self._debug_ctrl.execute_request(rq)

        brk_id = self._next_brk_id
        self._next_brk_id += 1
        self._brks[brk_id] = addr, True
        return brk_id

    def brk_display(self, report=False):
        """
        Return a Table describing the current set of breakpoints
        """
        output = interface.Group()
        breakpts = interface.Table(headings=["ID", "Address", "Enabled?"])
        for brk_id, (address, enabled) in self._brks.items():
            breakpts.add_row(
                [brk_id, "0x%08x"%address, "Y" if enabled else "N"])
        # Issue a warning if the number of enabled breakpoints is at the limit
        # supported by h/w
        if self._debug_ctrl.hw_breakpoint_limit is not None:
            if self._brk_enabled_count >= self._debug_ctrl.hw_breakpoint_limit:
                warn = '%d breakpoints enabled. h/w limit is %d'%(
                    self._brk_enabled_count,
                    self._debug_ctrl.hw_breakpoint_limit)
                output.append(interface.Warning(warn))

        output.append(breakpts)
        if report:
            return output
        TextAdaptor(output, gstrm.iout)
        return None

class BasicDebugController(BaseComponent):
    """
    Very simple debug controller that only knows about memory access
    transactions
    """
    def __init__(self, core_memory_slave, core=None):

        self._core_mem_master = self.CoreMemoryBusMaster(
            self, core_memory_slave)
        self._slave = self.DebugControllerSlave(self, core=core)


    @property
    def slave(self):
        "Accessor to the DebugControllerSlave port property"
        return self._slave


    class CoreMemoryBusMaster(AddressMasterPort):
        """
        Represents a MasterPort for accessing core memory across the bus
        """
        def __init__(self, port, memory_slave):

            AddressMasterPort.__init__(self)

            self._auto_connection = AddressConnection(self, memory_slave)
            self._debug_ctrl = port

        def execute_outwards(self, access_request):

            read_not_write = isinstance(access_request, ReadRequest)
            data = None if read_not_write else access_request.data
            rq = create_access_request(AccessType.MEMORY, read_not_write,
                                       access_request.region, data)
            self._debug_ctrl.slave.execute_outwards(rq)
            if read_not_write:
                access_request.data = rq.data

        def extend_access_path(self, access_path):
            """
            Fork the access path with an address-aware path for the
            AddressSlavePort we're mastering.
            """
            new_fork = AccessPath(self.slave.name, access_path.rank+1, self,
                                  AddressRange(0, self.slave.length()))
            access_path.add_fork(new_fork)


    class DebugControllerSlave(SlavePort):
        """
        Represents a SlavePort for accessing Debug Controller facilities
        """
        def __init__(self, debug_ctrl, core=None):
            SlavePort.__init__(self)

            self._debug_ctrl = debug_ctrl
            # In some architectures we need to set the "has_data_source" flag
            # on the core object, because there is no intervening mux 
            # through which we can track the transport connection being made.
            self._core = core

        def execute(self, access_request):
            """\
            Execute an access request w.r.t. this AddressSpace (synchronously).

            Raises:-
            - NoAccess: No access path currently available.
            """
            self.resolve_access_request(access_request)

        # AddressSlavePort
        def resolve_access_request(self, access_request):
            self._debug_ctrl.execute_request(access_request)

        def _extend_access_path(self, path):
            self._debug_ctrl.extend_access_path(path)
            try:
                self._core.has_data_source = True
            except AttributeError:
                print("No core object to set flag on")

        def execute_outwards(self, access_request):
            """
            Figure out what sort of debug transport we're connected to and
            whether we have to simplify the request we've been passed to execute
            """
            path = self._get_supporting_access_path(access_request)
            if path:
                path.execute_outwards(access_request)

            else:
                raise NoAccess(
                    "No access paths or couldn't reduce to memory accesses")

    def reset(self):
        "Perform a reset access request"
        reset_rq = create_access_request(AccessType.MISC, False, "reset")
        self.slave.execute_outwards(reset_rq)

    def jtagconf(self, irpre, drpre):
        "Perform a jtagconf access request"
        jtagconf_rq = create_access_request(AccessType.MISC, False, "jtagconf",
                                            [irpre, drpre])
        self.slave.execute_outwards(jtagconf_rq)

    def execute_request(self, rq):
        """
        Tries to execute the access request rq first by forwarding outwards
        but if there is no access path then try to convert it to
        a sequence of memory accesses.
        """
        self.slave.execute_outwards(rq)

    def extend_access_path(self, access_path):
        """\
        Extend an AccessPath via this mapping.

        Ignores paths that do not intersect this Mapping.
        """
        self._core_mem_master.extend_access_path(access_path)


class DebugController(BasicDebugController):
    """
    Shim layer that handles multiple types of access request, including
     - memory accesses, which are forwarded to the core's memory slave
     - general register accesses, which are forwarded to the core's register
     API
     - run control accesses, e.g. managing breakpoints, running, pausing
     and stepping.
    """

    class DebugControllerSlave(BasicDebugController.DebugControllerSlave):
        """
        Represents a SlavePort for accessing Debug Controller facilities
        as per the base class and with added properties to access the
        containing DebugControllers object's general register port
        and its run_ctrl port.
        """

        @property
        def regs(self):
            """
            Accessor to the containing DebugController object's
            general register port
            """
            return self._debug_ctrl.regs

        @property
        def run_ctrl(self):
            """
            Accessor to the containing DebugController object's
            run_ctrl port
            """
            return self._debug_ctrl.run_ctrl



    def __init__(self, core_memory_slave, core_register_names,
                 core_debug_regs=None, core=None):

        BasicDebugController.__init__(self, core_memory_slave, core=core)

        self._run_ctrl = self._run_ctrl_port
        self._register_port = GeneralRegisterPort(self, core_register_names,
                                                  self._run_ctrl)
        self._core_debug_regs = core_debug_regs


    @property
    def _run_ctrl_port(self):
        """
        Return the DebugController object's run ctrl port
        """
        return RunCtrlPort(self)

    @property
    def hw_breakpoint_limit(self):
        """
        Return the number of breakpoints supported by h/w or None if no limit
        """
        return None

    @property
    def subcomponents(self):
        return {"regs": "_register_port",
                "run_ctrl" : "_run_ctrl"}

    @property
    def regs(self):
        "Accessor to the GeneralRegisterPort property"
        return self._register_port

    @property
    def run_ctrl(self):
        "Accessor to the RunCtrlPort property"
        return self._run_ctrl

    def _reg_access_to_memory_access(self, rq):
        assert rq.type == AccessType.REGISTERS
        if self._core_debug_regs is None:
            raise NoAccess("No core debug registers available: can't "
                           "implement reg access as memory access!")
        requests, reads = [], []
        for reg in rq.meta:
            write_request = hasattr(rq, 'data') and rq.data is not None
            # if writing rq.data, we assume processor is already halted
            # we don't change the halted status of the processor -
            # leave that to the user.

            # if writing first supply the data to be written in DCRDR
            # need to convert from (presumed LE) array of dwords
            if write_request:
                sel_rq = WriteRequest(
                    AddressRange(self._core_debug_regs("DCRDR"),
                                 self._core_debug_regs("DCRDR")+4),
                    dwords_to_bytes(rq.data),
                    max_access_width=None)
                requests.append(sel_rq)

            # Select which register is being accessed:
            # set bit-16 if writing rq.data
            sel_rq = WriteRequest(
                AddressRange(self._core_debug_regs("DCRSR"),
                             self._core_debug_regs("DCRSR")+4),
                [reg, 0, 1 if write_request else 0, 0],
                max_access_width=None)
            requests.append(sel_rq)

            if not write_request:
                read_rq = ReadRequest(
                    AddressRange(self._core_debug_regs("DCRDR"),
                                 self._core_debug_regs("DCRDR")+4),
                    max_access_width=None)
                requests.append(read_rq)
                reads.append(read_rq)

        return requests, reads

    def _run_ctrl_to_memory_access(self, rq):
        assert rq.type == AccessType.RUN_CTRL
        if self._core_debug_regs is None:
            raise NoAccess("No core debug registers available: can't "
                           "implement run ctrl as memory access!")
        raise NotImplementedError(
            "Don't know how to perform run control accesses "
            "via register pokes!")

    def execute_request(self, rq):
        """
        Tries to execute the access request rq first by forwarding outwards
        but if there is no access path then try to convert it to
        a sequence of memory accesses.
        """
        try:
            self.slave.execute_outwards(rq)
        except NoAccess:
            if rq.type in (AccessType.REGISTERS, AccessType.RUN_CTRL):
                # Perhaps we can reduce these to memory accesses
                if rq.type == AccessType.REGISTERS:
                    requests, reads = self._reg_access_to_memory_access(rq)
                else:
                    # pylint: disable=assignment-from-no-return
                    requests, reads = self._run_ctrl_to_memory_access(rq)
                for req in requests:
                    self._core_mem_master.resolve_access_request(req)
                # Now the reads will have had data inserted into them
                if reads:
                    rq.data = sum((rd.data for rd in reads), [])
            else:
                raise

class CortexMDebugController(DebugController):
    """
    Debug Controller Class for Arm CortexM
    """
    def __init__(self, core_memory_slave, core_register_names, core,
                 core_debug_regs=None):
        super(CortexMDebugController, self).__init__(
            core_memory_slave=core_memory_slave,
            core_register_names=core_register_names,
            core_debug_regs=core_debug_regs
        )
        self._core = core

    @property
    def core(self):
        """
        Return the DebugController object's core
        """
        return self._core

    @property
    def core_debug_regs(self):
        """
        Return the DebugController object's core debug regs
        """
        return self._core_debug_regs

    @property
    def _run_ctrl_port(self):
        """
        Return the DebugController object's run ctrl port
        """
        return CortexMRunCtrlPort(self)

    @property
    def hw_breakpoint_limit(self):
        """
        Return the number of breakpoints supported by h/w or None if no limit
        """
        return self._core.arm_regs.BP_CTRL.NUM_COMP.read()

    def _run_ctrl_set_bpu_info(self, rq):
        '''
        Construct the breakpoint request state required for
        setting up the BPU.
        '''
        brks = self.run_ctrl.get_brks().copy()
        if rq.meta == RunCtrlPort.SET_BRK:
            # a SET_BRK can come from a brk_set or brk_enable
            # command. For the former
            try:
                # check if a brk_enable
                address, enabled = brks[self.run_ctrl.get_next_brk_id()]
                brks[self.run_ctrl.get_next_brk_id()] = address, True
            except KeyError:
                # brk_set so update the brks database
                brks[self.run_ctrl.get_next_brk_id()] = rq.data, True
        elif rq.meta == RunCtrlPort.DEL_BRK:
            # we only get this command if there is an existing enabled address
            for key in brks:
                address, enabled = brks[key]
                if address == rq.data and enabled:
                    brks[key] = address, False

        # now construct the breakpoint info
        bp_info = {}
        bp_index = 0
        for key in sorted(brks):
            # we will only use up to the number of comparators supported
            address, enabled = brks[key]
            if enabled:
                bp_info[bp_index] = address
                bp_index += 1
                if bp_index == self.hw_breakpoint_limit:
                    break
        return bp_info

    def _create_write_request(self, address, data): #pylint:disable=no-self-use
        return WriteRequest(
            AddressRange(address, address + 4),
            dwords_to_bytes([data]),
            max_access_width=None)

    def _get_bpt_comp_value(self, bp_info, comparator): #pylint:disable=no-self-use
        '''
        Return the 32 bit value for programming the BP_COMPx comparator with
        '''
        full_match = 1 # for checking if a full word match is required
        try:
            address = bp_info[comparator]
            if address & full_match:
                # break on upper or lower half words
                return (
                    (address & self._core.BP_COMP_ADDR_MASK) |
                    self._core.BP_COMP_MATCH_UPPER |
                    self._core.BP_COMP_MATCH_LOWER |
                    self._core.BP_COMP_ENABLE)
            # convert address
            if address & 2:
                # break on upper half word
                return (
                    (address & self._core.BP_COMP_ADDR_MASK) |
                    self._core.BP_COMP_MATCH_UPPER |
                    self._core.BP_COMP_ENABLE)
            # break on lower half word
            return (
                (address & self._core.BP_COMP_ADDR_MASK) |
                self._core.BP_COMP_MATCH_LOWER |
                self._core.BP_COMP_ENABLE)
        except KeyError:
            return 0

    def _run_ctrl_to_memory_access(self, rq):
        assert rq.type == AccessType.RUN_CTRL
        if self._core_debug_regs is None:
            raise NoAccess("No core debug registers available: can't "
                           "implement run ctrl as memory access!")
        requests, reads = [], []
        # Force enable of the DAP clock as this is require for proper
        # operation of the comparators
        if isinstance(self._core, IsInHydra):
            clkgen_force_enables = self._core.regs.CLKGEN_FORCE_ENABLES
            clkgen_force_enables.CLKGEN_FORCE_DAP_DEBUG_EN_CLK.write(1)
        # get the breakpoint info
        bp_info = self._run_ctrl_set_bpu_info(rq)
        if rq.meta == RunCtrlPort.SET_BRK or rq.meta == RunCtrlPort.DEL_BRK:
            # construct write requests for setting breakpoints
            # Set C_HALT  & C_DEBUG in DHCSR register
            requests.append(self._create_write_request(
                self._core_debug_regs("DHCSR"),
                self._core.DHCSR_DBGKEY |
                self._core.arm_regs.DHCSR.C_HALT.mask |
                self._core.arm_regs.DHCSR.C_DEBUGEN.mask))
            # Disable breakpoints while we set them up
            bpctrl = (
                self._core.arm_regs.BP_CTRL.KEY.mask +
                (self.hw_breakpoint_limit <<
                 self._core.arm_regs.BP_CTRL.NUM_COMP.start_bit))
            requests.append(self._create_write_request(
                self._core_debug_regs("BP_CTRL"), bpctrl))
            # update the BP_COMPx registers
            bpu_base = self._core_debug_regs("BP_COMP0")
            # disable all comparators first
            for comparator in range(self.hw_breakpoint_limit):
                requests.append(self._create_write_request(
                    bpu_base +
                    comparator *
                    self._core.info.layout_info.addr_units_per_code_word, 0))
            # only enable BPU if breakpoints are present
            if 0 in bp_info:
                bpctrl += self._core.arm_regs.BP_CTRL.ENABLE.mask
                requests.append(self._create_write_request(
                    self._core_debug_regs("BP_CTRL"), bpctrl))
                # set/enable breakpoints
                for comparator in range(self.hw_breakpoint_limit):
                    comp_value = self._get_bpt_comp_value(bp_info, comparator)
                    requests.append(self._create_write_request(
                        bpu_base +
                        comparator *
                        self._core.info.layout_info.addr_units_per_code_word,
                        comp_value))
            # clear DFSR
            requests.append(self._create_write_request(
                self._core_debug_regs("DFSR"),
                self._core.arm_regs.DFSR.mask))
            # run processor
            requests.append(self._create_write_request(
                self._core_debug_regs("DHCSR"),
                self._core.DHCSR_DBGKEY |
                self._core.arm_regs.DHCSR.C_DEBUGEN.mask))
        elif rq.meta == RunCtrlPort.SET_WATCH:
            wp_id, comparator, mask, function = rq.data
            # Set C_HALT  & C_DEBUG in DHCSR register
            requests.append(self._create_write_request(
                self._core_debug_regs("DHCSR"),
                self._core.DHCSR_DBGKEY |
                self._core.arm_regs.DHCSR.C_HALT.mask |
                self._core.arm_regs.DHCSR.C_DEBUGEN.mask))
            # enable DWT
            requests.append(self._create_write_request(
                self._core_debug_regs("DEMCR"),
                self._core.arm_regs.DEMCR.DWTENA.mask))
            # set watchpoint
            if wp_id not in (0, 1):
                raise ValueError(
                    "wp_id value should be 0 or 1 not %d" % wp_id)
            if function not in (0, 4, 5, 6, 7):
                raise ValueError(
                    "function value should be 0, 4, 5, 6 or 7 not %d" %
                    function)
            requests.append(self._create_write_request(
                self._core_debug_regs("DWT_FUNCTION%d" % wp_id), function))
            requests.append(self._create_write_request(
                self._core_debug_regs("DWT_MASK%d" % wp_id), mask))
            requests.append(self._create_write_request(
                self._core_debug_regs("DWT_COMP%d" % wp_id), comparator))
            # clear DFSR
            requests.append(self._create_write_request(
                self._core_debug_regs("DFSR"),
                self._core.arm_regs.DFSR.mask))

            # run processor
            requests.append(self._create_write_request(
                self._core_debug_regs("DHCSR"),
                self._core.DHCSR_DBGKEY |
                self._core.arm_regs.DHCSR.C_DEBUGEN.mask))
        elif rq.meta == RunCtrlPort.PAUSE:
            requests.append(self._create_write_request(
                self._core_debug_regs("DHCSR"),
                self._core.DHCSR_DBGKEY |
                self._core.arm_regs.DHCSR.C_HALT.mask |
                self._core.arm_regs.DHCSR.C_DEBUGEN.mask))
        elif rq.meta == RunCtrlPort.STEP:
            requests.append(self._create_write_request(
                self._core_debug_regs("DHCSR"),
                self._core.DHCSR_DBGKEY |
                self._core.arm_regs.DHCSR.C_STEP.mask |
                self._core.arm_regs.DHCSR.C_HALT.mask |
                self._core.arm_regs.DHCSR.C_DEBUGEN.mask))
            requests.append(self._create_write_request(
                self._core_debug_regs("DHCSR"),
                self._core.DHCSR_DBGKEY |
                self._core.arm_regs.DHCSR.C_STEP.mask |
                self._core.arm_regs.DHCSR.C_DEBUGEN.mask))
        elif rq.meta == RunCtrlPort.RUN:
            requests.append(self._create_write_request(
                self._core_debug_regs("DHCSR"),
                self._core.DHCSR_DBGKEY |
                self._core.arm_regs.DHCSR.C_DEBUGEN.mask))

        return requests, reads

class SimpleMemoryDebugController:
    """
    Simplified DebugController which only supports memory accesses.  This is
    used for occasional address spaces which aren't associated with a processor.
    """

    def execute_request(self, rq):
        """
        Tries to execute the access request rq first by forwarding outwards
        but if there is no access path then try to convert it to
        a sequence of memory accesses.
        """
        try:
            self.slave.execute_outwards(rq)
        except NoAccess:
            if rq.type in (AccessType.REGISTERS, AccessType.RUN_CTRL):
                # Perhaps we can reduce these to memory accesses
                if rq.type == AccessType.REGISTERS:
                    requests, reads = self._reg_access_to_memory_access(rq)
                else:
                    # pylint: disable=assignment-from-no-return
                    requests, reads = self._run_ctrl_to_memory_access(rq)
                for req in requests:
                    self._core_mem_master.resolve_access_request(req)
                # Now the reads will have had data inserted into them
                if reads:
                    rq.data = sum((rd.data for rd in reads), [])
            else:
                raise

    def extend_access_path(self, access_path):
        """\
        Extend an AccessPath via this mapping.

        Ignores paths that do not intersect this Mapping.
        """
        self._core_mem_master.extend_access_path(access_path)
    