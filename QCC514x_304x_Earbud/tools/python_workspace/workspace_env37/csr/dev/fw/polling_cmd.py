############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from csr.dev.framework import runtime
from csr.dev.hw.address_space import AddressSpace
from csr.wheels.bitsandbobs import timeout_clock

class PollingCmd(object):

    TOGGLE_BIT_POS = 15
    TOGGLE_BIT_MASK = (1 << TOGGLE_BIT_POS)
    TOGGLE_BIT_IND = 0x40
    
    class TimeoutError(Exception):
        ''' Indicates a command has timed out 
        '''
        pass

    # ------------------------------------------------------------------------
    # Command Protocol
    # ------------------------------------------------------------------------
    
    def _send_cmd(self, cmd_code, timeout = 0, blocking=True):
        """ 
        Sends the given cucmd setting the toggle bit appropriately and
        waiting for the result        
        """        
        self._start_response = self._read_full_response()
        #iprint("_send_cmd: cmd_code = 0x%x" % cmd_code)
        #iprint("_send_cmd: old_response = 0x%x" % start_response)
        if ((self._start_response >> self.TOGGLE_BIT_POS) == 0):
            #iprint("_send_cmd: cmd_code = 0x%x" % cmd_code)
            cmd_code += self.TOGGLE_BIT_IND
            
        self._write_cmd(cmd_code)
        
        try:
            # Fire the interrupt
            self._trigger.write(1)
            self._trigger.write(0)
        except (AttributeError, AddressSpace.WriteFailure, AddressSpace.ReadFailure):
            # Some clients are triggered directly by the command write so
            # don't have a separate trigger
            # Permit WriteFail and ReadFailure exception as some commands will reset the DUT
            pass
        if blocking:
            self._poll_completion(timeout=timeout)

    def _poll_completion(self, timeout=0):
        if timeout:
            start_time = timeout_clock()
                
        while(self._read_full_response() & self.TOGGLE_BIT_MASK == 
                                 self._start_response & self.TOGGLE_BIT_MASK):
            if timeout and timeout_clock() - start_time > timeout:
                raise self.TimeoutError
            if runtime is not None:
                runtime.coop_yield()

            
    # ------------------------------------------------------------------------
    # IO
    # ------------------------------------------------------------------------
            
    def _write_param(self, param, offset=0):
        """
        Write a single parameter word to device buffer.
        """
        self._parameters[offset].value = param
            
    def _write_params(self, params):
        """
        Write multiple parameter words to device buffer.
        """
        # Potential extension:: Simplify once Pointer supports slicing.
        offset = 0
        for param in params:
            self._write_param(param, offset)
            offset += 1
        
    def _write_cmd(self, cmd):
        """
        Write command code to device. 
        """
        self._command.value = cmd
    
    def _read_full_response(self):
        """
        Read response code from device.
        """
        return self._response.value
    
    def _read_response(self):
        """
        Read response code from device ignoring the toggle bit.
        """
        return self._read_full_response() & ~self.TOGGLE_BIT_MASK

    def _read_result(self, offset=0):
        """
        Read a single result word from device buffer.
        """
        return self._results[offset].value
    
    def _read_results(self, how_many):
        """
        Read multiple result words from device buffer.
        """
        # Potential extension:: Simplify once Pointer supports slicing.
        results = []
        for offset in range(how_many):
            results.append(self._read_result(offset))
        return results
            
