############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 - 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.model import interface

try:
    long
except NameError:
    long = int

class HasRegBasedBreakpoints(object):
    """
    Abstract mixin providing generic support for cores for which breakpoints 
    are controlled via memory-mapped registers. 
    """
    
    @property
    def num_brk_regs(self):
        """
        Return the number of (program) breakpoint registers that are available
        """
        raise PureVirtualError

    def _brk_enable(self, regid):
        """
        Set the enable bit for the given register
        """
        raise PureVirtualError

    def _brk_disable(self, regid):
        """
        Clear the enable bit for the given breakpoint register
        """
        raise PureVirtualError

    def _brk_reset_sticky_enable_flag(self, regid, enabled):
        """
        If the given breakpoint is sticky, reset the enabled flag in the sticky
        record dict as indicated
        """
        if regid in self._sticky_brk_pts:
            self._sticky_brk_pts[regid] = (self._sticky_brk_pts[regid][0],
                                           enabled)

    def brk_enable(self, regid):
        """
        Set the enable bit for the given register and register the change in the
        sticky list
        """
        self._brk_enable(regid)
        self._brk_reset_sticky_enable_flag(regid, True)

    def brk_disable(self, regid):
        """
        Clear the enable bit for the given register and register the change in 
        the sticky list
        """
        self._brk_disable(regid)
        self._brk_reset_sticky_enable_flag(regid, False)

    def brk_is_enabled(self, regid):
        """
        Check whether a given breakpoint is enabled
        """
        raise PureVirtualError

    def brk_address(self, regid):
        """
        Return the address in the given breakpoint register
        """
        raise PureVirtualError

    def _brk_set_reg(self, regid, address, overwrite=True, enable=True):
        """
        Write the specified breakpoint register if it is currently free and 
        optionally enable the breakpoint.  If overwrite==True, write the 
        register regardless of whether it's free.  Return True if breakpoint 
        was set, else False
        """
        raise PureVirtualError

    def _brk_set_at(self, address):
        """
        Find an unused breakpoint register and set it to this address, returning
        its index.  If none free, return None
        """
        for regid in range(self.num_brk_regs):
            if self._brk_set_reg(regid, address, overwrite=False):
                return regid
        return None

    def brk_set(self, addr_or_fn, line_no=None, sticky=False, at_end=False):
        """
        Set a breakpoint at the specified place, optionally making it sticky 
        (i.e. re-applied on reset, where this is possible).
        
        The location is specified either as an explicit instruction address or
        by specifying a function and an optional source line.  If a function is
        specified without a line number, the breakpoint is inserted at the 
        function's entry point; if a line number is specified, this must 
        correspond to a line within the function's source, and the breakpoint is
        set at the first instruction whose corresponding source line number is
        not less than the specified line number. 
        """
        
        address = None
        function = None
        if isinstance(addr_or_fn, (int, long)):
            address = addr_or_fn
            
        elif isinstance(addr_or_fn, str):
            function = addr_or_fn
        else:
            # If we pass in a callable firmware function object
            function = addr_or_fn.__name__
            
        if address is None:
            if not at_end:
                address = self.fw.env.functions[function]
                if line_no is not None:
                    # We want a breakpoint at the first instruction which appears on
                    # the given line of the file that the function is found in
                    address += self.fw.env.functions.get_offset_of_function_srcline(
                                                                function, line_no)
            else:
                address = self.fw.env.dwarf.get_function(function).end_address - 1
                    
        
        brk_reg_num = self._brk_set_at(address)
        if brk_reg_num is None:
            # No room at the inn
            return None
        
        # Save the sticky ones
        if sticky:
            self.brk_set_sticky(brk_reg_num, set=True)
        # Return the register number that was set.  This is need for subsequent
        # manipulation of the breakpoint
        return brk_reg_num
            
    def brk_set_sticky(self, regid, set=True):
        """
        Set or clear the sticky flag for a breakpoint
        """
        if set:
            self._sticky_brk_pts[regid] = (self.brk_address(regid),
                                           self.brk_is_enabled(regid))
        elif regid in self._sticky_brk_pts:
            del self._sticky_brk_pts[regid]
            
    def brk_reset_sticky(self):
        """
        Reset the break point state just to contain the sticky break points.  
        This function is intended to be called at reset. 
        """
        for regid in range(self.num_brk_regs):
            if regid in self._sticky_brk_pts:
                address, enabled = self._sticky_brk_pts[regid]
                self._brk_set_reg(regid, address, overwrite=True, 
                                  enable=enabled)
            else:
                self.brk_clear(regid)
            
        
    def brk_clear(self, regid):
        """
        Clear the breakpoint in the given register
        """
        self.brk_disable(regid)
        # This sets the address to 0 but doesn't clear the enable bit
        self._brk_set_reg(regid, self.brk_default_address_reg, overwrite=True)
        # So clear that here
        self._brk_disable(regid)
        if regid in self._sticky_brk_pts:
            self.brk_set_sticky(regid, set=False)

    def brk_clear_all(self):
        """
        Clear all the breakpoint registers
        """
        for regid in range(self.num_brk_regs):
            self.brk_clear(regid)
        
    def brk_default_address_reg(self):
        raise PureVirtualError
        
    def brk_display(self):
        """
        Construct a plain text table of break points 
        """
        breakpts = interface.Table(headings = ["Reg num", "Address", "Location",
                                               "Enabled?", "Sticky?"])
        for regid in range(self.num_brk_regs):
            address = self.brk_address(regid)
            if address != self.brk_default_address_reg:
                enabled_str = "Y" if self.brk_is_enabled(regid) else "N"
                sticky_str = "Y" if regid in self._sticky_brk_pts else "N"
                # Get the function the address is in plus the line number of
                # the breakpoint (we'll assume for simplicity that the caller
                # doesn't need to be told the source file)
                start_addr, fn, dwarf_sym = \
                            self.fw.env.functions.get_function_of_pc(address)
                if dwarf_sym is not None:
                    _,line = dwarf_sym.get_srcfile_and_lineno(address-start_addr)
                    location_str = "%s:%d" % (fn, line)
                else:
                    location_str = "No DWARF"
                breakpts.add_row([regid, address, location_str,
                                  enabled_str, sticky_str])
                
        return breakpts

