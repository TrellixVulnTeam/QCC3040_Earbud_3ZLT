############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import os
import sys
from collections import namedtuple
from csr.wheels.bitsandbobs import NameSpace
import argparse


def _get_name_from_args(args):
    """
    The first args to ArgumentParser.add_argument should contain something from
    which we can derive a name
    """
    names = [a for a in args if not a.startswith("-")]
    long_args = [a.lstrip("-").replace("-","_") for a in args if a.startswith("--")]
    
    if names:
        return names[0]
    if long_args:
        return long_args[0]
    

class CmdlineOption(object):
    """
    This class is essentially a proxy for the arguments that are passed to
    ArgumentParser.add_argument, with one additional attribute "is_multitoken",
    which tells us whether to tweak the value of the option before passing it
    to argparse to work around a bug in argparse's handling of arguments-within-
    arguments.
    """
    
    def __init__(self, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs
        
        # Glean some details we'll need later
        self.dest = kwargs.get("dest") or _get_name_from_args(args)
        self.default = kwargs.get("default")
        self.is_multitoken = kwargs.pop("is_multitoken", False) # pop this because it's not 
        # known to argparse
        self.option_strings = args
        

class CmdlineOptions(object):
    """
    This class represents a set of command line options and their values.  It
    is designed to it easy to access the metadata associated with the options. 
    """
    
   
    def __init__(self, options, custom_cmdline=None, **kwargs):
        
        self._options = options
        self._custom_cmdline = custom_cmdline
        self._parser_kwargs = kwargs

        self._values = {}
        self._unknown_args = None

    @property
    def params(self):
        """
        Return a list of the parameter names
        """
        return [o.dest for o in self._options]

    @property
    def option_strings(self):
        return {o.dest : o.option_strings for o in self._options}

    @property
    def defaults(self):
        """
        Return a dictionary mapping parameters to their defaults
        """
        return {o.dest : o.default for o in self._options}

    def set_from_dict(self, settings):
        """
        Take the settings in the given dict and update the internal values 
        dictionary with them.  This is intended for test scenarios.
        """
        if not self._values:
            self._values = self.defaults
        valid_params = set(self.params)
        invalid_params = {param for param in settings if param not in valid_params}
        if invalid_params:
            raise ValueError("Parameter(s) '{}' not recognised - valid "
                             "parameters are {}".format(
                                 ",".join(sorted(invalid_params)), 
                                ", ".join(sorted(valid_params))))
        self._values.update(settings)
        self._options_set = set(settings)

    def _munge_multi_token_arg_values(self, cmdline):
        """
        Scan the cmdline token list for any occurrences of options flagged as
        'multitoken', and insert a space at the beginning of the multitoken
        value.  This prevents argparse interpreting anything amongst those
        tokens as arguments in their own right.
        """
        multitoken_option_strings = set()
        for opt in self._options:
            if opt.is_multitoken:
                multitoken_option_strings.update(opt.option_strings)
        
        # Map the option_strings (i.e. what's seen in cmdline tokens) to the
        # attributes of the namespace object that ArgumentParser returns. 
        options_strings_to_dest = {}
        for (dest, opt_strs) in self.option_strings.items():
            options_strings_to_dest.update({opt_str : dest  
                                                    for opt_str in opt_strs})
            
        # While we're at it, keep track of what options we're seeing, so we 
        # know what has been set explicitly
        options_set = set()
        
        add_space = False
        modified_dests = set()
        modified_cmdline = []
        for token in cmdline:
            modified_token = " " + token if add_space else token
            
            add_space = False
            if token in multitoken_option_strings:
                add_space = True # set this so the following token is modified
                modified_dests.add(options_strings_to_dest[token])
            modified_cmdline.append(modified_token)
            
            for opt, dest in options_strings_to_dest.items():
                if token.startswith(opt):
                    # This is an option rather than a pure value, so store the option
                    # name it sets
                    options_set.add(dest)

            
        return modified_cmdline, modified_dests, options_set
    
    def _unmunge_options(self, modified_dests, args):
        for dest in modified_dests:
            arg = getattr(args, dest)
            # Take the added space back out
            if not arg.startswith(" "):
                raise RuntimeError("{} is supposed to be munged, but its value is {}".format(dest, arg))
            setattr(args, dest, arg[1:])

    def _parse_cmdline(self):
        """
        Parse the command line, either the real one, or a custom one if supplied
        at construction time.  This method is invoked on demand by public
        methods in this class.
        """
        from optparse import OptionParser
        
        parser = argparse.ArgumentParser(**self._parser_kwargs)

        for option in self._options:
            # We set default to None so we can detect unset arguments and apply
            # env var settings ahead of the real defaults
            kwargs = option.kwargs
            kwargs.update({"default" : None})
            parser.add_argument(*option.args, **kwargs)

        if self._custom_cmdline is None:
            custom_cmdline = sys.argv[1:] # don't pass "pydbg.py" 
        else:
            custom_cmdline = self._custom_cmdline[:]
            if custom_cmdline == []:
                custom_cmdline = [""]
        
        # Now munge any arguments marked as "multi_token_value"
        custom_cmdline, tweaked_dests, self._options_set = self._munge_multi_token_arg_values(custom_cmdline)
        
        options, self._unknown_args = parser.parse_known_args(custom_cmdline)

        self._unmunge_options(tweaked_dests, options)

        self._values = {param : getattr(options, param) for param in self.params}
        
        # Set defaults from env vars if present; otherwise from the defined default
        for param, default in self.defaults.items():
            if self._values[param] is None:
                self._values[param] = os.getenv("PYDBG_{}".format(param.upper()), default)
    
    @property    
    def values(self):
        """
        Return a dictionary of values containing the setting specified via 
        command line and environment.
        """
        if not self._values:
            
            self._parse_cmdline()
            
        return self._values

    def is_set_explicitly(self, dest_name):
        if not self._values:
            self._parse_cmdline()
        return dest_name in self._options_set
    
    @property
    def unknown_args(self):
        """
        Return the extra unlabelled command line contents
        """
        if self._unknown_args is None:
            self._parse_cmdline()
        return self._unknown_args
    
    @property
    def namespace(self):
        """
        Return as a NameSpace object, resembling what optparser/argparser return
        """
        options = NameSpace()
        for name, value in self.values.items():
            setattr(options, name, value)
        return options
    
    def update_from_namespace(self, options_ns):
        """
        Take the given namespace and update the values dictionary with any
        matching settings from it.  Any settings not given by the namespace are
        left alone.
        """
        updated_values = {}
        for name, value in self.values.items():
            updated_values[name] = getattr(options_ns, name, value)
        self._values = updated_values