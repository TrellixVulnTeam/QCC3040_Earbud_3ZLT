# Copyright (c) 2016-2018 Qualcomm Technologies International, Ltd.
#   %%version
"""
@file
Trace module objects file.

@section Description
Pylib class to access the Kalimba Trace hardware module.

See <a href="http://cognidox.europe.root.pri/cgi-perl/part-details?partnum=CS-327708-DD">CS-327708-DD</a>

The class implements the three logging methods supported by the hardware:
"DMEM" : Logging to shared RAM in the apps subsystem
"RMEM" : Logging to remote memory in the BlueTooth subsystem
"TBUS" : Logging to a host over the Tbridge

For the TBUS method two version have been implemented a background logger which
runs continuously in a python subprocess and a foreground logger which blocks.
"""

from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import bytes_to_dwords,words_to_dwords, timeout_clock
from csr.transport.kaltrace import KalTrace
from csr.dev.framework.connection.trb_log import TrbLogger
import time
from .hw_trace_agent import AppsHWTraceAgent
from csr.wheels.util import unique_idstring

class TraceLogger(object):
    '''
    Class representing Trace hardware settings.
    '''
    
    def __init__(self,cores):
        
        #Internal flag to enable debug output to be generated. 
        self._debug = False

        self._trace0 = cores[0].trace
        self._trace1 = cores[1].trace
        
        #Store handle to Tbridge logger(s) 
        self._hw_tracer = None  #Logger another process
        self._trb_logger = None #Logger in this process
        
        #Option to not use a background task for Tsamples.
        self._backgroundlogger = True

        #Store the transport so we know which device to trace
        self._trans = cores[0].subsystem.chip.device.transport

    def display_debug_output(self,debug=True):
        '''
        Causes all the trace modules to output debug information to 
        the commandline. 
        '''
        #Internal flag to enable debug output to be generated. 
        self._debug = debug
        
        #Ripple debug setting down to the individual trace modules. 
        self._trace0.display_debug_output(debug)
        self._trace1.display_debug_output(debug)
      
    def go(self):
        '''
        Enable the trace module. Logging will start and stop based on the 
        trigger setting. It may start immediate and run continuously, or 
        only capture a short burst.        
        '''
            
        if self._debug:
            iprint("trace.py: go() TRACE0=%d TRACE1=%d:"%(
                self._trace0.active,
                self._trace1.active))
            
        if ((self._trace0.active and self._trace0._logging_mode == "TBUS") 
            or 
            (self._trace1.active and self._trace1._logging_mode == "TBUS")):        
            if self._backgroundlogger:
                self._hw_tracer = AppsHWTraceAgent(self._debug, self._trans)
                if self._debug:
                    iprint("Calling trace.start()")
                self._hw_tracer.start() 
                time.sleep(0.5) #Little nap to let logger start.
            else:
                if self._trb_logger is None:
                    self._trb_logger = TrbLogger()
         
        self._trace0.go()
        self._trace1.go()
        
    def poll(self, blocking = True, timeout = float('inf')):
        '''
        The TBUS logger runs silently in the background and just needs 
        retrieving once the end trigger is hit. The memory logging may
        need the foreground task to help it along. 
        '''
        
        start = timeout_clock()
        
        while True:
            if self._debug:
                iprint("trace.py: TRIGGER_STATUS Trace0: 0x%x Trace1: 0x%x"%(
                    self._trace0.status(),
                    self._trace1.status()))

            #Test if either trace module is stalling its core.
            if (self._trace0.active == True) and (self._trace0.stalled() == True):
                self._trace0.retrieve_trace(None,True)
                
            if (self._trace1.active == True) and (self._trace1.stalled() == True):
                self._trace1.retrieve_trace(None,True)
            
            #Test if the logging is now complete
            finished = (
                (self._trace0.status() == self._trace0._expected_status) 
                and
                (self._trace1.status() == self._trace1._expected_status))
            timedout = (timeout_clock() - start) > timeout 
            if (blocking == False) or (timedout == True) or (finished == True):
                break
            else:
                time.sleep(0.1) #Don't need to rush aroud this loop. 
                
        if (timedout == True) or (finished == True):
            #Force the trace block(s) to output anything remaining.
            if self._trace0.active:
                self._trace0.flush(finished)
                
            if self._trace1.active:    
                self._trace1.flush(finished)
                
            #Tell the background logging process to finish
            if ((self._trace0.active == True and self._trace0._logging_mode == "TBUS") 
                or
                (self._trace1.active == True and self._trace1._logging_mode == "TBUS")): 
                
                if self._backgroundlogger:
                    self._hw_tracer.finish()
            
            if self._trace0.active:
                self._trace0.retrieve_trace(self._hw_tracer,False)
                
            if self._trace1.active:    
                self._trace1.retrieve_trace(self._hw_tracer,False)

            if self._debug:
                iprint("trace.py: Length of trace log Trace0: %d Trace1: %d"%(
                    len(self._trace0._trace_log),
                    len(self._trace1._trace_log)))
            
            #End the logging process now that the data has been retrieved. 
            if ((self._trace0.active == True and self._trace0._logging_mode == "TBUS") 
                or
                (self._trace1.active == True and self._trace1._logging_mode == "TBUS")):
                
                if self._backgroundlogger:
                    self._hw_tracer.stop()
                    if self._debug:
                        iprint("trace.py: Stopped subprocess.")
            
        return finished

    def status(self):                        
        return [self._trace0.status(),self._trace1.status()]
        
    def retrieve_traces(self):  
        return [self._trace0._trace_log,self._trace1._trace_log,
            self._trace0._timestamps,self._trace1._timestamps]
        
    def retrieve_trace_lengths(self):  
        return [len(self._trace0._trace_log),len(self._trace1._trace_log)]
        
    def get_expected_status(self):
        '''
        Return the expected end status for the triggers...
        '''
        return [self._trace0._expected_status,self._trace1._expected_status]

# Start/stop functions for instruction tracing using dummy functions.
# These names must be kept in sync with the implementations in each subsystem.
TRACE_START_FUNCTION = "tracepoint_trigger_1"
TRACE_STOP_FUNCTION = "tracepoint_trigger_2"

class InstructionTraceError(Exception):
    '''
    Exception for instruction trace related errors.
    '''
    pass

class TraceModule(object):
    '''
    Class representing Trace hardware settings.
    '''

    class HalTrace(object):
        '''
        Class used for abstracting all differences between subsystems.
        '''
        def __init__(self, core):
            #Store handle to the core which is configuring the trace module.
            self._core = core
            self.is_apps = False
            self.is_audio = False
            if 'apps' in self._core.nicknames[0]:
                self.is_apps = True
                self.px = self._core.processor_number
            if 'audio' in self._core.nicknames[0]:
                self.is_audio = True
                if "P0" in self._core.__str__():
                    self.px = 0
                elif "P1" in self._core.__str__():
                    self.px = 1
        
        def master_clock_en(self):
            #Enable the 'master clock' to trace hardware
            if self.is_apps:
                self._core.fields.CLKGEN_ENABLES.CLKGEN_DSP_TRACE_EN = 1
            if self.is_audio:
                # These should be enabled by default but let's make sure
                ACT_EN_TRACE = getattr(self._core.fields.KALIMBA_CLKGEN_CLK_ACTIVITY_ENABLES,
                                       "ACTIVITY_EN_TRACE%d" % self.px)
                self._core.fields.CLKGEN_ACTIVITY_ENABLES.CLKGEN_ACTIVITY_TBUS_TRACE = 1
                ACT_EN_TRACE.write(1)
        
        def abs_addr(self, addr):
            # Returns the absolute address given the relative one.
            if self.is_apps:
                return addr + 0x10000000
            if self.is_audio:
                return addr
            return addr

    def __init__(self, core):
        # Instantiate HAL class
        self._hal = TraceModule.HalTrace(core)

        # Store handle to the core which is configuring the trace module.
        self._core = core

        # Internal flag to enable debug output to be generated.
        self._debug = False

        # Store handle to Tbridge logger
        self._logging_mode = "Default"  # Default to using TSamples

        # Option to not use a background task for Tsamples.
        self._backgroundlogger = True

        # Mirror trace hardware parameters
        self._length = 0
        self._log_mem_addr = 0

        self.active = False  # Not in use unless trigger method called
        self._expected_status = 0x0

        # Store the compressed trace inside the class.
        self._trace_log = []
        self._timestamps = []

        # Call config method to setup defaults, these can be changed later if required.
        self.config()

    def start(self):
        '''
        Start tracing instructions executed by this core.

        This starts tracing instructions by setting the trace hardware
        registers to trace from the current PC. When 'stop' is called the
        processor is paused whilst we set the stop address to the current PC.
        The trace hardware will then end the trace.

        This has a number of advantages over the _start/_stop_using_dummy_func
        methods:
            * It works with the audio subsystem
            * It's able to trace until a panic
            * It's able to stop a trace that was started in another way

        Precondition:
            The core must be currently tracing instructions, i.e. 'start' must
            have been called.

        Examples:
            Example of how to acquire a 1 second trace of apps p0.

            >>> apps.trace.start()
            >>> import time; time.sleep(1)
            >>> encoded_trace = apps.trace.stop()
        '''
        # Configuring the trace hardware with no triggers set will cause
        # tracing to start as soon as the module is enabled in go().
        self.triggers()
        self._expected_status = 0xc
        self._core.subsystem.tracelogger.go()

    def stop(self, use_timestamp=False):
        '''
        Stop tracing instructions executed by this core.

        Returns:
            (list of int): A list of 32-bit integers representing the encoded
                trace. This can be passed to the TraceModuleAnalysis class for
                decoding and further processing. If the trace module was never
                enabled this list will be empty.

            If use_timestamp is True:
            (list of int): A list of 32-bit integers representing the encoded
                time-stamps. This can be passed to the TraceModuleAnalysis
                class for decoding and further processing. If the trace module
                was never enabled this list will be empty.
        '''
        if self._core is self._core.subsystem.cores[0]:
            tracing = int(self._core.fields.TRACE_0_CFG.TRACE_CFG_0_ENABLE)
        else:
            tracing = int(self._core.fields.TRACE_1_CFG.TRACE_CFG_1_ENABLE)
        if not tracing:
            return []

        # Pause the core so we can set the end address to the current PC and
        # reliably read the current status.
        core_was_running = self._core.is_running
        self._core.pause()

        # The expected status will be different depending on whether we've hit
        # the start marker or not.
        self._expected_status = 0xc | (self.status() & 1)

        if self._core is self._core.subsystem.cores[0]:
            self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN = 0
            self._core.fields.TRACE_0_END_TRIGGER = self._core.pc
            self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN = 1
        else:
            self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN = 0
            self._core.fields.TRACE_1_END_TRIGGER = self._core.pc
            self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN = 1

        # Only run the processor if it was running before this call
        if core_was_running:
            self._core.run()

        self._core.subsystem.tracelogger.poll()
        if use_timestamp:
            return self.encoded_trace, self.enc_timestamps
        return self.encoded_trace

    def _start_using_dummy_func(self):
        '''
        Start tracing instructions executed by this core.

        The start/stop interface to instruction tracing works by setting the
        'start_func' and 'stop_func' triggers to known functions on the core
        that have no side effects. Instruction trace is then enabled and the
        'start_func' is called. Tracing will continue until 'stop' is called,
        at which point the 'stop_func' will be called on the chip and the
        encoded trace will be returned.

        The start/stop methods are preferred to the using_dummy_func variants
        as they have a number of advantages. These variants are kept around
        for experimentation purposes and in case the newer methods have
        undiscovered bugs. These functions may be removed in the future.

        Raises:
            InstructionTraceError: If the subsystem does not implement the
                required functions for start/stop style tracing. See
                TRACE_START_FUNCTION and TRACE_STOP_FUNCTION.

        Examples:
            Example of how to acquire a 1 second trace of apps p0.

            >>> apps.trace.start()
            >>> import time; time.sleep(1)
            >>> encoded_trace = apps.trace.stop()
        '''
        try:
            self.triggers(start_func = TRACE_START_FUNCTION,
                          stop_func = TRACE_STOP_FUNCTION)
        except ValueError:
            raise InstructionTraceError("start/stop interface unsupported by subsystem")

        self._core.subsystem.tracelogger.go()
        self._core.fw.call(TRACE_START_FUNCTION)

    def _stop_using_dummy_func(self):
        '''
        Stop tracing instructions executed by this core.

        Precondition:
            The core must be currently tracing instructions, i.e. 'start' must
            have been called.

        Returns:
            (list of int): A list of 32-bit integers representing the encoded
                trace. This can be passed to the TraceModuleAnalysis class for
                decoding and further processing.

        Raises:
            InstructionTraceError: If the subsystem does not implement the
                required functions for start/stop style tracing. See
                TRACE_START_FUNCTION and TRACE_STOP_FUNCTION.
            InstructionTraceError: If this stop function was called before the
                start function.
        '''
        try:
            self._core.fw.call(TRACE_STOP_FUNCTION)
        except ValueError:
            raise InstructionTraceError("start/stop interface unsupported by subsystem")

        self._core.subsystem.tracelogger.poll()
        return self.encoded_trace

    def display_debug_output(self,debug=True):
        '''
        Causes this trace module to output debug information to 
        the commandline. 
        '''
        #Internal flag to enable debug output to be generated. 
        self._debug = debug
        
    def go(self):
        '''
        Enable the trace module. Logging will start and stop based on the 
        trigger setting. It may start immediate and run continuously. Or 
        only capture a short burst.        
        '''
        
        #Clear any previous stored trace.
        self._trace_log = []
        self._timestamps = []

        if self._core is self._core.subsystem.cores[0]:
            self._core.fields.TRACE_0_CFG.TRACE_CFG_0_ENABLE = self.active
        else:
            self._core.fields.TRACE_1_CFG.TRACE_CFG_1_ENABLE = self.active
            
    def status(self):
        '''
        Wrapper for trigger status register.
        '''
       
        if self._core is self._core.subsystem.cores[0]:
            return int(self._core.fields.TRACE_0_TRIGGER_STATUS)& 0x0f
        else:
            return int(self._core.fields.TRACE_1_TRIGGER_STATUS)& 0x0f
            
    def stalled(self):
        '''
        Wrapper for logger status registers.
        '''
        
        if self._logging_mode == "TBUS":
            return False  #TBUS can stall the core but only very briefly.
        
        if self._core is self._core.subsystem.cores[0]:
            if self._logging_mode == "DMEM":
                core_stalled = self._core.fields.TRACE_DMEM_STATUS.TRACE_DMEM_STATUS_CNTL_0_DUMP_DONE
            else:
                core_stalled = self._core.fields.TRACE_TBUS_STATUS.TRACE_TBUS_STATUS_CNTL_0_DUMP_DONE
        else:
            if self._logging_mode == "DMEM":
                core_stalled = self._core.fields.TRACE_DMEM_STATUS.TRACE_DMEM_STATUS_CNTL_1_DUMP_DONE
            else:
                core_stalled = self._core.fields.TRACE_TBUS_STATUS.TRACE_TBUS_STATUS_CNTL_1_DUMP_DONE
   
        return core_stalled 
        
    def flush(self, finished):
        '''
        Wrapper for logger config registers.
        '''
        
        if self._debug and not finished:
            iprint("trace.py: flush() Force flush of bitgen..")
        
        #Note these registers are write sensitive.
        if self._core is self._core.subsystem.cores[0]:
            if not finished:
                self._core.fields.TRACE_0_CFG.TRACE_CFG_0_FLUSH_BITGEN = 1
                self._core.fields.TRACE_0_CFG.TRACE_CFG_0_FLUSH_BITGEN = 0
            self._core.fields.TRACE_0_CFG.TRACE_CFG_0_FLUSH_FIFO = 1
        else:
            if not finished:
                self._core.fields.TRACE_1_CFG.TRACE_CFG_1_FLUSH_BITGEN = 1
                self._core.fields.TRACE_1_CFG.TRACE_CFG_1_FLUSH_BITGEN = 0
            self._core.fields.TRACE_1_CFG.TRACE_CFG_1_FLUSH_FIFO = 1
            
    def disable_trace(self):
        if self._core is self._core.subsystem.cores[0]:
            self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN = 0
            self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN = 0
        else:
            self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN = 0
            self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN = 0
        self._expected_status = 0x0
        self.active = False 
        if self._debug:
            if self._core is self._core.subsystem.cores[0]:
                iprint("trace.py: TRACE0: Disabled.")
            else:
                iprint("trace.py: TRACE1: Disabled.")
                
    def triggers(self,
                length = 0,
                start_addr = None,
                start_func = None,
                start_func_offset = 0,
                stop_addr = None,
                stop_func = None,
                stop_func_offset = 0,
                ):
        '''
        Configure the trace triggers.
        
        Calling this method flags the trace module as being active for
        logging. If neither trigger is set the trace will be captured
        immedaite the logger starts and could continue indefinitely. 
        This may result in a huge amount of trace data and the caller  
        is responsible for handling this... 
        '''
        
        self.active = True 
        
        #No simple way to programmatically select registers for the two trace modules 
        
        if self._core is self._core.subsystem.cores[0]:
        
            self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN = 0
            if start_addr is None and start_func is None:
                #Disable the start trace trigger.
                start_trigger_enabled = False
            else:
                if start_func is not None:
                    start_addr = (self._core.fw.env.functions.get_call_address(start_func) 
                        + start_func_offset)
                if self._debug:
                    iprint("Setting start trigger 0 to 0x%08X and length to %d" % (start_addr, length))
                self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_TRIGGER_LENGTH = length
                self._core.fields.TRACE_0_START_TRIGGER = start_addr
                start_trigger_enabled = True
                self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN = 1 

            self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN = 0    
            if stop_addr is None and stop_func is None:
                #Disable the stop trace trigger.
                stop_trigger_enabled = False
                
            else:
                if stop_func is not None:
                    stop_addr = (self._core.fw.env.functions.get_call_address(stop_func)
                        + stop_func_offset)
                if self._debug:
                    iprint("Setting stop trigger 0 to 0x%08X and length to %d" % (stop_addr, length))
                self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_TRIGGER_LENGTH = length
                self._core.fields.TRACE_0_END_TRIGGER = stop_addr
                stop_trigger_enabled = True
                self._core.fields.TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN = 1
        else:
            self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN = 0
            if start_addr is None and start_func is None:
                #Disable the start trace trigger.
                start_trigger_enabled = False
            else:
                if start_func is not None:
                    start_addr = (self._core.fw.env.functions.get_call_address(start_func) 
                        + start_func_offset)
                if self._debug:
                    iprint("Setting start trigger 1 to 0x%08X and length to %d" % (start_addr, length))
                self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_TRIGGER_LENGTH = length
                self._core.fields.TRACE_1_START_TRIGGER = start_addr
                start_trigger_enabled = True
                self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN = 1 
                
            self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN = 0
            if stop_addr is None and stop_func is None:
                #Disable the stop trace trigger.
                stop_trigger_enabled = False
            else:
                if stop_func is not None:
                    stop_addr = (self._core.fw.env.functions.get_call_address(stop_func) 
                        + stop_func_offset)
                if self._debug:
                    iprint("Setting stop trigger 1 to 0x%08X and length to %d" % (stop_addr, length))
                self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_TRIGGER_LENGTH = length
                self._core.fields.TRACE_1_END_TRIGGER = stop_addr
                stop_trigger_enabled = True
                self._core.fields.TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN = 1
                
        #Setup the "expected" final status for these triggers

        if start_trigger_enabled:
            self._expected_status = 0x3
        else:
            self._expected_status = 0x0
            
        if stop_trigger_enabled:
            #if the stop trigger is set then the start trigger won't complete.
            self._expected_status = self._expected_status & 0x1 #Clear start trigger complete.
            self._expected_status = self._expected_status + 0xc    

        if self._debug:
            if self._core is self._core.subsystem.cores[0]:
                iprint("trace.py: TRACE0: Initial status 0x%x expected completion trigger 0x%x"%(
                    self._core.fields.TRACE_0_TRIGGER_STATUS,
                    self._expected_status))
            else:
                iprint("trace.py: TRACE1: Initial status 0x%x expected completion trigger 0x%x"%(
                    self._core.fields.TRACE_1_TRIGGER_STATUS,
                    self._expected_status))
    
  
    def set_logging_mode_dmem(self,addr,length,wrap=0):
        '''
        Configure the destination for the trace log.
        
        Note: The trace hardware may generate one more samples than requested
        if the last word is a sync word
        '''
        self._logging_mode = "DMEM"
        
        self._length = length * 4 # Convert log dwords into DMEM bytes
        self._log_mem_addr = addr

        if self._core is self._core.subsystem.cores[0]:
            self._core.fields.TRACE_0_DMEM_BASE_ADDR = addr
            self._core.fields.TRACE_0_DMEM_CFG.TRACE_0_DMEM_CFG_WRAP = wrap
            self._core.fields.TRACE_0_DMEM_CFG.TRACE_0_DMEM_CFG_LENGTH = length
            self._core.fields.TRACE_0_DMEM_CFG.TRACE_0_DMEM_EN = 1
            
            #Make sure the other logging mode isn't enabled.
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_EN = 0
        else:
            self._core.fields.TRACE_1_DMEM_BASE_ADDR = addr
            self._core.fields.TRACE_1_DMEM_CFG.TRACE_1_DMEM_CFG_WRAP = wrap
            self._core.fields.TRACE_1_DMEM_CFG.TRACE_1_DMEM_CFG_LENGTH = length
            self._core.fields.TRACE_1_DMEM_CFG.TRACE_1_DMEM_EN = 1
            
            #Make sure the other logging mode isn't enabled.
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_EN = 0

        #Zero the DMEM region.
        addr = self._hal.abs_addr(addr)
        self._core.dm[addr:+addr+(self._length+8)] = [0x0] * (self._length+8)

    def set_logging_mode_tsample(self, backgroundlogger=True):
        '''
        Configure the destination for the trace log
        
        TBridge samples are the default. 
        '''
        
        self._logging_mode = "TBUS"

        if self._core is self._core.subsystem.cores[0]:
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_DEST_SYS = 6
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_DEST_BLK = 15
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_TAG = 14
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_SRC_BLK = 1
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_TRAN_TYPE = 0
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_EN = 1
            
            #Make sure the other logging mode isn't enabled.
            self._core.fields.TRACE_0_DMEM_CFG.TRACE_0_DMEM_EN = 0
        else:
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_DEST_SYS = 6
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_DEST_BLK = 15
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_TAG = 14
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_SRC_BLK = 2
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_TRAN_TYPE = 0
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_EN = 1
            
            #Make sure the other logging mode isn't enabled.
            self._core.fields.TRACE_1_DMEM_CFG.TRACE_1_DMEM_EN = 0
        
    def set_logging_mode_twrite(self, dest_sys, dest_blk, dest_addr, length=0, wrap=0):
        '''
        Configure the destination for the trace log
        Note: The trace hardware may generate one more sample than requested
        if the last word is a sync word.
        
        '''
        self._logging_mode = "RMEM"
        
        self._length = length * 4 # Convert log dwords into RMEM words
        self._log_mem_addr = dest_addr
        self._log_sys = dest_sys
        self._log_sys_block = dest_blk
        '''
        #Zero subsystem RAM first
        bt = self._core.subsystem.chip.bt_subsystem.core
        for i in range(0x8000,0x8000+length,2): 
            bt.data[i:i+2]=[0x0,0x0]
        '''
        if self._core is self._core.subsystem.cores[0]:
            self._core.fields.TRACE_0_TBUS_BASE_ADDR = dest_addr
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_DEST_SYS = dest_sys
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_DEST_BLK = dest_blk
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_SRC_BLK = 0
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_TRAN_TYPE = 1
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_WRAP = wrap
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_LENGTH = length
            self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_EN = 1
            
            #Make sure the other logging mode isn't enabled.
            self._core.fields.TRACE_0_DMEM_CFG.TRACE_0_DMEM_EN = 0
        else:
            self._core.fields.TRACE_1_TBUS_BASE_ADDR = dest_addr
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_DEST_SYS = dest_sys
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_DEST_BLK = dest_blk
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_SRC_BLK = 0
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_TRAN_TYPE = 1
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_WRAP = wrap
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_LENGTH = length
            self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_EN = 1
            
            #Make sure the other logging mode isn't enabled.
            self._core.fields.TRACE_1_DMEM_CFG.TRACE_1_DMEM_EN = 0

    def config(self, stall=1, sync_ins = 7, debugpios=False):
        '''
        Configure trace hardware.
        
        TSample logging is the default so unless someone has already 
        configured an alternative method configure this here. 
        
        Call with debugpios = True to enable PIO debugging for the trace
        module. Trace debug on low PIOs and Tbridge on high PIOs
        '''  
        #Enable the 'master clock' to trace hardware
        self._hal.master_clock_en()
        
        if debugpios:
            #Route trace hardware block debug signals out.
            
            #Get handles to the hardware objects needed to route out trace debug
            chip = self._core.subsystem._chip
            cur = self._core.subsystem._chip.curator_subsystem.core
            
            #Route (High) PIOs onto debug pins.
            chip.fpga_reg_write("FPGA_DEBUG_MUX_SEL", 27)
            
            #Iterate through routing all PIOs to the debug mux. 
            for i in range(0,16,2):
                cur.mem[cur.fw.env._info.abs["PIO_0001_SUBSYS_DEBUG_SELECT"] + i // 2] = 0x8080 + i + ((i + 1) << 8)
            for i in range(16,32,2):
                cur.mem[cur.fw.env._info.abs["PIO_0001_SUBSYS_DEBUG_SELECT"] + i // 2] = 0xa0a0 + i + ((i + 1) << 8)
            
            #Assign High PIOs to the APPS subsystem and Low to Curator. 
            cur.fields.PIO_CHIP_DEBUG_SELECT_SUBSYS = 0x04
            
            #Select Trace on the Low (Apps) PIOs
            self._core.fields.SUB_SYS_DEBUG_SELECT_LOW = 0x0039
        
            #Select TBridge on the High (Curator) PIOs
            cur.fields.SUB_SYS_DEBUG_SELECT = 0x0e0e
            cur.fields.BUS_BRIDGE_CONFIG.BUS_BRIDGE_DEBUG_SELECT = 4
        
        if self._logging_mode == "Default": 
            self.set_logging_mode_tsample()

        if self._core is self._core.subsystem.cores[0]:
            #Disable the trace module before changing settings.
            self._core.fields.TRACE_0_CFG.TRACE_CFG_0_ENABLE = 0
            self._core.fields.TRACE_0_CFG.TRACE_CFG_0_SYNC_INTERVAL = sync_ins
            self._core.fields.TRACE_0_CFG.TRACE_CFG_0_FLUSH_FIFO = 0
            self._core.fields.TRACE_0_CFG.TRACE_CFG_0_STALL_CORE_ON_TRACE_FULL = stall
            
            #This bit needs to match what the software decode in KalTrace expects.
            self._core.fields.TRACE_0_CFG.TRACE_CFG_0_CLR_STORED_ON_SYNC = 1
        else:
            #Disable the trace module before changing settings.
            self._core.fields.TRACE_1_CFG.TRACE_CFG_1_ENABLE = 0
            self._core.fields.TRACE_1_CFG.TRACE_CFG_1_SYNC_INTERVAL = sync_ins
            self._core.fields.TRACE_1_CFG.TRACE_CFG_1_FLUSH_FIFO = 0
            self._core.fields.TRACE_1_CFG.TRACE_CFG_1_STALL_CORE_ON_TRACE_FULL = stall
            
            #This bit needs to match what the software decode in KalTrace expects.
            self._core.fields.TRACE_1_CFG.TRACE_CFG_1_CLR_STORED_ON_SYNC = 1   
      
    def _trim_trace(self,trace):
        '''
        For traces retrieved from memory there may be trailing 'zeros' which 
        are not part of the log. We can't be sure now long the log will be, or
        that the last word isn't zeros. So best we can do is truncate it. 
        '''

        #Trim off all zero words from the end of the log.
        end_of_log = 0
        for i in range(len(trace)-1,-1,-1):
            if trace[i] != 0x0:
                end_of_log = i+1
                break
                
        return trace[0:end_of_log]

    @property
    def encoded_trace(self):
        '''
        list of int: Public accessor to the encoded trace that's recorded by
                     and stored in this object.
        '''
        return self._trace_log

    @property
    def enc_timestamps(self):
        '''
        list of int: Public accessor to the time-stamps that's recorded by
                     and stored in this object.
                     This time-stamp array has the same size and one-to-one
                     relation to encoded_trace.
        '''
        return self._timestamps

    def pcs(self, encoded_trace=None, enc_timestamps=None):
        '''
        Iterate over each PC in the instruction trace.

        Args:
            encoded_trace (list of int, optional):
                The encoded trace, presented as a list of 32-bit integers.
                If it's not specified the trace captured as part of this
                pydbg session is used.

            enc_timestamps (list of int, or True, optional):
                The time-stamp for the encoded trace, presented as a list
                of 32-bit integers.

                If it's True, the trace captured as part of this pydbg
                session is used.
                If it's not specified, only the next PC is generated.

        Yields:
            enc_timestamps is a list (or True):
                (int, int): A tuple of the nextPC and its time-stamp.
            enc_timestamps is None:
                int: The next PC in the instruction trace.
        
        Examples:
            >>> iprint([i for i in self.pcs(enc_timestamps=True)])
            [(4292, 11223340), (4294, 11223343), (4298, 11223343), (4302, 11223351)]

            >>> iprint([i for i in self.pcs()])
            [4292, 4294, 4298, 4302]
        '''
        encoded_trace = encoded_trace or self._trace_log

        self._kaltrace = KalTrace() 
        if enc_timestamps is None or type(enc_timestamps) == list:
            pass
        elif enc_timestamps is True:
            enc_timestamps = self._timestamps

        elf_filename = self._core.fw.env.build_info.build_dir + "\\" \
                     + self._core.fw.env.build_info._elf_file_basename

        for_kalsim = False
        context = self._kaltrace.context_create(encoded_trace, elf_filename, for_kalsim, enc_timestamps)
        try:
            rv = self._kaltrace.context_iteration_begin(context)
            while True:
                if type(rv) == tuple:
                    # A tuple of (PC, timestamp) is returned if timestamp is provided.
                    pc = rv[0]
                    timestamp = rv[1]
                    yield (pc & ~1, timestamp)  # Remove the minim / maxim marker before yielding
                else:
                    pc = rv
                    yield pc & ~1               # Remove the minim / maxim marker before yielding
                rv = self._kaltrace.context_iteration_next(context)
        except StopIteration:
            pass
        self._kaltrace.context_destroy(context)


    def save_encoded_trace(self, filename_prefix=None):
        '''
        Save the encoded trace data and its time-stamps in text files.
        The following two files are created:
            - PREFIX_raw_encoded_trace.txt
            - PREFIX_raw_timestamps.txt

        The core name (i.e. audio0/apps0/apps1) is expected to be a part of
        filename_prefix to keep the data file format simple.
        '''

        # Dump the encoded trace and time-stamps
        filename = filename_prefix + "_raw_encoded_trace.txt"
        fout = open(filename, "wt")
        for encoded_trace in self._trace_log: 
            fout.write("%08X\n"%(encoded_trace))
        fout.close()
        if self._debug:
            if len(self._trace_log):
                iprint("trace.py: Created: %s"%(filename))
            else:
                iprint("trace.py: Created: %s  (!) Empty trace file."%(filename))
        
        filename = filename_prefix + "_raw_timestamps.txt"
        fout = open(filename, "wt")
        for timestamp in self._timestamps: 
            fout.write("%d\n"%(timestamp))
        fout.close()
        if self._debug:
            if len(self._trace_log):
                iprint("trace.py: Created: %s" % (filename))
            else:
                iprint("trace.py: Created: %s  (!) Empty time-stamp file." % (filename))

    def decode_trace(self, output=None, use_timestamp=False):
        '''
        Decode the encoded trace data. The encoded trace and its time-stamps
        (stored in self._trace_log and self._timestamps) are passed to KalTrace.
        The decoded data (a list of PCs, num of instructions, etc.) are returned
        to the caller.

        If a string is specified to the output, the following text files are created.
         - Raw encoded trace data (an array of 32-bit words)
         - Raw time-stamp data (an array of 32-bit words)
         - PC list
         - Simple decoded trace in [pc, func_name, pc_offset] format
        '''

        # Get chip for decode file naming procedure
        chip = self._core.subsystem._chip

        # Setup the Kaltrace.dll used to decode trace logs
        self._kaltrace = KalTrace() 

        elf_filename = self._core.fw.env.build_info.build_dir + "\\" \
                     + self._core.fw.env.build_info._elf_file_basename
        suffix_id_str = unique_idstring(chip.device)

        if self._debug:
            iprint("trace.py: Using ELF: %s" % (elf_filename))

        if type(output) == str:
            # Dump the encoded trace and time-stamps
            filename_prefix = output + "_" + suffix_id_str
            self.save_encoded_trace(filename_prefix)

        # Decode the trace log with the ELF file by kaltrace DLL.
        if self._debug:
            iprint("trace.py: Started decoding %d words of traces."%(len(self._trace_log)))

        if use_timestamp:
            decoded_result = self._kaltrace.decode_trace_with_timestamps(self._trace_log, self._timestamps, elf_filename)
        else:
            decoded_result = self._kaltrace.decode_trace(self._trace_log, elf_filename)

        if decoded_result is None:
            if self._debug:
                iprint("trace.py: Trace log couldn't be decoded!")
            return None

        num_of_insts = decoded_result[0].num_instructions
        if self._debug:
            iprint("trace.py: Num of instructions: %d" % (num_of_insts))

        if type(output) == str:
            filename_trace = output + "_" + suffix_id_str + "_decoded_trace.txt"
            fout_trace = open(filename_trace, "wt")
            filename_pc = output + "_" + suffix_id_str + "_pc.txt"
            fout_pc = open(filename_pc, "wt")

        decoded_trace = []
        for i, pc in enumerate(decoded_result[0].pc_listing[0:num_of_insts]):
            func_name = self._core.fw.env.functions.get_function_of_pc(pc)[1]
            pc_offset = pc - self._core.fw.env.functions.get_call_address(func_name)

            if type(output) == str:
                # NB: "(pc & ~1)" Remove the minim / maxim marker.
                fout_trace.write("0x%08X: %s:+%d \n"%((pc & ~1), func_name, pc_offset))
                fout_pc.write("0x%08X\n"%(pc & ~1))
            decoded_trace.append([pc, func_name, pc_offset])

        if type(output) == str:
            fout_trace.close()
            fout_pc.close()
            if self._debug:
                iprint("trace.py: Created: %s" % (filename_trace))
                iprint("trace.py: Created: %s" % (filename_pc))
            
        decoded_output = {
            "pc_listing":           decoded_result[0].pc_listing[0:decoded_result[0].num_instructions],
            "num_instructions":     decoded_result[0].num_instructions, 
            "bits_per_pc":          decoded_result[0].bits_per_pc,
            "pc_listing_decoded":   decoded_trace
        }
        if use_timestamp:
            size = decoded_result[0].num_instructions_with_times
            decoded_output["times_listing"] = decoded_result[0].times_listing[0:size]
        else:
            decoded_output["times_listing"] = []
        self._kaltrace.free_trace(decoded_result)

        return decoded_output


    def playback_trace(self, decoded):
        iprint("Pausing core and saving PC.")
        self._core.pause()
        saved_pc = self._core.fields.REGFILE_PC
        pc_listing = decoded["pc_listing"]
        pc_len = len(pc_listing)
        for i in range(pc_len):
            pc = pc_listing[i]
            _pc, func, offset = decoded["pc_listing_decoded"][i]
            if _pc != pc:
                iprint("Inconsistency found in decoded trace. Aborting.")
                break
            iprint("PC %d/%d:0x%08X %s + %d PC:0x%08X" % (i, pc_len, pc, func, offset, pc), end=' ')
            self._core.fields.REGFILE_PC = pc
            input("(press any Enter to continue)")
        iprint("Restoring PC and resuming core.")
        self._core.fields.REGFILE_PC = saved_pc
        self._core.run()

    def retrieve_trace(self,hw_tracer=None, incomplete = False):
        '''
        Retrieve the trace log, either from the device memory or from the TBridge
        driver.
        '''
        if self._debug:
            iprint("trace.py: Retrieving: %s"%self._logging_mode)
            
        if self._logging_mode == "TBUS":
            '''
            NOTE: For the TBUS there is an implicit assumption Trace0 is read first.
            '''
            if self._backgroundlogger:
                ReadingLog = True
                while(ReadingLog): 
                    log = hw_tracer.get()
                    if log == "Marker1":
                        if self._core is self._core.subsystem.cores[0]:
                            #Trace 0: Found end marker..
                            ReadingLog = False
                        else:
                            #Trace 1: Ignore this trace should follow..
                            pass
                    elif log == "Marker2":
                        ReadingLog = False
                    else:  
                        if log == []:
                            pass #Drop any empty logs which get returned.
                        else:
                            self._trace_log.extend([log[1]])
                            self._timestamps.extend([log[0]])
            else:
                self._trace_log = self._trb_logger.report_trace()
          
        else:
        
            #Extract the trace from memory.

            #Check if there is more output to come? 
            if self._logging_mode == "DMEM":
                self._trace_log.extend(self._retrieve_dmem_log_helper(incomplete))
            else:
                self._trace_log.extend(self._retrieve_rmem_log_helper(incomplete))
        
            if incomplete:
                #Unstall the core and restart logging.
                if self._core is self._core.subsystem.cores[0]:
                    if self._logging_mode == "DMEM":
                        self._core.fields.TRACE_0_DMEM_CFG.TRACE_0_DMEM_EN = 1
                    else:
                        self._core.fields.TRACE_0_TBUS_CFG.TRACE_0_TBUS_EN = 1
                else:
                    if self._logging_mode == "DMEM":
                        self._core.fields.TRACE_1_DMEM_CFG.TRACE_1_DMEM_EN = 1
                    else:
                        self._core.fields.TRACE_1_TBUS_CFG.TRACE_1_TBUS_EN = 1
            else:
                #Don't know how long the final trace run was trim any zeroes.        
                self._trace_log = self._trim_trace(self._trace_log)  

                self._logging_mode = "Default"

        
        
    def _retrieve_dmem_log_helper(self,incomplete):
        '''
        Extract a 'fragment' of trace log from shared memory.
          
        Note we need to zero the memory region, otherwise next
        time round the python won't know if it is rereading the 
        same data.        
        '''
        trace_fragment = []
        length = self._length
        
        addr = self._hal.abs_addr(self._log_mem_addr)
              
        for i in range(addr,addr+length,4): 
            trace_word = bytes_to_dwords(self._core.dm[i:i+4])[0]
            self._core.dm[i:i+4] = [0x00,0x00,0x00,0x00] #zero the trace
            trace_fragment.append(trace_word)
            
        if self._debug:
            iprint("trace.py: Reading log 0x%x - 0x%x "%(addr, addr+length))

        return trace_fragment
        
    def _retrieve_rmem_log_helper(self,incomplete):
        '''
        Extract a 'fragment' of trace log from shared memory.
        '''
        trace_fragment = []
        length = self._length
        addr = self._log_mem_addr 
        trans = self._core.subsystem.chip.device.transport
        for i in range(addr,addr+length,4):
            '''
            Should be able to access the BT subsystem but just in case
            do raw reads. 
            
            trace_word = words_to_dwords(bt.data[i:i+2])[0]
            bt.data[i:i+2] = [0,0]
            '''
            trace_word = trans.mem_read32(self._log_sys,i,self._log_sys_block)
            trans.mem_write32(self._log_sys,i,0,self._log_sys_block)
            
            trace_fragment.append(trace_word)
        
        return trace_fragment
