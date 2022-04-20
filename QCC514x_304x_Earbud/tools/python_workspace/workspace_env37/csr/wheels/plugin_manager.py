############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides support for finding and loading plugins for additional firmware analysis
layers
"""
import os
import sys
import importlib
import re

from csr.wheels import wprint
from csr.wheels.importer import import_source_module_from_path


class PluginManager(object):
    
    def __init__(self, plugin_options, default_root=None):
        """
        plugin_options is a list of paths to packages that might be suitable
        for loading as plugins.
        """
        self._plugin_options = plugin_options or []
        self._default_root = default_root
        
    def get_plugins(self):
        """
        Return a dictionary mapping plugins from the name they should be inserted
        under to the callable that constructs them
        """
        # At present there's no way to distinguish one from another because we 
        # don't have any notion of the application's identity, so we can't match
        # the running application to a plugin.  So we just require there to be
        # exactly one plugin in the list.
        
        if len(self._plugin_options) > 1:
            raise NotImplementedError("Multiple plugins specified/detected: "
                                      "application matching not implemented yet!")
            
        if not self._plugin_options:
            return {}
        plugin_path = None
        plugin_spec = self._plugin_options[0] 
        plugin_spec_parts = plugin_spec.split(":")
        if len(plugin_spec_parts) == 1 or not re.match(r"[\w\.]+", plugin_spec_parts[-1]):
            # Plugin spec is either a pure path or a pure package
            if not os.path.exists(plugin_spec):
                # It's a pure package - import it from the default package root
                if self._default_root is not None:
                    default_path = os.path.join(self._default_root, *self._plugin_options[0].split("."))
                    if os.path.exists(default_path):
                        plugin_path = self._default_root
                        package_name = self._plugin_options[0]
            else:
                # It's a pure path: import the final dir as a package from its
                # parent dir
                plugin_path = os.path.dirname(plugin_spec)
                package_name = os.path.basename(plugin_spec)
        else:
            # The spec gives a path followed by a multi-part package
            plugin_path = ":".join(plugin_spec_parts[:-1])
            package_name = plugin_spec_parts[-1]
        
                
        if plugin_path is None:
            wprint("Specified plugin '{}' is not valid.  No plugin will be loaded.".format(self._plugin_options[0]))
            plugins = {}
        else:

            plugin_package = import_source_module_from_path(plugin_path, package_name)
            if plugin_package:
                name = getattr(plugin_package, "PYDBG_PLUGIN_CONTAINER_NAME")
                factory = getattr(plugin_package, "PYDBG_PLUGIN_CONTAINER_CLASS")
                plugins = {name : factory}
            else:
                raise ImportError("Failed to import the specified plugin {} !".format(package_name))
            
        return plugins