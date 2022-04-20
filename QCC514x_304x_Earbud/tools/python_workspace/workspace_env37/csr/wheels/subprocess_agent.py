############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from multiprocessing import Process, Pipe
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import PureVirtualError

class ISubprocessAgent(object):
    """
    Interface to simple agents controlling data-source subprocesses.  These can
    either be file- or pipe-based.  Usage is straightforward:
     - start() starts the subprocess
     - get() returns the latest data
     - stop() kills the subprocess
     
    Implementations must override the _target method, returning a *function*
    (not a method) that is what the subprocess will run, and the _args method,
    returning a tuple of arguments the subprocess will pass to the function
    when it calls it.  This tuple must be picklable (Google can tell you what
    that means); it may also be modified in specified ways by particular types 
    of SubprocessAgent, e.g. PipeSubprocessAgent appends a pipe to the argument
    list, so _args is not necessarily identical to the argument list of _target.
     
    """
    def start(self):
        """
        Start the function with the given args in a subprocess
        """
        raise PureVirtualError
    
    def get(self):
        """
        Get all the data produced since the last call to get
        """
        raise PureVirtualError
    
    def stop(self):
        """
        Kill the subprocess.  Any data not already "got" will be lost
        """
        raise PureVirtualError

    @property
    def _target(self):
        """
        The function that the subprocess should run
        """
        raise PureVirtualError
        
    @property
    def _args(self):
        """
        The tuple of arguments that the function should be passed.  Must be
        picklable
        """
        raise PureVirtualError


class PipeSubprocessAgent(ISubprocessAgent):
    """
    Simple framework for controlling a subprocess which sits in a loop writing
    data to a supplied pipe, e.g. a debug log poller or a program counter tracer.
    
    This class must be subclassed, and the subclass should provide _target and
    _args object members: _target is a simple function taking arguments 
    <_args>,source
    
    NOTE. The size of the PIPE is determined by the underlying OS. It will 
    typically by in the order of several Kilobytes. Therefore this class 
    should not be used as a log for later retrieval, data should be extracted
    at roughly the rate it is generated.     
    """
    
    def start(self):
        """
        Create a new slave process, passing it the required addresses for
        performing the read, and a handle on the pipe to write to
        """
        source, self._sink = Pipe()
        
        self._p = Process(target=self._target, 
                          args=self._args + (source,))
        self._p.start()
        

    def get(self):
        """
        Pull raw bytes out of the pipe from the slave
        """
        # Don't just do a blocking read - it seems to make the script 
        # un-killable.  If there's nothing here we'll come back later.
        if not self._sink.poll(1.0):
            return []
        
        return self._sink.recv()

    def stop(self):
        """
        Stop the subprocess.  It's up to the caller to get the rest of the
        data out of the pipe before this point.
        """
        self._sink.close()
        self._p.terminate()


class FileSubprocessAgent(ISubprocessAgent):
    """
    Variant of SubprocessAgent that writes to a file rather than a pipe, 
    presumably because a large enough quantity of data might be produced 
    in between the client's calls to get that a pipe might overflow.  It is
    the subprocess function's responsibility to flush writes at reasonable
    intervals as it could be killed at any time. 
    """
    def start(self):
        """
        Start the subprocess, appending the filename to the standard argument
        list
        """
        self._p = Process(target=self._target, args=self._args+(self._filename,))
        self._p.start()
        
    def get(self):
        """
        Read the most recently written contents of the file.  There is no
        need to call this function if all that's required is for a file to be
        written containing the necessary data.
        """
        try:
            self._filestream
        except AttributeError:
            try:
                self._filestream = open(self._filename)
            except IOError:
                # The subprocess hasn't opened the file
                iprint ("WARNING: SubprocessFileAgent.get called before "
                       "subprocess opened file!")
            return self._filestream.read()
        
    def stop(self):
        """
        Kill the subprocess, closing the read filestream if it has been opened
        """
        try:
            self._filestream.close()
        except AttributeError:
            pass
        try:
            self._p.terminate()
        except AttributeError:
            pass
        
    @property
    def _filename(self):
        """
        Filename accessor to be overridden by subclass
        """
        raise PureVirtualError
    
