# Copyright (c) 2014 - 2018, 2021 Qualcomm Technologies International, Ltd

import os
import sys
_mypath = os.path.abspath(os.path.dirname(__file__))
_changed_sys_path = _mypath not in sys.path
if _changed_sys_path:
    sys.path.append(_mypath)
from kalaccess import *
if _changed_sys_path:
    sys.path.append(_mypath)

print("kalspi.py has been renamed. Please use kalaccess.py instead")


class KalSpi(Kalaccess):
    pass
