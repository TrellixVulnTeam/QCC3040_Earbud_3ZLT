############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
import sys
import re
from subprocess import PIPE, Popen
from threading  import Thread
from Queue import Queue, Empty

class CsrProcess(object):
    '''
    A replacement for the pexpect class that works under windows. Mainly for
    running omnicli from a python test script.
    '''
    
    class TIMEOUT(RuntimeError):
        '''
        Indicates a timeout when waiting for output from the process
        '''

    def __init__(self, command, tag="", working_directory=None, logfile=None):
        '''
        Constructor spawns the process using "command". Optional
        working_directory can be specified.
        Properties:
            timeout
            logfile
            tag
            log_user 
        '''
        self.match = None
        self.timeout = 10
        self.logfile = logfile
        self.tag = tag
        self.log_user = False
        _ON_POSIX = 'posix' in sys.builtin_module_names
        self.process = Popen(command, stdin=PIPE, stdout=PIPE, 
                             close_fds=_ON_POSIX, cwd=working_directory)
        self.queue = Queue()
        self.read_thread = Thread(target=self.process_read_thread, 
                                 args=(self.process.stdout, self.queue))
        self.read_thread.daemon = True
        self.read_thread.start()
        
    def sendline(self, line):
        '''
        Send the string to the omnicli process and record it in the logfile
        and console if appropriate.
        '''
        if self.log_user:
            iprint("%s Tx: %s" % (self.tag, line))
        if self.logfile:
            self.logfile.write(self.tag + line + '\n')
        self.process.stdin.write(line + '\n')
        
    @staticmethod
    def process_read_thread(process_output, queue):
        '''
        Pass anything received on the process_output onto the queue so
        it can be processed in a non-blocking way. 
        '''
        for line in iter(process_output.readline, ''):
            queue.put(line)
        process_output.close()
        
    def readline(self, timeout=None):
        '''
        Read from the process queue with a timeout and record anything 
        received to the logfile. 
        '''
        try:
            line = self.queue.get(timeout=timeout or self.timeout)
        except Empty:
            raise self.TIMEOUT
        self.queue.task_done()
        if self.log_user:
            iprint("%s Rx: %s" % (self.tag, line),)
        if self.logfile:
            self.logfile.write(self.tag + line)
        return line
        
    def expect(self, match_strings, timeout=None):
        '''
        Implementation of pexpect.expect function. Takes a list of regular
        expressions and returns the index of the one where a match was found.
        This differs from the Omnicli.expect_raw_line which requires the line
        to start with a time stamp.
        If no match is found an Omnicli.TIMEOUT exception is raised. 
        The match object is equivalent to the pexpect one - the
        matched fields can be retrieved with pid.match.group(1) etc.
        '''
        while True:
            line = self.readline(timeout=timeout)
            for index,match in enumerate(match_strings):
                self.match = re.search(match, line)
                if self.match:
                    return index
                
    def flush(self):
        '''
        Read everything from the process output
        '''
        while not self.queue.empty():
            self.queue.get()
            self.queue.task_done()

    def terminate(self):
        self.process.terminate()
        self.wait()
    
    def wait(self):
        self.process.wait()
