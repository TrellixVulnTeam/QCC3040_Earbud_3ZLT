# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version

import os
from csr.wheels.global_streams import iprint
from csr.wheels.subprocess_agent import PipeSubprocessAgent
from csr.dev.framework.connection.trb_log import TrbLogger
from csr.transport.trb_raw import TrbRaw
from csr.dev.framework.connection.trb import TrbTransConnection

class HwTracer(object):
    
    def __init__(self, debug,dongle_name,dongle_id):
        """
        Setup the hardware trace module for logging over the TBridge
         
        This is nominally written to be subsystem-agnostics, but this is not
        guaranteed. 
        """
        self._trb_logger = TrbLogger(name = dongle_name,id = dongle_id)
        self._debug = debug
       
    def trace(self, pipe):
        """
        Execute the tracing algorithm.
        
        Retrieve log words from the TBridge driver and store locally until 
        the log is complete. This has to be done as the 'pipe' used to return
        samples to the main process has a limited size depending on the OS.
        
        Once logging is complete send the complete log to the main process, this
        will block until the main process has retrieved all the samples. Insert 
        a flag "End" so that the main process knows when it has got the entire
        log.
        """
        if self._debug:
            iprint("Trace subprocess started.")

        log0 = []
        log1 = []
        countdown = 2
        received_finish = False
        while not received_finish or countdown > 0:
            partlog = self._trb_logger.report_trace()
            log0.extend(partlog[0])
            log1.extend(partlog[1])
            self._trb_logger.clear()

            if received_finish:
                countdown = countdown -1
            elif pipe.poll():
                if self._debug:
                    iprint("Received finish")
                pipe.recv()

                # Go round two more times to ensure all trace data is captured
                received_finish = True

        for entry in log0:
            pipe.send(entry)
        pipe.send("Marker1" )
        for entry in log1:
            pipe.send(entry)
        pipe.send("Marker2" )
        pipe.close()


def hw_trace(debug,dongle_name,dongle_id,pipe):
    """
    Plain function wrapper called by the multiprocessing module to drive the
    hardware trace module logging class
    """
    tracer = HwTracer(debug,dongle_name,dongle_id)
    tracer.trace(pipe)

       
class AppsHWTraceAgent(PipeSubprocessAgent):
    """
    Use the HW trace module to grab a compressed program flow trace.
    """
    def __init__(self, debug, transport):
        # This only works if we're running on TRB
        if not isinstance(transport, TrbTransConnection):
            raise RuntimeError("Can only execute PC trace over TRB")
            
        dongle_name = transport.dongle_name
        dongle_id = transport.dongle_id
        self._debug = debug
        self._arg_tuple = (self._debug,dongle_name,dongle_id)

    @property
    def _target(self):
        return hw_trace

    @property
    def _args(self):
        return self._arg_tuple
    """
    Additional method to pass a 'flag' to the sub-process to stop logging.
    """   
    def finish(self):
        if self._debug:
            iprint("Request finish")
        self._sink.send("End")

       

