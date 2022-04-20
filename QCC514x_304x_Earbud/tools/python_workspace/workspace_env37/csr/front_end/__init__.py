############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2011 - 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
# Handy place to hang onto the most recent command line options.
import csr
csr.config_options = None

import logging

# Insert "TRACE" and "TRACE_VERBOSE" as logging levels in between "DEBUG" and 
# "INFO"
LOGGING_TRACE = 12
LOGGING_TRACE_VERBOSE = 11
logging.addLevelName(LOGGING_TRACE, "TRACE")
logging.addLevelName(LOGGING_TRACE_VERBOSE, "TRACE_VERBOSE")
logging.TRACE = LOGGING_TRACE
logging.TRACE_VERBOSE = LOGGING_TRACE_VERBOSE

def trace(self, message, *args, **kwargs):
    if self.isEnabledFor(LOGGING_TRACE):
        self._log(LOGGING_TRACE, message, args, **kwargs)
def trace_verbose(self, message, *args, **kwargs):
    if self.isEnabledFor(LOGGING_TRACE_VERBOSE):
        self._log(LOGGING_TRACE_VERBOSE, message, args, **kwargs)

logging.Logger.trace = trace
logging.Logger.trace_verbose = trace_verbose