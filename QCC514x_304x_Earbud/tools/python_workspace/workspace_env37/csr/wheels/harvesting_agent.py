############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Module to act as a generic master to a slave process that performs various
actions 
"""

from itertools import izip
from multiprocessing import Process, Pipe

def harvest(harvesters, pipes):
    """
    Run the harvesters in a round-robin fashion.  A "harvester" is a fn, args 
    tuple; a pipe is just a pipe, intended for writing to. There must be the 
    same number of harvesters as pipes.

    This is the function the slave process actually runs.
    """
    while 1:
        for ((fn, args),pipe) in izip(harvesters, pipes):
            result = fn(args)
            if result is not None:
                pipe.send(result)
                


class HarvestingAgent(object):
    """
    Simple framework for executing a set of "harvesters" in a round-robin fashion
    in a slave process.  Clients add "harvesters" by supplying a (function,
    arglist) tuple; a pipe is returned which the client should retain to read
    from whenever it wants data 
    """
    
    def __init__(self):
        
        self._harvesters = []
        self._pipes = []
        
    def add_harvester(self, fn, args):
        """
        Registers the given function + args as a harvester, returning the read
        end of its pipe.  This is only possible before the start() method is
        called.
        
        Note that function and args must be picklable. See
        
        https://docs.python.org/2/library/pickle.html#what-can-be-pickled-and-unpickled
        """
        try:
            self._p
            raise RuntimeError("HarvestingAgent: can't add harvester after "
                               "starting slave process!")
        except AttributeError:
            self._harvesters.append((fn, args))
            pipe = Pipe(duplex = False)
            self._pipes.append(pipe)
            return pipe[0]
        
    
    def start(self):
        """
        Fires off the slave process.  All harvesters should have been added by
        this time
        """
        # Set up and run the collective process
        pipe_sources = [pipe[1] for pipe in self._pipes]
        self._p = self._create_process(pipe_sources)
        self._p.start()
    
                
    def stop(self):
        """
        Close all the pipes and kill the slave process.
        
        CLIENTS MUST NOT TRY TO READ FROM A HARVESTER'S PIPE AFTER THIS
        """
        for sink, _ in self._pipes:
            sink.close()
        self._p.terminate()

    def _create_process(self, pipe_sources):
        return Process(target=harvest, args=(self._harvesters,
                                             pipe_sources))



def read_pipe(sink, max_reads = None):
    """
    Helper function for harvester clients that want to read their pipe
    """
    data = []
    i = 0
    while sink.poll(0.1) and (max_reads is None or i < max_reads):
        i += 1
        data.append(sink.recv())
    return data


        