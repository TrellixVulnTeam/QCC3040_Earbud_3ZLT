############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Utility routines for which we have no other home
"""

import os
import re

def is_murphy():
    '''
    Return True if we are on a Murphy host, as determined by the hostname.
    Hostname could be a bit "broken" e.g. Noneb1
    '''
    try:
        hostname = os.uname()[1]
    except AttributeError:
        return False
    if (re.match(r'camhydmur\d{3}b\d{1}', hostname, re.IGNORECASE)
            or re.match(r'camcurmur\d{3}b\d{1}', hostname, re.IGNORECASE)
            or re.match(r'camhydtc\d{2}v\d{2}', hostname, re.IGNORECASE)
            or re.match(r'Noneb\d{1}', hostname, re.IGNORECASE)
            or re.match(r'bansyststmur\d{3}b\d{1}', hostname, re.IGNORECASE)
            or re.match(r'camdevsysmrts2d\d{4}', hostname, re.IGNORECASE)):
        return True
    return False
    
def unique_idstring(device):
    '''
    Return a string which should be unique for the instance of pydbg which
    calls this method.
    '''
    return str(os.getpid())