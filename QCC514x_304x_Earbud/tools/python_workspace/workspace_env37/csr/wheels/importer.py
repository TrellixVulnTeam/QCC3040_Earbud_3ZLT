from types import ModuleType
import os
import sys
import importlib


def load_from_spec(spec):
    """
    Simplified import machinery.

    spec is a ModuleSpec object; path is the full file path to it
    
    Works for source-based modules and packages. Doesn't work for non-sourced-
    based, nor for namespace packages.
    
    """

    module = spec.loader.create_module(spec)
    
    if module is None:
        module = ModuleType(spec.name)
        
    # Populate attributes
    module.__name__ = spec.name
    module.__loader__ = spec.loader
    module.__package__ = spec.parent
    module.__spec__ = spec
    
    if spec.origin is None or not os.path.exists(spec.origin):
        raise ValueError("load_from_spec can't handle non-file-based modules/packages or namespace packages")
    
    module.__file__ = spec.origin
    if os.path.basename(module.__file__) == "__init__.py":
        module.__path__ = [os.path.dirname(module.__file__)]

    sys.modules[module.__name__] = module
    try:
        # Now execute the module
        spec.loader.exec_module(module)
    except BaseException:
        del sys.modules[module.__name__]
        raise

    return module

class DuplicateImportError(ImportError):
    """
    Indicates an attempt to manually import a package that clashes with a
    package that is already imported.
    """

def import_source_module_from_path(search_path, module_name, allow_replacement=False):
    """
    Imports a source-code module with the given name from the given search path,
    returning the module object.
    
    If the module can't be imported returns None, rather than raising ImportError.
    
    Note: this function ensures that previous imports of modules which have the
    given name don't interfere with this import.  Hence it is possible to
    import multiple modules with the same name from different search locations at
    different times.
    
    WARNING: if allow_replacement is set to True, this function may remove or replace 
    entries in sys.modules.  This is only safe if the removed/replaced module 
    performs all its own imports at import time, rather than at runtime.  An
    attempt to do a relative import after the parent package's entry in sys.modules
    has been replaced is not safe.
    """
    mod_cmpts = module_name.split(".")
    sub_mods = mod_cmpts[1:]


    # Find if there are already any modules imported from a top-level package
    # (TLP) with the same name as the requested TLP.  If so, there are
    # three possibilities:
    # 1. allow_replacement is False, and the existing TLP's __path__ doesn't
    # match the path we're looking on now - this is an error
    # 2. allow_replacement is False but the existing TLP's __path__ does match
    # the path we're looking on now - this simply means we are importing from
    # the same TLP as before, so we're safe to continue
    # 3. allow_replacement is True - this means the user is asserting that there
    # is no possibility of danger from imports within the module sources.
    # For example this can (and indeed must) be used for io_struct files from
    # CSR-style digital build directories, because those files are all named 
    # "io_struct.py" and must be imported as top-level modules, so replacement
    # is inevitable; but they are completely import-free, so replacement is
    # also safe.
    pkg_names = [name for name in sys.modules 
                 if name == mod_cmpts[0] or name.startswith(mod_cmpts[0]+".")]
    if pkg_names and not allow_replacement:
        # The only time this is legal is if the parent package is the same one
        # that we are looking for
        if all(os.path.dirname(pth) != search_path for pth in sys.modules[mod_cmpts[0]].__path__):
            raise DuplicateImportError("Attempting to import '{}' from {}, but the following module(s) "
                              "are already imported: {}".format(module_name, search_path,
                            ", ".join("{} ({})".format(p, sys.modules[p].__file__) 
                                      for p in pkg_names)))
    for pkg_name in pkg_names:
        del sys.modules[pkg_name]
    
    # If importlib.machinery is available (i.e. we're on Python 3) we'll use that
    # because it's more powerful and explicit.  And also imp is deprecated.
    try:
        import importlib.machinery
    except ImportError:
        # Py 2.7
        import imp
        
        # Import the top-level package/module
        try:
            file, pathname, description = imp.find_module(mod_cmpts[0], [search_path])
            try:
                module = imp.load_module(mod_cmpts[0], file, pathname, description)
            finally:
                if file is not None:
                    file.close()
        except ImportError:
            raise ImportError("Module not found using the Python 2 "
                              "imp module. Try using Python 3 instead.")

        # For any subpackages/modules import them one by one down the tree,
        # finally returning the requested module.  We use the previous module/package's
        # __path__ as our search path.
        for sub_mod in sub_mods:
            try:
                file, pathname, description = imp.find_module(sub_mod, module.__path__)
                try:
                    module = imp.load_module(".".join([module.__name__, sub_mod]),
                                             file, pathname, description)
                finally:
                    if file is not None:
                        file.close()
            except ImportError:
                raise ImportError("Module not found using the Python 2 "
                                  "imp module. Try using Python 3 instead.")
        
    else:
        # Py 3.  
        
        # The importlib.machinery classes don't involve sys.modules automatically
        # so we don't have to worry about returning an earlier import of a
        # module/package of the same name.
        
        # Import the top-level package/module
        spec = importlib.machinery.PathFinder.find_spec(mod_cmpts[0], [search_path])
        if spec is None:
            return None
        module = load_from_spec(spec)
        if module is None:
            return None
        
        # For any subpackages/modules import them one by one down the tree,
        # finally returning the requested module.  We use the previous module/package's
        # __path__ as our search path.
        for sub_mod in sub_mods:
            spec = importlib.machinery.PathFinder.find_spec(".".join([module.__name__, sub_mod]),
                                                           module.__path__)
            if spec is None:
                return None
            module = load_from_spec(spec)
            if module is None:
                return None

    return module

def import_source_file_as_module(source_file, allow_replacement=False):
    """
    Import the given source file as a top-level module
    """
    
    module, _ = os.path.splitext(os.path.basename(source_file))
    search_path = os.path.dirname(source_file)
    
    return import_source_module_from_path(search_path, module, 
                                          allow_replacement=allow_replacement)