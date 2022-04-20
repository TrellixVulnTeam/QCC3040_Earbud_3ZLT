############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import pkgutil
import importlib
import os

def get_factory_plugins():
    factory_plugins = {}
    
    here = os.path.realpath(os.path.dirname(__file__))
    
    for module_info in pkgutil.iter_modules([here]):
        
        mod = importlib.import_module("."+module_info[1], 
                                      package="csr.dev.hw.device.dynamic_factory")
        
        if hasattr(mod, "CHIP_VERSION_MAJOR") and hasattr(mod, "factory"):
            try:
                chip_versions = list(mod.CHIP_VERSION_MAJOR)
            except TypeError:
                chip_versions = [mod.CHIP_VERSION_MAJOR]
            for chip_version in chip_versions:
                factory_plugins[chip_version] = mod.factory
                
    return factory_plugins
