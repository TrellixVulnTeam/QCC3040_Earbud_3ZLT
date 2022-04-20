#!/usr/bin/env python
# Copyright (c) 2016 Qualcomm Technologies International, Ltd.
#   %%version
from csr.wheels.global_streams import iprint
import os
import time

# chose an implementation, depending on os
if os.name == 'nt':
    try:
        from .ptap_device_win32 import Win32Ptap as Ptap
    except ImportError:
        Win32Ptap = object
        class Ptap(Win32Ptap):
            pass
elif os.name == 'posix':
    from .ptap_device_linux import LinuxPtap as Ptap
else:
    raise ImportError("Platform ('%s') is not supported" % (os.name,))


# test
if __name__ == '__main__':
    ptap = Ptap()
    ptap.open()
    ptap.write([0x11, 0])
    for i in range(10):
        rx = ptap.read()
        if rx:
            iprint("Received len %d: %s" % (len(rx), " ".join(["%02x" % a for a in rx])))
            break
        time.sleep(0.01)
    ptap.write([int(a,16) for a in '30 00 00 00 02 04 64 00 00 04 00 00'.split()])
    for i in range(10):
        rx = ptap.read()
        if rx:
            iprint("Received len %d: %s" % (len(rx), " ".join(["%02x" % a for a in rx])))
            break
        time.sleep(0.01)


